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

#include <tbytevector.h>
#include <tdebug.h>
#include <id3v2tag.h>
#include <tstringlist.h>
#include <tpropertymap.h>

#include "aifffile.h"

using namespace TagLib;

class RIFF::AIFF::File::FilePrivate
{
public:
  FilePrivate() :
    properties(0),
    tag(0),
    hasID3v2(false) {}

  ~FilePrivate()
  {
    delete properties;
    delete tag;
  }

  Properties *properties;
  ID3v2::Tag *tag;

  bool hasID3v2;
};

////////////////////////////////////////////////////////////////////////////////
// public members
////////////////////////////////////////////////////////////////////////////////

RIFF::AIFF::File::File(FileName file, bool readProperties, Properties::ReadStyle) :
  RIFF::File(file, BigEndian),
  d(new FilePrivate())
{
  if(isOpen())
    read(readProperties);
}

RIFF::AIFF::File::File(IOStream *stream, bool readProperties, Properties::ReadStyle) :
  RIFF::File(stream, BigEndian),
  d(new FilePrivate())
{
  if(isOpen())
    read(readProperties);
}

RIFF::AIFF::File::~File()
{
  delete d;
}

ID3v2::Tag *RIFF::AIFF::File::tag() const
{
  return d->tag;
}

PropertyMap RIFF::AIFF::File::properties() const
{
  return d->tag->properties();
}

void RIFF::AIFF::File::removeUnsupportedProperties(const StringList &unsupported)
{
  d->tag->removeUnsupportedProperties(unsupported);
}

PropertyMap RIFF::AIFF::File::setProperties(const PropertyMap &properties)
{
  return d->tag->setProperties(properties);
}

RIFF::AIFF::Properties *RIFF::AIFF::File::audioProperties() const
{
  return d->properties;
}

bool RIFF::AIFF::File::save()
{
  if(readOnly()) {
    debug("RIFF::AIFF::File::save() -- File is read only.");
    return false;
  }

  if(!isValid()) {
    debug("RIFF::AIFF::File::save() -- Trying to save invalid file.");
    return false;
  }

  if(d->hasID3v2) {
    removeChunk("ID3 ");
    removeChunk("id3 ");
    d->hasID3v2 = false;
  }

  if(tag() && !tag()->isEmpty()) {
    setChunkData("ID3 ", d->tag->render());
    d->hasID3v2 = true;
  }

  return true;
}

bool RIFF::AIFF::File::hasID3v2Tag() const
{
  return d->hasID3v2;
}

////////////////////////////////////////////////////////////////////////////////
// private members
////////////////////////////////////////////////////////////////////////////////

void RIFF::AIFF::File::read(bool readProperties)
{
  for(unsigned int i = 0; i < chunkCount(); ++i) {
    const ByteVector name = chunkName(i);
    if(name == "ID3 " || name == "id3 ") {
      if(!d->tag) {
        d->tag = new ID3v2::Tag(this, chunkOffset(i));
        d->hasID3v2 = true;
      }
      else {
        debug("RIFF::AIFF::File::read() - Duplicate ID3v2 tag found.");
      }
    }
  }

  if(!d->tag)
    d->tag = new ID3v2::Tag();

  if(readProperties)
    d->properties = new Properties(this, Properties::Average);
}
