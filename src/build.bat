@echo off
cl /I..\include /EHsc reaper_osara.cpp /link /dll /out:reaper_osara%1.dll user32.lib ole32.lib
