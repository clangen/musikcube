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
#include <tpropertymap.h>
#include <flacfile.h>
#include <xiphcomment.h>
#include <id3v1tag.h>
#include <id3v2tag.h>
#include <cppunit/extensions/HelperMacros.h>
#include "utils.h"

using namespace std;
using namespace TagLib;

class TestFLAC : public CppUnit::TestFixture
{
  CPPUNIT_TEST_SUITE(TestFLAC);
  CPPUNIT_TEST(testSignature);
  CPPUNIT_TEST(testMultipleCommentBlocks);
  CPPUNIT_TEST(testReadPicture);
  CPPUNIT_TEST(testAddPicture);
  CPPUNIT_TEST(testReplacePicture);
  CPPUNIT_TEST(testRemoveAllPictures);
  CPPUNIT_TEST(testRepeatedSave1);
  CPPUNIT_TEST(testRepeatedSave2);
  CPPUNIT_TEST(testRepeatedSave3);
  CPPUNIT_TEST(testSaveMultipleValues);
  CPPUNIT_TEST(testDict);
  CPPUNIT_TEST(testInvalid);
  CPPUNIT_TEST(testAudioProperties);
  CPPUNIT_TEST(testZeroSizedPadding1);
  CPPUNIT_TEST(testZeroSizedPadding2);
  CPPUNIT_TEST(testShrinkPadding);
  CPPUNIT_TEST(testSaveID3v1);
  CPPUNIT_TEST(testUpdateID3v2);
  CPPUNIT_TEST(testEmptyID3v2);
  CPPUNIT_TEST(testStripTags);
  CPPUNIT_TEST_SUITE_END();

public:

  void testSignature()
  {
    FLAC::File f(TEST_FILE_PATH_C("no-tags.flac"));
    CPPUNIT_ASSERT_EQUAL(ByteVector("a1b141f766e9849ac3db1030a20a3c77"), f.audioProperties()->signature().toHex());
  }

  void testMultipleCommentBlocks()
  {
    ScopedFileCopy copy("multiple-vc", ".flac");
    string newname = copy.fileName();

    {
      FLAC::File f(newname.c_str());
      CPPUNIT_ASSERT_EQUAL(String("Artist 1"), f.tag()->artist());
      f.tag()->setArtist("The Artist");
      f.save();
    }
    {
      FLAC::File f(newname.c_str());
      CPPUNIT_ASSERT_EQUAL(String("The Artist"), f.tag()->artist());
      CPPUNIT_ASSERT_EQUAL(69L, f.find("Artist"));
      CPPUNIT_ASSERT_EQUAL(-1L, f.find("Artist", 70));
    }
  }

  void testReadPicture()
  {
    ScopedFileCopy copy("silence-44-s", ".flac");
    string newname = copy.fileName();

    FLAC::File f(newname.c_str());
    List<FLAC::Picture *> lst = f.pictureList();
    CPPUNIT_ASSERT_EQUAL((unsigned int)1, lst.size());

    FLAC::Picture *pic = lst.front();
    CPPUNIT_ASSERT_EQUAL(FLAC::Picture::FrontCover, pic->type());
    CPPUNIT_ASSERT_EQUAL(1, pic->width());
    CPPUNIT_ASSERT_EQUAL(1, pic->height());
    CPPUNIT_ASSERT_EQUAL(24, pic->colorDepth());
    CPPUNIT_ASSERT_EQUAL(0, pic->numColors());
    CPPUNIT_ASSERT_EQUAL(String("image/png"), pic->mimeType());
    CPPUNIT_ASSERT_EQUAL(String("A pixel."), pic->description());
    CPPUNIT_ASSERT_EQUAL((unsigned int)150, pic->data().size());
  }

