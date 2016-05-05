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

#include <stdlib.h>
#include <string.h>
#include <fstream>
#include <stdexcept>
#include <cppunit/TestResult.h>
#include <cppunit/TestResultCollector.h>
#include <cppunit/TestRunner.h>
#include <cppunit/extensions/TestFactoryRegistry.h>
#include <cppunit/BriefTestProgressListener.h>
#include <cppunit/CompilerOutputter.h>
#include <cppunit/XmlOutputter.h>

int main(int argc, char* argv[])
{
  std::string testPath = (argc > 1) ? std::string(argv[1]) : "";

  // Create the event manager and test controller
  CppUnit::TestResult controller;

  // Add a listener that collects test result
  CppUnit::TestResultCollector result;
  controller.addListener(&result);

  // Add a listener that print dots as test run.
  CppUnit::BriefTestProgressListener progress;
  controller.addListener(&progress);

  // Add the top suite to the test runner
  CppUnit::TestRunner runner;
  runner.addTest(CppUnit::TestFactoryRegistry::getRegistry().makeTest());

  try {
    std::cout << "Running "  << testPath;
    runner.run(controller, testPath);

    std::cerr << std::endl;

    // Print test in a compiler compatible format.
    CppUnit::CompilerOutputter outputter(&result, std::cerr);
    outputter.write();

#if defined(_MSC_VER) && _MSC_VER > 1500
    char *xml = NULL;
    ::_dupenv_s(&xml, NULL, "CPPUNIT_XML");
#else
    char *xml = ::getenv("CPPUNIT_XML");
#endif
    if(xml && !::strcmp(xml, "1")) {
      std::ofstream xmlfileout("cpptestresults.xml");
      CppUnit::XmlOutputter xmlout(&result, xmlfileout);
      xmlout.write();
    }
#if defined(_MSC_VER) && _MSC_VER > 1500
    ::free(xml);
#endif
  }
  catch(std::invalid_argument &e){
    std::cerr << std::endl
              << "ERROR: " << e.what()
              << std::endl;
    return 0;
  }

  return result.wasSuccessful() ? 0 : 1;
}
