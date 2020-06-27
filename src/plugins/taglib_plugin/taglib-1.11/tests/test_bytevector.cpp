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

#define _USE_MATH_DEFINES
#include <cmath>
#include <tbytevector.h>
#include <tbytevectorlist.h>
#include <cppunit/extensions/HelperMacros.h>

using namespace std;
using namespace TagLib;

class TestByteVector : public CppUnit::TestFixture
{
  CPPUNIT_TEST_SUITE(TestByteVector);
  CPPUNIT_TEST(testByteVector);
  CPPUNIT_TEST(testFind1);
  CPPUNIT_TEST(testFind2);
  CPPUNIT_TEST(testFind3);
  CPPUNIT_TEST(testRfind1);
  CPPUNIT_TEST(testRfind2);
  CPPUNIT_TEST(testRfind3);
  CPPUNIT_TEST(testToHex);
  CPPUNIT_TEST(testIntegerConversion);
  CPPUNIT_TEST(testFloatingPointConversion);
  CPPUNIT_TEST(testReplace);
  CPPUNIT_TEST(testReplaceAndDetach);
  CPPUNIT_TEST(testIterator);
  CPPUNIT_TEST(testResize);
  CPPUNIT_TEST(testAppend1);
  CPPUNIT_TEST(testAppend2);
  CPPUNIT_TEST(testBase64);
  CPPUNIT_TEST_SUITE_END();

public:
  void testByteVector()
  {
    ByteVector s1("foo");
    CPPUNIT_ASSERT(ByteVectorList::split(s1, " ").size() == 1);

    ByteVector s2("f");
    CPPUNIT_ASSERT(ByteVectorList::split(s2, " ").size() == 1);

    CPPUNIT_ASSERT(ByteVector().isEmpty());
    CPPUNIT_ASSERT_EQUAL(0U, ByteVector().size());
    CPPUNIT_ASSERT(ByteVector("asdf").clear().isEmpty());
    CPPUNIT_ASSERT_EQUAL(0U, ByteVector("asdf").clear().size());
    CPPUNIT_ASSERT_EQUAL(ByteVector(), ByteVector("asdf").clear());

    ByteVector i("blah blah");
    ByteVector j("blah");
    CPPUNIT_ASSERT(i.containsAt(j, 5, 0));
    CPPUNIT_ASSERT(i.containsAt(j, 6, 1));
    CPPUNIT_ASSERT(i.containsAt(j, 6, 1, 3));

    i.clear();
    CPPUNIT_ASSERT(i.isEmpty());
  }

  void testFind1()
  {
    CPPUNIT_ASSERT_EQUAL(4, ByteVector("....SggO."). find("SggO"));
    CPPUNIT_ASSERT_EQUAL(4, ByteVector("....SggO."). find("SggO", 0));
    CPPUNIT_ASSERT_EQUAL(4, ByteVector("....SggO."). find("SggO", 1));
    CPPUNIT_ASSERT_EQUAL(4, ByteVector("....SggO."). find("SggO", 2));
    CPPUNIT_ASSERT_EQUAL(4, ByteVector("....SggO."). find("SggO", 3));
    CPPUNIT_ASSERT_EQUAL(4, ByteVector("....SggO."). find("SggO", 4));
    CPPUNIT_ASSERT_EQUAL(-1, ByteVector("....SggO."). find("SggO", 5));
    CPPUNIT_ASSERT_EQUAL(-1, ByteVector("....SggO."). find("SggO", 6));
    CPPUNIT_ASSERT_EQUAL(-1, ByteVector("....SggO."). find("SggO", 7));
    CPPUNIT_ASSERT_EQUAL(-1, ByteVector("....SggO."). find("SggO", 8));

    // Intentional out-of-bounds access.
    ByteVector v("0123456789x");
    v.resize(10);
    v.data()[10] = 'x';
    CPPUNIT_ASSERT_EQUAL(-1, v.find("789x", 7));
  }

  void testFind2()
  {
    CPPUNIT_ASSERT_EQUAL(0, ByteVector("\x01", 1).find("\x01"));
    CPPUNIT_ASSERT_EQUAL(0, ByteVector("\x01\x02", 2).find("\x01\x02"));
    CPPUNIT_ASSERT_EQUAL(-1, ByteVector("\x01", 1).find("\x02"));
    CPPUNIT_ASSERT_EQUAL(-1, ByteVector("\x01\x02", 2).find("\x01\x03"));
  }

