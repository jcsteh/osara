/*
 * OSARA: Open Source Accessibility for the REAPER Application
 * Core utilities code
 * Copyright 2024 James Teh
 * License: GNU General Public License version 2.0
 */

#include "coreUtils.h"

#include <string>

#include "config.h"
#include "osara.h"

using namespace std;

namespace {
preview_register_t soundPreviewReg = {0};
}

void playSound(const char* fileName) {
	if (!settings::playSounds) {
		return;
	}
	if (soundPreviewReg.src) {
		StopPreview(&soundPreviewReg);
		PCM_Source_Destroy(soundPreviewReg.src);
	} else {
		// Initialise preview.
#ifdef _WIN32
		InitializeCriticalSection(&soundPreviewReg.cs);
#else
		pthread_mutex_init(&soundPreviewReg.mutex, nullptr);
#endif
		soundPreviewReg.volume = 1.0;
	}
	string path(GetResourcePath());
	path += "/osara/sounds/";
	path += fileName;
	soundPreviewReg.src = PCM_Source_CreateFromFile(path.c_str());
	soundPreviewReg.curpos = 0.0;
	PlayPreview(&soundPreviewReg);
}
