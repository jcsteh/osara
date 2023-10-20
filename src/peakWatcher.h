/*
 * OSARA: Open Source Accessibility for the REAPER Application
 * Peak Watcher header
 * Author: James Teh <jamie@jantrid.net>
 * Copyright 2015-2023 NV Access Limited, James Teh
 * License: GNU General Public License version 2.0
 */

#include "osara.h"

namespace peakWatcher {
void initialize();
void onSwitchTab();
} // namespace peakWatcher

void cmdPeakWatcher(Command* command);
void cmdReportPeakWatcherW1C1(Command* command);
void cmdReportPeakWatcherW1C2(Command* command);
void cmdReportPeakWatcherW2C1(Command* command);
void cmdReportPeakWatcherW2C2(Command* command);
void cmdResetPeakWatcherW1(Command* command);
void cmdResetPeakWatcherW2(Command* command);
void cmdPausePeakWatcher(Command* command);
