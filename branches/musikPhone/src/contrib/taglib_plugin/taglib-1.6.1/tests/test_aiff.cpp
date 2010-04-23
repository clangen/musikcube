#include <cppunit/extensions/HelperMacros.h>
#include <string>
#include <stdio.h>
#include <tag.h>
#include <tbytevectorlist.h>
#include <aifffile.h>
#include "utils.h"

using namespace std;
using namespace TagLib;

class TestAIFF : public CppUnit::TestFixture
{
  CPPUNIT_TEST_SUITE(TestAIFF);
  CPPUNIT_TEST(testReading);
  CPPUNIT_TEST_SUITE_END();

public:

  void testReading()
  {
    string filename = copyFile("empty", ".aiff");

    RIFF::AIFF::File *f = new RIFF::AIFF::File(filename.c_str());
    CPPUNIT_ASSERT_EQUAL(689, f->audioProperties()->bitrate());

    deleteFile(filename);
  }

};

CPPUNIT_TEST_SUITE_REGISTRATION(TestAIFF);