  void testAddPicture()
  {
    ScopedFileCopy copy("silence-44-s", ".flac");
    string newname = copy.fileName();

    {
      FLAC::File f(newname.c_str());
      List<FLAC::Picture *> lst = f.pictureList();
      CPPUNIT_ASSERT_EQUAL((unsigned int)1, lst.size());

      FLAC::Picture *newpic = new FLAC::Picture();
      newpic->setType(FLAC::Picture::BackCover);
      newpic->setWidth(5);
      newpic->setHeight(6);
      newpic->setColorDepth(16);
      newpic->setNumColors(7);
      newpic->setMimeType("image/jpeg");
      newpic->setDescription("new image");
      newpic->setData("JPEG data");
      f.addPicture(newpic);
      f.save();
    }
    {
      FLAC::File f(newname.c_str());
      List<FLAC::Picture *> lst = f.pictureList();
      CPPUNIT_ASSERT_EQUAL((unsigned int)2, lst.size());

      FLAC::Picture *pic = lst[0];
      CPPUNIT_ASSERT_EQUAL(FLAC::Picture::FrontCover, pic->type());
      CPPUNIT_ASSERT_EQUAL(1, pic->width());
      CPPUNIT_ASSERT_EQUAL(1, pic->height());
      CPPUNIT_ASSERT_EQUAL(24, pic->colorDepth());
      CPPUNIT_ASSERT_EQUAL(0, pic->numColors());
      CPPUNIT_ASSERT_EQUAL(String("image/png"), pic->mimeType());
      CPPUNIT_ASSERT_EQUAL(String("A pixel."), pic->description());
      CPPUNIT_ASSERT_EQUAL((unsigned int)150, pic->data().size());

      pic = lst[1];
      CPPUNIT_ASSERT_EQUAL(FLAC::Picture::BackCover, pic->type());
      CPPUNIT_ASSERT_EQUAL(5, pic->width());
      CPPUNIT_ASSERT_EQUAL(6, pic->height());
      CPPUNIT_ASSERT_EQUAL(16, pic->colorDepth());
      CPPUNIT_ASSERT_EQUAL(7, pic->numColors());
      CPPUNIT_ASSERT_EQUAL(String("image/jpeg"), pic->mimeType());
      CPPUNIT_ASSERT_EQUAL(String("new image"), pic->description());
      CPPUNIT_ASSERT_EQUAL(ByteVector("JPEG data"), pic->data());
    }
  }

  void testReplacePicture()
  {
    ScopedFileCopy copy("silence-44-s", ".flac");
    string newname = copy.fileName();

    {
      FLAC::File f(newname.c_str());
      List<FLAC::Picture *> lst = f.pictureList();
      CPPUNIT_ASSERT_EQUAL((unsigned int)1, lst.size());

      FLAC::Picture *newpic = new FLAC::Picture();
      newpic->setType(FLAC::Picture::BackCover);
      newpic->setWidth(5);
      newpic->setHeight(6);
      newpic->setColorDepth(16);
      newpic->setNumColors(7);
      newpic->setMimeType("image/jpeg");
      newpic->setDescription("new image");
      newpic->setData("JPEG data");
      f.removePictures();
      f.addPicture(newpic);
      f.save();
    }
    {
      FLAC::File f(newname.c_str());
      List<FLAC::Picture *> lst = f.pictureList();
      CPPUNIT_ASSERT_EQUAL((unsigned int)1, lst.size());

      FLAC::Picture *pic = lst[0];
      CPPUNIT_ASSERT_EQUAL(FLAC::Picture::BackCover, pic->type());
      CPPUNIT_ASSERT_EQUAL(5, pic->width());
      CPPUNIT_ASSERT_EQUAL(6, pic->height());
      CPPUNIT_ASSERT_EQUAL(16, pic->colorDepth());
      CPPUNIT_ASSERT_EQUAL(7, pic->numColors());
      CPPUNIT_ASSERT_EQUAL(String("image/jpeg"), pic->mimeType());
      CPPUNIT_ASSERT_EQUAL(String("new image"), pic->description());
      CPPUNIT_ASSERT_EQUAL(ByteVector("JPEG data"), pic->data());
    }
  }

