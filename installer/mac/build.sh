#!/bin/bash
version=$1
dmg=../osara_$version.dmg
cd "`dirname \"$0\"`"
rm -rf content/.data
mkdir content/.data
cd content/.data
cp ../../../../build/reaper_osara.dylib .
cp ../../../../config/reaper-kb.ini OSARA.ReaperKeyMap
cp ../../../../copying.txt .
cd ../..
rm -f $dmg
hdiutil create -volname "OSARA $version" -srcfolder content $dmg
rm -rf content/.data
