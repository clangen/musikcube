#!/bin/sh

MAJOR=$1
MINOR=$2
PATCH=$3

if [ -z "$MAJOR" ] || [ -z "$MINOR" ] || [ -z "$PATCH" ]; then
  echo "usage: ./update-version.sh <major> <minor> <patch>"
  exit
fi

sed -Ei.bak "s/(\s*)(#define VERSION_MAJOR )(.*)/\1\2${MAJOR}/g" src/musikcube/app/version.h
sed -Ei.bak "s/(\s*)(#define VERSION_MINOR )(.*)/\1\2${MINOR}/g" src/musikcube/app/version.h
sed -Ei.bak "s/(\s*)(#define VERSION_PATCH )(.*)/\1\2${PATCH}/g" src/musikcube/app/version.h
sed -Ei.bak "s/(\s*)(#define VERSION )(.*)/\1\2\"${MAJOR}.${MINOR}.${PATCH}\"/g" src/musikcube/app/version.h

# visual studio resource files are utf16-le, so sed can't operate on them
# directly. convert to utf8, process, then back to utf16-le
iconv -f utf-16le -t utf-8 src/musikcube/musikcube.rc > musikcube.rc.utf8
sed -Ei.bak "s/(\s*)(FILEVERSION )(.*)/\1\2${MAJOR}\,${MINOR}\,${PATCH}\,0/g" musikcube.rc.utf8
sed -Ei.bak "s/(\s*)(PRODUCTVERSION )(.*)/\1\2${MAJOR}\,${MINOR}\,${PATCH}\,0/g" musikcube.rc.utf8
sed -Ei.bak "s/(\s*)(VALUE \"FileVersion\", )(.*)/\1\2\"${MAJOR}.${MINOR}.${PATCH}.0\"/g" musikcube.rc.utf8
sed -Ei.bak "s/(\s*)(VALUE \"ProductVersion\", )(.*)/\1\2\"${MAJOR}.${MINOR}.${PATCH}.0\"/g" musikcube.rc.utf8
iconv -f utf-8 -t utf-16le musikcube.rc.utf8 > musikcube.rc.utf16
cp musikcube.rc.utf16 src/musikcube/musikcube.rc
rm musikcube.rc.*

sed -Ei.bak "s/(\s*)(set \(musikcube_VERSION_MAJOR )(.*)/\1\2${MAJOR}\)/g" CMakeLists.txt
sed -Ei.bak "s/(\s*)(set \(musikcube_VERSION_MINOR )(.*)/\1\2${MINOR}\)/g" CMakeLists.txt
sed -Ei.bak "s/(\s*)(set \(musikcube_VERSION_PATCH )(.*)/\1\2${PATCH}\)/g" CMakeLists.txt

sed -Ei.bak "s/(\s*)(%define version )(.*)/\1\2${MAJOR}.${MINOR}.${PATCH}/g" musikcube.spec

# ugh. there's a way to tell sed not to backup, but it's different on gnu and
# bsd sed variants. this is easier than trying to switch the args dynamically.
rm src/musikcube/app/version.h.bak
rm CMakeLists.txt.bak
rm musikcube.spec.bak