  void testFind3()
  {
    CPPUNIT_ASSERT_EQUAL(4, ByteVector("....SggO."). find('S'));
    CPPUNIT_ASSERT_EQUAL(4, ByteVector("....SggO."). find('S', 0));
    CPPUNIT_ASSERT_EQUAL(4, ByteVector("....SggO."). find('S', 1));
    CPPUNIT_ASSERT_EQUAL(4, ByteVector("....SggO."). find('S', 2));
    CPPUNIT_ASSERT_EQUAL(4, ByteVector("....SggO."). find('S', 3));
    CPPUNIT_ASSERT_EQUAL(4, ByteVector("....SggO."). find('S', 4));
    CPPUNIT_ASSERT_EQUAL(-1, ByteVector("....SggO."). find('S', 5));
    CPPUNIT_ASSERT_EQUAL(-1, ByteVector("....SggO."). find('S', 6));
    CPPUNIT_ASSERT_EQUAL(-1, ByteVector("....SggO."). find('S', 7));
    CPPUNIT_ASSERT_EQUAL(-1, ByteVector("....SggO."). find('S', 8));
  }

  void testRfind1()
  {
    CPPUNIT_ASSERT_EQUAL(1, ByteVector(".OggS....").rfind("OggS", 0));
    CPPUNIT_ASSERT_EQUAL(1, ByteVector(".OggS....").rfind("OggS", 1));
    CPPUNIT_ASSERT_EQUAL(1, ByteVector(".OggS....").rfind("OggS", 2));
    CPPUNIT_ASSERT_EQUAL(1, ByteVector(".OggS....").rfind("OggS", 3));
    CPPUNIT_ASSERT_EQUAL(1, ByteVector(".OggS....").rfind("OggS", 4));
    CPPUNIT_ASSERT_EQUAL(1, ByteVector(".OggS....").rfind("OggS", 5));
    CPPUNIT_ASSERT_EQUAL(1, ByteVector(".OggS....").rfind("OggS", 6));
    CPPUNIT_ASSERT_EQUAL(1, ByteVector(".OggS....").rfind("OggS", 7));
    CPPUNIT_ASSERT_EQUAL(1, ByteVector(".OggS....").rfind("OggS", 8));
    CPPUNIT_ASSERT_EQUAL(1, ByteVector(".OggS....").rfind("OggS"));
  }

  void testRfind2()
  {
    ByteVector r0("**************");
    ByteVector r1("OggS**********");
    ByteVector r2("**********OggS");
    ByteVector r3("OggS******OggS");
    ByteVector r4("OggS*OggS*OggS");

    CPPUNIT_ASSERT_EQUAL(-1, r0.find("OggS"));
    CPPUNIT_ASSERT_EQUAL(-1, r0.rfind("OggS"));
    CPPUNIT_ASSERT_EQUAL(0, r1.find("OggS"));
    CPPUNIT_ASSERT_EQUAL(0, r1.rfind("OggS"));
    CPPUNIT_ASSERT_EQUAL(10, r2.find("OggS"));
    CPPUNIT_ASSERT_EQUAL(10, r2.rfind("OggS"));
    CPPUNIT_ASSERT_EQUAL(0, r3.find("OggS"));
    CPPUNIT_ASSERT_EQUAL(10, r3.rfind("OggS"));
    CPPUNIT_ASSERT_EQUAL(10, r4.rfind("OggS"));
    CPPUNIT_ASSERT_EQUAL(10, r4.rfind("OggS", 0));
    CPPUNIT_ASSERT_EQUAL(5, r4.rfind("OggS", 7));
    CPPUNIT_ASSERT_EQUAL(10, r4.rfind("OggS", 12));
  }

  void testRfind3()
  {
    CPPUNIT_ASSERT_EQUAL(1, ByteVector(".OggS....").rfind('O', 0));
    CPPUNIT_ASSERT_EQUAL(1, ByteVector(".OggS....").rfind('O', 1));
    CPPUNIT_ASSERT_EQUAL(1, ByteVector(".OggS....").rfind('O', 2));
    CPPUNIT_ASSERT_EQUAL(1, ByteVector(".OggS....").rfind('O', 3));
    CPPUNIT_ASSERT_EQUAL(1, ByteVector(".OggS....").rfind('O', 4));
    CPPUNIT_ASSERT_EQUAL(1, ByteVector(".OggS....").rfind('O', 5));
    CPPUNIT_ASSERT_EQUAL(1, ByteVector(".OggS....").rfind('O', 6));
    CPPUNIT_ASSERT_EQUAL(1, ByteVector(".OggS....").rfind('O', 7));
    CPPUNIT_ASSERT_EQUAL(1, ByteVector(".OggS....").rfind('O', 8));
    CPPUNIT_ASSERT_EQUAL(1, ByteVector(".OggS....").rfind('O'));
  }

