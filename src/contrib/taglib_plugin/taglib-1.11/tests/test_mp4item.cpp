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
#include <mp4coverart.h>
#include <mp4item.h>
#include <cppunit/extensions/HelperMacros.h>
#include "utils.h"

using namespace std;
using namespace TagLib;

class TestMP4Item : public CppUnit::TestFixture
{
  CPPUNIT_TEST_SUITE(TestMP4Item);
  CPPUNIT_TEST(testCoverArtList);
  CPPUNIT_TEST_SUITE_END();

public:

  void testCoverArtList()
  {
    MP4::CoverArtList l;
    l.append(MP4::CoverArt(MP4::CoverArt::PNG, "foo"));
    l.append(MP4::CoverArt(MP4::CoverArt::JPEG, "bar"));

    MP4::Item i(l);
    MP4::CoverArtList l2 = i.toCoverArtList();

    CPPUNIT_ASSERT_EQUAL(MP4::CoverArt::PNG, l[0].format());
    CPPUNIT_ASSERT_EQUAL(ByteVector("foo"), l[0].data());
    CPPUNIT_ASSERT_EQUAL(MP4::CoverArt::JPEG, l[1].format());
    CPPUNIT_ASSERT_EQUAL(ByteVector("bar"), l[1].data());
  }

};

CPPUNIT_TEST_SUITE_REGISTRATION(TestMP4Item);
