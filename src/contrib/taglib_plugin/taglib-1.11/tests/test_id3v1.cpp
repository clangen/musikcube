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
#include <mpegfile.h>
#include <id3v1tag.h>
#include <id3v1genres.h>
#include <cppunit/extensions/HelperMacros.h>
#include "utils.h"

using namespace std;
using namespace TagLib;

class TestID3v1 : public CppUnit::TestFixture
{
  CPPUNIT_TEST_SUITE(TestID3v1);
  CPPUNIT_TEST(testStripWhiteSpace);
  CPPUNIT_TEST(testGenres);
  CPPUNIT_TEST_SUITE_END();

public:

  void testStripWhiteSpace()
  {
    ScopedFileCopy copy("xing", ".mp3");
    string newname = copy.fileName();

    {
      MPEG::File f(newname.c_str());
      f.ID3v1Tag(true)->setArtist("Artist     ");
      f.save();
    }

    {
      MPEG::File f(newname.c_str());
      CPPUNIT_ASSERT(f.ID3v1Tag(false));
      CPPUNIT_ASSERT_EQUAL(String("Artist"), f.ID3v1Tag(false)->artist());
    }
  }

  void testGenres()
  {
    CPPUNIT_ASSERT_EQUAL(String("Darkwave"), ID3v1::genre(50));
    CPPUNIT_ASSERT_EQUAL(100, ID3v1::genreIndex("Humour"));
  }

};

CPPUNIT_TEST_SUITE_REGISTRATION(TestID3v1);
