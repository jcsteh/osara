#!/bin/bash

scons
if [ $? -eq 0 ]; then
	hdiutil mount ./installer/osara_unknown.dmg
	/Volumes/OSARA\ unknown/Install\ OSARA\ extension.command
	hdiutil unmount /Volumes/OSARA\ unknown
fi
