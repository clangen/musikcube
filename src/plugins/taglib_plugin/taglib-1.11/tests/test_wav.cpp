/***************************************************************************
    copyright           : (C) 2010 by Lukas Lalinsky
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
#include <id3v2tag.h>
#include <infotag.h>
#include <tbytevectorlist.h>
#include <tpropertymap.h>
#include <wavfile.h>
#include <cppunit/extensions/HelperMacros.h>
#include "utils.h"

using namespace std;
using namespace TagLib;

class TestWAV : public CppUnit::TestFixture
{
  CPPUNIT_TEST_SUITE(TestWAV);
  CPPUNIT_TEST(testPCMProperties);
  CPPUNIT_TEST(testALAWProperties);
  CPPUNIT_TEST(testFloatProperties);
  CPPUNIT_TEST(testZeroSizeDataChunk);
  CPPUNIT_TEST(testID3v2Tag);
  CPPUNIT_TEST(testInfoTag);
  CPPUNIT_TEST(testStripTags);
  CPPUNIT_TEST(testDuplicateTags);
  CPPUNIT_TEST(testFuzzedFile1);
  CPPUNIT_TEST(testFuzzedFile2);
  CPPUNIT_TEST(testStripAndProperties);
  CPPUNIT_TEST(testPCMWithFactChunk);
  CPPUNIT_TEST_SUITE_END();

public:

  void testPCMProperties()
  {
    RIFF::WAV::File f(TEST_FILE_PATH_C("empty.wav"));
    CPPUNIT_ASSERT(f.audioProperties());
    CPPUNIT_ASSERT_EQUAL(3, f.audioProperties()->length());
    CPPUNIT_ASSERT_EQUAL(3, f.audioProperties()->lengthInSeconds());
    CPPUNIT_ASSERT_EQUAL(3675, f.audioProperties()->lengthInMilliseconds());
    CPPUNIT_ASSERT_EQUAL(32, f.audioProperties()->bitrate());
    CPPUNIT_ASSERT_EQUAL(2, f.audioProperties()->channels());
    CPPUNIT_ASSERT_EQUAL(1000, f.audioProperties()->sampleRate());
    CPPUNIT_ASSERT_EQUAL(16, f.audioProperties()->bitsPerSample());
    CPPUNIT_ASSERT_EQUAL(16, f.audioProperties()->sampleWidth());
    CPPUNIT_ASSERT_EQUAL(3675U, f.audioProperties()->sampleFrames());
    CPPUNIT_ASSERT_EQUAL(1, f.audioProperties()->format());
  }

  void testALAWProperties()
  {
    RIFF::WAV::File f(TEST_FILE_PATH_C("alaw.wav"));
    CPPUNIT_ASSERT(f.audioProperties());
    CPPUNIT_ASSERT_EQUAL(3, f.audioProperties()->length());
    CPPUNIT_ASSERT_EQUAL(3, f.audioProperties()->lengthInSeconds());
    CPPUNIT_ASSERT_EQUAL(3550, f.audioProperties()->lengthInMilliseconds());
    CPPUNIT_ASSERT_EQUAL(128, f.audioProperties()->bitrate());
    CPPUNIT_ASSERT_EQUAL(2, f.audioProperties()->channels());
    CPPUNIT_ASSERT_EQUAL(8000, f.audioProperties()->sampleRate());
    CPPUNIT_ASSERT_EQUAL(8, f.audioProperties()->bitsPerSample());
    CPPUNIT_ASSERT_EQUAL(8, f.audioProperties()->sampleWidth());
    CPPUNIT_ASSERT_EQUAL(28400U, f.audioProperties()->sampleFrames());
    CPPUNIT_ASSERT_EQUAL(6, f.audioProperties()->format());
  }

  void testFloatProperties()
  {
    RIFF::WAV::File f(TEST_FILE_PATH_C("float64.wav"));
    CPPUNIT_ASSERT(f.audioProperties());
    CPPUNIT_ASSERT_EQUAL(0, f.audioProperties()->length());
    CPPUNIT_ASSERT_EQUAL(0, f.audioProperties()->lengthInSeconds());
    CPPUNIT_ASSERT_EQUAL(97, f.audioProperties()->lengthInMilliseconds());
    CPPUNIT_ASSERT_EQUAL(5645, f.audioProperties()->bitrate());
    CPPUNIT_ASSERT_EQUAL(2, f.audioProperties()->channels());
    CPPUNIT_ASSERT_EQUAL(44100, f.audioProperties()->sampleRate());
    CPPUNIT_ASSERT_EQUAL(64, f.audioProperties()->bitsPerSample());
    CPPUNIT_ASSERT_EQUAL(64, f.audioProperties()->sampleWidth());
    CPPUNIT_ASSERT_EQUAL(4281U, f.audioProperties()->sampleFrames());
    CPPUNIT_ASSERT_EQUAL(3, f.audioProperties()->format());
  }

  void testZeroSizeDataChunk()
  {
    RIFF::WAV::File f(TEST_FILE_PATH_C("zero-size-chunk.wav"));
    CPPUNIT_ASSERT(!f.isValid());
  }

  void testID3v2Tag()
  {
    ScopedFileCopy copy("empty", ".wav");
    string filename = copy.fileName();

    {
      RIFF::WAV::File f(filename.c_str());
      CPPUNIT_ASSERT(f.isValid());
      CPPUNIT_ASSERT(!f.hasID3v2Tag());

      f.ID3v2Tag()->setTitle(L"Title");
      f.ID3v2Tag()->setArtist(L"Artist");
      f.save();
      CPPUNIT_ASSERT(f.hasID3v2Tag());
    }
    {
      RIFF::WAV::File f(filename.c_str());
      CPPUNIT_ASSERT(f.isValid());
      CPPUNIT_ASSERT(f.hasID3v2Tag());
      CPPUNIT_ASSERT_EQUAL(String(L"Title"),  f.ID3v2Tag()->title());
      CPPUNIT_ASSERT_EQUAL(String(L"Artist"), f.ID3v2Tag()->artist());

      f.ID3v2Tag()->setTitle(L"");
      f.ID3v2Tag()->setArtist(L"");
      f.save();
      CPPUNIT_ASSERT(!f.hasID3v2Tag());
    }
    {
      RIFF::WAV::File f(filename.c_str());
      CPPUNIT_ASSERT(f.isValid());
      CPPUNIT_ASSERT(!f.hasID3v2Tag());
      CPPUNIT_ASSERT_EQUAL(String(L""), f.ID3v2Tag()->title());
      CPPUNIT_ASSERT_EQUAL(String(L""), f.ID3v2Tag()->artist());
    }
  }

  void testInfoTag()
  {
    ScopedFileCopy copy("empty", ".wav");
    string filename = copy.fileName();

    {
      RIFF::WAV::File f(filename.c_str());
      CPPUNIT_ASSERT(f.isValid());
      CPPUNIT_ASSERT(!f.hasInfoTag());

      f.InfoTag()->setTitle(L"Title");
      f.InfoTag()->setArtist(L"Artist");
      f.save();
      CPPUNIT_ASSERT(f.hasInfoTag());
    }
    {
      RIFF::WAV::File f(filename.c_str());
      CPPUNIT_ASSERT(f.isValid());
      CPPUNIT_ASSERT(f.hasInfoTag());
      CPPUNIT_ASSERT_EQUAL(String(L"Title"),  f.InfoTag()->title());
      CPPUNIT_ASSERT_EQUAL(String(L"Artist"), f.InfoTag()->artist());

      f.InfoTag()->setTitle(L"");
      f.InfoTag()->setArtist(L"");
      f.save();
      CPPUNIT_ASSERT(!f.hasInfoTag());
    }

    {
      RIFF::WAV::File f(filename.c_str());
      CPPUNIT_ASSERT(f.isValid());
      CPPUNIT_ASSERT(!f.hasInfoTag());
      CPPUNIT_ASSERT_EQUAL(String(L""), f.InfoTag()->title());
      CPPUNIT_ASSERT_EQUAL(String(L""), f.InfoTag()->artist());
    }
  }

  void testStripTags()
  {
    ScopedFileCopy copy("empty", ".wav");
    string filename = copy.fileName();

    {
      RIFF::WAV::File f(filename.c_str());
      f.ID3v2Tag()->setTitle("test title");
      f.InfoTag()->setTitle("test title");
      f.save();
    }
    {
      RIFF::WAV::File f(filename.c_str());
      CPPUNIT_ASSERT(f.hasID3v2Tag());
      CPPUNIT_ASSERT(f.hasInfoTag());
      f.save(RIFF::WAV::File::ID3v2, true);
    }
    {
      RIFF::WAV::File f(filename.c_str());
      CPPUNIT_ASSERT(f.hasID3v2Tag());
      CPPUNIT_ASSERT(!f.hasInfoTag());
      f.ID3v2Tag()->setTitle("test title");
      f.InfoTag()->setTitle("test title");
      f.save();
    }
    {
      RIFF::WAV::File f(filename.c_str());
      CPPUNIT_ASSERT(f.hasID3v2Tag());
      CPPUNIT_ASSERT(f.hasInfoTag());
      f.save(RIFF::WAV::File::Info, true);
    }
    {
      RIFF::WAV::File f(filename.c_str());
      CPPUNIT_ASSERT(!f.hasID3v2Tag());
      CPPUNIT_ASSERT(f.hasInfoTag());
    }
  }

  void testDuplicateTags()
  {
    ScopedFileCopy copy("duplicate_tags", ".wav");

    RIFF::WAV::File f(copy.fileName().c_str());
    CPPUNIT_ASSERT_EQUAL(17052L, f.length());

    // duplicate_tags.wav has duplicate ID3v2/INFO tags.
    // title() returns "Title2" if can't skip the second tag.

    CPPUNIT_ASSERT(f.hasID3v2Tag());
    CPPUNIT_ASSERT_EQUAL(String("Title1"), f.ID3v2Tag()->title());

    CPPUNIT_ASSERT(f.hasInfoTag());
    CPPUNIT_ASSERT_EQUAL(String("Title1"), f.InfoTag()->title());

    f.save();
    CPPUNIT_ASSERT_EQUAL(15898L, f.length());
    CPPUNIT_ASSERT_EQUAL(-1L, f.find("Title2"));
  }

  void testFuzzedFile1()
  {
    RIFF::WAV::File f1(TEST_FILE_PATH_C("infloop.wav"));
    CPPUNIT_ASSERT(!f1.isValid());
  }

  void testFuzzedFile2()
  {
    RIFF::WAV::File f2(TEST_FILE_PATH_C("segfault.wav"));
    CPPUNIT_ASSERT(f2.isValid());
  }

  void testStripAndProperties()
  {
    ScopedFileCopy copy("empty", ".wav");

    {
      RIFF::WAV::File f(copy.fileName().c_str());
      f.ID3v2Tag()->setTitle("ID3v2");
      f.InfoTag()->setTitle("INFO");
      f.save();
    }
    {
      RIFF::WAV::File f(copy.fileName().c_str());
      CPPUNIT_ASSERT_EQUAL(String("ID3v2"), f.properties()["TITLE"].front());
      f.strip(RIFF::WAV::File::ID3v2);
      CPPUNIT_ASSERT_EQUAL(String("INFO"), f.properties()["TITLE"].front());
      f.strip(RIFF::WAV::File::Info);
      CPPUNIT_ASSERT(f.properties().isEmpty());
    }
  }

  void testPCMWithFactChunk()
  {
    RIFF::WAV::File f(TEST_FILE_PATH_C("pcm_with_fact_chunk.wav"));
    CPPUNIT_ASSERT(f.audioProperties());
    CPPUNIT_ASSERT_EQUAL(3, f.audioProperties()->length());
    CPPUNIT_ASSERT_EQUAL(3, f.audioProperties()->lengthInSeconds());
    CPPUNIT_ASSERT_EQUAL(3675, f.audioProperties()->lengthInMilliseconds());
    CPPUNIT_ASSERT_EQUAL(32, f.audioProperties()->bitrate());
    CPPUNIT_ASSERT_EQUAL(2, f.audioProperties()->channels());
    CPPUNIT_ASSERT_EQUAL(1000, f.audioProperties()->sampleRate());
    CPPUNIT_ASSERT_EQUAL(16, f.audioProperties()->bitsPerSample());
    CPPUNIT_ASSERT_EQUAL(16, f.audioProperties()->sampleWidth());
    CPPUNIT_ASSERT_EQUAL(3675U, f.audioProperties()->sampleFrames());
    CPPUNIT_ASSERT_EQUAL(1, f.audioProperties()->format());
  }

};

CPPUNIT_TEST_SUITE_REGISTRATION(TestWAV);
