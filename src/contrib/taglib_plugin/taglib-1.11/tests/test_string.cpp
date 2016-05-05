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
#include <string.h>
#include <cppunit/extensions/HelperMacros.h>

using namespace std;
using namespace TagLib;

class TestString : public CppUnit::TestFixture
{
  CPPUNIT_TEST_SUITE(TestString);
  CPPUNIT_TEST(testString);
  CPPUNIT_TEST(testRfind);
  CPPUNIT_TEST(testUTF16Encode);
  CPPUNIT_TEST(testUTF16Decode);
  CPPUNIT_TEST(testUTF16DecodeInvalidBOM);
  CPPUNIT_TEST(testUTF16DecodeEmptyWithBOM);
  CPPUNIT_TEST(testSurrogatePair);
  CPPUNIT_TEST(testAppendCharDetach);
  CPPUNIT_TEST(testAppendStringDetach);
  CPPUNIT_TEST(testToInt);
  CPPUNIT_TEST(testFromInt);
  CPPUNIT_TEST(testSubstr);
  CPPUNIT_TEST(testNewline);
  CPPUNIT_TEST(testEncodeNonLatin1);
  CPPUNIT_TEST(testEncodeEmpty);
  CPPUNIT_TEST(testIterator);
  CPPUNIT_TEST_SUITE_END();

public:

  void testString()
  {
    String s = "taglib string";
    ByteVector v = "taglib string";
    CPPUNIT_ASSERT(v == s.data(String::Latin1));

    char str[] = "taglib string";
    CPPUNIT_ASSERT(strcmp(s.toCString(), str) == 0);
    CPPUNIT_ASSERT(s == "taglib string");
    CPPUNIT_ASSERT(s != "taglib STRING");
    CPPUNIT_ASSERT(s != "taglib");
    CPPUNIT_ASSERT(s != "taglib string taglib");
    CPPUNIT_ASSERT(s == L"taglib string");
    CPPUNIT_ASSERT(s != L"taglib STRING");
    CPPUNIT_ASSERT(s != L"taglib");
    CPPUNIT_ASSERT(s != L"taglib string taglib");

    s.clear();
    CPPUNIT_ASSERT(s.isEmpty());
    CPPUNIT_ASSERT(!s.isNull()); // deprecated, but still worth it to check.

    String unicode("José Carlos", String::UTF8);
    CPPUNIT_ASSERT(strcmp(unicode.toCString(), "Jos\xe9 Carlos") == 0);

    String latin = "Jos\xe9 Carlos";
    CPPUNIT_ASSERT(strcmp(latin.toCString(true), "José Carlos") == 0);

    String c;
    c = "1";
    CPPUNIT_ASSERT(c == L"1");

    c = L'\u4E00';
    CPPUNIT_ASSERT(c == L"\u4E00");

    String unicode2(unicode.to8Bit(true), String::UTF8);
    CPPUNIT_ASSERT(unicode == unicode2);

    String unicode3(L"\u65E5\u672C\u8A9E");
    CPPUNIT_ASSERT(*(unicode3.toCWString() + 1) == L'\u672C');

    String unicode4(L"\u65e5\u672c\u8a9e", String::UTF16BE);
    CPPUNIT_ASSERT(unicode4[1] == L'\u672c');

    String unicode5(L"\u65e5\u672c\u8a9e", String::UTF16LE);
    CPPUNIT_ASSERT(unicode5[1] == L'\u2c67');

    std::wstring stduni = L"\u65e5\u672c\u8a9e";

    String unicode6(stduni, String::UTF16BE);
    CPPUNIT_ASSERT(unicode6[1] == L'\u672c');

    String unicode7(stduni, String::UTF16LE);
    CPPUNIT_ASSERT(unicode7[1] == L'\u2c67');

    CPPUNIT_ASSERT(String("  foo  ").stripWhiteSpace() == String("foo"));
    CPPUNIT_ASSERT(String("foo    ").stripWhiteSpace() == String("foo"));
    CPPUNIT_ASSERT(String("    foo").stripWhiteSpace() == String("foo"));

    CPPUNIT_ASSERT(memcmp(String("foo").data(String::Latin1).data(), "foo", 3) == 0);
    CPPUNIT_ASSERT(memcmp(String("f").data(String::Latin1).data(), "f", 1) == 0);
  }

  void testUTF16Encode()
  {
    String a("foo");
    ByteVector b("\0f\0o\0o", 6);
    ByteVector c("f\0o\0o\0", 6);
    ByteVector d("\377\376f\0o\0o\0", 8);
    CPPUNIT_ASSERT(a.data(String::UTF16BE) != a.data(String::UTF16LE));
    CPPUNIT_ASSERT(b == a.data(String::UTF16BE));
    CPPUNIT_ASSERT(c == a.data(String::UTF16LE));
    CPPUNIT_ASSERT_EQUAL(d, a.data(String::UTF16));
  }

