/***************************************************************************
    copyright            : (C) 2011 by Lukas Lalinsky
    email                : lalinsky@gmail.com
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

#ifndef TAGLIB_BYTEVECTORSTREAM_H
#define TAGLIB_BYTEVECTORSTREAM_H

#include "tbytevector.h"
#include "tiostream.h"
#include "taglib_export.h"
#include "taglib.h"

namespace TagLib {

  class String;
  class Tag;
  class AudioProperties;

  //! In-memory Stream class using ByteVector for its storage.

  class TAGLIB_EXPORT ByteVectorStream : public IOStream
  {
  public:
    /*!
     * Construct a ByteVectorStream from the bytes in \a data.
     */
    ByteVectorStream(const ByteVector &data);

    /*!
     * Destroys this ByteVectorStream instance.
     */
    ~ByteVectorStream() override;

    ByteVectorStream(const ByteVectorStream &) = delete;
    ByteVectorStream &operator=(const ByteVectorStream &) = delete;

    /*!
     * Returns an empty string.
     */
    FileName name() const override;

    /*!
     * Reads a block of size \a length at the current get pointer.
     */
    ByteVector readBlock(size_t length) override;

    /*!
     * Writes the block \a data at the current get pointer.
     *
     * \note This should be used instead of using the streaming output operator
     * for a ByteVector.  And even this function is significantly slower than
     * doing output with a char[].
     */
    void writeBlock(const ByteVector &data) override;

    /*!
     * Insert \a data at position \a start in the file overwriting \a replace
     * bytes of the original content.
     *
     * \note This method is slow since it requires rewriting all of the file
     * after the insertion point.
     */
    void insert(const ByteVector &data, offset_t start = 0, size_t replace = 0) override;

    /*!
     * Removes a block of the file starting a \a start and continuing for
     * \a length bytes.
     *
     * \note This method is slow since it involves rewriting all of the file
     * after the removed portion.
     */
    void removeBlock(offset_t start = 0, size_t length = 0) override;

    /*!
     * Returns \c false.
     */
    bool readOnly() const override;

    /*!
     * Returns \c true.
     */
    bool isOpen() const override;

    /*!
     * Move the I/O pointer to \a offset in the file from position \a p.  This
     * defaults to seeking from the beginning of the file.
     *
     * \see Position
     */
    void seek(offset_t offset, Position p = Beginning) override;

    /*!
     * Does nothing.
     */
    void clear() override;

    /*!
     * Returns the current offset within the file.
     */
    offset_t tell() const override;

    /*!
     * Returns the length of the file.
     */
    offset_t length() override;

    /*!
     * Truncates the file to a \a length.
     */
    void truncate(offset_t length) override;

    ByteVector *data();

  private:
    class ByteVectorStreamPrivate;
    TAGLIB_MSVC_SUPPRESS_WARNING_NEEDS_TO_HAVE_DLL_INTERFACE
    std::unique_ptr<ByteVectorStreamPrivate> d;
  };

}  // namespace TagLib

#endif
