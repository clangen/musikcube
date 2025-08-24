/***************************************************************************
 *
    copyright            : (C) 2008 by Lukas Lalinsky
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

#ifndef TAGLIB_POPULARIMETERFRAME_H
#define TAGLIB_POPULARIMETERFRAME_H

#include "taglib_export.h"
#include "id3v2frame.h"

namespace TagLib {

  namespace ID3v2 {

    //! An implementation of ID3v2 "popularimeter"

    /*!
     * This implements the ID3v2 popularimeter (POPM frame).  It consists of
     * an email, a rating and an optional counter.
     */

    class TAGLIB_EXPORT PopularimeterFrame : public Frame
    {
      friend class FrameFactory;

    public:
      /*!
       * Construct an empty popularimeter frame.
       */
      explicit PopularimeterFrame();

      /*!
       * Construct a popularimeter based on the data in \a data.
       */
      explicit PopularimeterFrame(const ByteVector &data);

      /*!
       * Destroys this PopularimeterFrame instance.
       */
      ~PopularimeterFrame() override;

      PopularimeterFrame(const PopularimeterFrame &) = delete;
      PopularimeterFrame &operator=(const PopularimeterFrame &) = delete;

      /*!
       * Returns the text of this popularimeter.
       *
       * \see text()
       */
      String toString() const override;

      /*!
       * Returns email, rating and counter.
       */
      StringList toStringList() const override;

      /*!
       * Returns the email.
       *
       * \see setEmail()
       */
      String email() const;

      /*!
       * Set the email.
       *
       * \see email()
       */
      void setEmail(const String &s);

      /*!
       * Returns the rating.
       *
       * \see setRating()
       */
      int rating() const;

      /*!
       * Set the rating.
       *
       * \see rating()
       */
      void setRating(int s);

      /*!
       * Returns the counter.
       *
       * \see setCounter()
       */
      unsigned int counter() const;

      /*!
       * Set the counter.
       *
       * \see counter()
       */
      void setCounter(unsigned int s);

    protected:
      // Reimplementations.

      void parseFields(const ByteVector &data) override;
      ByteVector renderFields() const override;

    private:
      /*!
       * The constructor used by the FrameFactory.
       */
      PopularimeterFrame(const ByteVector &data, Header *h);

      class PopularimeterFramePrivate;
      TAGLIB_MSVC_SUPPRESS_WARNING_NEEDS_TO_HAVE_DLL_INTERFACE
      std::unique_ptr<PopularimeterFramePrivate> d;
    };

  }  // namespace ID3v2
}  // namespace TagLib
#endif
