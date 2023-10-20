/*
 * OSARA: Open Source Accessibility for the REAPER Application
 * Exported API functions code
 * Exports functions to be called by REAPER extensions and scripts.
 * Author: James Teh <jamie@jantrid.net>
 * Copyright 2020-2023 James Teh
 * License: GNU General Public License version 2.0
 */

#include "osara.h"

// The _vararg_ version is needed for ReaScript.

void osara_outputMessage(const char* message) {
	outputMessage(message);
}

void* _vararg_osara_outputMessage(void** args, int nArgs) {
	osara_outputMessage((const char*)args[0]);
	return nullptr;
}

bool osara_isShortcutHelpEnabled() {
	return isShortcutHelpEnabled;
}

void registerExports(reaper_plugin_info_t* rec) {
	rec->Register("API_osara_outputMessage", (void*)osara_outputMessage);
	rec->Register("APIvararg_osara_outputMessage", (void*)_vararg_osara_outputMessage);
	rec->Register(
			"APIdef_osara_outputMessage",
			(void*)"void\0const char*\0message\0"
						 "Output a message to screen readers.\n"
						 "This should only be used in consultation with screen reader users. "
						 "Note that this may not work on Windows when certain GUI controls have "
						 "focus such as list boxes and trees."
	);
	rec->Register("API_osara_isShortcutHelpEnabled", (void*)osara_isShortcutHelpEnabled);
}
