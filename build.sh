#!/bin/bash

scons
if [ $? -eq 0 ]; then
	sudo hdiutil mount ./installer/osara_unknown.dmg
	/Volumes/OSARA\ unknown/Install\ OSARA\ extension.command
		sudo hdiutil unmount /Volumes/OSARA\ unknown
fi