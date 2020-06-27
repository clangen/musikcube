/**************************************************************************
    copyright            : (C) 2007 by Lukáš Lalinský
    email                : lalinsky@gmail.com
 **************************************************************************/

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

#include <tdebug.h>
#include <tstring.h>
#include "mp4file.h"
#include "mp4atom.h"
#include "mp4properties.h"

using namespace TagLib;

class MP4::Properties::PropertiesPrivate
{
public:
  PropertiesPrivate() :
    length(0),
    bitrate(0),
    sampleRate(0),
    channels(0),
    bitsPerSample(0),
    encrypted(false),
    codec(MP4::Properties::Unknown) {}

  int length;
  int bitrate;
  int sampleRate;
  int channels;
  int bitsPerSample;
  bool encrypted;
  Codec codec;
};

////////////////////////////////////////////////////////////////////////////////
// public members
////////////////////////////////////////////////////////////////////////////////

MP4::Properties::Properties(File *file, MP4::Atoms *atoms, ReadStyle style) :
  AudioProperties(style),
  d(new PropertiesPrivate())
{
  read(file, atoms);
}

MP4::Properties::~Properties()
{
  delete d;
}

int
MP4::Properties::channels() const
{
  return d->channels;
}

int
MP4::Properties::sampleRate() const
{
  return d->sampleRate;
}

int
MP4::Properties::length() const
{
  return lengthInSeconds();
}

int
MP4::Properties::lengthInSeconds() const
{
  return d->length / 1000;
}

int
MP4::Properties::lengthInMilliseconds() const
{
  return d->length;
}

int
MP4::Properties::bitrate() const
{
  return d->bitrate;
}

int
MP4::Properties::bitsPerSample() const
{
  return d->bitsPerSample;
}

bool
MP4::Properties::isEncrypted() const
{
  return d->encrypted;
}

MP4::Properties::Codec
MP4::Properties::codec() const
{
  return d->codec;
}

////////////////////////////////////////////////////////////////////////////////
// private members
////////////////////////////////////////////////////////////////////////////////

void
MP4::Properties::read(File *file, Atoms *atoms)
{
  MP4::Atom *moov = atoms->find("moov");
  if(!moov) {
    debug("MP4: Atom 'moov' not found");
    return;
  }

  MP4::Atom *trak = 0;
  ByteVector data;

  const MP4::AtomList trakList = moov->findall("trak");
  for(MP4::AtomList::ConstIterator it = trakList.begin(); it != trakList.end(); ++it) {
    trak = *it;
    MP4::Atom *hdlr = trak->find("mdia", "hdlr");
    if(!hdlr) {
      debug("MP4: Atom 'trak.mdia.hdlr' not found");
      return;
    }
    file->seek(hdlr->offset);
    data = file->readBlock(hdlr->length);
    if(data.containsAt("soun", 16)) {
      break;
    }
    trak = 0;
  }
  if(!trak) {
    debug("MP4: No audio tracks");
    return;
  }

  MP4::Atom *mdhd = trak->find("mdia", "mdhd");
  if(!mdhd) {
    debug("MP4: Atom 'trak.mdia.mdhd' not found");
    return;
  }

  file->seek(mdhd->offset);
  data = file->readBlock(mdhd->length);

  const unsigned int version = data[8];
  long long unit;
  long long length;
  if(version == 1) {
    if(data.size() < 36 + 8) {
      debug("MP4: Atom 'trak.mdia.mdhd' is smaller than expected");
      return;
    }
    unit   = data.toUInt(28U);
    length = data.toLongLong(32U);
  }
  else {
    if(data.size() < 24 + 8) {
      debug("MP4: Atom 'trak.mdia.mdhd' is smaller than expected");
      return;
    }
    unit   = data.toUInt(20U);
    length = data.toUInt(24U);
  }
  if(unit > 0 && length > 0)
    d->length = static_cast<int>(length * 1000.0 / unit + 0.5);

  MP4::Atom *atom = trak->find("mdia", "minf", "stbl", "stsd");
  if(!atom) {
    return;
  }

  file->seek(atom->offset);
  data = file->readBlock(atom->length);
  if(data.containsAt("mp4a", 20)) {
    d->codec         = AAC;
    d->channels      = data.toShort(40U);
    d->bitsPerSample = data.toShort(42U);
    d->sampleRate    = data.toUInt(46U);
    if(data.containsAt("esds", 56) && data[64] == 0x03) {
      unsigned int pos = 65;
      if(data.containsAt("\x80\x80\x80", pos)) {
        pos += 3;
      }
      pos += 4;
      if(data[pos] == 0x04) {
        pos += 1;
        if(data.containsAt("\x80\x80\x80", pos)) {
          pos += 3;
        }
        pos += 10;
        d->bitrate = static_cast<int>((data.toUInt(pos) + 500) / 1000.0 + 0.5);
      }
    }
  }
  else if(data.containsAt("alac", 20)) {
    if(atom->length == 88 && data.containsAt("alac", 56)) {
      d->codec         = ALAC;
      d->bitsPerSample = data.at(69);
      d->channels      = data.at(73);
      d->bitrate       = static_cast<int>(data.toUInt(80U) / 1000.0 + 0.5);
      d->sampleRate    = data.toUInt(84U);
    }
  }

  MP4::Atom *drms = atom->find("drms");
  if(drms) {
    d->encrypted = true;
  }
}