  void testRemoveAllPictures()
  {
    ScopedFileCopy copy("silence-44-s", ".flac");
    string newname = copy.fileName();

    {
      FLAC::File f(newname.c_str());
      List<FLAC::Picture *> lst = f.pictureList();
      CPPUNIT_ASSERT_EQUAL((unsigned int)1, lst.size());

      f.removePictures();
      f.save();
    }
    {
      FLAC::File f(newname.c_str());
      List<FLAC::Picture *> lst = f.pictureList();
      CPPUNIT_ASSERT_EQUAL((unsigned int)0, lst.size());
    }
  }

  void testRepeatedSave1()
  {
    ScopedFileCopy copy("silence-44-s", ".flac");
    string newname = copy.fileName();

    {
      FLAC::File f(newname.c_str());
      CPPUNIT_ASSERT_EQUAL(String("Silence"), f.tag()->title());
      f.tag()->setTitle("NEW TITLE");
      f.save();
      CPPUNIT_ASSERT_EQUAL(String("NEW TITLE"), f.tag()->title());
      f.tag()->setTitle("NEW TITLE 2");
      f.save();
      CPPUNIT_ASSERT_EQUAL(String("NEW TITLE 2"), f.tag()->title());
    }
    {
      FLAC::File f(newname.c_str());
      CPPUNIT_ASSERT_EQUAL(String("NEW TITLE 2"), f.tag()->title());
    }
  }

  void testRepeatedSave2()
  {
    ScopedFileCopy copy("no-tags", ".flac");

    FLAC::File f(copy.fileName().c_str());
    f.ID3v2Tag(true)->setTitle("0123456789");
    f.save();
    CPPUNIT_ASSERT_EQUAL(5735L, f.length());
    f.save();
    CPPUNIT_ASSERT_EQUAL(5735L, f.length());
    CPPUNIT_ASSERT(f.find("fLaC") >= 0);
  }

  void testRepeatedSave3()
  {
    ScopedFileCopy copy("no-tags", ".flac");

    FLAC::File f(copy.fileName().c_str());
    f.xiphComment()->setTitle(std::string(8 * 1024, 'X').c_str());
    f.save();
    CPPUNIT_ASSERT_EQUAL(12862L, f.length());
    f.save();
    CPPUNIT_ASSERT_EQUAL(12862L, f.length());
  }

  void testSaveMultipleValues()
  {
    ScopedFileCopy copy("silence-44-s", ".flac");
    string newname = copy.fileName();

    {
      FLAC::File f(newname.c_str());
      f.xiphComment(true)->addField("ARTIST", "artist 1", true);
      f.xiphComment(true)->addField("ARTIST", "artist 2", false);
      f.save();
    }
    {
      FLAC::File f(newname.c_str());
      Ogg::FieldListMap m = f.xiphComment()->fieldListMap();
      CPPUNIT_ASSERT_EQUAL((unsigned int)2, m["ARTIST"].size());
      CPPUNIT_ASSERT_EQUAL(String("artist 1"), m["ARTIST"][0]);
      CPPUNIT_ASSERT_EQUAL(String("artist 2"), m["ARTIST"][1]);
    }
  }

  void testDict()
  {
    // test unicode & multiple values with dict interface
    ScopedFileCopy copy("silence-44-s", ".flac");
    string newname = copy.fileName();

    {
      FLAC::File f(newname.c_str());
      PropertyMap dict;
      dict["ARTIST"].append("artøst 1");
      dict["ARTIST"].append("artöst 2");
      f.setProperties(dict);
      f.save();
    }
    {
      FLAC::File f(newname.c_str());
      PropertyMap dict = f.properties();
      CPPUNIT_ASSERT_EQUAL((unsigned int)2, dict["ARTIST"].size());
      CPPUNIT_ASSERT_EQUAL(String("artøst 1"), dict["ARTIST"][0]);
      CPPUNIT_ASSERT_EQUAL(String("artöst 2"), dict["ARTIST"][1]);
    }
  }

