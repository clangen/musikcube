/***************************************************************************
    copyright           : (C) 2011 by Mathias Panzenböck
    email               : grosser.meister.morti@gmx.net
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


#include "modproperties.h"

using namespace TagLib;
using namespace Mod;

class Mod::Properties::PropertiesPrivate
{
public:
  PropertiesPrivate() :
    channels(0),
    instrumentCount(0),
    lengthInPatterns(0)
  {
  }

  int           channels;
  unsigned int  instrumentCount;
  unsigned char lengthInPatterns;
};

Mod::Properties::Properties(AudioProperties::ReadStyle propertiesStyle) :
  AudioProperties(propertiesStyle),
  d(new PropertiesPrivate())
{
}

Mod::Properties::~Properties()
{
  delete d;
}

int Mod::Properties::length() const
{
  return 0;
}

int Mod::Properties::lengthInSeconds() const
{
  return 0;
}

int Mod::Properties::lengthInMilliseconds() const
{
  return 0;
}

int Mod::Properties::bitrate() const
{
  return 0;
}

int Mod::Properties::sampleRate() const
{
  return 0;
}

int Mod::Properties::channels() const
{
  return d->channels;
}

unsigned int Mod::Properties::instrumentCount() const
{
  return d->instrumentCount;
}

unsigned char Mod::Properties::lengthInPatterns() const
{
  return d->lengthInPatterns;
}

void Mod::Properties::setChannels(int channels)
{
  d->channels = channels;
}

void Mod::Properties::setInstrumentCount(unsigned int instrumentCount)
{
  d->instrumentCount = instrumentCount;
}

void Mod::Properties::setLengthInPatterns(unsigned char lengthInPatterns)
{
  d->lengthInPatterns = lengthInPatterns;
}
