#!/bin/bash
set -e
version=$1
dmg=../osara_$version.dmg
cd "`dirname \"$0\"`"
rm -rf content/.data
mkdir content/.data
cd content
cp ../../../copying.txt .
cd .data
cp ../../../../build/reaper_osara.dylib .
cp ../../../../config/mac/reaper-kb.ini OSARA.ReaperKeyMap
mkdir locale
cp ../../../../locale/*.po locale/
mkdir EBUR128
cp ../../../../include/EBUR128/* EBUR128/
cd ../..
rm -f $dmg
# We seem to need a delay here to avoid an "hdiutil: create failed - Resource busy" error.
sleep 1
hdiutil create -volname "OSARA $version" -srcfolder content $dmg
rm -rf content/copying.txt content/.data