  void testToHex()
  {
    ByteVector v("\xf0\xe1\xd2\xc3\xb4\xa5\x96\x87\x78\x69\x5a\x4b\x3c\x2d\x1e\x0f", 16);

    CPPUNIT_ASSERT_EQUAL(ByteVector("f0e1d2c3b4a5968778695a4b3c2d1e0f"), v.toHex());
  }

  void testIntegerConversion()
  {
    const ByteVector data("\x00\xff\x01\xff\x00\xff\x01\xff\x00\xff\x01\xff\x00\xff", 14);

    CPPUNIT_ASSERT_EQUAL((short)0x00ff, data.toShort());
    CPPUNIT_ASSERT_EQUAL((short)0xff00, data.toShort(false));
    CPPUNIT_ASSERT_EQUAL((short)0xff01, data.toShort(5U));
    CPPUNIT_ASSERT_EQUAL((short)0x01ff, data.toShort(5U, false));
    CPPUNIT_ASSERT_EQUAL((short)0xff, data.toShort(13U));
    CPPUNIT_ASSERT_EQUAL((short)0xff, data.toShort(13U, false));

    CPPUNIT_ASSERT_EQUAL((unsigned short)0x00ff, data.toUShort());
    CPPUNIT_ASSERT_EQUAL((unsigned short)0xff00, data.toUShort(false));
    CPPUNIT_ASSERT_EQUAL((unsigned short)0xff01, data.toUShort(5U));
    CPPUNIT_ASSERT_EQUAL((unsigned short)0x01ff, data.toUShort(5U, false));
    CPPUNIT_ASSERT_EQUAL((unsigned short)0xff, data.toUShort(13U));
    CPPUNIT_ASSERT_EQUAL((unsigned short)0xff, data.toUShort(13U, false));

    CPPUNIT_ASSERT_EQUAL(0x00ff01ffU, data.toUInt());
    CPPUNIT_ASSERT_EQUAL(0xff01ff00U, data.toUInt(false));
    CPPUNIT_ASSERT_EQUAL(0xff01ff00U, data.toUInt(5U));
    CPPUNIT_ASSERT_EQUAL(0x00ff01ffU, data.toUInt(5U, false));
    CPPUNIT_ASSERT_EQUAL(0x00ffU, data.toUInt(12U));
    CPPUNIT_ASSERT_EQUAL(0xff00U, data.toUInt(12U, false));

    CPPUNIT_ASSERT_EQUAL(0x00ff01U, data.toUInt(0U, 3U));
    CPPUNIT_ASSERT_EQUAL(0x01ff00U, data.toUInt(0U, 3U, false));
    CPPUNIT_ASSERT_EQUAL(0xff01ffU, data.toUInt(5U, 3U));
    CPPUNIT_ASSERT_EQUAL(0xff01ffU, data.toUInt(5U, 3U, false));
    CPPUNIT_ASSERT_EQUAL(0x00ffU, data.toUInt(12U, 3U));
    CPPUNIT_ASSERT_EQUAL(0xff00U, data.toUInt(12U, 3U, false));

    CPPUNIT_ASSERT_EQUAL((long long)0x00ff01ff00ff01ffULL, data.toLongLong());
    CPPUNIT_ASSERT_EQUAL((long long)0xff01ff00ff01ff00ULL, data.toLongLong(false));
    CPPUNIT_ASSERT_EQUAL((long long)0xff01ff00ff01ff00ULL, data.toLongLong(5U));
    CPPUNIT_ASSERT_EQUAL((long long)0x00ff01ff00ff01ffULL, data.toLongLong(5U, false));
    CPPUNIT_ASSERT_EQUAL((long long)0x00ffU, data.toLongLong(12U));
    CPPUNIT_ASSERT_EQUAL((long long)0xff00U, data.toLongLong(12U, false));
}