  void testUTF16Decode()
  {
    String a("foo");
    ByteVector b("\0f\0o\0o", 6);
    ByteVector c("f\0o\0o\0", 6);
    ByteVector d("\377\376f\0o\0o\0", 8);
    CPPUNIT_ASSERT_EQUAL(a, String(b, String::UTF16BE));
    CPPUNIT_ASSERT_EQUAL(a, String(c, String::UTF16LE));
    CPPUNIT_ASSERT_EQUAL(a, String(d, String::UTF16));
  }

  // this test is expected to print "TagLib: String::prepare() -
  // Invalid UTF16 string." on the console 3 times
  void testUTF16DecodeInvalidBOM()
  {
    ByteVector b(" ", 1);
    ByteVector c("  ", 2);
    ByteVector d("  \0f\0o\0o", 8);
    CPPUNIT_ASSERT_EQUAL(String(), String(b, String::UTF16));
    CPPUNIT_ASSERT_EQUAL(String(), String(c, String::UTF16));
    CPPUNIT_ASSERT_EQUAL(String(), String(d, String::UTF16));
  }

  void testUTF16DecodeEmptyWithBOM()
  {
    ByteVector a("\377\376", 2);
    ByteVector b("\376\377", 2);
    CPPUNIT_ASSERT_EQUAL(String(), String(a, String::UTF16));
    CPPUNIT_ASSERT_EQUAL(String(), String(b, String::UTF16));
  }

  void testSurrogatePair()
  {
    // Make sure that a surrogate pair is converted into single UTF-8 char
    // and vice versa.

    const ByteVector v1("\xff\xfe\x42\xd8\xb7\xdf\xce\x91\x4b\x5c");
    const ByteVector v2("\xf0\xa0\xae\xb7\xe9\x87\x8e\xe5\xb1\x8b");

    const String s1(v1, String::UTF16);
    CPPUNIT_ASSERT_EQUAL(s1.data(String::UTF8), v2);

    const String s2(v2, String::UTF8);
    CPPUNIT_ASSERT_EQUAL(s2.data(String::UTF16), v1);
  }

  void testAppendStringDetach()
  {
    String a("a");
    String b = a;
    a += "b";
    CPPUNIT_ASSERT_EQUAL(String("ab"), a);
    CPPUNIT_ASSERT_EQUAL(String("a"), b);
  }

  void testAppendCharDetach()
  {
    String a("a");
    String b = a;
    a += 'b';
    CPPUNIT_ASSERT_EQUAL(String("ab"), a);
    CPPUNIT_ASSERT_EQUAL(String("a"), b);
  }

  void testRfind()
  {
    CPPUNIT_ASSERT_EQUAL(-1, String("foo.bar").rfind(".", 0));
    CPPUNIT_ASSERT_EQUAL(-1, String("foo.bar").rfind(".", 1));
    CPPUNIT_ASSERT_EQUAL(-1, String("foo.bar").rfind(".", 2));
    CPPUNIT_ASSERT_EQUAL(3, String("foo.bar").rfind(".", 3));
    CPPUNIT_ASSERT_EQUAL(3, String("foo.bar").rfind(".", 4));
    CPPUNIT_ASSERT_EQUAL(3, String("foo.bar").rfind(".", 5));
    CPPUNIT_ASSERT_EQUAL(3, String("foo.bar").rfind(".", 6));
    CPPUNIT_ASSERT_EQUAL(3, String("foo.bar").rfind(".", 7));
    CPPUNIT_ASSERT_EQUAL(3, String("foo.bar").rfind("."));
  }

  void testToInt()
  {
    bool ok;
    CPPUNIT_ASSERT_EQUAL(String("123").toInt(&ok), 123);
    CPPUNIT_ASSERT_EQUAL(ok, true);

    CPPUNIT_ASSERT_EQUAL(String("-123").toInt(&ok), -123);
    CPPUNIT_ASSERT_EQUAL(ok, true);

    CPPUNIT_ASSERT_EQUAL(String("abc").toInt(&ok), 0);
    CPPUNIT_ASSERT_EQUAL(ok, false);

    CPPUNIT_ASSERT_EQUAL(String("1x").toInt(&ok), 1);
    CPPUNIT_ASSERT_EQUAL(ok, false);

    CPPUNIT_ASSERT_EQUAL(String("").toInt(&ok), 0);
    CPPUNIT_ASSERT_EQUAL(ok, false);

    CPPUNIT_ASSERT_EQUAL(String("-").toInt(&ok), 0);
    CPPUNIT_ASSERT_EQUAL(ok, false);

    CPPUNIT_ASSERT_EQUAL(String("123").toInt(), 123);
    CPPUNIT_ASSERT_EQUAL(String("-123").toInt(), -123);
    CPPUNIT_ASSERT_EQUAL(String("123aa").toInt(), 123);
    CPPUNIT_ASSERT_EQUAL(String("-123aa").toInt(), -123);

    CPPUNIT_ASSERT_EQUAL(String("0000").toInt(), 0);
    CPPUNIT_ASSERT_EQUAL(String("0001").toInt(), 1);

    String("2147483648").toInt(&ok);
    CPPUNIT_ASSERT_EQUAL(ok, false);

    String("-2147483649").toInt(&ok);
    CPPUNIT_ASSERT_EQUAL(ok, false);
  }

