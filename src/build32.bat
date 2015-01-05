@echo off
cl /I..\include /EHsc reaper_osara.cpp /link /dll /out:reaper_osara32.dll user32.lib ole32.lib
