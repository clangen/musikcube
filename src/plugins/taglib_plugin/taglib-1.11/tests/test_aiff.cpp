/***************************************************************************
    copyright           : (C) 2009 by Lukas Lalinsky
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
#include <tag.h>
#include <tbytevectorlist.h>
#include <aifffile.h>
#include <cppunit/extensions/HelperMacros.h>
#include "utils.h"

using namespace std;
using namespace TagLib;

class TestAIFF : public CppUnit::TestFixture
{
  CPPUNIT_TEST_SUITE(TestAIFF);
  CPPUNIT_TEST(testAiffProperties);
  CPPUNIT_TEST(testAiffCProperties);
  CPPUNIT_TEST(testSaveID3v2);
  CPPUNIT_TEST(testSaveID3v23);
  CPPUNIT_TEST(testDuplicateID3v2);
  CPPUNIT_TEST(testFuzzedFile1);
  CPPUNIT_TEST(testFuzzedFile2);
  CPPUNIT_TEST_SUITE_END();

public:

  void testAiffProperties()
  {
    RIFF::AIFF::File f(TEST_FILE_PATH_C("empty.aiff"));
    CPPUNIT_ASSERT(f.audioProperties());
    CPPUNIT_ASSERT_EQUAL(0, f.audioProperties()->lengthInSeconds());
    CPPUNIT_ASSERT_EQUAL(67, f.audioProperties()->lengthInMilliseconds());
    CPPUNIT_ASSERT_EQUAL(706, f.audioProperties()->bitrate());
    CPPUNIT_ASSERT_EQUAL(44100, f.audioProperties()->sampleRate());
    CPPUNIT_ASSERT_EQUAL(1, f.audioProperties()->channels());
    CPPUNIT_ASSERT_EQUAL(16, f.audioProperties()->bitsPerSample());
    CPPUNIT_ASSERT_EQUAL(2941U, f.audioProperties()->sampleFrames());
    CPPUNIT_ASSERT_EQUAL(false, f.audioProperties()->isAiffC());
  }

  void testAiffCProperties()
  {
    RIFF::AIFF::File f(TEST_FILE_PATH_C("alaw.aifc"));
    CPPUNIT_ASSERT(f.audioProperties());
    CPPUNIT_ASSERT_EQUAL(0, f.audioProperties()->lengthInSeconds());
    CPPUNIT_ASSERT_EQUAL(37, f.audioProperties()->lengthInMilliseconds());
    CPPUNIT_ASSERT_EQUAL(355, f.audioProperties()->bitrate());
    CPPUNIT_ASSERT_EQUAL(44100, f.audioProperties()->sampleRate());
    CPPUNIT_ASSERT_EQUAL(1, f.audioProperties()->channels());
    CPPUNIT_ASSERT_EQUAL(16, f.audioProperties()->bitsPerSample());
    CPPUNIT_ASSERT_EQUAL(1622U, f.audioProperties()->sampleFrames());
    CPPUNIT_ASSERT_EQUAL(true, f.audioProperties()->isAiffC());
    CPPUNIT_ASSERT_EQUAL(ByteVector("ALAW"), f.audioProperties()->compressionType());
    CPPUNIT_ASSERT_EQUAL(String("SGI CCITT G.711 A-law"), f.audioProperties()->compressionName());
  }

  void testSaveID3v2()
  {
    ScopedFileCopy copy("empty", ".aiff");
    string newname = copy.fileName();

    {
      RIFF::AIFF::File f(newname.c_str());
      CPPUNIT_ASSERT(!f.hasID3v2Tag());

      f.tag()->setTitle(L"TitleXXX");
      f.save();
      CPPUNIT_ASSERT(f.hasID3v2Tag());
    }
    {
      RIFF::AIFF::File f(newname.c_str());
      CPPUNIT_ASSERT(f.hasID3v2Tag());
      CPPUNIT_ASSERT_EQUAL(String(L"TitleXXX"), f.tag()->title());

      f.tag()->setTitle("");
      f.save();
      CPPUNIT_ASSERT(!f.hasID3v2Tag());
    }
    {
      RIFF::AIFF::File f(newname.c_str());
      CPPUNIT_ASSERT(!f.hasID3v2Tag());
    }
  }

  void testSaveID3v23()
  {
    ScopedFileCopy copy("empty", ".aiff");
    string newname = copy.fileName();

    String xxx = ByteVector(254, 'X');
    {
      RIFF::AIFF::File f(newname.c_str());
      CPPUNIT_ASSERT_EQUAL(false, f.hasID3v2Tag());

      f.tag()->setTitle(xxx);
      f.tag()->setArtist("Artist A");
      f.save(ID3v2::v3);
      CPPUNIT_ASSERT_EQUAL(true, f.hasID3v2Tag());
    }
    {
      RIFF::AIFF::File f2(newname.c_str());
      CPPUNIT_ASSERT_EQUAL((unsigned int)3, f2.tag()->header()->majorVersion());
      CPPUNIT_ASSERT_EQUAL(String("Artist A"), f2.tag()->artist());
      CPPUNIT_ASSERT_EQUAL(xxx, f2.tag()->title());
    }
  }

  void testDuplicateID3v2()
  {
    ScopedFileCopy copy("duplicate_id3v2", ".aiff");

    // duplicate_id3v2.aiff has duplicate ID3v2 tag chunks.
    // title() returns "Title2" if can't skip the second tag.

    RIFF::AIFF::File f(copy.fileName().c_str());
    CPPUNIT_ASSERT(f.hasID3v2Tag());
    CPPUNIT_ASSERT_EQUAL(String("Title1"), f.tag()->title());

    f.save();
    CPPUNIT_ASSERT_EQUAL(7030L, f.length());
    CPPUNIT_ASSERT_EQUAL(-1L, f.find("Title2"));
  }

  void testFuzzedFile1()
  {
    RIFF::AIFF::File f(TEST_FILE_PATH_C("segfault.aif"));
    CPPUNIT_ASSERT(!f.isValid());
  }

  void testFuzzedFile2()
  {
    RIFF::AIFF::File f(TEST_FILE_PATH_C("excessive_alloc.aif"));
    CPPUNIT_ASSERT(!f.isValid());
  }

};

CPPUNIT_TEST_SUITE_REGISTRATION(TestAIFF);
