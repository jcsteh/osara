#!/bin/bash
set -e
target=~/"Library/Application Support/REAPER"
cd "`dirname \"$0\"`/.data"
cp reaper_osara.dylib "$target/UserPlugins"
cp OSARA.ReaperKeyMap "$target/KeyMaps"
echo Done.
