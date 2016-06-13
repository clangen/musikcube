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
#include <apetag.h>
#include <id3v1tag.h>
#include <tbytevectorlist.h>
#include <tpropertymap.h>
#include <wavpackfile.h>
#include <cppunit/extensions/HelperMacros.h>
#include "utils.h"

using namespace std;
using namespace TagLib;

class TestWavPack : public CppUnit::TestFixture
{
  CPPUNIT_TEST_SUITE(TestWavPack);
  CPPUNIT_TEST(testNoLengthProperties);
  CPPUNIT_TEST(testMultiChannelProperties);
  CPPUNIT_TEST(testTaggedProperties);
  CPPUNIT_TEST(testFuzzedFile);
  CPPUNIT_TEST(testStripAndProperties);
  CPPUNIT_TEST(testRepeatedSave);
  CPPUNIT_TEST_SUITE_END();

public:

  void testNoLengthProperties()
  {
    WavPack::File f(TEST_FILE_PATH_C("no_length.wv"));
    CPPUNIT_ASSERT(f.audioProperties());
    CPPUNIT_ASSERT_EQUAL(3, f.audioProperties()->length());
    CPPUNIT_ASSERT_EQUAL(3, f.audioProperties()->lengthInSeconds());
    CPPUNIT_ASSERT_EQUAL(3705, f.audioProperties()->lengthInMilliseconds());
    CPPUNIT_ASSERT_EQUAL(1, f.audioProperties()->bitrate());
    CPPUNIT_ASSERT_EQUAL(2, f.audioProperties()->channels());
    CPPUNIT_ASSERT_EQUAL(16, f.audioProperties()->bitsPerSample());
    CPPUNIT_ASSERT_EQUAL(true, f.audioProperties()->isLossless());
    CPPUNIT_ASSERT_EQUAL(44100, f.audioProperties()->sampleRate());
    CPPUNIT_ASSERT_EQUAL(163392U, f.audioProperties()->sampleFrames());
    CPPUNIT_ASSERT_EQUAL(1031, f.audioProperties()->version());
  }

  void testMultiChannelProperties()
  {
    WavPack::File f(TEST_FILE_PATH_C("four_channels.wv"));
    CPPUNIT_ASSERT(f.audioProperties());
    CPPUNIT_ASSERT_EQUAL(3, f.audioProperties()->length());
    CPPUNIT_ASSERT_EQUAL(3, f.audioProperties()->lengthInSeconds());
    CPPUNIT_ASSERT_EQUAL(3833, f.audioProperties()->lengthInMilliseconds());
    CPPUNIT_ASSERT_EQUAL(112, f.audioProperties()->bitrate());
    CPPUNIT_ASSERT_EQUAL(4, f.audioProperties()->channels());
    CPPUNIT_ASSERT_EQUAL(16, f.audioProperties()->bitsPerSample());
    CPPUNIT_ASSERT_EQUAL(false, f.audioProperties()->isLossless());
    CPPUNIT_ASSERT_EQUAL(44100, f.audioProperties()->sampleRate());
    CPPUNIT_ASSERT_EQUAL(169031U, f.audioProperties()->sampleFrames());
    CPPUNIT_ASSERT_EQUAL(1031, f.audioProperties()->version());
  }

  void testTaggedProperties()
  {
    WavPack::File f(TEST_FILE_PATH_C("tagged.wv"));
    CPPUNIT_ASSERT(f.audioProperties());
    CPPUNIT_ASSERT_EQUAL(3, f.audioProperties()->length());
    CPPUNIT_ASSERT_EQUAL(3, f.audioProperties()->lengthInSeconds());
    CPPUNIT_ASSERT_EQUAL(3550, f.audioProperties()->lengthInMilliseconds());
    CPPUNIT_ASSERT_EQUAL(172, f.audioProperties()->bitrate());
    CPPUNIT_ASSERT_EQUAL(2, f.audioProperties()->channels());
    CPPUNIT_ASSERT_EQUAL(16, f.audioProperties()->bitsPerSample());
    CPPUNIT_ASSERT_EQUAL(false, f.audioProperties()->isLossless());
    CPPUNIT_ASSERT_EQUAL(44100, f.audioProperties()->sampleRate());
    CPPUNIT_ASSERT_EQUAL(156556U, f.audioProperties()->sampleFrames());
    CPPUNIT_ASSERT_EQUAL(1031, f.audioProperties()->version());
  }

  void testFuzzedFile()
  {
    WavPack::File f(TEST_FILE_PATH_C("infloop.wv"));
    CPPUNIT_ASSERT(f.isValid());
  }

  void testStripAndProperties()
  {
    ScopedFileCopy copy("click", ".wv");

    {
      WavPack::File f(copy.fileName().c_str());
      f.APETag(true)->setTitle("APE");
      f.ID3v1Tag(true)->setTitle("ID3v1");
      f.save();
    }
    {
      WavPack::File f(copy.fileName().c_str());
      CPPUNIT_ASSERT_EQUAL(String("APE"), f.properties()["TITLE"].front());
      f.strip(WavPack::File::APE);
      CPPUNIT_ASSERT_EQUAL(String("ID3v1"), f.properties()["TITLE"].front());
      f.strip(WavPack::File::ID3v1);
      CPPUNIT_ASSERT(f.properties().isEmpty());
    }
  }

  void testRepeatedSave()
  {
    ScopedFileCopy copy("click", ".wv");

    {
      WavPack::File f(copy.fileName().c_str());
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
      WavPack::File f(copy.fileName().c_str());
      CPPUNIT_ASSERT(f.hasAPETag());
      CPPUNIT_ASSERT(f.hasID3v1Tag());
    }
  }

};

CPPUNIT_TEST_SUITE_REGISTRATION(TestWavPack);
