/* Copyright (C) 2004 Scott Wheeler <wheeler@kde.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <iostream>
#include <iomanip>
#include <string.h>

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>

#include <tlist.h>
#include <fileref.h>
#include <tfile.h>
#include <tag.h>
#include <tpropertymap.h>

using namespace std;

bool isArgument(const char *s)
{
  return strlen(s) == 2 && s[0] == '-';
}

bool isFile(const char *s)
{
  struct stat st;
#ifdef _WIN32
  return ::stat(s, &st) == 0 && (st.st_mode & (S_IFREG));
#else
  return ::stat(s, &st) == 0 && (st.st_mode & (S_IFREG | S_IFLNK));
#endif
}

void usage()
{
  cout << endl;
  cout << "Usage: tagwriter <fields> <files>" << endl;
  cout << endl;
  cout << "Where the valid fields are:" << endl;
  cout << "  -t <title>"   << endl;
  cout << "  -a <artist>"  << endl;
  cout << "  -A <album>"   << endl;
  cout << "  -c <comment>" << endl;
  cout << "  -g <genre>"   << endl;
  cout << "  -y <year>"    << endl;
  cout << "  -T <track>"   << endl;
  cout << "  -R <tagname> <tagvalue>"   << endl;
  cout << "  -I <tagname> <tagvalue>"   << endl;
  cout << "  -D <tagname>"   << endl;
  cout << endl;

  exit(1);
}

void checkForRejectedProperties(const TagLib::PropertyMap &tags)
{ // stolen from tagreader.cpp
  if(tags.size() > 0) {
    unsigned int longest = 0;
    for(TagLib::PropertyMap::ConstIterator i = tags.begin(); i != tags.end(); ++i) {
      if(i->first.size() > longest) {
        longest = i->first.size();
      }
    }
    cout << "-- rejected TAGs (properties) --" << endl;
    for(TagLib::PropertyMap::ConstIterator i = tags.begin(); i != tags.end(); ++i) {
      for(TagLib::StringList::ConstIterator j = i->second.begin(); j != i->second.end(); ++j) {
        cout << left << std::setw(longest) << i->first << " - " << '"' << *j << '"' << endl;
      }
    }
  }
}

int main(int argc, char *argv[])
{
  TagLib::List<TagLib::FileRef> fileList;

  while(argc > 0 && isFile(argv[argc - 1])) {

    TagLib::FileRef f(argv[argc - 1]);

    if(!f.isNull() && f.tag())
      fileList.append(f);

    argc--;
  }

  if(fileList.isEmpty())
    usage();

  for(int i = 1; i < argc - 1; i += 2) {

    if(isArgument(argv[i]) && i + 1 < argc && !isArgument(argv[i + 1])) {

      char field = argv[i][1];
      TagLib::String value = argv[i + 1];

      TagLib::List<TagLib::FileRef>::ConstIterator it;
      for(it = fileList.begin(); it != fileList.end(); ++it) {

        TagLib::Tag *t = (*it).tag();

        switch (field) {
        case 't':
          t->setTitle(value);
          break;
        case 'a':
          t->setArtist(value);
          break;
        case 'A':
          t->setAlbum(value);
          break;
        case 'c':
          t->setComment(value);
          break;
        case 'g':
          t->setGenre(value);
          break;
        case 'y':
          t->setYear(value.toInt());
          break;
        case 'T':
          t->setTrack(value.toInt());
          break;
        case 'R':
        case 'I':
          if(i + 2 < argc) {
            TagLib::PropertyMap map = (*it).file()->properties ();
            if(field == 'R') {
              map.replace(value, TagLib::String(argv[i + 2]));
            }
            else {
              map.insert(value, TagLib::String(argv[i + 2]));
            }
            ++i;
            checkForRejectedProperties((*it).file()->setProperties(map));
          }
          else {
            usage();
          }
          break;
        case 'D': {
          TagLib::PropertyMap map = (*it).file()->properties();
          map.erase(value);
          checkForRejectedProperties((*it).file()->setProperties(map));
          break;
        }
        default:
          usage();
          break;
        }
      }
    }
    else
      usage();
  }

  TagLib::List<TagLib::FileRef>::ConstIterator it;
  for(it = fileList.begin(); it != fileList.end(); ++it)
    (*it).file()->save();

  return 0;
}
