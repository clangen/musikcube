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

#ifndef TAGLIB_ID3V2FRAMEFACTORY_H
#define TAGLIB_ID3V2FRAMEFACTORY_H

#include "tbytevector.h"
#include "taglib_export.h"
#include "id3v2frame.h"
#include "id3v2header.h"

namespace TagLib {

  namespace ID3v2 {

    class TextIdentificationFrame;

    //! A factory for creating ID3v2 frames during parsing

    /*!
     * This factory abstracts away the frame creation process and instantiates
     * the appropriate ID3v2::Frame subclasses based on the contents of the
     * data.
     *
     * Reimplementing this factory is the key to adding support for frame types
     * not directly supported by TagLib to your application.  To do so you would
     * subclass this factory and reimplement createFrame().  Then by setting your
     * factory to be the default factory in ID3v2::Tag constructor you can
     * implement behavior that will allow for new ID3v2::Frame subclasses (also
     * provided by you) to be used.
     * See <a href="https://github.com/taglib/taglib/blob/master/tests/test_id3v2framefactory.cpp">
     * tests/test_id3v2framefactory.cpp</a> for an example.
     *
     * This implements both <i>abstract factory</i> and <i>singleton</i> patterns
     * of which more information is available on the web and in software design
     * textbooks (notably <i>Design Patterns</i>).
     *
     * \note You do not need to use this factory to create new frames to add to
     * an ID3v2::Tag.  You can instantiate frame subclasses directly (with \c new)
     * and add them to a tag using ID3v2::Tag::addFrame()
     *
     * \see ID3v2::Tag::addFrame()
     */

    class TAGLIB_EXPORT FrameFactory
    {
    public:
      FrameFactory(const FrameFactory &) = delete;
      FrameFactory &operator=(const FrameFactory &) = delete;

      static FrameFactory *instance();

      /*!
       * Create a frame based on \a origData.  \a tagHeader should be a valid
       * ID3v2::Header instance.
       */
      virtual Frame *createFrame(const ByteVector &origData, const Header *tagHeader) const;

      /*!
       * Creates a textual frame which corresponds to a single key in the
       * PropertyMap interface.  TIPL and TMCL do not belong to this category
       * and are thus handled explicitly in the Frame class.
       */
      virtual Frame *createFrameForProperty(
        const String &key, const StringList &values) const;

      /*!
       * After a tag has been read, this tries to rebuild some of them
       * information, most notably the recording date, from frames that
       * have been deprecated and can't be upgraded directly.
       */
      virtual void rebuildAggregateFrames(ID3v2::Tag *tag) const;

      /*!
       * Returns the default text encoding for text frames.  If setTextEncoding()
       * has not been explicitly called this will only be used for new text
       * frames.  However, if this value has been set explicitly all frames will be
       * converted to this type (unless it's explicitly set differently for the
       * individual frame) when being rendered.
       *
       * \see setDefaultTextEncoding()
       */
      String::Type defaultTextEncoding() const;

      /*!
       * Set the default text encoding for all text frames that are created to
       * \a encoding.  If no value is set the frames with either default to the
       * encoding type that was parsed and new frames default to Latin1.
       *
       * Valid string types for ID3v2 tags are Latin1, UTF8, UTF16 and UTF16BE.
       *
       * \see defaultTextEncoding()
       */
      void setDefaultTextEncoding(String::Type encoding);

      /*!
       * Returns \c true if defaultTextEncoding() is used.
       * The default text encoding is used when setDefaultTextEncoding() has
       * been called.  In this case, reimplementations of FrameFactory should
       * use defaultTextEncoding() on the frames (having a text encoding field)
       * they create.
       *
       * \see defaultTextEncoding()
       * \see setDefaultTextEncoding()
       */
      bool isUsingDefaultTextEncoding() const;

    protected:
      /*!
       * Constructs a frame factory.  Because this is a singleton this method is
       * protected, but may be used for subclasses.
       */
      FrameFactory();

      /*!
       * Destroys the frame factory.
       */
      ~FrameFactory();

      /*!
       * This method checks for compliance to the current ID3v2 standard (2.4)
       * and does nothing in the common case.  However if a frame is found that
       * is not compatible with the current standard, this method either updates
       * the frame or indicates that it should be discarded.
       *
       * This method with return \c true (with or without changes to the frame) if
       * this frame should be kept or \c false if it should be discarded.
       *
       * See the id3v2.4.0-changes.txt document for further information.
       */
      virtual bool updateFrame(Frame::Header *header) const;

      /*!
       * Creates and prepares the frame header for createFrame().
       *
       * \param data data of the frame (might be modified)
       * \param tagHeader the tag header
       * \return {header, ok}: header is a created frame header or nullptr
       *         if the frame is invalid; ok is \c true if the frame is supported.
       */
      std::pair<Frame::Header *, bool> prepareFrameHeader(
          ByteVector &data, const Header *tagHeader) const;

      /*!
       * Create a frame based on \a data.  \a header should be a valid frame
       * header and \a tagHeader a valid ID3v2::Header instance.
       *
       * This method is called by the public overloaded method
       * createFrame(const ByteVector &, const Header *) after creating
       * \a header from verified \a data using prepareFrameHeader(), so
       * this method is provided to be reimplemented in derived classes.
       */
      virtual Frame *createFrame(const ByteVector &data, Frame::Header *header,
                                 const Header *tagHeader) const;

    private:
      static FrameFactory factory;

      class FrameFactoryPrivate;
      TAGLIB_MSVC_SUPPRESS_WARNING_NEEDS_TO_HAVE_DLL_INTERFACE
      std::unique_ptr<FrameFactoryPrivate> d;
    };

  }  // namespace ID3v2
}  // namespace TagLib

#endif
