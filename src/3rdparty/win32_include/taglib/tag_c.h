/***************************************************************************
    copyright            : (C) 2003 by Scott Wheeler
    email                : wheeler@kde.org
 ***************************************************************************/

/***************************************************************************
 *   This library is free software; you can redistribute it and/or modify  *
 *   it  under the terms of the GNU Lesser General Public License version  *
 *   2.1 as published by the Free Software Foundation.                     *
 *                                                                         *
 *   This library is distributed in the hope that it will be useful, but   *
 *   WITHOUT ANY WARRANTY; without even the implied warranty of            *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU     *
 *   Lesser General Public License for more details.                       *
 *                                                                         *
 *   You should have received a copy of the GNU Lesser General Public      *
 *   License along with this library; if not, write to the Free Software   *
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  *
 *   USA                                                                   *
 ***************************************************************************/

#ifndef TAGLIB_TAG_C
#define TAGLIB_TAG_C

/* Do not include this in the main TagLib documentation. */
#ifndef DO_NOT_DOCUMENT

#ifdef __cplusplus
extern "C" {
#endif

#if defined(TAGLIB_STATIC)
#define TAGLIB_C_EXPORT
#elif defined(_WIN32) || defined(_WIN64)
#ifdef MAKE_TAGLIB_C_LIB
#define TAGLIB_C_EXPORT __declspec(dllexport)
#else
#define TAGLIB_C_EXPORT __declspec(dllimport)
#endif
#elif defined(__GNUC__) && (__GNUC__ > 4 || __GNUC__ == 4 && __GNUC_MINOR__ >= 1)
#define TAGLIB_C_EXPORT __attribute__ ((visibility("default")))
#else
#define TAGLIB_C_EXPORT
#endif

#include <wchar.h>
#ifdef _MSC_VER
/* minwindef.h contains typedef int BOOL */
#include <windows.h>
#elif !defined BOOL
#define BOOL int
#endif

/*******************************************************************************
 * [ TagLib C Binding ]
 *
 * This is an interface to TagLib's "simple" API, meaning that you can read and
 * modify media files in a generic, but not specialized way.  This is a rough
 * representation of TagLib::File and TagLib::Tag, for which the documentation
 * is somewhat more complete and worth consulting.
 *******************************************************************************/

/*
 * These are used to give the C API some type safety (as opposed to
 * using void * ), but pointers to them are simply cast to the corresponding C++
 * types in the implementation.
 */

typedef struct { int dummy; } TagLib_File;
typedef struct { int dummy; } TagLib_Tag;
typedef struct { int dummy; } TagLib_AudioProperties;
typedef struct { int dummy; } TagLib_IOStream;

/*!
 * By default all strings coming into or out of TagLib's C API are in UTF8.
 * However, it may be desirable for TagLib to operate on Latin1 (ISO-8859-1)
 * strings in which case this should be set to FALSE.
 */
TAGLIB_C_EXPORT void taglib_set_strings_unicode(BOOL unicode);

/*!
 * TagLib can keep track of strings that are created when outputting tag values
 * and clear them using taglib_tag_clear_strings().  This is enabled by default.
 * However if you wish to do more fine grained management of strings, you can do
 * so by setting \a management to FALSE.
 */
TAGLIB_C_EXPORT void taglib_set_string_management_enabled(BOOL management);

/*!
 * Explicitly free a string returned from TagLib
 */
TAGLIB_C_EXPORT void taglib_free(void* pointer);

/*******************************************************************************
 * Stream API
 ******************************************************************************/

/*!
 * Creates a byte vector stream from \a size bytes of \a data.
 * Such a stream can be used with taglib_file_new_iostream() to create a file
 * from memory.
 */
TAGLIB_C_EXPORT TagLib_IOStream *taglib_memory_iostream_new(const char *data, unsigned int size);

/*!
 * Frees and closes the stream.
 */
TAGLIB_C_EXPORT void taglib_iostream_free(TagLib_IOStream *stream);

/*******************************************************************************
 * File API
 ******************************************************************************/

typedef enum {
  TagLib_File_MPEG,
  TagLib_File_OggVorbis,
  TagLib_File_FLAC,
  TagLib_File_MPC,
  TagLib_File_OggFlac,
  TagLib_File_WavPack,
  TagLib_File_Speex,
  TagLib_File_TrueAudio,
  TagLib_File_MP4,
  TagLib_File_ASF,
  TagLib_File_AIFF,
  TagLib_File_WAV,
  TagLib_File_APE,
  TagLib_File_IT,
  TagLib_File_Mod,
  TagLib_File_S3M,
  TagLib_File_XM,
  TagLib_File_Opus,
  TagLib_File_DSF,
  TagLib_File_DSDIFF,
  TagLib_File_SHORTEN
} TagLib_File_Type;

/*!
 * Creates a TagLib file based on \a filename.  TagLib will try to guess the file
 * type.
 *
 * \returns NULL if the file type cannot be determined or the file cannot
 * be opened.
 */
TAGLIB_C_EXPORT TagLib_File *taglib_file_new(const char *filename);
#ifdef _WIN32
TAGLIB_C_EXPORT TagLib_File *taglib_file_new_wchar(const wchar_t *filename);
#endif

/*!
 * Creates a TagLib file based on \a filename.  Rather than attempting to guess
 * the type, it will use the one specified by \a type.
 */
TAGLIB_C_EXPORT TagLib_File *taglib_file_new_type(const char *filename, TagLib_File_Type type);
#ifdef _WIN32
TAGLIB_C_EXPORT TagLib_File *taglib_file_new_type_wchar(const wchar_t *filename, TagLib_File_Type type);
#endif

/*!
 * Creates a TagLib file from a \a stream.
 * A byte vector stream can be used to read a file from memory and write to
 * memory, e.g. when fetching network data.
 * The stream has to be created using taglib_memory_iostream_new() and shall be
 * freed using taglib_iostream_free() \e after this file is freed with
 * taglib_file_free().
 */
TAGLIB_C_EXPORT TagLib_File *taglib_file_new_iostream(TagLib_IOStream *stream);

/*!
 * Frees and closes the file.
 */
TAGLIB_C_EXPORT void taglib_file_free(TagLib_File *file);

/*!
 * Returns \c true if the file is open and readable and valid information for
 * the Tag and / or AudioProperties was found.
 */

TAGLIB_C_EXPORT BOOL taglib_file_is_valid(const TagLib_File *file);

/*!
 * Returns a pointer to the tag associated with this file.  This will be freed
 * automatically when the file is freed.
 */
TAGLIB_C_EXPORT TagLib_Tag *taglib_file_tag(const TagLib_File *file);

/*!
 * Returns a pointer to the audio properties associated with this file.  This
 * will be freed automatically when the file is freed.
 */
TAGLIB_C_EXPORT const TagLib_AudioProperties *taglib_file_audioproperties(const TagLib_File *file);

/*!
 * Saves the \a file to disk.
 */
TAGLIB_C_EXPORT BOOL taglib_file_save(TagLib_File *file);

/******************************************************************************
 * Tag API
 ******************************************************************************/

/*!
 * Returns a string with this tag's title.
 *
 * \note By default this string should be UTF8 encoded and its memory should be
 * freed using taglib_tag_free_strings().
 */
TAGLIB_C_EXPORT char *taglib_tag_title(const TagLib_Tag *tag);

/*!
 * Returns a string with this tag's artist.
 *
 * \note By default this string should be UTF8 encoded and its memory should be
 * freed using taglib_tag_free_strings().
 */
TAGLIB_C_EXPORT char *taglib_tag_artist(const TagLib_Tag *tag);

/*!
 * Returns a string with this tag's album name.
 *
 * \note By default this string should be UTF8 encoded and its memory should be
 * freed using taglib_tag_free_strings().
 */
TAGLIB_C_EXPORT char *taglib_tag_album(const TagLib_Tag *tag);

/*!
 * Returns a string with this tag's comment.
 *
 * \note By default this string should be UTF8 encoded and its memory should be
 * freed using taglib_tag_free_strings().
 */
TAGLIB_C_EXPORT char *taglib_tag_comment(const TagLib_Tag *tag);

/*!
 * Returns a string with this tag's genre.
 *
 * \note By default this string should be UTF8 encoded and its memory should be
 * freed using taglib_tag_free_strings().
 */
TAGLIB_C_EXPORT char *taglib_tag_genre(const TagLib_Tag *tag);

/*!
 * Returns the tag's year or 0 if the year is not set.
 */
TAGLIB_C_EXPORT unsigned int taglib_tag_year(const TagLib_Tag *tag);

/*!
 * Returns the tag's track number or 0 if the track number is not set.
 */
TAGLIB_C_EXPORT unsigned int taglib_tag_track(const TagLib_Tag *tag);

/*!
 * Sets the tag's title.
 *
 * \note By default this string should be UTF8 encoded.
 */
TAGLIB_C_EXPORT void taglib_tag_set_title(TagLib_Tag *tag, const char *title);

/*!
 * Sets the tag's artist.
 *
 * \note By default this string should be UTF8 encoded.
 */
TAGLIB_C_EXPORT void taglib_tag_set_artist(TagLib_Tag *tag, const char *artist);

/*!
 * Sets the tag's album.
 *
 * \note By default this string should be UTF8 encoded.
 */
TAGLIB_C_EXPORT void taglib_tag_set_album(TagLib_Tag *tag, const char *album);

/*!
 * Sets the tag's comment.
 *
 * \note By default this string should be UTF8 encoded.
 */
TAGLIB_C_EXPORT void taglib_tag_set_comment(TagLib_Tag *tag, const char *comment);

/*!
 * Sets the tag's genre.
 *
 * \note By default this string should be UTF8 encoded.
 */
TAGLIB_C_EXPORT void taglib_tag_set_genre(TagLib_Tag *tag, const char *genre);

/*!
 * Sets the tag's year.  0 indicates that this field should be cleared.
 */
TAGLIB_C_EXPORT void taglib_tag_set_year(TagLib_Tag *tag, unsigned int year);

/*!
 * Sets the tag's track number.  0 indicates that this field should be cleared.
 */
TAGLIB_C_EXPORT void taglib_tag_set_track(TagLib_Tag *tag, unsigned int track);

/*!
 * Frees all of the strings that have been created by the tag.
 */
TAGLIB_C_EXPORT void taglib_tag_free_strings(void);

/******************************************************************************
 * Audio Properties API
 ******************************************************************************/

/*!
 * Returns the length of the file in seconds.
 */
TAGLIB_C_EXPORT int taglib_audioproperties_length(const TagLib_AudioProperties *audioProperties);

/*!
 * Returns the bitrate of the file in kb/s.
 */
TAGLIB_C_EXPORT int taglib_audioproperties_bitrate(const TagLib_AudioProperties *audioProperties);

/*!
 * Returns the sample rate of the file in Hz.
 */
TAGLIB_C_EXPORT int taglib_audioproperties_samplerate(const TagLib_AudioProperties *audioProperties);

/*!
 * Returns the number of channels in the audio stream.
 */
TAGLIB_C_EXPORT int taglib_audioproperties_channels(const TagLib_AudioProperties *audioProperties);

/*******************************************************************************
 * Special convenience ID3v2 functions
 *******************************************************************************/

typedef enum {
  TagLib_ID3v2_Latin1,
  TagLib_ID3v2_UTF16,
  TagLib_ID3v2_UTF16BE,
  TagLib_ID3v2_UTF8
} TagLib_ID3v2_Encoding;

/*!
 * This sets the default encoding for ID3v2 frames that are written to tags.
 */

TAGLIB_C_EXPORT void taglib_id3v2_set_default_text_encoding(TagLib_ID3v2_Encoding encoding);

/******************************************************************************
 * Properties API
 ******************************************************************************/

/*!
 * Sets the property \a prop with \a value.  Use \a value = NULL to remove
 * the property, otherwise it will be replaced.
 */
TAGLIB_C_EXPORT void taglib_property_set(TagLib_File *file, const char *prop, const char *value);

/*!
 * Appends \a value to the property \a prop (sets it if non-existing).
 * Use \a value = NULL to remove all values associated with the property.
 */
TAGLIB_C_EXPORT void taglib_property_set_append(TagLib_File *file, const char *prop, const char *value);

/*!
 * Get the keys of the property map.
 *
 * \return NULL terminated array of C-strings (char *), only NULL if empty.
 * It must be freed by the client using taglib_property_free().
 */
TAGLIB_C_EXPORT char** taglib_property_keys(const TagLib_File *file);

/*!
 * Get value(s) of property \a prop.
 *
 * \return NULL terminated array of C-strings (char *), only NULL if empty.
 * It must be freed by the client using taglib_property_free().
 */
TAGLIB_C_EXPORT char** taglib_property_get(const TagLib_File *file, const char *prop);

/*!
 * Frees the NULL terminated array \a props and the C-strings it contains.
 */
TAGLIB_C_EXPORT void taglib_property_free(char **props);

/******************************************************************************
 * Complex Properties API
 ******************************************************************************/

/*!
 * Types which can be stored in a TagLib_Variant.
 *
 * \related TagLib::Variant::Type
 * These correspond to TagLib::Variant::Type, but ByteVectorList, VariantList,
 * VariantMap are not supported and will be returned as their string
 * representation.
 */
typedef enum {
  TagLib_Variant_Void,
  TagLib_Variant_Bool,
  TagLib_Variant_Int,
  TagLib_Variant_UInt,
  TagLib_Variant_LongLong,
  TagLib_Variant_ULongLong,
  TagLib_Variant_Double,
  TagLib_Variant_String,
  TagLib_Variant_StringList,
  TagLib_Variant_ByteVector
} TagLib_Variant_Type;

/*!
 * Discriminated union used in complex property attributes.
 *
 * \e type must be set according to the \e value union used.
 * \e size is only required for TagLib_Variant_ByteVector and must contain
 * the number of bytes.
 *
 * \related TagLib::Variant.
 */
typedef struct {
  TagLib_Variant_Type type;
  unsigned int size;
  union {
    char *stringValue;
    char *byteVectorValue;
    char **stringListValue;
    BOOL boolValue;
    int intValue;
    unsigned int uIntValue;
    long long longLongValue;
    unsigned long long uLongLongValue;
    double doubleValue;
  } value;
} TagLib_Variant;

/*!
 * Attribute of a complex property.
 * Complex properties consist of a NULL-terminated array of pointers to
 * this structure with \e key and \e value.
 */
typedef struct {
  char *key;
  TagLib_Variant value;
} TagLib_Complex_Property_Attribute;

/*!
 * Picture data extracted from a complex property by the convenience function
 * taglib_picture_from_complex_property().
 */
typedef struct {
  char *mimeType;
  char *description;
  char *pictureType;
  char *data;
  unsigned int size;
} TagLib_Complex_Property_Picture_Data;

/*!
 * Declare complex property attributes to set a picture.
 * Can be used to define a variable \a var which can be used with
 * taglib_complex_property_set() and a "PICTURE" key to set an
 * embedded picture with the picture data \a dat of size \a siz
 * and description \a desc, mime type \a mime and picture type
 * \a typ (size is unsigned int, the other input parameters char *).
 */
#define TAGLIB_COMPLEX_PROPERTY_PICTURE(var, dat, siz, desc, mime, typ)   \
const TagLib_Complex_Property_Attribute            \
var##Attrs[] = {                                   \
  {                                                \
    (char *)"data",                                \
    {                                              \
      TagLib_Variant_ByteVector,                   \
      (siz),                                       \
      {                                            \
        (char *)(dat)                              \
      }                                            \
    }                                              \
  },                                               \
  {                                                \
    (char *)"mimeType",                            \
    {                                              \
      TagLib_Variant_String,                       \
      0U,                                          \
      {                                            \
        (char *)(mime)                             \
      }                                            \
    }                                              \
  },                                               \
  {                                                \
    (char *)"description",                         \
    {                                              \
      TagLib_Variant_String,                       \
      0U,                                          \
      {                                            \
        (char *)(desc)                             \
      }                                            \
    }                                              \
  },                                               \
  {                                                \
    (char *)"pictureType",                         \
    {                                              \
      TagLib_Variant_String,                       \
      0U,                                          \
      {                                            \
        (char *)(typ)                              \
      }                                            \
    }                                              \
  }                                                \
};                                                 \
const TagLib_Complex_Property_Attribute *var[] = { \
  &var##Attrs[0], &var##Attrs[1], &var##Attrs[2],  \
  &var##Attrs[3], NULL                             \
}

