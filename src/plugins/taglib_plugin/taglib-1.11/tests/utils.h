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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <fcntl.h>
#include <sys/fcntl.h>
#include <sys/stat.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <fstream>

using namespace std;

inline string testFilePath(const string &filename)
{
  return string(TESTS_DIR "data/") + filename;
}

#define TEST_FILE_PATH_C(f) testFilePath(f).c_str()

inline string copyFile(const string &filename, const string &ext)
{
  char testFileName[1024];

#ifdef _WIN32
  char tempDir[MAX_PATH + 1];
  GetTempPathA(sizeof(tempDir), tempDir);
  wsprintfA(testFileName, "%s\\taglib-test%s", tempDir, ext.c_str());
#else
  snprintf(testFileName, sizeof(testFileName), "/%s/taglib-test%s", P_tmpdir, ext.c_str());
#endif

  string sourceFileName = testFilePath(filename) + ext;
  ifstream source(sourceFileName.c_str(), std::ios::binary);
  ofstream destination(testFileName, std::ios::binary);
  destination << source.rdbuf();
  return string(testFileName);
}

inline void deleteFile(const string &filename)
{
  remove(filename.c_str());
}

inline bool fileEqual(const string &filename1, const string &filename2)
{
  char buf1[BUFSIZ];
  char buf2[BUFSIZ];

  ifstream stream1(filename1.c_str(), ios_base::in | ios_base::binary);
  ifstream stream2(filename2.c_str(), ios_base::in | ios_base::binary);

  if(!stream1 && !stream2) return true;
  if(!stream1 || !stream2) return false;

  for(;;)
  {
    stream1.read(buf1, BUFSIZ);
    stream2.read(buf2, BUFSIZ);

    streamsize n1 = stream1.gcount();
    streamsize n2 = stream2.gcount();

    if(n1 != n2) return false;

    if(n1 == 0) break;

    if(memcmp(buf1, buf2, static_cast<size_t>(n1)) != 0) return false;
  }

  return stream1.good() == stream2.good();
}

#ifdef TAGLIB_STRING_H

namespace TagLib {

  inline String longText(size_t length, bool random = false)
  {
    const wchar_t chars[] = L"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz_";

    std::wstring text(length, L'X');

    if(random) {
      for(size_t i = 0; i < length; ++i)
        text[i] = chars[rand() % 53];
    }

    return String(text);
  }
}

#endif

class ScopedFileCopy
{
public:
  ScopedFileCopy(const string &filename, const string &ext, bool deleteFile=true) :
    m_deleteFile(deleteFile),
    m_filename(copyFile(filename, ext))
  {
  }

  ~ScopedFileCopy()
  {
    if(m_deleteFile)
      deleteFile(m_filename);
  }

  string fileName() const
  {
    return m_filename;
  }

private:
  const bool m_deleteFile;
  const string m_filename;
};
