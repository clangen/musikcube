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

#ifndef TAGLIB_FILESTREAM_H
#define TAGLIB_FILESTREAM_H

#include "tbytevector.h"
#include "tiostream.h"
#include "taglib_export.h"
#include "taglib.h"

namespace TagLib {

  class String;
  class Tag;
  class AudioProperties;

  //! I/O stream with data from a file.

  class TAGLIB_EXPORT FileStream : public IOStream
  {
  public:
    /*!
     * Construct a FileStream object and open the \a fileName.  \a fileName should be a
     * C-string in the local file system encoding.
     */
    FileStream(FileName fileName, bool openReadOnly = false);

    /*!
     * Construct a FileStream object using an existing \a fileDescriptor.
     */
    FileStream(int fileDescriptor, bool openReadOnly = false);

    /*!
     * Destroys this FileStream instance.
     */
    ~FileStream() override;

    FileStream(const FileStream &) = delete;
    FileStream &operator=(const FileStream &) = delete;

    /*!
     * Returns the file name in the local file system encoding.
     */
    FileName name() const override;

    /*!
     * Reads a block of size \a length at the current get pointer.
     */
    ByteVector readBlock(size_t length) override;

    /*!
     * Attempts to write the block \a data at the current get pointer.  If the
     * file is currently only opened read only -- i.e. readOnly() returns \c true --
     * this attempts to reopen the file in read/write mode.
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
     * Returns \c true if the file is read only (or if the file can not be opened).
     */
    bool readOnly() const override;

    /*!
     * Since the file can currently only be opened as an argument to the
     * constructor (sort-of by design), this returns if that open succeeded.
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
     * Reset the end-of-file and error flags on the file.
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

  protected:

    /*!
     * Returns the buffer size that is used for internal buffering.
     */
    static unsigned int bufferSize();

  private:
    class FileStreamPrivate;
    TAGLIB_MSVC_SUPPRESS_WARNING_NEEDS_TO_HAVE_DLL_INTERFACE
    std::unique_ptr<FileStreamPrivate> d;
  };

}  // namespace TagLib

#endif
