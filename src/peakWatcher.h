/*
 * OSARA: Open Source Accessibility for the REAPER Application
 * Peak Watcher header
 * Copyright 2015-2023 NV Access Limited, James Teh
 * License: GNU General Public License version 2.0
 */

#include "osara.h"

namespace peakWatcher {
void initialize();
void onSwitchTab();
}

void cmdPeakWatcher(int command);
void cmdReportPeakWatcherW1C1(int command);
void cmdReportPeakWatcherW1C2(int command);
void cmdReportPeakWatcherW2C1(int command);
void cmdReportPeakWatcherW2C2(int command);
void cmdResetPeakWatcherW1(int command);
void cmdResetPeakWatcherW2(int command);
void cmdPausePeakWatcher(int command);

