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

// This file is not part of the public API!

#ifndef TAGLIB_MP4ATOM_H
#define TAGLIB_MP4ATOM_H

#include "tfile.h"
#include "tlist.h"

namespace TagLib {
  namespace MP4 {

    enum AtomDataType {
      TypeImplicit  = 0,  // for use with tags for which no type needs to be indicated because only one type is allowed
      TypeUTF8      = 1,  // without any count or null terminator
      TypeUTF16     = 2,  // also known as UTF-16BE
      TypeSJIS      = 3,  // deprecated unless it is needed for special Japanese characters
      TypeHTML      = 6,  // the HTML file header specifies which HTML version
      TypeXML       = 7,  // the XML header must identify the DTD or schemas
      TypeUUID      = 8,  // also known as GUID; stored as 16 bytes in binary (valid as an ID)
      TypeISRC      = 9,  // stored as UTF-8 text (valid as an ID)
      TypeMI3P      = 10, // stored as UTF-8 text (valid as an ID)
      TypeGIF       = 12, // (deprecated) a GIF image
      TypeJPEG      = 13, // a JPEG image
      TypePNG       = 14, // a PNG image
      TypeURL       = 15, // absolute, in UTF-8 characters
      TypeDuration  = 16, // in milliseconds, 32-bit integer
      TypeDateTime  = 17, // in UTC, counting seconds since midnight, January 1, 1904; 32 or 64-bits
      TypeGenred    = 18, // a list of enumerated values
      TypeInteger   = 21, // a signed big-endian integer with length one of { 1,2,3,4,8 } bytes
      TypeRIAAPA    = 24, // RIAA parental advisory; { -1=no, 1=yes, 0=unspecified }, 8-bit integer
      TypeUPC       = 25, // Universal Product Code, in text UTF-8 format (valid as an ID)
      TypeBMP       = 27, // Windows bitmap image
      TypeUndefined = 255 // undefined
    };

#ifndef DO_NOT_DOCUMENT
    struct AtomData {
      AtomData(AtomDataType ptype, const ByteVector &pdata) :
        type(ptype), data(pdata) { }
      AtomDataType type;
      int locale { 0 };
      ByteVector data;
    };

    class Atom;
    using AtomList = TagLib::List<Atom *>;
    using AtomDataList = TagLib::List<AtomData>;

    class TAGLIB_EXPORT Atom
    {
    public:
      Atom(File *file);
      ~Atom();
      Atom(const Atom &) = delete;
      Atom &operator=(const Atom &) = delete;
      Atom *find(const char *name1, const char *name2 = nullptr, const char *name3 = nullptr, const char *name4 = nullptr);
      bool path(AtomList &path, const char *name1, const char *name2 = nullptr, const char *name3 = nullptr);
      AtomList findall(const char *name, bool recursive = false) const;
      void addToOffset(offset_t delta);
      void prependChild(Atom *atom);
      bool removeChild(Atom *meta);
      offset_t offset() const;
      offset_t length() const;
      const ByteVector &name() const;
      const AtomList &children() const;

    private:
      class AtomPrivate;
      TAGLIB_MSVC_SUPPRESS_WARNING_NEEDS_TO_HAVE_DLL_INTERFACE
      std::unique_ptr<AtomPrivate> d;
    };

    //! Root-level atoms
    class TAGLIB_EXPORT Atoms
    {
    public:
      Atoms(File *file);
      ~Atoms();
      Atoms(const Atoms &) = delete;
      Atoms &operator=(const Atoms &) = delete;
      Atom *find(const char *name1, const char *name2 = nullptr, const char *name3 = nullptr, const char *name4 = nullptr) const;
      AtomList path(const char *name1, const char *name2 = nullptr, const char *name3 = nullptr, const char *name4 = nullptr) const;
      bool checkRootLevelAtoms();
      const AtomList &atoms() const;

    private:
      class AtomsPrivate;
      TAGLIB_MSVC_SUPPRESS_WARNING_NEEDS_TO_HAVE_DLL_INTERFACE
      std::unique_ptr<AtomsPrivate> d;
    };
#endif  // DO_NOT_DOCUMENT
  } // namespace MP4
} // namespace TagLib

#endif
