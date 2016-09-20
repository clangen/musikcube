/***************************************************************************
    copyright           : (C) 2012 by Lukas Lalinsky
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
#include <flacunknownmetadatablock.h>
#include <cppunit/extensions/HelperMacros.h>
#include "utils.h"

using namespace std;
using namespace TagLib;

class TestFLACUnknownMetadataBlock : public CppUnit::TestFixture
{
  CPPUNIT_TEST_SUITE(TestFLACUnknownMetadataBlock);
  CPPUNIT_TEST(testAccessors);
  CPPUNIT_TEST_SUITE_END();

public:

  void testAccessors()
  {
    ByteVector data("abc\x01", 4);
    FLAC::UnknownMetadataBlock block(42, data);
    CPPUNIT_ASSERT_EQUAL(42, block.code());
    CPPUNIT_ASSERT_EQUAL(data, block.data());
    CPPUNIT_ASSERT_EQUAL(data, block.render());
    ByteVector data2("xxx", 3);
    block.setCode(13);
    block.setData(data2);
    CPPUNIT_ASSERT_EQUAL(13, block.code());
    CPPUNIT_ASSERT_EQUAL(data2, block.data());
    CPPUNIT_ASSERT_EQUAL(data2, block.render());
  }

};

CPPUNIT_TEST_SUITE_REGISTRATION(TestFLACUnknownMetadataBlock);
