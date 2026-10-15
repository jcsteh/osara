/*
 * OSARA: Open Source Accessibility for the REAPER Application
 * Configuration header
 * Copyright 2022-2024 James Teh
 * License: GNU General Public License version 2.0
 */

#pragma once

#include <string>
#include "osara.h"

namespace settings {
// Declare the variable for each setting. For example, to access the reportScrub
// setting from C++ code, you would use settings::reportScrub.
#define BoolSetting(name, sectionId, displayName, default) extern bool name;
#include "settings.h"
#undef BoolSetting
}

const char CONFIG_SECTION[] = "osara";

void loadConfig();
void cmdConfig(int command);
void registerSettingCommands();
bool handleSettingCommand(int command);
void cmdConfigReaperOptimal(int command);
void maybeAutoConfigReaperOptimal();

// These functions allow restart requests to be deferred while changing REAPER
// configuration at startup. This is necessary so that multiple types of changes
// can be applied with a single restart. This also allows all of the changes to
// be reported in a single message.
void deferRestart();
void requestRestart(const std::string& summary = {});
void endRestartDeferral();
