/**************************************************************************
    copyright            : (C) 2007,2011 by Lukáš Lalinský
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
#include <tpropertymap.h>
#include "mp4atom.h"
#include "mp4tag.h"
#include "id3v1genres.h"

using namespace TagLib;

class MP4::Tag::TagPrivate
{
public:
  TagPrivate() :
    file(0),
    atoms(0) {}

  TagLib::File *file;
  Atoms *atoms;
  ItemMap items;
};

MP4::Tag::Tag() :
  d(new TagPrivate())
{
}

MP4::Tag::Tag(TagLib::File *file, MP4::Atoms *atoms) :
  d(new TagPrivate())
{
  d->file = file;
  d->atoms = atoms;

  MP4::Atom *ilst = atoms->find("moov", "udta", "meta", "ilst");
  if(!ilst) {
    //debug("Atom moov.udta.meta.ilst not found.");
    return;
  }

  for(AtomList::ConstIterator it = ilst->children.begin(); it != ilst->children.end(); ++it) {
    MP4::Atom *atom = *it;
    file->seek(atom->offset + 8);
    if(atom->name == "----") {
      parseFreeForm(atom);
    }
    else if(atom->name == "trkn" || atom->name == "disk") {
      parseIntPair(atom);
    }
    else if(atom->name == "cpil" || atom->name == "pgap" || atom->name == "pcst" ||
            atom->name == "hdvd") {
      parseBool(atom);
    }
    else if(atom->name == "tmpo") {
      parseInt(atom);
    }
    else if(atom->name == "tvsn" || atom->name == "tves" || atom->name == "cnID" ||
            atom->name == "sfID" || atom->name == "atID" || atom->name == "geID") {
      parseUInt(atom);
    }
    else if(atom->name == "plID") {
      parseLongLong(atom);
    }
    else if(atom->name == "stik" || atom->name == "rtng" || atom->name == "akID") {
      parseByte(atom);
    }
    else if(atom->name == "gnre") {
      parseGnre(atom);
    }
    else if(atom->name == "covr") {
      parseCovr(atom);
    }
    else {
      parseText(atom);
    }
  }
}

MP4::Tag::~Tag()
{
  delete d;
}

MP4::AtomDataList
MP4::Tag::parseData2(const MP4::Atom *atom, int expectedFlags, bool freeForm)
{
  AtomDataList result;
  ByteVector data = d->file->readBlock(atom->length - 8);
  int i = 0;
  unsigned int pos = 0;
  while(pos < data.size()) {
    const int length = static_cast<int>(data.toUInt(pos));
    if(length < 12) {
      debug("MP4: Too short atom");
      return result;
    }

    const ByteVector name = data.mid(pos + 4, 4);
    const int flags = static_cast<int>(data.toUInt(pos + 8));
    if(freeForm && i < 2) {
      if(i == 0 && name != "mean") {
        debug("MP4: Unexpected atom \"" + name + "\", expecting \"mean\"");
        return result;
      }
      else if(i == 1 && name != "name") {
        debug("MP4: Unexpected atom \"" + name + "\", expecting \"name\"");
        return result;
      }
      result.append(AtomData(AtomDataType(flags), data.mid(pos + 12, length - 12)));
    }
    else {
      if(name != "data") {
        debug("MP4: Unexpected atom \"" + name + "\", expecting \"data\"");
        return result;
      }
      if(expectedFlags == -1 || flags == expectedFlags) {
        result.append(AtomData(AtomDataType(flags), data.mid(pos + 16, length - 16)));
      }
    }
    pos += length;
    i++;
  }
  return result;
}

ByteVectorList
MP4::Tag::parseData(const MP4::Atom *atom, int expectedFlags, bool freeForm)
{
  AtomDataList data = parseData2(atom, expectedFlags, freeForm);
  ByteVectorList result;
  for(AtomDataList::ConstIterator it = data.begin(); it != data.end(); ++it) {
    result.append(it->data);
  }
  return result;
}

void
MP4::Tag::parseInt(const MP4::Atom *atom)
{
  ByteVectorList data = parseData(atom);
  if(!data.isEmpty()) {
    addItem(atom->name, (int)data[0].toShort());
  }
}

void
MP4::Tag::parseUInt(const MP4::Atom *atom)
{
  ByteVectorList data = parseData(atom);
  if(!data.isEmpty()) {
    addItem(atom->name, data[0].toUInt());
  }
}

void
MP4::Tag::parseLongLong(const MP4::Atom *atom)
{
  ByteVectorList data = parseData(atom);
  if(!data.isEmpty()) {
    addItem(atom->name, data[0].toLongLong());
  }
}

void
MP4::Tag::parseByte(const MP4::Atom *atom)
{
  ByteVectorList data = parseData(atom);
  if(!data.isEmpty()) {
    addItem(atom->name, static_cast<unsigned char>(data[0].at(0)));
  }
}

void
MP4::Tag::parseGnre(const MP4::Atom *atom)
{
  ByteVectorList data = parseData(atom);
  if(!data.isEmpty()) {
    int idx = (int)data[0].toShort();
    if(idx > 0) {
      addItem("\251gen", StringList(ID3v1::genre(idx - 1)));
    }
  }
}

void
MP4::Tag::parseIntPair(const MP4::Atom *atom)
{
  ByteVectorList data = parseData(atom);
  if(!data.isEmpty()) {
    const int a = data[0].toShort(2U);
    const int b = data[0].toShort(4U);
    addItem(atom->name, MP4::Item(a, b));
  }
}

void
MP4::Tag::parseBool(const MP4::Atom *atom)
{
  ByteVectorList data = parseData(atom);
  if(!data.isEmpty()) {
    bool value = data[0].size() ? data[0][0] != '\0' : false;
    addItem(atom->name, value);
  }
}

void
MP4::Tag::parseText(const MP4::Atom *atom, int expectedFlags)
{
  ByteVectorList data = parseData(atom, expectedFlags);
  if(!data.isEmpty()) {
    StringList value;
    for(ByteVectorList::ConstIterator it = data.begin(); it != data.end(); ++it) {
      value.append(String(*it, String::UTF8));
    }
    addItem(atom->name, value);
  }
}

void
MP4::Tag::parseFreeForm(const MP4::Atom *atom)
{
  AtomDataList data = parseData2(atom, -1, true);
  if(data.size() > 2) {
    AtomDataList::ConstIterator itBegin = data.begin();

    String name = "----:";
    name += String((itBegin++)->data, String::UTF8);  // data[0].data
    name += ':';
    name += String((itBegin++)->data, String::UTF8);  // data[1].data

    AtomDataType type = itBegin->type; // data[2].type

    for(AtomDataList::ConstIterator it = itBegin; it != data.end(); ++it) {
      if(it->type != type) {
        debug("MP4: We currently don't support values with multiple types");
        break;
      }
    }
    if(type == TypeUTF8) {
      StringList value;
      for(AtomDataList::ConstIterator it = itBegin; it != data.end(); ++it) {
        value.append(String(it->data, String::UTF8));
      }
      Item item(value);
      item.setAtomDataType(type);
      addItem(name, item);
    }
    else {
      ByteVectorList value;
      for(AtomDataList::ConstIterator it = itBegin; it != data.end(); ++it) {
        value.append(it->data);
      }
      Item item(value);
      item.setAtomDataType(type);
      addItem(name, item);
    }
  }
}

void
MP4::Tag::parseCovr(const MP4::Atom *atom)
{
  MP4::CoverArtList value;
  ByteVector data = d->file->readBlock(atom->length - 8);
  unsigned int pos = 0;
  while(pos < data.size()) {
    const int length = static_cast<int>(data.toUInt(pos));
    if(length < 12) {
      debug("MP4: Too short atom");
      break;;
    }

    const ByteVector name = data.mid(pos + 4, 4);
    const int flags = static_cast<int>(data.toUInt(pos + 8));
    if(name != "data") {
      debug("MP4: Unexpected atom \"" + name + "\", expecting \"data\"");
      break;
    }
    if(flags == TypeJPEG || flags == TypePNG || flags == TypeBMP ||
       flags == TypeGIF || flags == TypeImplicit) {
      value.append(MP4::CoverArt(MP4::CoverArt::Format(flags),
                                 data.mid(pos + 16, length - 16)));
    }
    else {
      debug("MP4: Unknown covr format " + String::number(flags));
    }
    pos += length;
  }
  if(!value.isEmpty())
    addItem(atom->name, value);
}

ByteVector
MP4::Tag::padIlst(const ByteVector &data, int length) const
{
  if(length == -1) {
    length = ((data.size() + 1023) & ~1023) - data.size();
  }
  return renderAtom("free", ByteVector(length, '\1'));
}

ByteVector
MP4::Tag::renderAtom(const ByteVector &name, const ByteVector &data) const
{
  return ByteVector::fromUInt(data.size() + 8) + name + data;
}

ByteVector
MP4::Tag::renderData(const ByteVector &name, int flags, const ByteVectorList &data) const
{
  ByteVector result;
  for(ByteVectorList::ConstIterator it = data.begin(); it != data.end(); ++it) {
    result.append(renderAtom("data", ByteVector::fromUInt(flags) + ByteVector(4, '\0') + *it));
  }
  return renderAtom(name, result);
}

ByteVector
MP4::Tag::renderBool(const ByteVector &name, const MP4::Item &item) const
{
  ByteVectorList data;
  data.append(ByteVector(1, item.toBool() ? '\1' : '\0'));
  return renderData(name, TypeInteger, data);
}

ByteVector
MP4::Tag::renderInt(const ByteVector &name, const MP4::Item &item) const
{
  ByteVectorList data;
  data.append(ByteVector::fromShort(item.toInt()));
  return renderData(name, TypeInteger, data);
}

ByteVector
MP4::Tag::renderUInt(const ByteVector &name, const MP4::Item &item) const
{
  ByteVectorList data;
  data.append(ByteVector::fromUInt(item.toUInt()));
  return renderData(name, TypeInteger, data);
}

ByteVector
MP4::Tag::renderLongLong(const ByteVector &name, const MP4::Item &item) const
{
  ByteVectorList data;
  data.append(ByteVector::fromLongLong(item.toLongLong()));
  return renderData(name, TypeInteger, data);
}

ByteVector
MP4::Tag::renderByte(const ByteVector &name, const MP4::Item &item) const
{
  ByteVectorList data;
  data.append(ByteVector(1, item.toByte()));
  return renderData(name, TypeInteger, data);
}

ByteVector
MP4::Tag::renderIntPair(const ByteVector &name, const MP4::Item &item) const
{
  ByteVectorList data;
  data.append(ByteVector(2, '\0') +
              ByteVector::fromShort(item.toIntPair().first) +
              ByteVector::fromShort(item.toIntPair().second) +
              ByteVector(2, '\0'));
  return renderData(name, TypeImplicit, data);
}

ByteVector
MP4::Tag::renderIntPairNoTrailing(const ByteVector &name, const MP4::Item &item) const
{
  ByteVectorList data;
  data.append(ByteVector(2, '\0') +
              ByteVector::fromShort(item.toIntPair().first) +
              ByteVector::fromShort(item.toIntPair().second));
  return renderData(name, TypeImplicit, data);
}

ByteVector
MP4::Tag::renderText(const ByteVector &name, const MP4::Item &item, int flags) const
{
  ByteVectorList data;
  StringList value = item.toStringList();
  for(StringList::ConstIterator it = value.begin(); it != value.end(); ++it) {
    data.append(it->data(String::UTF8));
  }
  return renderData(name, flags, data);
}

ByteVector
MP4::Tag::renderCovr(const ByteVector &name, const MP4::Item &item) const
{
  ByteVector data;
  MP4::CoverArtList value = item.toCoverArtList();
  for(MP4::CoverArtList::ConstIterator it = value.begin(); it != value.end(); ++it) {
    data.append(renderAtom("data", ByteVector::fromUInt(it->format()) +
                                   ByteVector(4, '\0') + it->data()));
  }
  return renderAtom(name, data);
}

ByteVector
MP4::Tag::renderFreeForm(const String &name, const MP4::Item &item) const
{
  StringList header = StringList::split(name, ":");
  if(header.size() != 3) {
    debug("MP4: Invalid free-form item name \"" + name + "\"");
    return ByteVector();
  }
  ByteVector data;
  data.append(renderAtom("mean", ByteVector::fromUInt(0) + header[1].data(String::UTF8)));
  data.append(renderAtom("name", ByteVector::fromUInt(0) + header[2].data(String::UTF8)));
  AtomDataType type = item.atomDataType();
  if(type == TypeUndefined) {
    if(!item.toStringList().isEmpty()) {
      type = TypeUTF8;
    }
    else {
      type = TypeImplicit;
    }
  }
  if(type == TypeUTF8) {
    StringList value = item.toStringList();
    for(StringList::ConstIterator it = value.begin(); it != value.end(); ++it) {
      data.append(renderAtom("data", ByteVector::fromUInt(type) + ByteVector(4, '\0') + it->data(String::UTF8)));
    }
  }
  else {
    ByteVectorList value = item.toByteVectorList();
    for(ByteVectorList::ConstIterator it = value.begin(); it != value.end(); ++it) {
      data.append(renderAtom("data", ByteVector::fromUInt(type) + ByteVector(4, '\0') + *it));
    }
  }
  return renderAtom("----", data);
}

bool
MP4::Tag::save()
{
  ByteVector data;
  for(MP4::ItemMap::ConstIterator it = d->items.begin(); it != d->items.end(); ++it) {
    const String name = it->first;
    if(name.startsWith("----")) {
      data.append(renderFreeForm(name, it->second));
    }
    else if(name == "trkn") {
      data.append(renderIntPair(name.data(String::Latin1), it->second));
    }
    else if(name == "disk") {
      data.append(renderIntPairNoTrailing(name.data(String::Latin1), it->second));
    }
    else if(name == "cpil" || name == "pgap" || name == "pcst" || name == "hdvd") {
      data.append(renderBool(name.data(String::Latin1), it->second));
    }
    else if(name == "tmpo") {
      data.append(renderInt(name.data(String::Latin1), it->second));
    }
    else if(name == "tvsn" || name == "tves" || name == "cnID" ||
            name == "sfID" || name == "atID" || name == "geID") {
      data.append(renderUInt(name.data(String::Latin1), it->second));
    }
    else if(name == "plID") {
      data.append(renderLongLong(name.data(String::Latin1), it->second));
    }
    else if(name == "stik" || name == "rtng" || name == "akID") {
      data.append(renderByte(name.data(String::Latin1), it->second));
    }
    else if(name == "covr") {
      data.append(renderCovr(name.data(String::Latin1), it->second));
    }
    else if(name.size() == 4){
      data.append(renderText(name.data(String::Latin1), it->second));
    }
    else {
      debug("MP4: Unknown item name \"" + name + "\"");
    }
  }
  data = renderAtom("ilst", data);

  AtomList path = d->atoms->path("moov", "udta", "meta", "ilst");
  if(path.size() == 4) {
    saveExisting(data, path);
  }
  else {
    saveNew(data);
  }

  return true;
}

void
MP4::Tag::updateParents(const AtomList &path, long delta, int ignore)
{
  if(static_cast<int>(path.size()) <= ignore)
    return;

  AtomList::ConstIterator itEnd = path.end();
  std::advance(itEnd, 0 - ignore);

  for(AtomList::ConstIterator it = path.begin(); it != itEnd; ++it) {
    d->file->seek((*it)->offset);
    long size = d->file->readBlock(4).toUInt();
    // 64-bit
    if (size == 1) {
      d->file->seek(4, File::Current); // Skip name
      long long longSize = d->file->readBlock(8).toLongLong();
      // Seek the offset of the 64-bit size
      d->file->seek((*it)->offset + 8);
      d->file->writeBlock(ByteVector::fromLongLong(longSize + delta));
    }
    // 32-bit
    else {
      d->file->seek((*it)->offset);
      d->file->writeBlock(ByteVector::fromUInt(size + delta));
    }
  }
}

void
MP4::Tag::updateOffsets(long delta, long offset)
{
  MP4::Atom *moov = d->atoms->find("moov");
  if(moov) {
    MP4::AtomList stco = moov->findall("stco", true);
    for(MP4::AtomList::ConstIterator it = stco.begin(); it != stco.end(); ++it) {
      MP4::Atom *atom = *it;
      if(atom->offset > offset) {
        atom->offset += delta;
      }
      d->file->seek(atom->offset + 12);
      ByteVector data = d->file->readBlock(atom->length - 12);
      unsigned int count = data.toUInt();
      d->file->seek(atom->offset + 16);
      unsigned int pos = 4;
      while(count--) {
        long o = static_cast<long>(data.toUInt(pos));
        if(o > offset) {
          o += delta;
        }
        d->file->writeBlock(ByteVector::fromUInt(o));
        pos += 4;
      }
    }

    MP4::AtomList co64 = moov->findall("co64", true);
    for(MP4::AtomList::ConstIterator it = co64.begin(); it != co64.end(); ++it) {
      MP4::Atom *atom = *it;
      if(atom->offset > offset) {
        atom->offset += delta;
      }
      d->file->seek(atom->offset + 12);
      ByteVector data = d->file->readBlock(atom->length - 12);
      unsigned int count = data.toUInt();
      d->file->seek(atom->offset + 16);
      unsigned int pos = 4;
      while(count--) {
        long long o = data.toLongLong(pos);
        if(o > offset) {
          o += delta;
        }
        d->file->writeBlock(ByteVector::fromLongLong(o));
        pos += 8;
      }
    }
  }

  MP4::Atom *moof = d->atoms->find("moof");
  if(moof) {
    MP4::AtomList tfhd = moof->findall("tfhd", true);
    for(MP4::AtomList::ConstIterator it = tfhd.begin(); it != tfhd.end(); ++it) {
      MP4::Atom *atom = *it;
      if(atom->offset > offset) {
        atom->offset += delta;
      }
      d->file->seek(atom->offset + 9);
      ByteVector data = d->file->readBlock(atom->length - 9);
      const unsigned int flags = data.toUInt(0, 3, true);
      if(flags & 1) {
        long long o = data.toLongLong(7U);
        if(o > offset) {
          o += delta;
        }
        d->file->seek(atom->offset + 16);
        d->file->writeBlock(ByteVector::fromLongLong(o));
      }
    }
  }
}

void
MP4::Tag::saveNew(ByteVector data)
{
  data = renderAtom("meta", ByteVector(4, '\0') +
                    renderAtom("hdlr", ByteVector(8, '\0') + ByteVector("mdirappl") +
                               ByteVector(9, '\0')) +
                    data + padIlst(data));

  AtomList path = d->atoms->path("moov", "udta");
  if(path.size() != 2) {
    path = d->atoms->path("moov");
    data = renderAtom("udta", data);
  }

  long offset = path.back()->offset + 8;
  d->file->insert(data, offset, 0);

  updateParents(path, data.size());
  updateOffsets(data.size(), offset);

  // Insert the newly created atoms into the tree to keep it up-to-date.

  d->file->seek(offset);
  path.back()->children.prepend(new Atom(d->file));
}

void
MP4::Tag::saveExisting(ByteVector data, const AtomList &path)
{
  AtomList::ConstIterator it = path.end();

  MP4::Atom *ilst = *(--it);
  long offset = ilst->offset;
  long length = ilst->length;

  MP4::Atom *meta = *(--it);
  AtomList::ConstIterator index = meta->children.find(ilst);

  // check if there is an atom before 'ilst', and possibly use it as padding
  if(index != meta->children.begin()) {
    AtomList::ConstIterator prevIndex = index;
    prevIndex--;
    MP4::Atom *prev = *prevIndex;
    if(prev->name == "free") {
      offset = prev->offset;
      length += prev->length;
    }
  }
  // check if there is an atom after 'ilst', and possibly use it as padding
  AtomList::ConstIterator nextIndex = index;
  nextIndex++;
  if(nextIndex != meta->children.end()) {
    MP4::Atom *next = *nextIndex;
    if(next->name == "free") {
      length += next->length;
    }
  }

  long delta = data.size() - length;
  if(delta > 0 || (delta < 0 && delta > -8)) {
    data.append(padIlst(data));
    delta = data.size() - length;
  }
  else if(delta < 0) {
    data.append(padIlst(data, -delta - 8));
    delta = 0;
  }

  d->file->insert(data, offset, length);

  if(delta) {
    updateParents(path, delta, 1);
    updateOffsets(delta, offset);
  }
}

String
MP4::Tag::title() const
{
  if(d->items.contains("\251nam"))
    return d->items["\251nam"].toStringList().toString(", ");
  return String();
}

String
MP4::Tag::artist() const
{
  if(d->items.contains("\251ART"))
    return d->items["\251ART"].toStringList().toString(", ");
  return String();
}

String
MP4::Tag::album() const
{
  if(d->items.contains("\251alb"))
    return d->items["\251alb"].toStringList().toString(", ");
  return String();
}

String
MP4::Tag::comment() const
{
  if(d->items.contains("\251cmt"))
    return d->items["\251cmt"].toStringList().toString(", ");
  return String();
}

String
MP4::Tag::genre() const
{
  if(d->items.contains("\251gen"))
    return d->items["\251gen"].toStringList().toString(", ");
  return String();
}

unsigned int
MP4::Tag::year() const
{
  if(d->items.contains("\251day"))
    return d->items["\251day"].toStringList().toString().toInt();
  return 0;
}

unsigned int
MP4::Tag::track() const
{
  if(d->items.contains("trkn"))
    return d->items["trkn"].toIntPair().first;
  return 0;
}

void
MP4::Tag::setTitle(const String &value)
{
  d->items["\251nam"] = StringList(value);
}

void
MP4::Tag::setArtist(const String &value)
{
  d->items["\251ART"] = StringList(value);
}

void
MP4::Tag::setAlbum(const String &value)
{
  d->items["\251alb"] = StringList(value);
}

void
MP4::Tag::setComment(const String &value)
{
  d->items["\251cmt"] = StringList(value);
}

void
MP4::Tag::setGenre(const String &value)
{
  d->items["\251gen"] = StringList(value);
}

void
MP4::Tag::setYear(unsigned int value)
{
  d->items["\251day"] = StringList(String::number(value));
}

void
MP4::Tag::setTrack(unsigned int value)
{
  d->items["trkn"] = MP4::Item(value, 0);
}

bool MP4::Tag::isEmpty() const
{
  return d->items.isEmpty();
}

MP4::ItemMap &MP4::Tag::itemListMap()
{
  return d->items;
}

const MP4::ItemMap &MP4::Tag::itemMap() const
{
  return d->items;
}

MP4::Item MP4::Tag::item(const String &key) const
{
  return d->items[key];
}

void MP4::Tag::setItem(const String &key, const Item &value)
{
  d->items[key] = value;
}

void MP4::Tag::removeItem(const String &key)
{
  d->items.erase(key);
}

bool MP4::Tag::contains(const String &key) const
{
  return d->items.contains(key);
}

namespace
{
  const char *keyTranslation[][2] = {
    { "\251nam", "TITLE" },
    { "\251ART", "ARTIST" },
    { "\251alb", "ALBUM" },
    { "\251cmt", "COMMENT" },
    { "\251gen", "GENRE" },
    { "\251day", "DATE" },
    { "\251wrt", "COMPOSER" },
    { "\251grp", "GROUPING" },
    { "aART", "ALBUMARTIST" },
    { "trkn", "TRACKNUMBER" },
    { "disk", "DISCNUMBER" },
    { "cpil", "COMPILATION" },
    { "tmpo", "BPM" },
    { "cprt", "COPYRIGHT" },
    { "\251lyr", "LYRICS" },
    { "\251too", "ENCODEDBY" },
    { "soal", "ALBUMSORT" },
    { "soaa", "ALBUMARTISTSORT" },
    { "soar", "ARTISTSORT" },
    { "sonm", "TITLESORT" },
    { "soco", "COMPOSERSORT" },
    { "sosn", "SHOWSORT" },
    { "----:com.apple.iTunes:MusicBrainz Track Id", "MUSICBRAINZ_TRACKID" },
    { "----:com.apple.iTunes:MusicBrainz Artist Id", "MUSICBRAINZ_ARTISTID" },
    { "----:com.apple.iTunes:MusicBrainz Album Id", "MUSICBRAINZ_ALBUMID" },
    { "----:com.apple.iTunes:MusicBrainz Album Artist Id", "MUSICBRAINZ_ALBUMARTISTID" },
    { "----:com.apple.iTunes:MusicBrainz Release Group Id", "MUSICBRAINZ_RELEASEGROUPID" },
    { "----:com.apple.iTunes:MusicBrainz Work Id", "MUSICBRAINZ_WORKID" },
    { "----:com.apple.iTunes:ASIN", "ASIN" },
    { "----:com.apple.iTunes:LABEL", "LABEL" },
    { "----:com.apple.iTunes:LYRICIST", "LYRICIST" },
    { "----:com.apple.iTunes:CONDUCTOR", "CONDUCTOR" },
    { "----:com.apple.iTunes:REMIXER", "REMIXER" },
    { "----:com.apple.iTunes:ENGINEER", "ENGINEER" },
    { "----:com.apple.iTunes:PRODUCER", "PRODUCER" },
    { "----:com.apple.iTunes:DJMIXER", "DJMIXER" },
    { "----:com.apple.iTunes:MIXER", "MIXER" },
    { "----:com.apple.iTunes:SUBTITLE", "SUBTITLE" },
    { "----:com.apple.iTunes:DISCSUBTITLE", "DISCSUBTITLE" },
    { "----:com.apple.iTunes:MOOD", "MOOD" },
    { "----:com.apple.iTunes:ISRC", "ISRC" },
    { "----:com.apple.iTunes:CATALOGNUMBER", "CATALOGNUMBER" },
    { "----:com.apple.iTunes:BARCODE", "BARCODE" },
    { "----:com.apple.iTunes:SCRIPT", "SCRIPT" },
    { "----:com.apple.iTunes:LANGUAGE", "LANGUAGE" },
    { "----:com.apple.iTunes:LICENSE", "LICENSE" },
    { "----:com.apple.iTunes:MEDIA", "MEDIA" },
  };
  const size_t keyTranslationSize = sizeof(keyTranslation) / sizeof(keyTranslation[0]);

  String translateKey(const String &key)
  {
    for(size_t i = 0; i < keyTranslationSize; ++i) {
      if(key == keyTranslation[i][0])
        return keyTranslation[i][1];
    }

    return String();
  }
}

PropertyMap MP4::Tag::properties() const
{
  PropertyMap props;
  for(MP4::ItemMap::ConstIterator it = d->items.begin(); it != d->items.end(); ++it) {
    const String key = translateKey(it->first);
    if(!key.isEmpty()) {
      if(key == "TRACKNUMBER" || key == "DISCNUMBER") {
        MP4::Item::IntPair ip = it->second.toIntPair();
        String value = String::number(ip.first);
        if(ip.second) {
          value += "/" + String::number(ip.second);
        }
        props[key] = value;
      }
      else if(key == "BPM") {
        props[key] = String::number(it->second.toInt());
      }
      else if(key == "COMPILATION") {
        props[key] = String::number(it->second.toBool());
      }
      else {
        props[key] = it->second.toStringList();
      }
    }
    else {
      props.unsupportedData().append(it->first);
    }
  }
  return props;
}

void MP4::Tag::removeUnsupportedProperties(const StringList &props)
{
  for(StringList::ConstIterator it = props.begin(); it != props.end(); ++it)
    d->items.erase(*it);
}

PropertyMap MP4::Tag::setProperties(const PropertyMap &props)
{
  static Map<String, String> reverseKeyMap;
  if(reverseKeyMap.isEmpty()) {
    int numKeys = sizeof(keyTranslation) / sizeof(keyTranslation[0]);
    for(int i = 0; i < numKeys; i++) {
      reverseKeyMap[keyTranslation[i][1]] = keyTranslation[i][0];
    }
  }

  PropertyMap origProps = properties();
  for(PropertyMap::ConstIterator it = origProps.begin(); it != origProps.end(); ++it) {
    if(!props.contains(it->first) || props[it->first].isEmpty()) {
      d->items.erase(reverseKeyMap[it->first]);
    }
  }

  PropertyMap ignoredProps;
  for(PropertyMap::ConstIterator it = props.begin(); it != props.end(); ++it) {
    if(reverseKeyMap.contains(it->first)) {
      String name = reverseKeyMap[it->first];
      if((it->first == "TRACKNUMBER" || it->first == "DISCNUMBER") && !it->second.isEmpty()) {
        int first = 0, second = 0;
        StringList parts = StringList::split(it->second.front(), "/");
        if(!parts.isEmpty()) {
          first = parts[0].toInt();
          if(parts.size() > 1) {
            second = parts[1].toInt();
          }
          d->items[name] = MP4::Item(first, second);
        }
      }
      else if(it->first == "BPM" && !it->second.isEmpty()) {
        int value = it->second.front().toInt();
        d->items[name] = MP4::Item(value);
      }
      else if(it->first == "COMPILATION" && !it->second.isEmpty()) {
        bool value = (it->second.front().toInt() != 0);
        d->items[name] = MP4::Item(value);
      }
      else {
        d->items[name] = it->second;
      }
    }
    else {
      ignoredProps.insert(it->first, it->second);
    }
  }

  return ignoredProps;
}

void MP4::Tag::addItem(const String &name, const Item &value)
{
  if(!d->items.contains(name)) {
    d->items.insert(name, value);
  }
  else {
    debug("MP4: Ignoring duplicate atom \"" + name + "\"");
  }
}
