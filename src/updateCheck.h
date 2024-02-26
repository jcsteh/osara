/*
 * OSARA: Open Source Accessibility for the REAPER Application
 * Update check header
 * Author: James Teh <jamie@jantrid.net>
 * Copyright 2024 James Teh
 * License: GNU General Public License version 2.0
 */

#pragma once

#include "osara.h"

void startUpdateCheck(bool manual=false);
void cancelUpdateCheck();
void cmdCheckForUpdate(Command* command);
