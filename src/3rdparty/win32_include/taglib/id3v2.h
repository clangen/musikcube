#ifndef TAGLIB_ID3V2_H
#define TAGLIB_ID3V2_H

namespace TagLib {
  //! An ID3v2 implementation

  /*!
   * This is a relatively complete and flexible framework for working with ID3v2
   * tags.
   *
   * More information about ID3v2 tags can be found at
   * - <a href="https://github.com/taglib/taglib/blob/master/taglib/mpeg/id3v2/id3v2.2.0.txt">
   *   id3v2.2.0.txt</a>
   * - <a href="https://github.com/taglib/taglib/blob/master/taglib/mpeg/id3v2/id3v2.3.0.txt">
   *   id3v2.3.0.txt</a>
   * - <a href="https://github.com/taglib/taglib/blob/master/taglib/mpeg/id3v2/id3v2.4.0-structure.txt">
   *   id3v2.4.0-structure.txt</a>
   * - <a href="https://github.com/taglib/taglib/blob/master/taglib/mpeg/id3v2/id3v2.4.0-frames.txt">
   *   id3v2.4.0-frames.txt</a>
   *
   * \see ID3v2::Tag
   */
  namespace ID3v2 {
    /*!
     * Used to specify which version of the ID3 standard to use when saving tags.
     */
    enum Version {
      v3 = 3, //!< ID3v2.3
      v4 = 4  //!< ID3v2.4
    };
  } // namespace ID3v2
}  // namespace TagLib

#endif
