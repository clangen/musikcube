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

#ifndef TAGLIB_H
#define TAGLIB_H

#define TAGLIB_MAJOR_VERSION 2
#define TAGLIB_MINOR_VERSION 1
#define TAGLIB_PATCH_VERSION 1

#if (defined(_MSC_VER) && _MSC_VER >= 1600)
#define TAGLIB_CONSTRUCT_BITSET(x) static_cast<unsigned long long>(x)
#else
#define TAGLIB_CONSTRUCT_BITSET(x) static_cast<unsigned long>(x)
#endif

#define TAGLIB_DEPRECATED [[deprecated]]

#ifndef _WIN32
#include <sys/types.h>
#endif

//! A namespace for all TagLib related classes and functions

/*!
 * This namespace contains everything in TagLib.  For projects working with
 * TagLib extensively it may be convenient to add a
 * \code
 * using namespace TagLib;
 * \endcode
 */

namespace TagLib {

  class String;

  // Offset or length type for I/O streams.
  // In Win32, always 64bit. Otherwise, equivalent to off_t.
#ifdef _WIN32
  using offset_t = long long;
#elif !defined(__illumos__)
  using offset_t = off_t;
#endif

}  // namespace TagLib

/*!
 * \mainpage TagLib
 *
 * \section intro Introduction
 *
 * TagLib is a library for reading and editing audio metadata, commonly known as \e tags.
 *
 * Features:
 * - A clean, high level, C++ API for handling audio metadata.
 * - Format specific APIs for advanced API users.
 * - ID3v1, ID3v2, APE, FLAC, Xiph, iTunes-style MP4 and WMA tag formats.
 * - MP3, MPC, FLAC, MP4, ASF, AIFF, WAV, DSF, DFF, TrueAudio, WavPack, Ogg FLAC, Ogg Vorbis, Speex and Opus file formats.
 * - Basic audio file properties such as length, sample rate, etc.
 * - Long term binary and source compatibility.
 * - Extensible design, notably the ability to add other formats or extend current formats as a library user.
 * - Full support for unicode and internationalized tags.
 * - Dual <a href="http://www.mozilla.org/MPL/MPL-1.1.html">MPL</a> and
 *   <a href="http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html">LGPL</a> licenses.
 * - No external toolkit dependencies.
 *
 * \section why Why TagLib?
 *
 * TagLib originally was written to provide an updated and improved ID3v2 implementation in C++ for use
 * in a variety of Open Source projects.  Since development began in 2002 and the 1.0 release in 2004
 * it has expanded to cover a wide variety of tag and file formats and is used in a wide variety of
 * Open Source and proprietary applications.  It now supports a variety of UNIXes, including Apple's
 * macOS, as well as Microsoft Windows.
 *
 * \section commercial Usage in Commercial Applications
 *
 * TagLib's licenses \e do allow usage within proprietary (\e closed) applications, however TagLib is \e not
 * public domain.  Please note the requirements of the LGPL or MPL, and adhere to at least one of them.
 * In simple terms, you must at a minimum note your usage of TagLib, note the licensing terms of TagLib and
 * if you make changes to TagLib publish them.  Please review the licenses above before using TagLib in your
 * software.  Note that you may choose either the MPL or the LGPL, you do not have to fulfill the
 * requirements of both.
 *
 * \section installing Installing TagLib
 *
 * Please see the <a href="http://taglib.org/">TagLib website</a> for the latest
 * downloads.
 *
 * TagLib can be built using the CMake build system. TagLib installs a CMake
 * configuration and a taglib-config and pkg-config file to
 * make it easier to integrate into various build systems.  Note that TagLib's include install directory \e must
 * be included in the header include path. Simply adding <taglib/tag.h> will \e not work.
 *
 * Detailed instructions about building TagLib itself and building with TagLib
 * can be found in <a href="https://github.com/taglib/taglib/blob/master/INSTALL.md">INSTALL.md</a>
 *
 * \section start Getting Started
 *
 * TagLib provides both simple, abstract APIs which make it possible to ignore the differences between tagging
 * formats and format specific APIs which allow programmers to work with the features of specific tagging
 * schemes.  There is a similar abstraction mechanism for \link TagLib::AudioProperties AudioProperties \endlink.
 *
 * The best place to start is with the <b>Class Hierarchy</b> linked at the top of the page.
 * The \link TagLib::File File \endlink and \link TagLib::AudioProperties AudioProperties \endlink
 * classes and their subclasses are the core of TagLib.  The \link TagLib::FileRef FileRef \endlink
 * class is also a convenient way for using a value-based handle.
 *
 * \note When working with \link TagLib::FileRef FileRef \endlink please consider that it has only
 * the most basic (extension-based) file type resolution.  Please see its documentation on how to
 * plug in more advanced file type resolution.
 * (Such resolution may be part of later TagLib releases by default.)
 *
 * Here's a very simple example with TagLib:
 *
 * \code {.cpp}
 * TagLib::FileRef f("Latex Solar Beef.mp3");
 * TagLib::String artist = f.tag()->artist(); // artist == "Frank Zappa"
 *
 * f.tag()->setAlbum("Fillmore East");
 * f.save();
 *
 * TagLib::FileRef g("Free City Rhymes.ogg");
 * TagLib::String album = g.tag()->album(); // album == "NYC Ghosts & Flowers"
 *
 * g.tag()->setTrack(1);
 * g.save();
 * \endcode
 *
 * If the basic tag interface, which provides methods like
 * \link TagLib::Tag::title() title() \endlink,
 * \link TagLib::Tag::artist() artist() \endlink,
 * \link TagLib::Tag::album() album() \endlink,
 * \link TagLib::Tag::comment() comment() \endlink,
 * \link TagLib::Tag::genre() genre() \endlink,
 * \link TagLib::Tag::year() year() \endlink,
 * \link TagLib::Tag::track() track() \endlink
 * and the corresponding setters, is not enough, the
 * \link TagLib::PropertyMap PropertyMap \endlink interface
 * offers a flexible abstraction for textual metadata.
 * See \ref p_propertymapping for details about the mapping of tags to properties.
 *
 * \code {.cpp}
 * TagLib::PropertyMap props = f.properties();
 * TagLib::StringList artists = props["ARTIST"];
 * artists.append("Jim Pons");
 * props["ARTIST"] = artists;
 * f.setProperties(props);
 * f.save();
 * \endcode
 *
 * An additional \link TagLib::FileRef::complexProperties() abstraction \endlink is
 * provided to handle complex (i.e. non textual) properties.
 *
 * \code {.cpp}
 * TagLib::ByteVector data = ...;
 * f.setComplexProperties("PICTURE", {
 *   {
 *     {"data", data},
 *     {"pictureType", "Front Cover"},
 *     {"mimeType", "image/jpeg"}
 *   }
 * });
 * \endcode
 *
 * Finally, for full control, there are specific types for all supported metadata formats.
 *
 * \code {.cpp}
 * if(auto file = dynamic_cast<TagLib::MPEG::File *>(f.file())) {
 *   if(auto id3v2Tag = file->ID3v2Tag()) {
 *     auto frames = id3v2Tag->frameList("SYLT");
 *     if(!frames.isEmpty()) {
 *       if(auto syltFrame = dynamic_cast<TagLib::ID3v2::SynchronizedLyricsFrame *>(
 *           frames.front())) {
 *         auto text = syltFrame->synchedText();
 *         // ...
 *       }
 *     }
 *   }
 * }
 * \endcode
 *
 * More examples can be found in the <a href="https://github.com/taglib/taglib/tree/master/examples">
 * examples</a> directory of the source distribution.
 *
 * \section Contact
 *
 * Questions about TagLib should be directed to the TagLib mailing list, not directly to the author.
 *
 *  - <a href="http://taglib.org/">TagLib Homepage</a>
 *  - <a href="https://mail.kde.org/mailman/listinfo/taglib-devel">TagLib Mailing List (taglib-devel@kde.org)</a>
 *
 * \author <a href="https://github.com/taglib/taglib/blob/master/AUTHORS">TagLib authors</a>.
 */

#endif
