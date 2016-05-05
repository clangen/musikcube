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


#include "tdebug.h"
#include "modfilebase.h"

using namespace TagLib;
using namespace Mod;

Mod::FileBase::FileBase(FileName file) : TagLib::File(file)
{
}

Mod::FileBase::FileBase(IOStream *stream) : TagLib::File(stream)
{
}

void Mod::FileBase::writeString(const String &s, unsigned long size, char padding)
{
  ByteVector data(s.data(String::Latin1));
  data.resize(size, padding);
  writeBlock(data);
}

bool Mod::FileBase::readString(String &s, unsigned long size)
{
  ByteVector data(readBlock(size));
  if(data.size() < size) return false;
  int index = data.find((char) 0);
  if(index > -1)
  {
    data.resize(index);
  }
  data.replace('\xff', ' ');

  s = data;
  return true;
}

void Mod::FileBase::writeByte(unsigned char byte)
{
  ByteVector data(1, byte);
  writeBlock(data);
}

void Mod::FileBase::writeU16L(unsigned short number)
{
  writeBlock(ByteVector::fromShort(number, false));
}

void Mod::FileBase::writeU32L(unsigned long number)
{
  writeBlock(ByteVector::fromUInt(number, false));
}

void Mod::FileBase::writeU16B(unsigned short number)
{
  writeBlock(ByteVector::fromShort(number, true));
}

void Mod::FileBase::writeU32B(unsigned long number)
{
  writeBlock(ByteVector::fromUInt(number, true));
}

bool Mod::FileBase::readByte(unsigned char &byte)
{
  ByteVector data(readBlock(1));
  if(data.size() < 1) return false;
  byte = data[0];
  return true;
}

bool Mod::FileBase::readU16L(unsigned short &number)
{
  ByteVector data(readBlock(2));
  if(data.size() < 2) return false;
  number = data.toUShort(false);
  return true;
}

bool Mod::FileBase::readU32L(unsigned long &number) {
  ByteVector data(readBlock(4));
  if(data.size() < 4) return false;
  number = data.toUInt(false);
  return true;
}

bool Mod::FileBase::readU16B(unsigned short &number)
{
  ByteVector data(readBlock(2));
  if(data.size() < 2) return false;
  number = data.toUShort(true);
  return true;
}

bool Mod::FileBase::readU32B(unsigned long &number) {
  ByteVector data(readBlock(4));
  if(data.size() < 4) return false;
  number = data.toUInt(true);
  return true;
}
