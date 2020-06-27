/***************************************************************************
    copyright           : (C) 2015 by Tsuda Kageyu
    email               : tsuda.kageyu@gmail.com
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

#include <speexfile.h>
#include <oggpageheader.h>
#include <cppunit/extensions/HelperMacros.h>
#include "utils.h"

using namespace std;
using namespace TagLib;

class TestSpeex : public CppUnit::TestFixture
{
  CPPUNIT_TEST_SUITE(TestSpeex);
  CPPUNIT_TEST(testAudioProperties);
  CPPUNIT_TEST(testSplitPackets);
  CPPUNIT_TEST_SUITE_END();

public:

  void testAudioProperties()
  {
    Ogg::Speex::File f(TEST_FILE_PATH_C("empty.spx"));
    CPPUNIT_ASSERT(f.audioProperties());
    CPPUNIT_ASSERT_EQUAL(3, f.audioProperties()->lengthInSeconds());
    CPPUNIT_ASSERT_EQUAL(3685, f.audioProperties()->lengthInMilliseconds());
    CPPUNIT_ASSERT_EQUAL(53, f.audioProperties()->bitrate());
    CPPUNIT_ASSERT_EQUAL(-1, f.audioProperties()->bitrateNominal());
    CPPUNIT_ASSERT_EQUAL(2, f.audioProperties()->channels());
    CPPUNIT_ASSERT_EQUAL(44100, f.audioProperties()->sampleRate());
  }

  void testSplitPackets()
  {
    ScopedFileCopy copy("empty", ".spx");
    string newname = copy.fileName();

    const String text = longText(128 * 1024, true);

    {
      Ogg::Speex::File f(newname.c_str());
      f.tag()->setTitle(text);
      f.save();
    }
    {
      Ogg::Speex::File f(newname.c_str());
      CPPUNIT_ASSERT(f.isValid());
      CPPUNIT_ASSERT_EQUAL(156330L, f.length());
      CPPUNIT_ASSERT_EQUAL(23, f.lastPageHeader()->pageSequenceNumber());
      CPPUNIT_ASSERT_EQUAL(80U, f.packet(0).size());
      CPPUNIT_ASSERT_EQUAL(131116U, f.packet(1).size());
      CPPUNIT_ASSERT_EQUAL(93U, f.packet(2).size());
      CPPUNIT_ASSERT_EQUAL(93U, f.packet(3).size());
      CPPUNIT_ASSERT_EQUAL(text, f.tag()->title());

      CPPUNIT_ASSERT(f.audioProperties());
      CPPUNIT_ASSERT_EQUAL(3685, f.audioProperties()->lengthInMilliseconds());

      f.tag()->setTitle("ABCDE");
      f.save();
    }
    {
      Ogg::Speex::File f(newname.c_str());
      CPPUNIT_ASSERT(f.isValid());
      CPPUNIT_ASSERT_EQUAL(24317L, f.length());
      CPPUNIT_ASSERT_EQUAL(7, f.lastPageHeader()->pageSequenceNumber());
      CPPUNIT_ASSERT_EQUAL(80U, f.packet(0).size());
      CPPUNIT_ASSERT_EQUAL(49U, f.packet(1).size());
      CPPUNIT_ASSERT_EQUAL(93U, f.packet(2).size());
      CPPUNIT_ASSERT_EQUAL(93U, f.packet(3).size());
      CPPUNIT_ASSERT_EQUAL(String("ABCDE"), f.tag()->title());

      CPPUNIT_ASSERT(f.audioProperties());
      CPPUNIT_ASSERT_EQUAL(3685, f.audioProperties()->lengthInMilliseconds());
    }
  }

};

CPPUNIT_TEST_SUITE_REGISTRATION(TestSpeex);
