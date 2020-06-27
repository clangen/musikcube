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

#include <tstring.h>
#include <tmap.h>
#include <cppunit/extensions/HelperMacros.h>

using namespace std;
using namespace TagLib;

class TestMap : public CppUnit::TestFixture
{
  CPPUNIT_TEST_SUITE(TestMap);
  CPPUNIT_TEST(testInsert);
  CPPUNIT_TEST(testDetach);
  CPPUNIT_TEST_SUITE_END();

public:

  void testInsert()
  {
    Map<String, int> m1;
    m1.insert("foo", 3);
    m1.insert("bar", 5);
    CPPUNIT_ASSERT_EQUAL(2U, m1.size());
    CPPUNIT_ASSERT_EQUAL(3, m1["foo"]);
    CPPUNIT_ASSERT_EQUAL(5, m1["bar"]);
    m1.insert("foo", 7);
    CPPUNIT_ASSERT_EQUAL(2U, m1.size());
    CPPUNIT_ASSERT_EQUAL(7, m1["foo"]);
    CPPUNIT_ASSERT_EQUAL(5, m1["bar"]);
  }

  void testDetach()
  {
    Map<String, int> m1;
    m1.insert("alice", 5);
    m1.insert("bob", 9);
    m1.insert("carol", 11);

    Map<String, int> m2 = m1;
    Map<String, int>::Iterator it = m2.find("bob");
    (*it).second = 99;
    CPPUNIT_ASSERT_EQUAL(9,  m1["bob"]);
    CPPUNIT_ASSERT_EQUAL(99, m2["bob"]);
  }

};

CPPUNIT_TEST_SUITE_REGISTRATION(TestMap);