  void testFloatingPointConversion()
  {
    const double Tolerance = 1.0e-7;

    const ByteVector pi32le("\xdb\x0f\x49\x40", 4);
    CPPUNIT_ASSERT(std::abs(pi32le.toFloat32LE(0) - M_PI) < Tolerance);
    CPPUNIT_ASSERT_EQUAL(pi32le, ByteVector::fromFloat32LE(pi32le.toFloat32LE(0)));

    const ByteVector pi32be("\x40\x49\x0f\xdb", 4);
    CPPUNIT_ASSERT(std::abs(pi32be.toFloat32BE(0) - M_PI) < Tolerance);
    CPPUNIT_ASSERT_EQUAL(pi32be, ByteVector::fromFloat32BE(pi32be.toFloat32BE(0)));

    const ByteVector pi64le("\x18\x2d\x44\x54\xfb\x21\x09\x40", 8);
    CPPUNIT_ASSERT(std::abs(pi64le.toFloat64LE(0) - M_PI) < Tolerance);
    CPPUNIT_ASSERT_EQUAL(pi64le, ByteVector::fromFloat64LE(pi64le.toFloat64LE(0)));

    const ByteVector pi64be("\x40\x09\x21\xfb\x54\x44\x2d\x18", 8);
    CPPUNIT_ASSERT(std::abs(pi64be.toFloat64BE(0) - M_PI) < Tolerance);
    CPPUNIT_ASSERT_EQUAL(pi64be, ByteVector::fromFloat64BE(pi64be.toFloat64BE(0)));

    const ByteVector pi80le("\x00\xc0\x68\x21\xa2\xda\x0f\xc9\x00\x40", 10);
    CPPUNIT_ASSERT(std::abs(pi80le.toFloat80LE(0) - M_PI) < Tolerance);

    const ByteVector pi80be("\x40\x00\xc9\x0f\xda\xa2\x21\x68\xc0\x00", 10);
    CPPUNIT_ASSERT(std::abs(pi80be.toFloat80BE(0) - M_PI) < Tolerance);
  }

  void testReplace()
  {
    {
      ByteVector a("abcdabf");
      a.replace(ByteVector(""), ByteVector("<a>"));
      CPPUNIT_ASSERT_EQUAL(ByteVector("abcdabf"), a);
    }
    {
      ByteVector a("abcdabf");
      a.replace(ByteVector("foobartoolong"), ByteVector("<a>"));
      CPPUNIT_ASSERT_EQUAL(ByteVector("abcdabf"), a);
    }
    {
      ByteVector a("abcdabf");
      a.replace(ByteVector("xx"), ByteVector("yy"));
      CPPUNIT_ASSERT_EQUAL(ByteVector("abcdabf"), a);
    }
    {
      ByteVector a("abcdabf");
      a.replace(ByteVector("a"), ByteVector("x"));
      CPPUNIT_ASSERT_EQUAL(ByteVector("xbcdxbf"), a);
      a.replace(ByteVector("x"), ByteVector("a"));
      CPPUNIT_ASSERT_EQUAL(ByteVector("abcdabf"), a);
    }
    {
      ByteVector a("abcdabf");
      a.replace('a', 'x');
      CPPUNIT_ASSERT_EQUAL(ByteVector("xbcdxbf"), a);
      a.replace('x', 'a');
      CPPUNIT_ASSERT_EQUAL(ByteVector("abcdabf"), a);
    }
    {
      ByteVector a("abcdabf");
      a.replace(ByteVector("ab"), ByteVector("xy"));
      CPPUNIT_ASSERT_EQUAL(ByteVector("xycdxyf"), a);
      a.replace(ByteVector("xy"), ByteVector("ab"));
      CPPUNIT_ASSERT_EQUAL(ByteVector("abcdabf"), a);
    }
    {
      ByteVector a("abcdabf");
      a.replace(ByteVector("a"), ByteVector("<a>"));
      CPPUNIT_ASSERT_EQUAL(ByteVector("<a>bcd<a>bf"), a);
      a.replace(ByteVector("<a>"), ByteVector("a"));
      CPPUNIT_ASSERT_EQUAL(ByteVector("abcdabf"), a);
    }
    {
      ByteVector a("abcdabf");
      a.replace(ByteVector("b"), ByteVector("<b>"));
      CPPUNIT_ASSERT_EQUAL(ByteVector("a<b>cda<b>f"), a);
      a.replace(ByteVector("<b>"), ByteVector("b"));
      CPPUNIT_ASSERT_EQUAL(ByteVector("abcdabf"), a);
    }
    {
      ByteVector a("abcdabc");
      a.replace(ByteVector("c"), ByteVector("<c>"));
      CPPUNIT_ASSERT_EQUAL(ByteVector("ab<c>dab<c>"), a);
      a.replace(ByteVector("<c>"), ByteVector("c"));
      CPPUNIT_ASSERT_EQUAL(ByteVector("abcdabc"), a);
    }
    {
      ByteVector a("abcdaba");
      a.replace(ByteVector("a"), ByteVector("<a>"));
      CPPUNIT_ASSERT_EQUAL(ByteVector("<a>bcd<a>b<a>"), a);
      a.replace(ByteVector("<a>"), ByteVector("a"));
      CPPUNIT_ASSERT_EQUAL(ByteVector("abcdaba"), a);
    }
  }

