#!/bin/bash
mkdir extFiles
cd extFiles
cp ../../../build/reaper_osara.dylib .
cp ../../../config/reaper-kb.ini OSARA.ReaperKeyMap
cd ..
pkgbuild --identifier org.nvaccess.osara.extension --root extFiles --install-location /.tmp_osaraExtInst --scripts extScripts extension.pkg
pkgbuild --identifier org.nvaccess.osara.replaceKeyMap --nopayload --scripts repkeyScripts replaceKeyMap.pkg
productbuild --distribution dist.xml osara.pkg
