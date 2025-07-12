/*
 * OSARA: Open Source Accessibility for the REAPER Application
 * Exported API functions code
 * Exports functions to be called by REAPER extensions and scripts.
 * Copyright 2020-2023 James Teh
 * License: GNU General Public License version 2.0
 */

// Microsoft wants strncpy_s, but Mac doesn't have it.
#define _CRT_SECURE_NO_WARNINGS
#include "buildVersion.h"
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

void osara_getVersion(char* versionOut, uintptr_t versionOut_sz) {
	strncpy(versionOut, OSARA_VERSION, versionOut_sz);
}
void* _vararg_osara_getVersion(void** args, int nArgs) {
	osara_getVersion((char*)args[0], (uintptr_t)args[1]);
	return nullptr;
}

void registerExports(reaper_plugin_info_t* rec) {
	rec->Register("API_osara_outputMessage", (void*)osara_outputMessage);
	rec->Register("APIvararg_osara_outputMessage",
		(void*)_vararg_osara_outputMessage);
	rec->Register("APIdef_osara_outputMessage",
		(void*)"void\0const char*\0message\0"
		"Output a message to screen readers.\n"
		"This should only be used in consultation with screen reader users. "
		"Note that this may not work on Windows when certain GUI controls have "
		"focus such as list boxes and trees.");
	rec->Register("API_osara_isShortcutHelpEnabled",
		(void*)osara_isShortcutHelpEnabled);
	rec->Register("API_osara_outputMessage", (void*)osara_outputMessage);
	rec->Register("APIvararg_osara_getVersion",
		(void*)_vararg_osara_getVersion);
	rec->Register("APIdef_osara_getVersion",
		(void*)"void\0char*,int\0versionOut,versionOut_sz\0"
		"Get the version of OSARA.\n"
		"This will be in the form: year.month.day.build,commit\n"
		"For example: 2024.3.6.1332,13560ef7");
}