  void testReplaceAndDetach()
  {
    {
      ByteVector a("abcdabf");
      ByteVector b = a;
      a.replace(ByteVector("a"), ByteVector("x"));
      CPPUNIT_ASSERT_EQUAL(ByteVector("xbcdxbf"), a);
      CPPUNIT_ASSERT_EQUAL(ByteVector("abcdabf"), b);
    }
    {
      ByteVector a("abcdabf");
      ByteVector b = a;
      a.replace('a', 'x');
      CPPUNIT_ASSERT_EQUAL(ByteVector("xbcdxbf"), a);
      CPPUNIT_ASSERT_EQUAL(ByteVector("abcdabf"), b);
    }
    {
      ByteVector a("abcdabf");
      ByteVector b = a;
      a.replace(ByteVector("ab"), ByteVector("xy"));
      CPPUNIT_ASSERT_EQUAL(ByteVector("xycdxyf"), a);
      CPPUNIT_ASSERT_EQUAL(ByteVector("abcdabf"), b);
    }
    {
      ByteVector a("abcdabf");
      ByteVector b = a;
      a.replace(ByteVector("a"), ByteVector("<a>"));
      CPPUNIT_ASSERT_EQUAL(ByteVector("<a>bcd<a>bf"), a);
      CPPUNIT_ASSERT_EQUAL(ByteVector("abcdabf"), b);
    }
    {
      ByteVector a("ab<c>dab<c>");
      ByteVector b = a;
      a.replace(ByteVector("<c>"), ByteVector("c"));
      CPPUNIT_ASSERT_EQUAL(ByteVector("abcdabc"), a);
      CPPUNIT_ASSERT_EQUAL(ByteVector("ab<c>dab<c>"), b);
    }
  }

  void testIterator()
  {
    ByteVector v1("taglib");
    ByteVector v2 = v1;

    ByteVector::Iterator it1 = v1.begin();
    ByteVector::Iterator it2 = v2.begin();

    CPPUNIT_ASSERT_EQUAL('t', *it1);
    CPPUNIT_ASSERT_EQUAL('t', *it2);

    std::advance(it1, 4);
    std::advance(it2, 4);
    *it2 = 'I';
    CPPUNIT_ASSERT_EQUAL('i', *it1);
    CPPUNIT_ASSERT_EQUAL('I', *it2);
    CPPUNIT_ASSERT_EQUAL(ByteVector("taglib"), v1);
    CPPUNIT_ASSERT_EQUAL(ByteVector("taglIb"), v2);

    ByteVector::ReverseIterator it3 = v1.rbegin();
    ByteVector::ReverseIterator it4 = v2.rbegin();

    CPPUNIT_ASSERT_EQUAL('b', *it3);
    CPPUNIT_ASSERT_EQUAL('b', *it4);

    std::advance(it3, 4);
    std::advance(it4, 4);
    *it4 = 'A';
    CPPUNIT_ASSERT_EQUAL('a', *it3);
    CPPUNIT_ASSERT_EQUAL('A', *it4);
    CPPUNIT_ASSERT_EQUAL(ByteVector("taglib"), v1);
    CPPUNIT_ASSERT_EQUAL(ByteVector("tAglIb"), v2);

    ByteVector v3;
    v3 = ByteVector("0123456789").mid(3, 4);

    it1 = v3.begin();
    it2 = v3.end() - 1;
    CPPUNIT_ASSERT_EQUAL('3', *it1);
    CPPUNIT_ASSERT_EQUAL('6', *it2);

    it3 = v3.rbegin();
    it4 = v3.rend() - 1;
    CPPUNIT_ASSERT_EQUAL('6', *it3);
    CPPUNIT_ASSERT_EQUAL('3', *it4);
  }

