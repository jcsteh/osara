/*
 * OSARA: Open Source Accessibility for the REAPER Application
 * Header for code related to FX chain windows
 * Author: James Teh <jamie@jantrid.net>
 * Copyright 2020 James Teh
 * License: GNU General Public License version 2.0
 */

bool isFxListFocused();
bool maybeSwitchToFxPluginWindow();
bool maybeReportFxChainBypass(bool aboutToToggle=false);
bool maybeReportFxChainBypassDelayed();
bool maybeOpenFxPresetDialog();
bool maybeSwitchFxTab(bool previous);
