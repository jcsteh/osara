#!/bin/bash
set -e
osara_dylib="reaper_osara.dylib"
osara_keymap="OSARA.ReaperKeyMap"
userplugins=~/"Library/Application Support/REAPER/UserPlugins"
keymaps=~/"Library/Application Support/REAPER/KeyMaps"
cd "`dirname \"$0\"`/.data"
echo "copying $osara_dylib to $userplugins"
cp "$osara_dylib" "$userplugins"
echo "copying $osara_keymap to $keymaps"
cp "$osara_keymap" "$keymaps"
echo Done.