  void testResize()
  {
    ByteVector a = ByteVector("0123456789");
    ByteVector b = a.mid(3, 4);
    b.resize(6, 'A');
    CPPUNIT_ASSERT_EQUAL((unsigned int)6, b.size());
    CPPUNIT_ASSERT_EQUAL('6', b[3]);
    CPPUNIT_ASSERT_EQUAL('A', b[4]);
    CPPUNIT_ASSERT_EQUAL('A', b[5]);
    b.resize(10, 'B');
    CPPUNIT_ASSERT_EQUAL((unsigned int)10, b.size());
    CPPUNIT_ASSERT_EQUAL('6', b[3]);
    CPPUNIT_ASSERT_EQUAL('B', b[6]);
    CPPUNIT_ASSERT_EQUAL('B', b[9]);
    b.resize(3, 'C');
    CPPUNIT_ASSERT_EQUAL((unsigned int)3, b.size());
    CPPUNIT_ASSERT_EQUAL(-1, b.find('C'));
    b.resize(3);
    CPPUNIT_ASSERT_EQUAL((unsigned int)3, b.size());

    // Check if a and b were properly detached.

    CPPUNIT_ASSERT_EQUAL((unsigned int)10, a.size());
    CPPUNIT_ASSERT_EQUAL('3', a[3]);
    CPPUNIT_ASSERT_EQUAL('5', a[5]);

    // Special case that refCount == 1 and d->offset != 0.

    ByteVector c = ByteVector("0123456789").mid(3, 4);
    c.resize(6, 'A');
    CPPUNIT_ASSERT_EQUAL((unsigned int)6, c.size());
    CPPUNIT_ASSERT_EQUAL('6', c[3]);
    CPPUNIT_ASSERT_EQUAL('A', c[4]);
    CPPUNIT_ASSERT_EQUAL('A', c[5]);
    c.resize(10, 'B');
    CPPUNIT_ASSERT_EQUAL((unsigned int)10, c.size());
    CPPUNIT_ASSERT_EQUAL('6', c[3]);
    CPPUNIT_ASSERT_EQUAL('B', c[6]);
    CPPUNIT_ASSERT_EQUAL('B', c[9]);
    c.resize(3, 'C');
    CPPUNIT_ASSERT_EQUAL((unsigned int)3, c.size());
    CPPUNIT_ASSERT_EQUAL(-1, c.find('C'));
  }

  void testAppend1()
  {
    ByteVector v1("foo");
    v1.append("bar");
    CPPUNIT_ASSERT_EQUAL(ByteVector("foobar"), v1);

    ByteVector v2("foo");
    v2.append("b");
    CPPUNIT_ASSERT_EQUAL(ByteVector("foob"), v2);

    ByteVector v3;
    v3.append("b");
    CPPUNIT_ASSERT_EQUAL(ByteVector("b"), v3);

    ByteVector v4("foo");
    v4.append(v1);
    CPPUNIT_ASSERT_EQUAL(ByteVector("foofoobar"), v4);

    ByteVector v5("foo");
    v5.append('b');
    CPPUNIT_ASSERT_EQUAL(ByteVector("foob"), v5);

    ByteVector v6;
    v6.append('b');
    CPPUNIT_ASSERT_EQUAL(ByteVector("b"), v6);

    ByteVector v7("taglib");
    ByteVector v8 = v7;

    v7.append("ABC");
    CPPUNIT_ASSERT_EQUAL(ByteVector("taglibABC"), v7);
    v7.append('1');
    v7.append('2');
    v7.append('3');
    CPPUNIT_ASSERT_EQUAL(ByteVector("taglibABC123"), v7);
    CPPUNIT_ASSERT_EQUAL(ByteVector("taglib"), v8);
  }

  void testAppend2()
  {
    ByteVector a("1234");
    a.append(a);
    CPPUNIT_ASSERT_EQUAL(ByteVector("12341234"), a);
  }

