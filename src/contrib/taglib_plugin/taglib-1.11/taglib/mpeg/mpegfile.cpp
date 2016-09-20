/***************************************************************************
    copyright            : (C) 2002 - 2008 by Scott Wheeler
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

#include <tagunion.h>
#include <tagutils.h>
#include <id3v2tag.h>
#include <id3v2header.h>
#include <id3v1tag.h>
#include <apefooter.h>
#include <apetag.h>
#include <tdebug.h>

#include "mpegfile.h"
#include "mpegheader.h"
#include "mpegutils.h"
#include "tpropertymap.h"

using namespace TagLib;

namespace
{
  enum { ID3v2Index = 0, APEIndex = 1, ID3v1Index = 2 };
}

class MPEG::File::FilePrivate
{
public:
  FilePrivate(const ID3v2::FrameFactory *frameFactory = ID3v2::FrameFactory::instance()) :
    ID3v2FrameFactory(frameFactory),
    ID3v2Location(-1),
    ID3v2OriginalSize(0),
    APELocation(-1),
    APEOriginalSize(0),
    ID3v1Location(-1),
    properties(0) {}

  ~FilePrivate()
  {
    delete properties;
  }

  const ID3v2::FrameFactory *ID3v2FrameFactory;

  long ID3v2Location;
  long ID3v2OriginalSize;

  long APELocation;
  long APEOriginalSize;

  long ID3v1Location;

  TagUnion tag;

  Properties *properties;
};

////////////////////////////////////////////////////////////////////////////////
// public members
////////////////////////////////////////////////////////////////////////////////

MPEG::File::File(FileName file, bool readProperties, Properties::ReadStyle) :
  TagLib::File(file),
  d(new FilePrivate())
{
  if(isOpen())
    read(readProperties);
}

MPEG::File::File(FileName file, ID3v2::FrameFactory *frameFactory,
                 bool readProperties, Properties::ReadStyle) :
  TagLib::File(file),
  d(new FilePrivate(frameFactory))
{
  if(isOpen())
    read(readProperties);
}

MPEG::File::File(IOStream *stream, ID3v2::FrameFactory *frameFactory,
                 bool readProperties, Properties::ReadStyle) :
  TagLib::File(stream),
  d(new FilePrivate(frameFactory))
{
  if(isOpen())
    read(readProperties);
}

MPEG::File::~File()
{
  delete d;
}

TagLib::Tag *MPEG::File::tag() const
{
  return &d->tag;
}

PropertyMap MPEG::File::properties() const
{
  return d->tag.properties();
}

void MPEG::File::removeUnsupportedProperties(const StringList &properties)
{
  d->tag.removeUnsupportedProperties(properties);
}

PropertyMap MPEG::File::setProperties(const PropertyMap &properties)
{
  // update ID3v1 tag if it exists, but ignore the return value

  if(ID3v1Tag())
    ID3v1Tag()->setProperties(properties);

  return ID3v2Tag(true)->setProperties(properties);
}

MPEG::Properties *MPEG::File::audioProperties() const
{
  return d->properties;
}

bool MPEG::File::save()
{
  return save(AllTags);
}

bool MPEG::File::save(int tags)
{
  return save(tags, true);
}

bool MPEG::File::save(int tags, bool stripOthers)
{
  return save(tags, stripOthers, 4);
}

bool MPEG::File::save(int tags, bool stripOthers, int id3v2Version)
{
  return save(tags, stripOthers, id3v2Version, true);
}

bool MPEG::File::save(int tags, bool stripOthers, int id3v2Version, bool duplicateTags)
{
  if(readOnly()) {
    debug("MPEG::File::save() -- File is read only.");
    return false;
  }

  // Create the tags if we've been asked to.

  if(duplicateTags) {

    // Copy the values from the tag that does exist into the new tag,
    // except if the existing tag is to be stripped.

    if((tags & ID3v2) && ID3v1Tag() && !(stripOthers && !(tags & ID3v1)))
      Tag::duplicate(ID3v1Tag(), ID3v2Tag(true), false);

    if((tags & ID3v1) && d->tag[ID3v2Index] && !(stripOthers && !(tags & ID3v2)))
      Tag::duplicate(ID3v2Tag(), ID3v1Tag(true), false);
  }

  // Remove all the tags not going to be saved.

  if(stripOthers)
    strip(~tags, false);

  if(ID3v2 & tags) {

    if(ID3v2Tag() && !ID3v2Tag()->isEmpty()) {

      // ID3v2 tag is not empty. Update the old one or create a new one.

      if(d->ID3v2Location < 0)
        d->ID3v2Location = 0;

      const ByteVector data = ID3v2Tag()->render(id3v2Version);
      insert(data, d->ID3v2Location, d->ID3v2OriginalSize);

      if(d->APELocation >= 0)
        d->APELocation += (static_cast<long>(data.size()) - d->ID3v2OriginalSize);

      if(d->ID3v1Location >= 0)
        d->ID3v1Location += (static_cast<long>(data.size()) - d->ID3v2OriginalSize);

      d->ID3v2OriginalSize = data.size();
    }
    else {

      // ID3v2 tag is empty. Remove the old one.

      strip(ID3v2, false);
    }
  }

  if(ID3v1 & tags) {

    if(ID3v1Tag() && !ID3v1Tag()->isEmpty()) {

      // ID3v1 tag is not empty. Update the old one or create a new one.

      if(d->ID3v1Location >= 0) {
        seek(d->ID3v1Location);
      }
      else {
        seek(0, End);
        d->ID3v1Location = tell();
      }

      writeBlock(ID3v1Tag()->render());
    }
    else {

      // ID3v1 tag is empty. Remove the old one.

      strip(ID3v1, false);
    }
  }

  if(APE & tags) {

    if(APETag() && !APETag()->isEmpty()) {

      // APE tag is not empty. Update the old one or create a new one.

      if(d->APELocation < 0) {
        if(d->ID3v1Location >= 0)
          d->APELocation = d->ID3v1Location;
        else
          d->APELocation = length();
      }

      const ByteVector data = APETag()->render();
      insert(data, d->APELocation, d->APEOriginalSize);

      if(d->ID3v1Location >= 0)
        d->ID3v1Location += (static_cast<long>(data.size()) - d->APEOriginalSize);

      d->APEOriginalSize = data.size();
    }
    else {

      // APE tag is empty. Remove the old one.

      strip(APE, false);
    }
  }

  return true;
}

ID3v2::Tag *MPEG::File::ID3v2Tag(bool create)
{
  return d->tag.access<ID3v2::Tag>(ID3v2Index, create);
}

ID3v1::Tag *MPEG::File::ID3v1Tag(bool create)
{
  return d->tag.access<ID3v1::Tag>(ID3v1Index, create);
}

APE::Tag *MPEG::File::APETag(bool create)
{
  return d->tag.access<APE::Tag>(APEIndex, create);
}

bool MPEG::File::strip(int tags)
{
  return strip(tags, true);
}

bool MPEG::File::strip(int tags, bool freeMemory)
{
  if(readOnly()) {
    debug("MPEG::File::strip() - Cannot strip tags from a read only file.");
    return false;
  }

  if((tags & ID3v2) && d->ID3v2Location >= 0) {
    removeBlock(d->ID3v2Location, d->ID3v2OriginalSize);

    if(d->APELocation >= 0)
      d->APELocation -= d->ID3v2OriginalSize;

    if(d->ID3v1Location >= 0)
      d->ID3v1Location -= d->ID3v2OriginalSize;

    d->ID3v2Location = -1;
    d->ID3v2OriginalSize = 0;

    if(freeMemory)
      d->tag.set(ID3v2Index, 0);
  }

  if((tags & ID3v1) && d->ID3v1Location >= 0) {
    truncate(d->ID3v1Location);

    d->ID3v1Location = -1;

    if(freeMemory)
      d->tag.set(ID3v1Index, 0);
  }

  if((tags & APE) && d->APELocation >= 0) {
    removeBlock(d->APELocation, d->APEOriginalSize);

    if(d->ID3v1Location >= 0)
      d->ID3v1Location -= d->APEOriginalSize;

    d->APELocation = -1;
    d->APEOriginalSize = 0;

    if(freeMemory)
      d->tag.set(APEIndex, 0);
  }

  return true;
}

void MPEG::File::setID3v2FrameFactory(const ID3v2::FrameFactory *factory)
{
  d->ID3v2FrameFactory = factory;
}

long MPEG::File::nextFrameOffset(long position)
{
  bool foundLastSyncPattern = false;

  ByteVector buffer;

  while(true) {
    seek(position);
    buffer = readBlock(bufferSize());

    if(buffer.size() <= 0)
      return -1;

    if(foundLastSyncPattern && secondSynchByte(buffer[0]))
      return position - 1;

    for(unsigned int i = 0; i < buffer.size() - 1; i++) {
      if(firstSyncByte(buffer[i]) && secondSynchByte(buffer[i + 1]))
        return position + i;
    }

    foundLastSyncPattern = firstSyncByte(buffer[buffer.size() - 1]);
    position += buffer.size();
  }
}

long MPEG::File::previousFrameOffset(long position)
{
  bool foundFirstSyncPattern = false;
  ByteVector buffer;

  while (position > 0) {
    long size = std::min<long>(position, bufferSize());
    position -= size;

    seek(position);
    buffer = readBlock(size);

    if(buffer.size() <= 0)
      break;

    if(foundFirstSyncPattern && firstSyncByte(buffer[buffer.size() - 1]))
      return position + buffer.size() - 1;

    for(int i = buffer.size() - 2; i >= 0; i--) {
      if(firstSyncByte(buffer[i]) && secondSynchByte(buffer[i + 1]))
        return position + i;
    }

    foundFirstSyncPattern = secondSynchByte(buffer[0]);
  }
  return -1;
}

long MPEG::File::firstFrameOffset()
{
  long position = 0;

  if(hasID3v2Tag())
    position = d->ID3v2Location + ID3v2Tag()->header()->completeTagSize();

  return nextFrameOffset(position);
}

long MPEG::File::lastFrameOffset()
{
  long position;

  if(hasAPETag())
    position = d->APELocation - 1;
  else if(hasID3v1Tag())
    position = d->ID3v1Location - 1;
  else
    position = length();

  return previousFrameOffset(position);
}

bool MPEG::File::hasID3v1Tag() const
{
  return (d->ID3v1Location >= 0);
}

bool MPEG::File::hasID3v2Tag() const
{
  return (d->ID3v2Location >= 0);
}

bool MPEG::File::hasAPETag() const
{
  return (d->APELocation >= 0);
}

////////////////////////////////////////////////////////////////////////////////
// private members
////////////////////////////////////////////////////////////////////////////////

void MPEG::File::read(bool readProperties)
{
  // Look for an ID3v2 tag

  d->ID3v2Location = findID3v2();

  if(d->ID3v2Location >= 0) {
    d->tag.set(ID3v2Index, new ID3v2::Tag(this, d->ID3v2Location, d->ID3v2FrameFactory));
    d->ID3v2OriginalSize = ID3v2Tag()->header()->completeTagSize();
  }

  // Look for an ID3v1 tag

  d->ID3v1Location = Utils::findID3v1(this);

  if(d->ID3v1Location >= 0)
    d->tag.set(ID3v1Index, new ID3v1::Tag(this, d->ID3v1Location));

  // Look for an APE tag

  d->APELocation = Utils::findAPE(this, d->ID3v1Location);

  if(d->APELocation >= 0) {
    d->tag.set(APEIndex, new APE::Tag(this, d->APELocation));
    d->APEOriginalSize = APETag()->footer()->completeTagSize();
    d->APELocation = d->APELocation + APE::Footer::size() - d->APEOriginalSize;
  }

  if(readProperties)
    d->properties = new Properties(this);

  // Make sure that we have our default tag types available.

  ID3v2Tag(true);
  ID3v1Tag(true);
}

long MPEG::File::findID3v2()
{
  if(!isValid())
    return -1;

  // An ID3v2 tag or MPEG frame is most likely be at the beginning of the file.

  const ByteVector headerID = ID3v2::Header::fileIdentifier();

  seek(0);

  const ByteVector data = readBlock(headerID.size());
  if(data.size() < headerID.size())
    return -1;

  if(data == headerID)
    return 0;

  if(firstSyncByte(data[0]) && secondSynchByte(data[1]))
    return -1;

  // Look for the entire file, if neither an MEPG frame or ID3v2 tag was found
  // at the beginning of the file.
  // We don't care about the inefficiency of the code, since this is a seldom case.

  const long tagOffset = find(headerID);
  if(tagOffset < 0)
    return -1;

  const long frameOffset = firstFrameOffset();
  if(frameOffset < tagOffset)
    return -1;

  return tagOffset;
}
