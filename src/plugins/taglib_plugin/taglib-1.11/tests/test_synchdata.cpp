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


#include <id3v2synchdata.h>
#include <cppunit/extensions/HelperMacros.h>

using namespace std;
using namespace TagLib;

class TestID3v2SynchData : public CppUnit::TestFixture
{
  CPPUNIT_TEST_SUITE(TestID3v2SynchData);
  CPPUNIT_TEST(test1);
  CPPUNIT_TEST(test2);
  CPPUNIT_TEST(test3);
  CPPUNIT_TEST(testToUIntBroken);
  CPPUNIT_TEST(testToUIntBrokenAndTooLarge);
  CPPUNIT_TEST(testDecode1);
  CPPUNIT_TEST(testDecode2);
  CPPUNIT_TEST(testDecode3);
  CPPUNIT_TEST(testDecode4);
  CPPUNIT_TEST_SUITE_END();

public:

  void test1()
  {
    char data[] = { 0, 0, 0, 127 };
    ByteVector v(data, 4);

    CPPUNIT_ASSERT_EQUAL(ID3v2::SynchData::toUInt(v), (unsigned int)127);
    CPPUNIT_ASSERT_EQUAL(ID3v2::SynchData::fromUInt(127), v);
  }

  void test2()
  {
    char data[] = { 0, 0, 1, 0 };
    ByteVector v(data, 4);

    CPPUNIT_ASSERT_EQUAL(ID3v2::SynchData::toUInt(v), (unsigned int)128);
    CPPUNIT_ASSERT_EQUAL(ID3v2::SynchData::fromUInt(128), v);
  }

  void test3()
  {
    char data[] = { 0, 0, 1, 1 };
    ByteVector v(data, 4);

    CPPUNIT_ASSERT_EQUAL(ID3v2::SynchData::toUInt(v), (unsigned int)129);
    CPPUNIT_ASSERT_EQUAL(ID3v2::SynchData::fromUInt(129), v);
  }

  void testToUIntBroken()
  {
    char data[] = { 0, 0, 0, -1 };
    char data2[] = { 0, 0, -1, -1 };

    CPPUNIT_ASSERT_EQUAL((unsigned int)255, ID3v2::SynchData::toUInt(ByteVector(data, 4)));
    CPPUNIT_ASSERT_EQUAL((unsigned int)65535, ID3v2::SynchData::toUInt(ByteVector(data2, 4)));
  }

  void testToUIntBrokenAndTooLarge()
  {
    char data[] = { 0, 0, 0, -1, 0 };
    ByteVector v(data, 5);

    CPPUNIT_ASSERT_EQUAL((unsigned int)255, ID3v2::SynchData::toUInt(v));
  }

  void testDecode1()
  {
    ByteVector a("\xff\x00\x00", 3);
    a = ID3v2::SynchData::decode(a);
    CPPUNIT_ASSERT_EQUAL((unsigned int)2, a.size());
    CPPUNIT_ASSERT_EQUAL(ByteVector("\xff\x00", 2), a);
  }

  void testDecode2()
  {
    ByteVector a("\xff\x44", 2);
    a = ID3v2::SynchData::decode(a);
    CPPUNIT_ASSERT_EQUAL((unsigned int)2, a.size());
    CPPUNIT_ASSERT_EQUAL(ByteVector("\xff\x44", 2), a);
  }

  void testDecode3()
  {
    ByteVector a("\xff\xff\x00", 3);
    a = ID3v2::SynchData::decode(a);
    CPPUNIT_ASSERT_EQUAL((unsigned int)2, a.size());
    CPPUNIT_ASSERT_EQUAL(ByteVector("\xff\xff", 2), a);
  }

  void testDecode4()
  {
    ByteVector a("\xff\xff\xff", 3);
    a = ID3v2::SynchData::decode(a);
    CPPUNIT_ASSERT_EQUAL((unsigned int)3, a.size());
    CPPUNIT_ASSERT_EQUAL(ByteVector("\xff\xff\xff", 3), a);
  }

};

CPPUNIT_TEST_SUITE_REGISTRATION(TestID3v2SynchData);
