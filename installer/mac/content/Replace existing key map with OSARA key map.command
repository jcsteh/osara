#!/bin/bash
set -e
cd ~/"Library/Application Support/REAPER"
echo "copying KeyMaps/OSARA.ReaperKeyMap to reaper-kb.ini"
cp KeyMaps/OSARA.ReaperKeyMap reaper-kb.ini
echo Done.