  void testInvalid()
  {
    ScopedFileCopy copy("silence-44-s", ".flac");
    PropertyMap map;
    map[L"H\x00c4\x00d6"] = String("bla");
    FLAC::File f(copy.fileName().c_str());
    PropertyMap invalid = f.setProperties(map);
    CPPUNIT_ASSERT_EQUAL((unsigned int)1, invalid.size());
    CPPUNIT_ASSERT_EQUAL((unsigned int)0, f.properties().size());
  }

  void testAudioProperties()
  {
    FLAC::File f(TEST_FILE_PATH_C("sinewave.flac"));
    CPPUNIT_ASSERT(f.audioProperties());
    CPPUNIT_ASSERT_EQUAL(3, f.audioProperties()->length());
    CPPUNIT_ASSERT_EQUAL(3, f.audioProperties()->lengthInSeconds());
    CPPUNIT_ASSERT_EQUAL(3550, f.audioProperties()->lengthInMilliseconds());
    CPPUNIT_ASSERT_EQUAL(145, f.audioProperties()->bitrate());
    CPPUNIT_ASSERT_EQUAL(44100, f.audioProperties()->sampleRate());
    CPPUNIT_ASSERT_EQUAL(2, f.audioProperties()->channels());
    CPPUNIT_ASSERT_EQUAL(16, f.audioProperties()->bitsPerSample());
    CPPUNIT_ASSERT_EQUAL(16, f.audioProperties()->sampleWidth());
    CPPUNIT_ASSERT_EQUAL(156556ULL, f.audioProperties()->sampleFrames());
    CPPUNIT_ASSERT_EQUAL(
      ByteVector("\xcf\xe3\xd9\xda\xba\xde\xab\x2c\xbf\x2c\xa2\x35\x27\x4b\x7f\x76"),
      f.audioProperties()->signature());
  }

  void testZeroSizedPadding1()
  {
    ScopedFileCopy copy("zero-sized-padding", ".flac");

    FLAC::File f(copy.fileName().c_str());
    CPPUNIT_ASSERT(f.isValid());
  }

  void testZeroSizedPadding2()
  {
    ScopedFileCopy copy("silence-44-s", ".flac");

    {
      FLAC::File f(copy.fileName().c_str());
      f.xiphComment()->setTitle("ABC");
      f.save();
    }
    {
      FLAC::File f(copy.fileName().c_str());
      f.xiphComment()->setTitle(std::string(3067, 'X').c_str());
      f.save();
    }
    {
      FLAC::File f(copy.fileName().c_str());
      CPPUNIT_ASSERT(f.isValid());
    }
  }

  void testShrinkPadding()
  {
    ScopedFileCopy copy("no-tags", ".flac");

    {
      FLAC::File f(copy.fileName().c_str());
      f.xiphComment()->setTitle(std::wstring(128 * 1024, L'X').c_str());
      f.save();
      CPPUNIT_ASSERT(f.length() > 128 * 1024);
    }
    {
      FLAC::File f(copy.fileName().c_str());
      f.xiphComment()->setTitle("0123456789");
      f.save();
      CPPUNIT_ASSERT(f.length() < 8 * 1024);
    }
  }

  void testSaveID3v1()
  {
    ScopedFileCopy copy("no-tags", ".flac");

    ByteVector audioStream;
    {
      FLAC::File f(copy.fileName().c_str());
      CPPUNIT_ASSERT(!f.hasID3v1Tag());
      CPPUNIT_ASSERT_EQUAL((long)4692, f.length());

      f.seek(0x0100);
      audioStream = f.readBlock(4436);

      f.ID3v1Tag(true)->setTitle("01234 56789 ABCDE FGHIJ");
      f.save();
      CPPUNIT_ASSERT(f.hasID3v1Tag());
      CPPUNIT_ASSERT_EQUAL((long)4820, f.length());

      f.seek(0x0100);
      CPPUNIT_ASSERT_EQUAL(audioStream, f.readBlock(4436));
    }
  }

