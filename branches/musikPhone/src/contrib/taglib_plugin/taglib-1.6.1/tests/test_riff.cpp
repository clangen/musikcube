#include <cppunit/extensions/HelperMacros.h>
#include <string>
#include <stdio.h>
#include <tag.h>
#include <tbytevectorlist.h>
#include <rifffile.h>
#include "utils.h"

using namespace std;
using namespace TagLib;

class PublicRIFF : public RIFF::File
{
public:
  PublicRIFF(FileName file) : RIFF::File(file, BigEndian) {};
  TagLib::uint chunkCount() { return RIFF::File::chunkCount(); };
  TagLib::uint chunkOffset(TagLib::uint i) { return RIFF::File::chunkOffset(i); };
  ByteVector chunkName(TagLib::uint i) { return RIFF::File::chunkName(i); };
  ByteVector chunkData(TagLib::uint i) { return RIFF::File::chunkData(i); };
  void setChunkData(const ByteVector &name, const ByteVector &data) {
    RIFF::File::setChunkData(name, data);
  };
  virtual TagLib::Tag* tag() const { return 0; };
  virtual TagLib::AudioProperties* audioProperties() const { return 0;};
  virtual bool save() { return false; };
};

class TestRIFF : public CppUnit::TestFixture
{
  CPPUNIT_TEST_SUITE(TestRIFF);
  CPPUNIT_TEST(testPadding);
  CPPUNIT_TEST_SUITE_END();

public:

  void testPadding()
  {
    string filename = copyFile("empty", ".aiff");

    PublicRIFF *f = new PublicRIFF(filename.c_str());
    CPPUNIT_ASSERT_EQUAL(ByteVector("TEST"), f->chunkName(2));
    CPPUNIT_ASSERT_EQUAL(TagLib::uint(0x1728 + 8), f->chunkOffset(2));

    f->setChunkData("TEST", "foo");
    delete f;

    f = new PublicRIFF(filename.c_str());
    CPPUNIT_ASSERT_EQUAL(ByteVector("TEST"), f->chunkName(2));
    CPPUNIT_ASSERT_EQUAL(ByteVector("foo"), f->chunkData(2));
    CPPUNIT_ASSERT_EQUAL(TagLib::uint(0x1728 + 8), f->chunkOffset(2));

    f->setChunkData("SSND", "abcd");

    CPPUNIT_ASSERT_EQUAL(ByteVector("SSND"), f->chunkName(1));
    CPPUNIT_ASSERT_EQUAL(ByteVector("abcd"), f->chunkData(1));

    f->seek(f->chunkOffset(1));
    CPPUNIT_ASSERT_EQUAL(ByteVector("abcd"), f->readBlock(4));

    CPPUNIT_ASSERT_EQUAL(ByteVector("TEST"), f->chunkName(2));
    CPPUNIT_ASSERT_EQUAL(ByteVector("foo"), f->chunkData(2));

    f->seek(f->chunkOffset(2));
    CPPUNIT_ASSERT_EQUAL(ByteVector("foo"), f->readBlock(3));

    delete f;

    f = new PublicRIFF(filename.c_str());

    CPPUNIT_ASSERT_EQUAL(ByteVector("SSND"), f->chunkName(1));
    CPPUNIT_ASSERT_EQUAL(ByteVector("abcd"), f->chunkData(1));

    CPPUNIT_ASSERT_EQUAL(ByteVector("TEST"), f->chunkName(2));
    CPPUNIT_ASSERT_EQUAL(ByteVector("foo"), f->chunkData(2));

    deleteFile(filename);
  }

};

CPPUNIT_TEST_SUITE_REGISTRATION(TestRIFF);
