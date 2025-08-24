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

#ifndef TAGLIB_UNKNOWNFRAME_H
#define TAGLIB_UNKNOWNFRAME_H

#include "taglib_export.h"
#include "id3v2frame.h"

namespace TagLib {

  namespace ID3v2 {

    //! A frame type \e unknown to TagLib.

    /*!
     * This class represents a frame type not known (or more often simply
     * unimplemented) in TagLib.  This is here to provide a basic API for
     * manipulating the binary data of unknown frames and to provide a means
     * of rendering such \e unknown frames.
     *
     * Please note that a cleaner way of handling frame types that TagLib
     * does not understand is to subclass ID3v2::Frame and ID3v2::FrameFactory
     * to have your frame type supported through the standard ID3v2 mechanism.
     */

    class TAGLIB_EXPORT UnknownFrame : public Frame
    {
      friend class FrameFactory;

    public:
      UnknownFrame(const ByteVector &data);
      ~UnknownFrame() override;

      UnknownFrame(const UnknownFrame &) = delete;
      UnknownFrame &operator=(const UnknownFrame &) = delete;

      String toString() const override;

      /*!
       * Returns the field data (everything but the header) for this frame.
       */
      ByteVector data() const;

    protected:
      void parseFields(const ByteVector &data) override;
      ByteVector renderFields() const override;

    private:
      UnknownFrame(const ByteVector &data, Header *h);

      class UnknownFramePrivate;
      TAGLIB_MSVC_SUPPRESS_WARNING_NEEDS_TO_HAVE_DLL_INTERFACE
      std::unique_ptr<UnknownFramePrivate> d;
    };

  }  // namespace ID3v2
}  // namespace TagLib
#endif
