#!/bin/sh
clang -dynamiclib -o reaper_osara.dylib -current_version 1.0 -compatibility_version 1.0 -fvisibility=hidden -undefined dynamic_lookup -stdlib=libc++ -std=c++11 -I ../include -I ../include/WDL -I ../include/WDL/wdl/swell reaper_osara.cpp
