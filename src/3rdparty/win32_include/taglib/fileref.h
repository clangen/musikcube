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

#ifndef TAGLIB_FILEREF_H
#define TAGLIB_FILEREF_H

#include "tfile.h"
#include "tstringlist.h"

#include "taglib_export.h"
#include "audioproperties.h"

namespace TagLib {

  class Tag;

  //! This class provides a simple abstraction for creating and handling files

  /*!
   * FileRef exists to provide a minimal, generic and value-based wrapper around
   * a File.  It is lightweight and implicitly shared, and as such suitable for
   * pass-by-value use.  This hides some of the uglier details of TagLib::File
   * and the non-generic portions of the concrete file implementations.
   *
   * This class is useful in a "simple usage" situation where it is desirable
   * to be able to get and set some of the tag information that is similar
   * across file types.
   *
   * Also note that it is probably a good idea to plug this into your mime
   * type system rather than using the constructor that accepts a file name using
   * the FileTypeResolver.
   *
   * \see FileTypeResolver
   * \see addFileTypeResolver()
   */

  class TAGLIB_EXPORT FileRef
  {
  public:

    //! A class for pluggable file type resolution.

    /*!
     * %File type resolver, better implement StreamTypeResolver in order to
     * support both file and stream resolution.
     */

    class TAGLIB_EXPORT FileTypeResolver
    {
    public:
      FileTypeResolver();
      /*!
       * Destroys this FileTypeResolver instance.
       */
      virtual ~FileTypeResolver() = 0;

      FileTypeResolver(const FileTypeResolver &) = delete;
      FileTypeResolver &operator=(const FileTypeResolver &) = delete;

      /*!
       * This method must be overridden to provide an additional file type
       * resolver.  If the resolver is able to determine the file type it should
       * return a valid File object; if not it should return nullptr.
       *
       * \note The created file is then owned by the FileRef and should not be
       * deleted.  Deletion will happen automatically when the FileRef passes
       * out of scope.
       */
      virtual File *createFile(FileName fileName,
                               bool readAudioProperties = true,
                               AudioProperties::ReadStyle
                               audioPropertiesStyle = AudioProperties::Average) const = 0;
    private:
      class FileTypeResolverPrivate;
      TAGLIB_MSVC_SUPPRESS_WARNING_NEEDS_TO_HAVE_DLL_INTERFACE
      std::unique_ptr<FileTypeResolverPrivate> d;
    };

    //! A class for pluggable stream type resolution.

    /*!
     * This class is used to extend TagLib's very basic file name based file
     * type resolution.
     *
     * This can be accomplished with:
     *
     * \code
     *
     * class MyStreamTypeResolver : StreamTypeResolver
     * {
     *   TagLib::File *createFile(TagLib::FileName *fileName, bool readProps,
     *                     AudioProperties::ReadStyle readStyle) const override
     *   {
     *     if(someCheckForAnMP3File(fileName))
     *       return new TagLib::MPEG::File(fileName, readProps, readStyle);
     *     return nullptr;
     *   }
     *
     *   TagLib::File *createFileFromStream(TagLib::IOStream *s, bool readProps,
     *                     AudioProperties::ReadStyle readStyle) const override
     *   {
     *     if(someCheckForAnMP3Stream(s))
     *       return new TagLib::MPEG::File(s, readProps, readStyle);
     *     return nullptr;
     *   }
     * }
     *
     * FileRef::addFileTypeResolver(new MyStreamTypeResolver);
     *
     * \endcode
     *
     * Naturally a less contrived example would be slightly more complex.  This
     * can be used to plug in mime-type detection systems or to add new file types
     * to TagLib.
     */

    class TAGLIB_EXPORT StreamTypeResolver : public FileTypeResolver
    {
    public:
      StreamTypeResolver();
      /*!
       * Destroys this StreamTypeResolver instance.
       */
      virtual ~StreamTypeResolver() override = 0; // virtual is needed by SWIG

      StreamTypeResolver(const StreamTypeResolver &) = delete;
      StreamTypeResolver &operator=(const StreamTypeResolver &) = delete;

