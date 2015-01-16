@echo off
rc reaper_osara.rc
cl /I..\include /EHsc reaper_osara.cpp /link /dll /out:reaper_osara%1.dll reaper_osara.res user32.lib ole32.lib