/*!
 * Sets the complex property \a key with \a value.  Use \a value = NULL to
 * remove the property, otherwise it will be replaced with the NULL
 * terminated array of attributes in \a value.
 *
 * A picture can be set with the TAGLIB_COMPLEX_PROPERTY_PICTURE macro:
 *
 * \code {.c}
 * TagLib_File *file = taglib_file_new("myfile.mp3");
 * FILE *fh = fopen("mypicture.jpg", "rb");
 * if(fh) {
 *   fseek(fh, 0L, SEEK_END);
 *   long size = ftell(fh);
 *   fseek(fh, 0L, SEEK_SET);
 *   char *data = (char *)malloc(size);
 *   fread(data, size, 1, fh);
 *   TAGLIB_COMPLEX_PROPERTY_PICTURE(props, data, size, "Written by TagLib",
 *                                   "image/jpeg", "Front Cover");
 *   taglib_complex_property_set(file, "PICTURE", props);
 *   taglib_file_save(file);
 *   free(data);
 *   fclose(fh);
 * }
 * \endcode
 */
TAGLIB_C_EXPORT BOOL taglib_complex_property_set(
  TagLib_File *file, const char *key,
  const TagLib_Complex_Property_Attribute **value);

/*!
 * Appends \a value to the complex property \a key (sets it if non-existing).
 * Use \a value = NULL to remove all values associated with the \a key.
 */
