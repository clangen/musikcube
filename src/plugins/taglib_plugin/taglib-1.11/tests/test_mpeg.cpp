/***************************************************************************
    copyright           : (C) 2007 by Lukas Lalinsky
    email               : lukas@oxygene.sk
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

#include <string>
#include <stdio.h>
#include <tstring.h>
#include <tpropertymap.h>
#include <mpegfile.h>
#include <id3v2tag.h>
#include <id3v1tag.h>
#include <apetag.h>
#include <mpegproperties.h>
#include <xingheader.h>
#include <mpegheader.h>
#include <cppunit/extensions/HelperMacros.h>
#include "utils.h"

using namespace std;
using namespace TagLib;

class TestMPEG : public CppUnit::TestFixture
{
  CPPUNIT_TEST_SUITE(TestMPEG);
  CPPUNIT_TEST(testAudioPropertiesXingHeaderCBR);
  CPPUNIT_TEST(testAudioPropertiesXingHeaderVBR);
  CPPUNIT_TEST(testAudioPropertiesVBRIHeader);
  CPPUNIT_TEST(testAudioPropertiesNoVBRHeaders);
  CPPUNIT_TEST(testSkipInvalidFrames1);
  CPPUNIT_TEST(testSkipInvalidFrames2);
  CPPUNIT_TEST(testSkipInvalidFrames3);
  CPPUNIT_TEST(testVersion2DurationWithXingHeader);
  CPPUNIT_TEST(testSaveID3v24);
  CPPUNIT_TEST(testSaveID3v24WrongParam);
  CPPUNIT_TEST(testSaveID3v23);
  CPPUNIT_TEST(testDuplicateID3v2);
  CPPUNIT_TEST(testFuzzedFile);
  CPPUNIT_TEST(testFrameOffset);
  CPPUNIT_TEST(testStripAndProperties);
  CPPUNIT_TEST(testRepeatedSave1);
  CPPUNIT_TEST(testRepeatedSave2);
  CPPUNIT_TEST(testRepeatedSave3);
  CPPUNIT_TEST(testEmptyID3v2);
  CPPUNIT_TEST(testEmptyID3v1);
  CPPUNIT_TEST(testEmptyAPE);
  CPPUNIT_TEST(testIgnoreGarbage);
  CPPUNIT_TEST_SUITE_END();

public:

  void testAudioPropertiesXingHeaderCBR()
  {
    MPEG::File f(TEST_FILE_PATH_C("lame_cbr.mp3"));
    CPPUNIT_ASSERT(f.audioProperties());
    CPPUNIT_ASSERT_EQUAL(1887, f.audioProperties()->lengthInSeconds());
    CPPUNIT_ASSERT_EQUAL(1887164, f.audioProperties()->lengthInMilliseconds());
    CPPUNIT_ASSERT_EQUAL(64, f.audioProperties()->bitrate());
    CPPUNIT_ASSERT_EQUAL(1, f.audioProperties()->channels());
    CPPUNIT_ASSERT_EQUAL(44100, f.audioProperties()->sampleRate());
    CPPUNIT_ASSERT_EQUAL(MPEG::XingHeader::Xing, f.audioProperties()->xingHeader()->type());
  }

  void testAudioPropertiesXingHeaderVBR()
  {
    MPEG::File f(TEST_FILE_PATH_C("lame_vbr.mp3"));
    CPPUNIT_ASSERT(f.audioProperties());
    CPPUNIT_ASSERT_EQUAL(1887, f.audioProperties()->lengthInSeconds());
    CPPUNIT_ASSERT_EQUAL(1887164, f.audioProperties()->lengthInMilliseconds());
    CPPUNIT_ASSERT_EQUAL(70, f.audioProperties()->bitrate());
    CPPUNIT_ASSERT_EQUAL(1, f.audioProperties()->channels());
    CPPUNIT_ASSERT_EQUAL(44100, f.audioProperties()->sampleRate());
    CPPUNIT_ASSERT_EQUAL(MPEG::XingHeader::Xing, f.audioProperties()->xingHeader()->type());
  }

  void testAudioPropertiesVBRIHeader()
  {
    MPEG::File f(TEST_FILE_PATH_C("rare_frames.mp3"));
    CPPUNIT_ASSERT(f.audioProperties());
    CPPUNIT_ASSERT_EQUAL(222, f.audioProperties()->lengthInSeconds());
    CPPUNIT_ASSERT_EQUAL(222198, f.audioProperties()->lengthInMilliseconds());
    CPPUNIT_ASSERT_EQUAL(233, f.audioProperties()->bitrate());
    CPPUNIT_ASSERT_EQUAL(2, f.audioProperties()->channels());
    CPPUNIT_ASSERT_EQUAL(44100, f.audioProperties()->sampleRate());
    CPPUNIT_ASSERT_EQUAL(MPEG::XingHeader::VBRI, f.audioProperties()->xingHeader()->type());
  }

  void testAudioPropertiesNoVBRHeaders()
  {
    MPEG::File f(TEST_FILE_PATH_C("bladeenc.mp3"));
    CPPUNIT_ASSERT(f.audioProperties());
    CPPUNIT_ASSERT_EQUAL(3, f.audioProperties()->lengthInSeconds());
    CPPUNIT_ASSERT_EQUAL(3553, f.audioProperties()->lengthInMilliseconds());
    CPPUNIT_ASSERT_EQUAL(64, f.audioProperties()->bitrate());
    CPPUNIT_ASSERT_EQUAL(1, f.audioProperties()->channels());
    CPPUNIT_ASSERT_EQUAL(44100, f.audioProperties()->sampleRate());
    CPPUNIT_ASSERT(!f.audioProperties()->xingHeader());

    const long last = f.lastFrameOffset();
    const MPEG::Header lastHeader(&f, last, false);

    CPPUNIT_ASSERT_EQUAL(28213L, last);
    CPPUNIT_ASSERT_EQUAL(209, lastHeader.frameLength());
  }

  void testSkipInvalidFrames1()
  {
    MPEG::File f(TEST_FILE_PATH_C("invalid-frames1.mp3"));
    CPPUNIT_ASSERT(f.audioProperties());
    CPPUNIT_ASSERT_EQUAL(0, f.audioProperties()->lengthInSeconds());
    CPPUNIT_ASSERT_EQUAL(392, f.audioProperties()->lengthInMilliseconds());
    CPPUNIT_ASSERT_EQUAL(160, f.audioProperties()->bitrate());
    CPPUNIT_ASSERT_EQUAL(2, f.audioProperties()->channels());
    CPPUNIT_ASSERT_EQUAL(44100, f.audioProperties()->sampleRate());
    CPPUNIT_ASSERT(!f.audioProperties()->xingHeader());
  }

  void testSkipInvalidFrames2()
  {
    MPEG::File f(TEST_FILE_PATH_C("invalid-frames2.mp3"));
    CPPUNIT_ASSERT(f.audioProperties());
    CPPUNIT_ASSERT_EQUAL(0, f.audioProperties()->lengthInSeconds());
    CPPUNIT_ASSERT_EQUAL(314, f.audioProperties()->lengthInMilliseconds());
    CPPUNIT_ASSERT_EQUAL(192, f.audioProperties()->bitrate());
    CPPUNIT_ASSERT_EQUAL(2, f.audioProperties()->channels());
    CPPUNIT_ASSERT_EQUAL(44100, f.audioProperties()->sampleRate());
    CPPUNIT_ASSERT(!f.audioProperties()->xingHeader());
  }

  void testSkipInvalidFrames3()
  {
    MPEG::File f(TEST_FILE_PATH_C("invalid-frames3.mp3"));
    CPPUNIT_ASSERT(f.audioProperties());
    CPPUNIT_ASSERT_EQUAL(0, f.audioProperties()->lengthInSeconds());
    CPPUNIT_ASSERT_EQUAL(183, f.audioProperties()->lengthInMilliseconds());
    CPPUNIT_ASSERT_EQUAL(320, f.audioProperties()->bitrate());
    CPPUNIT_ASSERT_EQUAL(2, f.audioProperties()->channels());
    CPPUNIT_ASSERT_EQUAL(44100, f.audioProperties()->sampleRate());
    CPPUNIT_ASSERT(!f.audioProperties()->xingHeader());
  }

  void testVersion2DurationWithXingHeader()
  {
    MPEG::File f(TEST_FILE_PATH_C("mpeg2.mp3"));
    CPPUNIT_ASSERT(f.audioProperties());
    CPPUNIT_ASSERT_EQUAL(5387, f.audioProperties()->lengthInSeconds());
    CPPUNIT_ASSERT_EQUAL(5387285, f.audioProperties()->lengthInMilliseconds());
  }

  void testSaveID3v24()
  {
    ScopedFileCopy copy("xing", ".mp3");
    string newname = copy.fileName();

    String xxx = ByteVector(254, 'X');
    {
      MPEG::File f(newname.c_str());
      CPPUNIT_ASSERT_EQUAL(false, f.hasID3v2Tag());

      f.tag()->setTitle(xxx);
      f.tag()->setArtist("Artist A");
      f.save(MPEG::File::AllTags, File::StripOthers, ID3v2::v4);
      CPPUNIT_ASSERT_EQUAL(true, f.hasID3v2Tag());
    }
    {
      MPEG::File f2(newname.c_str());
      CPPUNIT_ASSERT_EQUAL((unsigned int)4, f2.ID3v2Tag()->header()->majorVersion());
      CPPUNIT_ASSERT_EQUAL(String("Artist A"), f2.tag()->artist());
      CPPUNIT_ASSERT_EQUAL(xxx, f2.tag()->title());
    }
  }

  void testSaveID3v24WrongParam()
  {
    ScopedFileCopy copy("xing", ".mp3");
    string newname = copy.fileName();

    String xxx = ByteVector(254, 'X');
    {
      MPEG::File f(newname.c_str());
      f.tag()->setTitle(xxx);
      f.tag()->setArtist("Artist A");
      f.save(MPEG::File::AllTags, true, 8);
    }
    {
      MPEG::File f2(newname.c_str());
      CPPUNIT_ASSERT_EQUAL((unsigned int)4, f2.ID3v2Tag()->header()->majorVersion());
      CPPUNIT_ASSERT_EQUAL(String("Artist A"), f2.tag()->artist());
      CPPUNIT_ASSERT_EQUAL(xxx, f2.tag()->title());
    }
  }

  void testSaveID3v23()
  {
    ScopedFileCopy copy("xing", ".mp3");
    string newname = copy.fileName();

    String xxx = ByteVector(254, 'X');
    {
      MPEG::File f(newname.c_str());
      CPPUNIT_ASSERT_EQUAL(false, f.hasID3v2Tag());

      f.tag()->setTitle(xxx);
      f.tag()->setArtist("Artist A");
      f.save(MPEG::File::AllTags, File::StripOthers, ID3v2::v3);
      CPPUNIT_ASSERT_EQUAL(true, f.hasID3v2Tag());
    }
    {
      MPEG::File f2(newname.c_str());
      CPPUNIT_ASSERT_EQUAL((unsigned int)3, f2.ID3v2Tag()->header()->majorVersion());
      CPPUNIT_ASSERT_EQUAL(String("Artist A"), f2.tag()->artist());
      CPPUNIT_ASSERT_EQUAL(xxx, f2.tag()->title());
    }
  }

  void testDuplicateID3v2()
  {
    MPEG::File f(TEST_FILE_PATH_C("duplicate_id3v2.mp3"));

    // duplicate_id3v2.mp3 has duplicate ID3v2 tags.
    // Sample rate will be 32000 if can't skip the second tag.

    CPPUNIT_ASSERT(f.hasID3v2Tag());
    CPPUNIT_ASSERT_EQUAL(44100, f.audioProperties()->sampleRate());
  }

  void testFuzzedFile()
  {
    MPEG::File f(TEST_FILE_PATH_C("excessive_alloc.mp3"));
    CPPUNIT_ASSERT(f.isValid());
  }

  void testFrameOffset()
  {
    {
      MPEG::File f(TEST_FILE_PATH_C("ape.mp3"));
      CPPUNIT_ASSERT(f.isValid());
      CPPUNIT_ASSERT_EQUAL((long)0x0000, f.firstFrameOffset());
      CPPUNIT_ASSERT_EQUAL((long)0x1FD6, f.lastFrameOffset());
    }
    {
      MPEG::File f(TEST_FILE_PATH_C("ape-id3v1.mp3"));
      CPPUNIT_ASSERT(f.isValid());
      CPPUNIT_ASSERT_EQUAL((long)0x0000, f.firstFrameOffset());
      CPPUNIT_ASSERT_EQUAL((long)0x1FD6, f.lastFrameOffset());
    }
    {
      MPEG::File f(TEST_FILE_PATH_C("ape-id3v2.mp3"));
      CPPUNIT_ASSERT(f.isValid());
      CPPUNIT_ASSERT_EQUAL((long)0x041A, f.firstFrameOffset());
      CPPUNIT_ASSERT_EQUAL((long)0x23F0, f.lastFrameOffset());
    }
  }

  void testStripAndProperties()
  {
    ScopedFileCopy copy("xing", ".mp3");

    {
      MPEG::File f(copy.fileName().c_str());
      f.ID3v2Tag(true)->setTitle("ID3v2");
      f.APETag(true)->setTitle("APE");
      f.ID3v1Tag(true)->setTitle("ID3v1");
      f.save();
    }
    {
      MPEG::File f(copy.fileName().c_str());
      CPPUNIT_ASSERT_EQUAL(String("ID3v2"), f.properties()["TITLE"].front());
      f.strip(MPEG::File::ID3v2);
      CPPUNIT_ASSERT_EQUAL(String("APE"), f.properties()["TITLE"].front());
      f.strip(MPEG::File::APE);
      CPPUNIT_ASSERT_EQUAL(String("ID3v1"), f.properties()["TITLE"].front());
      f.strip(MPEG::File::ID3v1);
      CPPUNIT_ASSERT(f.properties().isEmpty());
    }
  }

  void testRepeatedSave1()
  {
    ScopedFileCopy copy("xing", ".mp3");

    {
      MPEG::File f(copy.fileName().c_str());
      f.ID3v2Tag(true)->setTitle(std::string(4096, 'X').c_str());
      f.save();
    }
    {
      MPEG::File f(copy.fileName().c_str());
      f.ID3v2Tag(true)->setTitle("");
      f.save();
      f.ID3v2Tag(true)->setTitle(std::string(4096, 'X').c_str());
      f.save();
      CPPUNIT_ASSERT_EQUAL(5141L, f.firstFrameOffset());
    }
  }

  void testRepeatedSave2()
  {
    ScopedFileCopy copy("xing", ".mp3");

    MPEG::File f(copy.fileName().c_str());
    f.ID3v2Tag(true)->setTitle("0123456789");
    f.save();
    f.save();
    CPPUNIT_ASSERT_EQUAL(-1L, f.find("ID3", 3));
  }

  void testRepeatedSave3()
  {
    ScopedFileCopy copy("xing", ".mp3");

    {
      MPEG::File f(copy.fileName().c_str());
      CPPUNIT_ASSERT(!f.hasAPETag());
      CPPUNIT_ASSERT(!f.hasID3v1Tag());

      f.APETag(true)->setTitle("01234 56789 ABCDE FGHIJ");
      f.save();
      f.APETag()->setTitle("0");
      f.save();
      f.ID3v1Tag(true)->setTitle("01234 56789 ABCDE FGHIJ");
      f.APETag()->setTitle("01234 56789 ABCDE FGHIJ 01234 56789 ABCDE FGHIJ 01234 56789");
      f.save();
    }
    {
      MPEG::File f(copy.fileName().c_str());
      CPPUNIT_ASSERT(f.hasAPETag());
      CPPUNIT_ASSERT(f.hasID3v1Tag());
    }
  }

  void testEmptyID3v2()
  {
    ScopedFileCopy copy("xing", ".mp3");

    {
      MPEG::File f(copy.fileName().c_str());
      f.ID3v2Tag(true)->setTitle("0123456789");
      f.save(MPEG::File::ID3v2);
    }
    {
      MPEG::File f(copy.fileName().c_str());
      f.ID3v2Tag(true)->setTitle("");
      f.save(MPEG::File::ID3v2, File::StripNone);
    }
    {
      MPEG::File f(copy.fileName().c_str());
      CPPUNIT_ASSERT(!f.hasID3v2Tag());
    }
  }

  void testEmptyID3v1()
  {
    ScopedFileCopy copy("xing", ".mp3");

    {
      MPEG::File f(copy.fileName().c_str());
      f.ID3v1Tag(true)->setTitle("0123456789");
      f.save(MPEG::File::ID3v1);
    }
    {
      MPEG::File f(copy.fileName().c_str());
      f.ID3v1Tag(true)->setTitle("");
      f.save(MPEG::File::ID3v1, File::StripNone);
    }
    {
      MPEG::File f(copy.fileName().c_str());
      CPPUNIT_ASSERT(!f.hasID3v1Tag());
    }
  }

  void testEmptyAPE()
  {
    ScopedFileCopy copy("xing", ".mp3");

    {
      MPEG::File f(copy.fileName().c_str());
      f.APETag(true)->setTitle("0123456789");
      f.save(MPEG::File::APE);
    }
    {
      MPEG::File f(copy.fileName().c_str());
      f.APETag(true)->setTitle("");
      f.save(MPEG::File::APE, File::StripNone);
    }
    {
      MPEG::File f(copy.fileName().c_str());
      CPPUNIT_ASSERT(!f.hasAPETag());
    }
  }

  void testIgnoreGarbage()
  {
    const ScopedFileCopy copy("garbage", ".mp3");
    {
      MPEG::File f(copy.fileName().c_str());
      CPPUNIT_ASSERT(f.isValid());
      CPPUNIT_ASSERT(f.hasID3v2Tag());
      CPPUNIT_ASSERT_EQUAL(2255L, f.firstFrameOffset());
      CPPUNIT_ASSERT_EQUAL(6015L, f.lastFrameOffset());
      CPPUNIT_ASSERT_EQUAL(String("Title A"), f.ID3v2Tag()->title());
      f.ID3v2Tag()->setTitle("Title B");
      f.save();
    }
    {
      MPEG::File f(copy.fileName().c_str());
      CPPUNIT_ASSERT(f.isValid());
      CPPUNIT_ASSERT(f.hasID3v2Tag());
      CPPUNIT_ASSERT_EQUAL(String("Title B"), f.ID3v2Tag()->title());
    }
  }

};

CPPUNIT_TEST_SUITE_REGISTRATION(TestMPEG);