      /*!
       * This method must be overridden to provide an additional stream type
       * resolver.  If the resolver is able to determine the file type it should
       * return a valid File object; if not it should return nullptr.
       *
       * \note The created file is then owned by the FileRef and should not be
       * deleted.  Deletion will happen automatically when the FileRef passes
       * out of scope.
       *
       * \see createFile()
       */
      virtual File *createFileFromStream(IOStream *stream,
                               bool readAudioProperties = true,
                               AudioProperties::ReadStyle
                               audioPropertiesStyle = AudioProperties::Average) const = 0;
    private:
      class StreamTypeResolverPrivate;
      TAGLIB_MSVC_SUPPRESS_WARNING_NEEDS_TO_HAVE_DLL_INTERFACE
      std::unique_ptr<StreamTypeResolverPrivate> d;
    };

    /*!
     * Creates a null FileRef.
     */
    FileRef();

    /*!
     * Create a FileRef from \a fileName.  If \a readAudioProperties is \c true then
     * the audio properties will be read using \a audioPropertiesStyle.  If
     * \a readAudioProperties is \c false then \a audioPropertiesStyle will be
     * ignored.
     *
     * Also see the note in the class documentation about why you may not want to
     * use this method in your application.
     */
    explicit FileRef(FileName fileName,
                     bool readAudioProperties = true,
                     AudioProperties::ReadStyle
                     audioPropertiesStyle = AudioProperties::Average);

    /*!
     * Construct a FileRef from an opened \a IOStream.  If \a readAudioProperties
     * is \c true then the audio properties will be read using \a audioPropertiesStyle.
     * If \a readAudioProperties is \c false then \a audioPropertiesStyle will be
     * ignored.
     *
     * Also see the note in the class documentation about why you may not want to
     * use this method in your application.
     *
     * \note TagLib will *not* take ownership of the stream, the caller is
     * responsible for deleting it after the File object.
     */
    explicit FileRef(IOStream* stream,
                     bool readAudioProperties = true,
                     AudioProperties::ReadStyle
                     audioPropertiesStyle = AudioProperties::Average);

    /*!
     * Construct a FileRef using \a file.  The FileRef now takes ownership of the
     * pointer and will delete the File when it passes out of scope.
     */
    explicit FileRef(File *file);

    /*!
     * Make a copy of \a ref.
     */
    FileRef(const FileRef &ref);

    /*!
     * Destroys this FileRef instance.
     */
    ~FileRef();

    /*!
     * Returns a pointer to the represented file's tag.
     *
     * \warning This pointer will become invalid when this FileRef and all
     * copies pass out of scope.
     *
     * \warning Do not cast it to any subclasses of Tag.
     * Use tag returning methods of appropriate subclasses of File instead.
     *
     * \see File::tag()
     */
    Tag *tag() const;

    /*!
     * Exports the tags of the file as dictionary mapping (human readable) tag
     * names (uppercase Strings) to StringLists of tag values. Calls this
     * method on the wrapped File instance.
     * For each metadata object of the file that could not be parsed into the PropertyMap
     * format, the returned map's unsupportedData() list will contain one entry identifying
     * that object (e.g. the frame type for ID3v2 tags). Use removeUnsupportedProperties()
     * to remove (a subset of) them.
     * For files that contain more than one tag (e.g. an MP3 with both an ID3v1 and an ID3v2
     * tag) only the most "modern" one will be exported (ID3v2 in this case).
     */
    PropertyMap properties() const;

    /*!
     * Removes unsupported properties, or a subset of them, from the file's metadata.
     * The parameter \a properties must contain only entries from
     * properties().unsupportedData().
     */
    void removeUnsupportedProperties(const StringList& properties);

    /*!
     * Sets the tags of the wrapped File to those specified in \a properties.
     * If some value(s) could not be written to the specific metadata format,
     * the returned PropertyMap will contain those value(s). Otherwise it will be empty,
     * indicating that no problems occurred.
     * With file types that support several tag formats (for instance, MP3 files can have
     * ID3v1, ID3v2, and APEv2 tags), this function will create the most appropriate one
     * (ID3v2 for MP3 files). Older formats will be updated as well, if they exist, but won't
     * be taken into account for the return value of this function.
     * See the documentation of the subclass implementations for detailed descriptions.
     */
    PropertyMap setProperties(const PropertyMap &properties);

    /*!
     * Get the keys of complex properties, i.e. properties which cannot be
     * represented simply by a string.
     * Because such properties might be expensive to fetch, there are separate
     * operations to get the available keys - which is expected to be cheap -
     * and getting and setting the property values.
     * Calls the method on the wrapped File, which collects the keys from one
     * or more of its tags.
     */
    StringList complexPropertyKeys() const;

