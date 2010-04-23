#include <cppunit/extensions/HelperMacros.h>
#include <string>
#include <stdio.h>
#include <tag.h>
#include <tstringlist.h>
#include <tbytevectorlist.h>
#include <oggfile.h>
#include <vorbisfile.h>
#include <oggpageheader.h>
#include "utils.h"

using namespace std;
using namespace TagLib;

class TestOGG : public CppUnit::TestFixture
{
  CPPUNIT_TEST_SUITE(TestOGG);
  CPPUNIT_TEST(testSimple);
  CPPUNIT_TEST(testSplitPackets);
  CPPUNIT_TEST_SUITE_END();

public:

  void testSimple()
  {
    string newname = copyFile("empty", ".ogg");

    Vorbis::File *f = new Vorbis::File(newname.c_str());
    f->tag()->setArtist("The Artist");
    f->save();
    delete f;

    f = new Vorbis::File(newname.c_str());
    CPPUNIT_ASSERT_EQUAL(String("The Artist"), f->tag()->artist());
    delete f;

    deleteFile(newname);
  }

  void testSplitPackets()
  {
    string newname = copyFile("empty", ".ogg");

    Vorbis::File *f = new Vorbis::File(newname.c_str());
    f->tag()->addField("test", ByteVector(128 * 1024, 'x') + ByteVector(1, '\0'));
    f->save();
    delete f;

    f = new Vorbis::File(newname.c_str());
    CPPUNIT_ASSERT_EQUAL(19, f->lastPageHeader()->pageSequenceNumber());
    delete f;

    deleteFile(newname);
  }

};

CPPUNIT_TEST_SUITE_REGISTRATION(TestOGG);