  void testUpdateID3v2()
  {
    ScopedFileCopy copy("no-tags", ".flac");

    {
      FLAC::File f(copy.fileName().c_str());
      f.ID3v2Tag(true)->setTitle("0123456789");
      f.save();
    }
    {
      FLAC::File f(copy.fileName().c_str());
      f.ID3v2Tag()->setTitle("ABCDEFGHIJ");
      f.save();
    }
    {
      FLAC::File f(copy.fileName().c_str());
      CPPUNIT_ASSERT_EQUAL(String("ABCDEFGHIJ"), f.ID3v2Tag()->title());
    }
  }

  void testEmptyID3v2()
  {
    ScopedFileCopy copy("no-tags", ".flac");

    {
      FLAC::File f(copy.fileName().c_str());
      f.ID3v2Tag(true);
      f.save();
    }
    {
      FLAC::File f(copy.fileName().c_str());
      CPPUNIT_ASSERT(!f.hasID3v2Tag());
    }
  }

  void testStripTags()
  {
    ScopedFileCopy copy("silence-44-s", ".flac");

    {
      FLAC::File f(copy.fileName().c_str());
      f.xiphComment(true)->setTitle("XiphComment Title");
      f.ID3v1Tag(true)->setTitle("ID3v1 Title");
      f.ID3v2Tag(true)->setTitle("ID3v2 Title");
      f.save();
    }
    {
      FLAC::File f(copy.fileName().c_str());
      CPPUNIT_ASSERT(f.hasXiphComment());
      CPPUNIT_ASSERT(f.hasID3v1Tag());
      CPPUNIT_ASSERT(f.hasID3v2Tag());
      CPPUNIT_ASSERT_EQUAL(String("XiphComment Title"), f.xiphComment()->title());
      CPPUNIT_ASSERT_EQUAL(String("ID3v1 Title"), f.ID3v1Tag()->title());
      CPPUNIT_ASSERT_EQUAL(String("ID3v2 Title"), f.ID3v2Tag()->title());
      f.strip(FLAC::File::ID3v2);
      f.save();
    }
    {
      FLAC::File f(copy.fileName().c_str());
      CPPUNIT_ASSERT(f.hasXiphComment());
      CPPUNIT_ASSERT(f.hasID3v1Tag());
      CPPUNIT_ASSERT(!f.hasID3v2Tag());
      CPPUNIT_ASSERT_EQUAL(String("XiphComment Title"), f.xiphComment()->title());
      CPPUNIT_ASSERT_EQUAL(String("ID3v1 Title"), f.ID3v1Tag()->title());
      f.strip(FLAC::File::ID3v1);
      f.save();
    }
    {
      FLAC::File f(copy.fileName().c_str());
      CPPUNIT_ASSERT(f.hasXiphComment());
      CPPUNIT_ASSERT(!f.hasID3v1Tag());
      CPPUNIT_ASSERT(!f.hasID3v2Tag());
      CPPUNIT_ASSERT_EQUAL(String("XiphComment Title"), f.xiphComment()->title());
      f.strip(FLAC::File::XiphComment);
      f.save();
    }
    {
      FLAC::File f(copy.fileName().c_str());
      CPPUNIT_ASSERT(f.hasXiphComment());
      CPPUNIT_ASSERT(!f.hasID3v1Tag());
      CPPUNIT_ASSERT(!f.hasID3v2Tag());
      CPPUNIT_ASSERT(f.xiphComment()->isEmpty());
      CPPUNIT_ASSERT_EQUAL(String("reference libFLAC 1.1.0 20030126"), f.xiphComment()->vendorID());
    }
  }

};

CPPUNIT_TEST_SUITE_REGISTRATION(TestFLAC);
