#!/bin/sh
include_dir=../include
wdl_dir=../include/WDL
swell_dir=../include/WDL/WDL/swell
clang++ -dynamiclib -o reaper_osara.dylib -current_version 1.0 -compatibility_version 1.0 -fvisibility=hidden -stdlib=libc++ -std=c++11 -I $include_dir -I $wdl_dir -I $swell_dir reaper_osara.cpp $swell_dir/swell.cpp $swell_dir/swell-ini.cpp $swell_dir/swell-dlg.mm $swell_dir/swell-gdi.mm $swell_dir/swell-kb.mm $swell_dir/swell-misc.mm $swell_dir/swell-miscdlg.mm $swell_dir/swell-menu.mm $swell_dir/swell-wnd.mm $swell_dir/swell-modstub.mm -framework Cocoa -framework Carbon -framework Appkit osxa11y.mm
