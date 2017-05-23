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

#include <tbytevector.h>
#include <tbytevectorlist.h>
#include <cppunit/extensions/HelperMacros.h>

using namespace std;
using namespace TagLib;

class TestByteVectorList : public CppUnit::TestFixture
{
  CPPUNIT_TEST_SUITE(TestByteVectorList);
  CPPUNIT_TEST(testSplitSingleChar);
  CPPUNIT_TEST(testSplitSingleChar_2);
  CPPUNIT_TEST_SUITE_END();

public:

  void testSplitSingleChar()
  {
    ByteVector v("a b");

    ByteVectorList l = ByteVectorList::split(v, " ");
    CPPUNIT_ASSERT_EQUAL((unsigned int)2, l.size());
    CPPUNIT_ASSERT_EQUAL(ByteVector("a"), l[0]);
    CPPUNIT_ASSERT_EQUAL(ByteVector("b"), l[1]);
  }

  void testSplitSingleChar_2()
  {
    ByteVector v("a");

    ByteVectorList l = ByteVectorList::split(v, " ");
    CPPUNIT_ASSERT_EQUAL((unsigned int)1, l.size());
    CPPUNIT_ASSERT_EQUAL(ByteVector("a"), l[0]);
  }

};

CPPUNIT_TEST_SUITE_REGISTRATION(TestByteVectorList);
