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

#include <tstring.h>
#include <tdebug.h>
#include "aifffile.h"
#include "aiffproperties.h"

using namespace TagLib;

class RIFF::AIFF::Properties::PropertiesPrivate
{
public:
  PropertiesPrivate() :
    length(0),
    bitrate(0),
    sampleRate(0),
    channels(0),
    bitsPerSample(0),
    sampleFrames(0) {}

  int length;
  int bitrate;
  int sampleRate;
  int channels;
  int bitsPerSample;

  ByteVector compressionType;
  String compressionName;

  unsigned int sampleFrames;
};

////////////////////////////////////////////////////////////////////////////////
// public members
////////////////////////////////////////////////////////////////////////////////

RIFF::AIFF::Properties::Properties(const ByteVector &, ReadStyle style) :
  AudioProperties(style),
  d(new PropertiesPrivate())
{
  debug("RIFF::AIFF::Properties::Properties() - This constructor is no longer used.");
}

RIFF::AIFF::Properties::Properties(File *file, ReadStyle style) :
  AudioProperties(style),
  d(new PropertiesPrivate())
{
  read(file);
}

RIFF::AIFF::Properties::~Properties()
{
  delete d;
}

int RIFF::AIFF::Properties::length() const
{
  return lengthInSeconds();
}

int RIFF::AIFF::Properties::lengthInSeconds() const
{
  return d->length / 1000;
}

int RIFF::AIFF::Properties::lengthInMilliseconds() const
{
  return d->length;
}

int RIFF::AIFF::Properties::bitrate() const
{
  return d->bitrate;
}

int RIFF::AIFF::Properties::sampleRate() const
{
  return d->sampleRate;
}

int RIFF::AIFF::Properties::channels() const
{
  return d->channels;
}

int RIFF::AIFF::Properties::bitsPerSample() const
{
  return d->bitsPerSample;
}

int RIFF::AIFF::Properties::sampleWidth() const
{
  return bitsPerSample();
}

unsigned int RIFF::AIFF::Properties::sampleFrames() const
{
  return d->sampleFrames;
}

bool RIFF::AIFF::Properties::isAiffC() const
{
  return (!d->compressionType.isEmpty());
}

ByteVector RIFF::AIFF::Properties::compressionType() const
{
  return d->compressionType;
}

String RIFF::AIFF::Properties::compressionName() const
{
  return d->compressionName;
}

////////////////////////////////////////////////////////////////////////////////
// private members
////////////////////////////////////////////////////////////////////////////////

void RIFF::AIFF::Properties::read(File *file)
{
  ByteVector data;
  unsigned int streamLength = 0;
  for(unsigned int i = 0; i < file->chunkCount(); i++) {
    const ByteVector name = file->chunkName(i);
    if(name == "COMM") {
      if(data.isEmpty())
        data = file->chunkData(i);
      else
        debug("RIFF::AIFF::Properties::read() - Duplicate 'COMM' chunk found.");
    }
    else if(name == "SSND") {
      if(streamLength == 0)
        streamLength = file->chunkDataSize(i) + file->chunkPadding(i);
      else
        debug("RIFF::AIFF::Properties::read() - Duplicate 'SSND' chunk found.");
    }
  }

  if(data.size() < 18) {
    debug("RIFF::AIFF::Properties::read() - 'COMM' chunk not found or too short.");
    return;
  }

  if(streamLength == 0) {
    debug("RIFF::AIFF::Properties::read() - 'SSND' chunk not found.");
    return;
  }

  d->channels      = data.toShort(0U);
  d->sampleFrames  = data.toUInt(2U);
  d->bitsPerSample = data.toShort(6U);

  const long double sampleRate = data.toFloat80BE(8);
  if(sampleRate >= 1.0)
    d->sampleRate = static_cast<int>(sampleRate + 0.5);

  if(d->sampleFrames > 0 && d->sampleRate > 0) {
    const double length = d->sampleFrames * 1000.0 / sampleRate;
    d->length  = static_cast<int>(length + 0.5);
    d->bitrate = static_cast<int>(streamLength * 8.0 / length + 0.5);
  }

  if(data.size() >= 23) {
    d->compressionType = data.mid(18, 4);
    d->compressionName
      = String(data.mid(23, static_cast<unsigned char>(data[22])), String::Latin1);
  }
}
