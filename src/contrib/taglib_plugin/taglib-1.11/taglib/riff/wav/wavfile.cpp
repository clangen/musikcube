/***************************************************************************
    copyright            : (C) 2008 by Scott Wheeler
    email                : wheeler@kde.org
 ***************************************************************************/

/***************************************************************************
 *   This library is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU Lesser General Public License version   *
 *   2.1 as published by the Free Software Foundation.                     *
 *                                                                         *
 *   This library is distributed in the hope that it will be useful, but   *
 *   WITHOUT ANY WARRANTY; without even the implied warranty of            *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU     *
 *   Lesser General Public License for more details.                       *
 *                                                                         *
 *   You should have received a copy of the GNU Lesser General Public      *
 *   License along with this library; if not, write to the Free Software   *
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA         *
 *   02110-1301  USA                                                       *
 *                                                                         *
 *   Alternatively, this file is available under the Mozilla Public        *
 *   License Version 1.1.  You may obtain a copy of the License at         *
 *   http://www.mozilla.org/MPL/                                           *
 ***************************************************************************/

#include "tbytevector.h"
#include "tdebug.h"
#include "tstringlist.h"
#include "tpropertymap.h"

#include "wavfile.h"
#include "id3v2tag.h"
#include "infotag.h"
#include "tagunion.h"

using namespace TagLib;

namespace
{
  enum { ID3v2Index = 0, InfoIndex = 1 };
}

class RIFF::WAV::File::FilePrivate
{
public:
  FilePrivate() :
    properties(0),
    hasID3v2(false),
    hasInfo(false) {}

  ~FilePrivate()
  {
    delete properties;
  }

  Properties *properties;
  TagUnion tag;

  bool hasID3v2;
  bool hasInfo;
};

////////////////////////////////////////////////////////////////////////////////
// public members
////////////////////////////////////////////////////////////////////////////////

RIFF::WAV::File::File(FileName file, bool readProperties, Properties::ReadStyle) :
  RIFF::File(file, LittleEndian),
  d(new FilePrivate())
{
  if(isOpen())
    read(readProperties);
}

RIFF::WAV::File::File(IOStream *stream, bool readProperties, Properties::ReadStyle) :
  RIFF::File(stream, LittleEndian),
  d(new FilePrivate())
{
  if(isOpen())
    read(readProperties);
}

RIFF::WAV::File::~File()
{
  delete d;
}

ID3v2::Tag *RIFF::WAV::File::tag() const
{
  return ID3v2Tag();
}

ID3v2::Tag *RIFF::WAV::File::ID3v2Tag() const
{
  return d->tag.access<ID3v2::Tag>(ID3v2Index, false);
}

RIFF::Info::Tag *RIFF::WAV::File::InfoTag() const
{
  return d->tag.access<RIFF::Info::Tag>(InfoIndex, false);
}

void RIFF::WAV::File::strip(TagTypes tags)
{
  removeTagChunks(tags);

  if(tags & ID3v2)
    d->tag.set(ID3v2Index, new ID3v2::Tag());

  if(tags & Info)
    d->tag.set(InfoIndex, new RIFF::Info::Tag());
}

PropertyMap RIFF::WAV::File::properties() const
{
  return d->tag.properties();
}

void RIFF::WAV::File::removeUnsupportedProperties(const StringList &unsupported)
{
  d->tag.removeUnsupportedProperties(unsupported);
}

PropertyMap RIFF::WAV::File::setProperties(const PropertyMap &properties)
{
  InfoTag()->setProperties(properties);
  return ID3v2Tag()->setProperties(properties);
}

RIFF::WAV::Properties *RIFF::WAV::File::audioProperties() const
{
  return d->properties;
}

bool RIFF::WAV::File::save()
{
  return RIFF::WAV::File::save(AllTags);
}

bool RIFF::WAV::File::save(TagTypes tags, bool stripOthers, int id3v2Version)
{
  if(readOnly()) {
    debug("RIFF::WAV::File::save() -- File is read only.");
    return false;
  }

  if(!isValid()) {
    debug("RIFF::WAV::File::save() -- Trying to save invalid file.");
    return false;
  }

  if(stripOthers)
    strip(static_cast<TagTypes>(AllTags & ~tags));

  if(tags & ID3v2) {
    removeTagChunks(ID3v2);

    if(ID3v2Tag() && !ID3v2Tag()->isEmpty()) {
      setChunkData("ID3 ", ID3v2Tag()->render(id3v2Version));
      d->hasID3v2 = true;
    }
  }

  if(tags & Info) {
    removeTagChunks(Info);

    if(InfoTag() && !InfoTag()->isEmpty()) {
      setChunkData("LIST", InfoTag()->render(), true);
      d->hasInfo = true;
    }
  }

  return true;
}

bool RIFF::WAV::File::hasID3v2Tag() const
{
  return d->hasID3v2;
}

bool RIFF::WAV::File::hasInfoTag() const
{
  return d->hasInfo;
}

////////////////////////////////////////////////////////////////////////////////
// private members
////////////////////////////////////////////////////////////////////////////////

void RIFF::WAV::File::read(bool readProperties)
{
  for(unsigned int i = 0; i < chunkCount(); ++i) {
    const ByteVector name = chunkName(i);
    if(name == "ID3 " || name == "id3 ") {
      if(!d->tag[ID3v2Index]) {
        d->tag.set(ID3v2Index, new ID3v2::Tag(this, chunkOffset(i)));
        d->hasID3v2 = true;
      }
      else {
        debug("RIFF::WAV::File::read() - Duplicate ID3v2 tag found.");
      }
    }
    else if(name == "LIST") {
      const ByteVector data = chunkData(i);
      if(data.startsWith("INFO")) {
        if(!d->tag[InfoIndex]) {
          d->tag.set(InfoIndex, new RIFF::Info::Tag(data));
          d->hasInfo = true;
        }
        else {
          debug("RIFF::WAV::File::read() - Duplicate INFO tag found.");
        }
      }
    }
  }

  if(!d->tag[ID3v2Index])
    d->tag.set(ID3v2Index, new ID3v2::Tag());

  if(!d->tag[InfoIndex])
    d->tag.set(InfoIndex, new RIFF::Info::Tag());

  if(readProperties)
    d->properties = new Properties(this, Properties::Average);
}

void RIFF::WAV::File::removeTagChunks(TagTypes tags)
{
  if((tags & ID3v2) && d->hasID3v2) {
    removeChunk("ID3 ");
    removeChunk("id3 ");

    d->hasID3v2 = false;
  }

  if((tags & Info) && d->hasInfo) {
    for(int i = static_cast<int>(chunkCount()) - 1; i >= 0; --i) {
      if(chunkName(i) == "LIST" && chunkData(i).startsWith("INFO"))
        removeChunk(i);
    }

    d->hasInfo = false;
  }
}