TAGLIB_C_EXPORT BOOL taglib_complex_property_set_append(
  TagLib_File *file, const char *key,
  const TagLib_Complex_Property_Attribute **value);

/*!
 * Get the keys of the complex properties.
 *
 * \return NULL terminated array of C-strings (char *), only NULL if empty.
 * It must be freed by the client using taglib_complex_property_free_keys().
 */
TAGLIB_C_EXPORT char** taglib_complex_property_keys(const TagLib_File *file);

/*!
 * Get value(s) of complex property \a key.
 *
 * \return NULL terminated array of property values, which are themselves an
 * array of property attributes, only NULL if empty.
 * It must be freed by the client using taglib_complex_property_free().
 */
TAGLIB_C_EXPORT TagLib_Complex_Property_Attribute*** taglib_complex_property_get(
  const TagLib_File *file, const char *key);

/*!
 * Extract the complex property values of a picture.
 *
 * This function can be used to get the data from a "PICTURE" complex property
 * without having to traverse the whole variant map. A picture can be
 * retrieved like this:
 *
 * \code {.c}
 * TagLib_File *file = taglib_file_new("myfile.mp3");
 * TagLib_Complex_Property_Attribute*** properties =
 *   taglib_complex_property_get(file, "PICTURE");
 * TagLib_Complex_Property_Picture_Data picture;
 * taglib_picture_from_complex_property(properties, &picture);
 * // Do something with picture.mimeType, picture.description,
 * // picture.pictureType, picture.data, picture.size, e.g. extract it.
 * FILE *fh = fopen("mypicture.jpg", "wb");
 * if(fh) {
 *   fwrite(picture.data, picture.size, 1, fh);
 *   fclose(fh);
 * }
 * taglib_complex_property_free(properties);
 * \endcode
 *
 * Note that the data in \a picture contains pointers to data in \a properties,
 * i.e. it only lives as long as the properties, until they are freed with
 * taglib_complex_property_free().
 * If you want to access multiple pictures or additional properties of FLAC
 * pictures ("width", "height", "numColors", "colorDepth" int values), you
 * have to traverse the \a properties yourself.
 */
TAGLIB_C_EXPORT void taglib_picture_from_complex_property(
  TagLib_Complex_Property_Attribute*** properties,
  TagLib_Complex_Property_Picture_Data *picture);

/*!
 * Frees the NULL terminated array \a keys (as returned by
 * taglib_complex_property_keys()) and the C-strings it contains.
 */
TAGLIB_C_EXPORT void taglib_complex_property_free_keys(char **keys);

/*!
 * Frees the NULL terminated array \a props of property attribute arrays
 * (as returned by taglib_complex_property_get()) and the data such as
 * C-strings and byte vectors contained in these attributes.
 */
TAGLIB_C_EXPORT void taglib_complex_property_free(
  TagLib_Complex_Property_Attribute ***props);

#ifdef __cplusplus
}
#endif
#endif /* DO_NOT_DOCUMENT */
#endif