    /*!
     * Get the complex properties for a given \a key.
     * In order to be flexible for different metadata formats, the properties
     * are represented as variant maps.  Despite this dynamic nature, some
     * degree of standardization should be achieved between formats:
     *
     * - PICTURE
     *   - data: ByteVector with picture data
     *   - description: String with description
     *   - pictureType: String with type as specified for ID3v2,
     *     e.g. "Front Cover", "Back Cover", "Band"
     *   - mimeType: String with image format, e.g. "image/jpeg"
     *   - optionally more information found in the tag, such as
     *     "width", "height", "numColors", "colorDepth" int values
     *     in FLAC pictures
     * - GENERALOBJECT
     *   - data: ByteVector with object data
     *   - description: String with description
     *   - fileName: String with file name
     *   - mimeType: String with MIME type
     *   - this is currently only implemented for ID3v2 GEOB frames
     *
     * Calls the method on the wrapped File, which gets the properties from one
     * or more of its tags.
     */
    List<VariantMap> complexProperties(const String &key) const;

    /*!
     * Set all complex properties for a given \a key using variant maps as
     * \a value with the same format as returned by complexProperties().
     * An empty list as \a value removes all complex properties for \a key.
     */
    bool setComplexProperties(const String &key, const List<VariantMap> &value);

    /*!
     * Returns the audio properties for this FileRef.  If no audio properties
     * were read then this will return a null pointer.
     */
    AudioProperties *audioProperties() const;

    /*!
     * Returns a pointer to the file represented by this handler class.
     *
     * As a general rule this call should be avoided since if you need to work
     * with file objects directly, you are probably better served instantiating
     * the File subclasses (i.e. MPEG::File) manually and working with their APIs.
     *
     * This <i>handle</i> exists to provide a minimal, generic and value-based
     * wrapper around a File.  Accessing the file directly generally indicates
     * a moving away from this simplicity (and into things beyond the scope of
     * FileRef).
     *
     * \warning This pointer will become invalid when this FileRef and all
     * copies pass out of scope.
     */
    File *file() const;

    /*!
     * Saves the file.  Returns \c true on success.
     */
    bool save();

    /*!
     * Adds a FileTypeResolver to the list of those used by TagLib.  Each
     * additional FileTypeResolver is added to the front of a list of resolvers
     * that are tried.  If the FileTypeResolver returns zero the next resolver
     * is tried.
     *
     * Returns a pointer to the added resolver (the same one that's passed in --
     * this is mostly so that static initializers have something to use for
     * assignment).
     *
     * \see FileTypeResolver
     */
    static const FileTypeResolver *addFileTypeResolver(const FileTypeResolver *resolver);

    /*!
     * Remove all resolvers added by addFileTypeResolver().
     */
    static void clearFileTypeResolvers();

    /*!
     * As is mentioned elsewhere in this class's documentation, the default file
     * type resolution code provided by TagLib only works by comparing file
     * extensions.
     *
     * This method returns the list of file extensions that are used by default.
     *
     * The extensions are all returned in lowercase, though the comparison used
     * by TagLib for resolution is case-insensitive.
     *
     * \note This does not account for any additional file type resolvers that
     * are plugged in.  Also note that this is not intended to replace a proper
     * mime-type resolution system, but is just here for reference.
     *
     * \see FileTypeResolver
     */
    static StringList defaultFileExtensions();

    /*!
     * Returns \c true if the file (and as such other pointers) are null.
     */
    bool isNull() const;

    /*!
     * Assign the file pointed to by \a ref to this FileRef.
     */
    FileRef &operator=(const FileRef &ref);

    /*!
     * Exchanges the content of the FileRef with the content of \a ref.
     */
    void swap(FileRef &ref) noexcept;

    /*!
     * Returns \c true if this FileRef and \a ref point to the same File object.
     */
    bool operator==(const FileRef &ref) const;

    /*!
     * Returns \c true if this FileRef and \a ref do not point to the same File
     * object.
     */
    bool operator!=(const FileRef &ref) const;

  private:
    void parse(FileName fileName, bool readAudioProperties, AudioProperties::ReadStyle audioPropertiesStyle);
    void parse(IOStream *stream, bool readAudioProperties, AudioProperties::ReadStyle audioPropertiesStyle);

    class FileRefPrivate;
    TAGLIB_MSVC_SUPPRESS_WARNING_NEEDS_TO_HAVE_DLL_INTERFACE
    std::shared_ptr<FileRefPrivate> d;
  };

} // namespace TagLib

#endif
