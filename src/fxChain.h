/*
 * OSARA: Open Source Accessibility for the REAPER Application
 * Header for code related to FX chain windows
 * Author: James Teh <jamie@jantrid.net>
 * Copyright 2020-2023 James Teh
 * License: GNU General Public License version 2.0
 */

#include <sstream>

bool getFocusedFx(MediaTrack** track = nullptr, MediaItem_Take** take = nullptr, int* fx = nullptr);
bool isFxListFocused();
void shortenFxName(const char* name, std::ostringstream& s);
bool maybeSwitchToFxPluginWindow();
bool maybeReportFxChainBypass(bool aboutToToggle = false);
bool maybeReportFxChainBypassDelayed();
bool maybeOpenFxPresetDialog();
bool maybeSwitchFxTab(bool previous);
