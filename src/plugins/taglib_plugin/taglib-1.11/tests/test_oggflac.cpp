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
#include <tstringlist.h>
#include <tbytevectorlist.h>
#include <oggfile.h>
#include <oggflacfile.h>
#include <oggpageheader.h>
#include <cppunit/extensions/HelperMacros.h>
#include "utils.h"

using namespace std;
using namespace TagLib;

class TestOggFLAC : public CppUnit::TestFixture
{
  CPPUNIT_TEST_SUITE(TestOggFLAC);
  CPPUNIT_TEST(testFramingBit);
  CPPUNIT_TEST(testFuzzedFile);
  CPPUNIT_TEST(testSplitPackets);
  CPPUNIT_TEST_SUITE_END();

public:

  void testFramingBit()
  {
    ScopedFileCopy copy("empty_flac", ".oga");
    string newname = copy.fileName();

    {
      Ogg::FLAC::File f(newname.c_str());
      f.tag()->setArtist("The Artist");
      f.save();
    }
    {
      Ogg::FLAC::File f(newname.c_str());
      CPPUNIT_ASSERT_EQUAL(String("The Artist"), f.tag()->artist());

      f.seek(0, File::End);
      CPPUNIT_ASSERT_EQUAL(9134L, f.tell());
    }
  }

  void testFuzzedFile()
  {
    Ogg::FLAC::File f(TEST_FILE_PATH_C("segfault.oga"));
    CPPUNIT_ASSERT(!f.isValid());
  }

  void testSplitPackets()
  {
    ScopedFileCopy copy("empty_flac", ".oga");
    string newname = copy.fileName();

    const String text = longText(128 * 1024, true);

    {
      Ogg::FLAC::File f(newname.c_str());
      f.tag()->setTitle(text);
      f.save();
    }
    {
      Ogg::FLAC::File f(newname.c_str());
      CPPUNIT_ASSERT(f.isValid());
      CPPUNIT_ASSERT_EQUAL(141141L, f.length());
      CPPUNIT_ASSERT_EQUAL(21, f.lastPageHeader()->pageSequenceNumber());
      CPPUNIT_ASSERT_EQUAL(51U, f.packet(0).size());
      CPPUNIT_ASSERT_EQUAL(131126U, f.packet(1).size());
      CPPUNIT_ASSERT_EQUAL(22U, f.packet(2).size());
      CPPUNIT_ASSERT_EQUAL(8196U, f.packet(3).size());
      CPPUNIT_ASSERT_EQUAL(text, f.tag()->title());

      CPPUNIT_ASSERT(f.audioProperties());
      CPPUNIT_ASSERT_EQUAL(3705, f.audioProperties()->lengthInMilliseconds());

      f.tag()->setTitle("ABCDE");
      f.save();
    }
    {
      Ogg::FLAC::File f(newname.c_str());
      CPPUNIT_ASSERT(f.isValid());
      CPPUNIT_ASSERT_EQUAL(9128L, f.length());
      CPPUNIT_ASSERT_EQUAL(5, f.lastPageHeader()->pageSequenceNumber());
      CPPUNIT_ASSERT_EQUAL(51U, f.packet(0).size());
      CPPUNIT_ASSERT_EQUAL(59U, f.packet(1).size());
      CPPUNIT_ASSERT_EQUAL(22U, f.packet(2).size());
      CPPUNIT_ASSERT_EQUAL(8196U, f.packet(3).size());
      CPPUNIT_ASSERT_EQUAL(String("ABCDE"), f.tag()->title());

      CPPUNIT_ASSERT(f.audioProperties());
      CPPUNIT_ASSERT_EQUAL(3705, f.audioProperties()->lengthInMilliseconds());
    }
  }

};

CPPUNIT_TEST_SUITE_REGISTRATION(TestOggFLAC);
