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

#include <tfile.h>
#include <cppunit/extensions/HelperMacros.h>
#include "utils.h"

using namespace TagLib;

// File subclass that gives tests access to filesystem operations
class PlainFile : public File {
public:
  PlainFile(FileName name) : File(name) { }
  Tag *tag() const { return NULL; }
  AudioProperties *audioProperties() const { return NULL; }
  bool save(){ return false; }
  void truncate(long length) { File::truncate(length); }
};

class TestFile : public CppUnit::TestFixture
{
  CPPUNIT_TEST_SUITE(TestFile);
  CPPUNIT_TEST(testFindInSmallFile);
  CPPUNIT_TEST(testRFindInSmallFile);
  CPPUNIT_TEST(testSeek);
  CPPUNIT_TEST(testTruncate);
  CPPUNIT_TEST_SUITE_END();

public:

  void testFindInSmallFile()
  {
    ScopedFileCopy copy("empty", ".ogg");
    std::string name = copy.fileName();
    {
      PlainFile file(name.c_str());
      file.seek(0);
      file.writeBlock(ByteVector("0123456239", 10));
      file.truncate(10);
    }
    {
      PlainFile file(name.c_str());
      CPPUNIT_ASSERT_EQUAL(10l, file.length());

      CPPUNIT_ASSERT_EQUAL(2l, file.find(ByteVector("23", 2)));
      CPPUNIT_ASSERT_EQUAL(2l, file.find(ByteVector("23", 2), 2));
      CPPUNIT_ASSERT_EQUAL(7l, file.find(ByteVector("23", 2), 3));

      file.seek(0);
      const ByteVector v = file.readBlock(file.length());
      CPPUNIT_ASSERT_EQUAL((unsigned int)10, v.size());

      CPPUNIT_ASSERT_EQUAL((long)v.find("23"),    file.find("23"));
      CPPUNIT_ASSERT_EQUAL((long)v.find("23", 2), file.find("23", 2));
      CPPUNIT_ASSERT_EQUAL((long)v.find("23", 3), file.find("23", 3));
    }
  }

  void testRFindInSmallFile()
  {
    ScopedFileCopy copy("empty", ".ogg");
    std::string name = copy.fileName();
    {
      PlainFile file(name.c_str());
      file.seek(0);
      file.writeBlock(ByteVector("0123456239", 10));
      file.truncate(10);
    }
    {
      PlainFile file(name.c_str());
      CPPUNIT_ASSERT_EQUAL(10l, file.length());

      CPPUNIT_ASSERT_EQUAL(7l, file.rfind(ByteVector("23", 2)));
      CPPUNIT_ASSERT_EQUAL(7l, file.rfind(ByteVector("23", 2), 7));
      CPPUNIT_ASSERT_EQUAL(2l, file.rfind(ByteVector("23", 2), 6));

      file.seek(0);
      const ByteVector v = file.readBlock(file.length());
      CPPUNIT_ASSERT_EQUAL((unsigned int)10, v.size());

      CPPUNIT_ASSERT_EQUAL((long)v.rfind("23"),    file.rfind("23"));
      CPPUNIT_ASSERT_EQUAL((long)v.rfind("23", 7), file.rfind("23", 7));
      CPPUNIT_ASSERT_EQUAL((long)v.rfind("23", 6), file.rfind("23", 6));
    }
  }

  void testSeek()
  {
    ScopedFileCopy copy("empty", ".ogg");
    std::string name = copy.fileName();

    PlainFile f(name.c_str());
    CPPUNIT_ASSERT_EQUAL((long)0, f.tell());
    CPPUNIT_ASSERT_EQUAL((long)4328, f.length());

    f.seek(100, File::Beginning);
    CPPUNIT_ASSERT_EQUAL((long)100, f.tell());
    f.seek(100, File::Current);
    CPPUNIT_ASSERT_EQUAL((long)200, f.tell());
    f.seek(-300, File::Current);
    CPPUNIT_ASSERT_EQUAL((long)200, f.tell());

    f.seek(-100, File::End);
    CPPUNIT_ASSERT_EQUAL((long)4228, f.tell());
    f.seek(-100, File::Current);
    CPPUNIT_ASSERT_EQUAL((long)4128, f.tell());
    f.seek(300, File::Current);
    CPPUNIT_ASSERT_EQUAL((long)4428, f.tell());
  }

  void testTruncate()
  {
    ScopedFileCopy copy("empty", ".ogg");
    std::string name = copy.fileName();

    {
      PlainFile f(name.c_str());
      CPPUNIT_ASSERT_EQUAL(4328L, f.length());

      f.truncate(2000);
      CPPUNIT_ASSERT_EQUAL(2000L, f.length());
    }
    {
      PlainFile f(name.c_str());
      CPPUNIT_ASSERT_EQUAL(2000L, f.length());
    }
  }

};

CPPUNIT_TEST_SUITE_REGISTRATION(TestFile);

