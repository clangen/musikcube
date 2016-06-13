/***************************************************************************
    copyright            : (C) 2012 by Tsuda Kageyu
    email                : tsuda.kageyu@gmail.com
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

#include <tdebug.h>
#include <tfile.h>

#include "infotag.h"
#include "riffutils.h"

using namespace TagLib;
using namespace RIFF::Info;

namespace
{
  const RIFF::Info::StringHandler defaultStringHandler;
  const RIFF::Info::StringHandler *stringHandler = &defaultStringHandler;
}

class RIFF::Info::Tag::TagPrivate
{
public:
  FieldListMap fieldListMap;
};

////////////////////////////////////////////////////////////////////////////////
// StringHandler implementation
////////////////////////////////////////////////////////////////////////////////

StringHandler::StringHandler()
{
}

StringHandler::~StringHandler()
{
}

String RIFF::Info::StringHandler::parse(const ByteVector &data) const
{
  return String(data, String::UTF8);
}

ByteVector RIFF::Info::StringHandler::render(const String &s) const
{
  return s.data(String::UTF8);
}

////////////////////////////////////////////////////////////////////////////////
// public members
////////////////////////////////////////////////////////////////////////////////

RIFF::Info::Tag::Tag(const ByteVector &data) :
  TagLib::Tag(),
  d(new TagPrivate())
{
  parse(data);
}

RIFF::Info::Tag::Tag() :
  TagLib::Tag(),
  d(new TagPrivate())
{
}

RIFF::Info::Tag::~Tag()
{
  delete d;
}

String RIFF::Info::Tag::title() const
{
  return fieldText("INAM");
}

String RIFF::Info::Tag::artist() const
{
  return fieldText("IART");
}

String RIFF::Info::Tag::album() const
{
  return fieldText("IPRD");
}

String RIFF::Info::Tag::comment() const
{
  return fieldText("ICMT");
}

String RIFF::Info::Tag::genre() const
{
  return fieldText("IGNR");
}

unsigned int RIFF::Info::Tag::year() const
{
  return fieldText("ICRD").substr(0, 4).toInt();
}

unsigned int RIFF::Info::Tag::track() const
{
  return fieldText("IPRT").toInt();
}

void RIFF::Info::Tag::setTitle(const String &s)
{
  setFieldText("INAM", s);
}

void RIFF::Info::Tag::setArtist(const String &s)
{
  setFieldText("IART", s);
}

void RIFF::Info::Tag::setAlbum(const String &s)
{
  setFieldText("IPRD", s);
}

void RIFF::Info::Tag::setComment(const String &s)
{
  setFieldText("ICMT", s);
}

void RIFF::Info::Tag::setGenre(const String &s)
{
  setFieldText("IGNR", s);
}

void RIFF::Info::Tag::setYear(unsigned int i)
{
  if(i != 0)
    setFieldText("ICRD", String::number(i));
  else
    d->fieldListMap.erase("ICRD");
}

void RIFF::Info::Tag::setTrack(unsigned int i)
{
  if(i != 0)
    setFieldText("IPRT", String::number(i));
  else
    d->fieldListMap.erase("IPRT");
}

bool RIFF::Info::Tag::isEmpty() const
{
  return d->fieldListMap.isEmpty();
}

FieldListMap RIFF::Info::Tag::fieldListMap() const
{
  return d->fieldListMap;
}

String RIFF::Info::Tag::fieldText(const ByteVector &id) const
{
  if(d->fieldListMap.contains(id))
    return String(d->fieldListMap[id]);
  else
    return String();
}

void RIFF::Info::Tag::setFieldText(const ByteVector &id, const String &s)
{
  // id must be four-byte long pure ascii string.
  if(!isValidChunkName(id))
    return;

  if(!s.isEmpty())
    d->fieldListMap[id] = s;
  else
    removeField(id);
}

void RIFF::Info::Tag::removeField(const ByteVector &id)
{
  if(d->fieldListMap.contains(id))
    d->fieldListMap.erase(id);
}

ByteVector RIFF::Info::Tag::render() const
{
  ByteVector data("INFO");

  FieldListMap::ConstIterator it = d->fieldListMap.begin();
  for(; it != d->fieldListMap.end(); ++it) {
    ByteVector text = stringHandler->render(it->second);
    if(text.isEmpty())
      continue;

    data.append(it->first);
    data.append(ByteVector::fromUInt(text.size() + 1, false));
    data.append(text);

    do {
      data.append('\0');
    } while(data.size() & 1);
  }

  if(data.size() == 4)
    return ByteVector();
  else
    return data;
}

void RIFF::Info::Tag::setStringHandler(const StringHandler *handler)
{
  if(handler)
    stringHandler = handler;
  else
    stringHandler = &defaultStringHandler;
}

////////////////////////////////////////////////////////////////////////////////
// protected members
////////////////////////////////////////////////////////////////////////////////

void RIFF::Info::Tag::parse(const ByteVector &data)
{
  unsigned int p = 4;
  while(p < data.size()) {
    const unsigned int size = data.toUInt(p + 4, false);
    if(size > data.size() - p - 8)
      break;

    const ByteVector id = data.mid(p, 4);
    if(isValidChunkName(id)) {
      const String text = stringHandler->parse(data.mid(p + 8, size));
      d->fieldListMap[id] = text;
    }

    p += ((size + 1) & ~1) + 8;
  }
}
