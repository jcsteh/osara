#!/bin/bash
version=$1
dmg=../osara_$version.dmg
cd "`dirname \"$0\"`"
rm -rf content/.data
mkdir content/.data
cd content
cp ../../../copying.txt .
cd .data
cp ../../../../build/reaper_osara.dylib .
cp ../../../../config/reaper-kb.ini OSARA.ReaperKeyMap
cd ../..
rm -f $dmg
hdiutil create -volname "OSARA $version" -srcfolder content $dmg
rm -rf content/copying.txt content/.data