  void testFromInt()
  {
    CPPUNIT_ASSERT_EQUAL(String::number(0), String("0"));
    CPPUNIT_ASSERT_EQUAL(String::number(12345678), String("12345678"));
    CPPUNIT_ASSERT_EQUAL(String::number(-12345678), String("-12345678"));
  }

  void testSubstr()
  {
    CPPUNIT_ASSERT_EQUAL(String("01"), String("0123456").substr(0, 2));
    CPPUNIT_ASSERT_EQUAL(String("12"), String("0123456").substr(1, 2));
    CPPUNIT_ASSERT_EQUAL(String("123456"), String("0123456").substr(1, 200));
  }

  void testNewline()
  {
    ByteVector cr("abc\x0dxyz", 7);
    ByteVector lf("abc\x0axyz", 7);
    ByteVector crlf("abc\x0d\x0axyz", 8);

    CPPUNIT_ASSERT_EQUAL((unsigned int)7, String(cr).size());
    CPPUNIT_ASSERT_EQUAL((unsigned int)7, String(lf).size());
    CPPUNIT_ASSERT_EQUAL((unsigned int)8, String(crlf).size());

    CPPUNIT_ASSERT_EQUAL(L'\x0d', String(cr)[3]);
    CPPUNIT_ASSERT_EQUAL(L'\x0a', String(lf)[3]);
    CPPUNIT_ASSERT_EQUAL(L'\x0d', String(crlf)[3]);
    CPPUNIT_ASSERT_EQUAL(L'\x0a', String(crlf)[4]);
  }

  void testEncodeNonLatin1()
  {
    const String jpn(L"\u65E5\u672C\u8A9E");
    CPPUNIT_ASSERT_EQUAL(ByteVector("\xE5\x2C\x9E"), jpn.data(String::Latin1));
    CPPUNIT_ASSERT_EQUAL(ByteVector("\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E"), jpn.data(String::UTF8));
    CPPUNIT_ASSERT_EQUAL(ByteVector("\xFF\xFE\xE5\x65\x2C\x67\x9E\x8A"), jpn.data(String::UTF16));
    CPPUNIT_ASSERT_EQUAL(ByteVector("\xE5\x65\x2C\x67\x9E\x8A"), jpn.data(String::UTF16LE));
    CPPUNIT_ASSERT_EQUAL(ByteVector("\x65\xE5\x67\x2C\x8A\x9E"), jpn.data(String::UTF16BE));
    CPPUNIT_ASSERT_EQUAL(std::string("\xE5\x2C\x9E"), jpn.to8Bit(false));
    CPPUNIT_ASSERT_EQUAL(std::string("\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E"), jpn.to8Bit(true));
  }

  void testEncodeEmpty()
  {
    const String empty;
    CPPUNIT_ASSERT(empty.data(String::Latin1).isEmpty());
    CPPUNIT_ASSERT(empty.data(String::UTF8).isEmpty());
    CPPUNIT_ASSERT_EQUAL(ByteVector("\xFF\xFE"), empty.data(String::UTF16));
    CPPUNIT_ASSERT(empty.data(String::UTF16LE).isEmpty());
    CPPUNIT_ASSERT(empty.data(String::UTF16BE).isEmpty());
    CPPUNIT_ASSERT(empty.to8Bit(false).empty());
    CPPUNIT_ASSERT(empty.to8Bit(true).empty());
  }

  void testIterator()
  {
    String s1 = "taglib string";
    String s2 = s1;

    String::Iterator it1 = s1.begin();
    String::Iterator it2 = s2.begin();

    CPPUNIT_ASSERT_EQUAL(L't', *it1);
    CPPUNIT_ASSERT_EQUAL(L't', *it2);

    std::advance(it1, 4);
    std::advance(it2, 4);
    *it2 = L'I';
    CPPUNIT_ASSERT_EQUAL(L'i', *it1);
    CPPUNIT_ASSERT_EQUAL(L'I', *it2);
  }

};

CPPUNIT_TEST_SUITE_REGISTRATION(TestString);

