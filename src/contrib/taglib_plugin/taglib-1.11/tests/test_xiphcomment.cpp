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
#include <xiphcomment.h>
#include <vorbisfile.h>
#include <tpropertymap.h>
#include <tdebug.h>
#include <cppunit/extensions/HelperMacros.h>
#include "utils.h"

using namespace std;
using namespace TagLib;

class TestXiphComment : public CppUnit::TestFixture
{
  CPPUNIT_TEST_SUITE(TestXiphComment);
  CPPUNIT_TEST(testYear);
  CPPUNIT_TEST(testSetYear);
  CPPUNIT_TEST(testTrack);
  CPPUNIT_TEST(testSetTrack);
  CPPUNIT_TEST(testInvalidKeys);
  CPPUNIT_TEST(testClearComment);
  CPPUNIT_TEST(testRemoveFields);
  CPPUNIT_TEST(testPicture);
  CPPUNIT_TEST_SUITE_END();

public:

  void testYear()
  {
    Ogg::XiphComment cmt;
    CPPUNIT_ASSERT_EQUAL((unsigned int)0, cmt.year());
    cmt.addField("YEAR", "2009");
    CPPUNIT_ASSERT_EQUAL((unsigned int)2009, cmt.year());
    cmt.addField("DATE", "2008");
    CPPUNIT_ASSERT_EQUAL((unsigned int)2008, cmt.year());
  }

  void testSetYear()
  {
    Ogg::XiphComment cmt;
    cmt.addField("YEAR", "2009");
    cmt.addField("DATE", "2008");
    cmt.setYear(1995);
    CPPUNIT_ASSERT(cmt.fieldListMap()["YEAR"].isEmpty());
    CPPUNIT_ASSERT_EQUAL(String("1995"), cmt.fieldListMap()["DATE"].front());
  }

  void testTrack()
  {
    Ogg::XiphComment cmt;
    CPPUNIT_ASSERT_EQUAL((unsigned int)0, cmt.track());
    cmt.addField("TRACKNUM", "7");
    CPPUNIT_ASSERT_EQUAL((unsigned int)7, cmt.track());
    cmt.addField("TRACKNUMBER", "8");
    CPPUNIT_ASSERT_EQUAL((unsigned int)8, cmt.track());
  }

  void testSetTrack()
  {
    Ogg::XiphComment cmt;
    cmt.addField("TRACKNUM", "7");
    cmt.addField("TRACKNUMBER", "8");
    cmt.setTrack(3);
    CPPUNIT_ASSERT(cmt.fieldListMap()["TRACKNUM"].isEmpty());
    CPPUNIT_ASSERT_EQUAL(String("3"), cmt.fieldListMap()["TRACKNUMBER"].front());
  }

  void testInvalidKeys()
  {
    PropertyMap map;
    map[""] = String("invalid key: empty string");
    map["A=B"] = String("invalid key: contains '='");
    map["A~B"] = String("invalid key: contains '~'");

    Ogg::XiphComment cmt;
    PropertyMap unsuccessful = cmt.setProperties(map);
    CPPUNIT_ASSERT_EQUAL((unsigned int)3, unsuccessful.size());
    CPPUNIT_ASSERT(cmt.properties().isEmpty());
  }

  void testClearComment()
  {
    ScopedFileCopy copy("empty", ".ogg");

    {
      Ogg::Vorbis::File f(copy.fileName().c_str());
      f.tag()->addField("COMMENT", "Comment1");
      f.save();
    }
    {
      Ogg::Vorbis::File f(copy.fileName().c_str());
      f.tag()->setComment("");
      CPPUNIT_ASSERT_EQUAL(String(""), f.tag()->comment());
    }
  }

  void testRemoveFields()
  {
    Ogg::Vorbis::File f(TEST_FILE_PATH_C("empty.ogg"));
    f.tag()->addField("title", "Title1");
    f.tag()->addField("Title", "Title1", false);
    f.tag()->addField("titlE", "Title2", false);
    f.tag()->addField("TITLE", "Title3", false);
    f.tag()->addField("artist", "Artist1");
    f.tag()->addField("ARTIST", "Artist2", false);
    CPPUNIT_ASSERT_EQUAL(String("Title1 Title1 Title2 Title3"), f.tag()->title());
    CPPUNIT_ASSERT_EQUAL(String("Artist1 Artist2"), f.tag()->artist());

    f.tag()->removeFields("title", "Title1");
    CPPUNIT_ASSERT_EQUAL(String("Title2 Title3"), f.tag()->title());
    CPPUNIT_ASSERT_EQUAL(String("Artist1 Artist2"), f.tag()->artist());

    f.tag()->removeFields("Artist");
    CPPUNIT_ASSERT_EQUAL(String("Title2 Title3"), f.tag()->title());
    CPPUNIT_ASSERT(f.tag()->artist().isEmpty());

    f.tag()->removeAllFields();
    CPPUNIT_ASSERT(f.tag()->title().isEmpty());
    CPPUNIT_ASSERT(f.tag()->artist().isEmpty());
    CPPUNIT_ASSERT_EQUAL(String("Xiph.Org libVorbis I 20050304"), f.tag()->vendorID());
  }

  void testPicture()
  {
    ScopedFileCopy copy("empty", ".ogg");
    string newname = copy.fileName();

    {
      Vorbis::File f(newname.c_str());
      FLAC::Picture *newpic = new FLAC::Picture();
      newpic->setType(FLAC::Picture::BackCover);
      newpic->setWidth(5);
      newpic->setHeight(6);
      newpic->setColorDepth(16);
      newpic->setNumColors(7);
      newpic->setMimeType("image/jpeg");
      newpic->setDescription("new image");
      newpic->setData("JPEG data");
      f.tag()->addPicture(newpic);
      f.save();
    }
    {
      Vorbis::File f(newname.c_str());
      List<FLAC::Picture *> lst = f.tag()->pictureList();
      CPPUNIT_ASSERT_EQUAL((unsigned int)1, lst.size());
      CPPUNIT_ASSERT_EQUAL((int)5, lst[0]->width());
      CPPUNIT_ASSERT_EQUAL((int)6, lst[0]->height());
      CPPUNIT_ASSERT_EQUAL((int)16, lst[0]->colorDepth());
      CPPUNIT_ASSERT_EQUAL((int)7, lst[0]->numColors());
      CPPUNIT_ASSERT_EQUAL(String("image/jpeg"), lst[0]->mimeType());
      CPPUNIT_ASSERT_EQUAL(String("new image"), lst[0]->description());
      CPPUNIT_ASSERT_EQUAL(ByteVector("JPEG data"), lst[0]->data());
    }
  }

};

CPPUNIT_TEST_SUITE_REGISTRATION(TestXiphComment);
