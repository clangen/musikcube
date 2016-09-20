/***************************************************************************
    copyright           : (C) 2012 by Tsuda Kageyu
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

#include <string>
#include <stdio.h>
#include <infotag.h>
#include <cppunit/extensions/HelperMacros.h>
#include "utils.h"

using namespace std;
using namespace TagLib;

class TestInfoTag : public CppUnit::TestFixture
{
  CPPUNIT_TEST_SUITE(TestInfoTag);
  CPPUNIT_TEST(testTitle);
  CPPUNIT_TEST(testNumericFields);
  CPPUNIT_TEST_SUITE_END();

public:
  void testTitle()
  {
    RIFF::Info::Tag tag;

    CPPUNIT_ASSERT_EQUAL(String(""), tag.title());
    tag.setTitle("Test title 1");
    tag.setFieldText("TEST", "Dummy Text");

    CPPUNIT_ASSERT_EQUAL(String("Test title 1"), tag.title());

    RIFF::Info::FieldListMap map = tag.fieldListMap();
    CPPUNIT_ASSERT_EQUAL(String("Test title 1"), map["INAM"]);
    CPPUNIT_ASSERT_EQUAL(String("Dummy Text"), map["TEST"]);
  }

  void testNumericFields()
  {
    RIFF::Info::Tag tag;

    CPPUNIT_ASSERT_EQUAL((unsigned int)0, tag.track());
    tag.setTrack(1234);
    CPPUNIT_ASSERT_EQUAL((unsigned int)1234, tag.track());
    CPPUNIT_ASSERT_EQUAL(String("1234"), tag.fieldText("IPRT"));

    CPPUNIT_ASSERT_EQUAL((unsigned int)0, tag.year());
    tag.setYear(1234);
    CPPUNIT_ASSERT_EQUAL((unsigned int)1234, tag.year());
    CPPUNIT_ASSERT_EQUAL(String("1234"), tag.fieldText("ICRD"));
  }
};

CPPUNIT_TEST_SUITE_REGISTRATION(TestInfoTag);