  void testBase64()
  {
    ByteVector sempty;
    ByteVector t0("a"); // test 1 byte
    ByteVector t1("any carnal pleasure.");
    ByteVector t2("any carnal pleasure");
    ByteVector t3("any carnal pleasur");
    ByteVector s0("a"); // test 1 byte
    ByteVector s1("any carnal pleasure.");
    ByteVector s2("any carnal pleasure");
    ByteVector s3("any carnal pleasur");
    ByteVector eempty;
    ByteVector e0("YQ==");
    ByteVector e1("YW55IGNhcm5hbCBwbGVhc3VyZS4=");
    ByteVector e2("YW55IGNhcm5hbCBwbGVhc3VyZQ==");
    ByteVector e3("YW55IGNhcm5hbCBwbGVhc3Vy");

    // Encode
    CPPUNIT_ASSERT_EQUAL(eempty, sempty.toBase64());
    CPPUNIT_ASSERT_EQUAL(e0, s0.toBase64());
    CPPUNIT_ASSERT_EQUAL(e1, s1.toBase64());
    CPPUNIT_ASSERT_EQUAL(e2, s2.toBase64());
    CPPUNIT_ASSERT_EQUAL(e3, s3.toBase64());

    // Decode
    CPPUNIT_ASSERT_EQUAL(sempty, ByteVector::fromBase64(eempty));
    CPPUNIT_ASSERT_EQUAL(s0, ByteVector::fromBase64(e0));
    CPPUNIT_ASSERT_EQUAL(s1, ByteVector::fromBase64(e1));
    CPPUNIT_ASSERT_EQUAL(s2, ByteVector::fromBase64(e2));
    CPPUNIT_ASSERT_EQUAL(s3, ByteVector::fromBase64(e3));

    CPPUNIT_ASSERT_EQUAL(t0, ByteVector::fromBase64(s0.toBase64()));
    CPPUNIT_ASSERT_EQUAL(t1, ByteVector::fromBase64(s1.toBase64()));
    CPPUNIT_ASSERT_EQUAL(t2, ByteVector::fromBase64(s2.toBase64()));
    CPPUNIT_ASSERT_EQUAL(t3, ByteVector::fromBase64(s3.toBase64()));

    ByteVector all((unsigned int)256);

    // in order
    {
      for(int i = 0; i < 256; i++){
        all[i]=(unsigned char)i;
        }
      ByteVector b64 = all.toBase64();
      ByteVector original = ByteVector::fromBase64(b64);
      CPPUNIT_ASSERT_EQUAL(all,original);
    }

    // reverse
    {
      for(int i = 0; i < 256; i++){
        all[i]=(unsigned char)255-i;
        }
      ByteVector b64 = all.toBase64();
      ByteVector original = ByteVector::fromBase64(b64);
      CPPUNIT_ASSERT_EQUAL(all,original);
    }

    // all zeroes
    {
      for(int i = 0; i < 256; i++){
        all[i]=0;
        }
      ByteVector b64 = all.toBase64();
      ByteVector original = ByteVector::fromBase64(b64);
      CPPUNIT_ASSERT_EQUAL(all,original);
    }

    // all ones
    {
      for(int i = 0; i < 256; i++){
        all[i]=(unsigned char)0xff;
        }
      ByteVector b64 = all.toBase64();
      ByteVector original = ByteVector::fromBase64(b64);
      CPPUNIT_ASSERT_EQUAL(all,original);
    }

    // Missing end bytes
    {
      // No missing bytes
      ByteVector m0("YW55IGNhcm5hbCBwbGVhc3VyZQ==");
      CPPUNIT_ASSERT_EQUAL(s2,ByteVector::fromBase64(m0));

      // 1 missing byte
      ByteVector m1("YW55IGNhcm5hbCBwbGVhc3VyZQ=");
      CPPUNIT_ASSERT_EQUAL(sempty,ByteVector::fromBase64(m1));

      // 2 missing bytes
      ByteVector m2("YW55IGNhcm5hbCBwbGVhc3VyZQ");
      CPPUNIT_ASSERT_EQUAL(sempty,ByteVector::fromBase64(m2));

      // 3 missing bytes
      ByteVector m3("YW55IGNhcm5hbCBwbGVhc3VyZ");
      CPPUNIT_ASSERT_EQUAL(sempty,ByteVector::fromBase64(m3));
    }

    // Grok invalid characters
    {
      ByteVector invalid("abd\x00\x01\x02\x03\x04");
      CPPUNIT_ASSERT_EQUAL(sempty,ByteVector::fromBase64(invalid));
    }

  }

};

CPPUNIT_TEST_SUITE_REGISTRATION(TestByteVector);

