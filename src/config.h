/*
 * OSARA: Open Source Accessibility for the REAPER Application
 * Configuration header
 * Author: James Teh <jamie@jantrid.net>
 * Copyright 2022-2023 James Teh
 * License: GNU General Public License version 2.0
 */

#pragma once

#include "osara.h"

namespace settings {
// Declare the variable for each setting. For example, to access the reportScrub
// setting from C++ code, you would use settings::reportScrub.
#define BoolSetting(name, sectionId, displayName, default) extern bool name;
#include "settings.h"
#undef BoolSetting
} // namespace settings

const char CONFIG_SECTION[] = "osara";

void loadConfig();
void cmdConfig(Command* command);
void registerSettingCommands();
bool handleSettingCommand(int command);
