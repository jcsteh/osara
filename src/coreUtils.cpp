/*
 * OSARA: Open Source Accessibility for the REAPER Application
 * Core utilities code
 * Copyright 2024 James Teh
 * License: GNU General Public License version 2.0
 */

#include "coreUtils.h"

#include <string>

#include "osara.h"
#include "config.h"

using namespace std;

namespace {
preview_register_t soundPreviewReg = {0};
double lastSoundCursorPos = 0.0;

bool isMarkerOrRegionBetween(double start, double end) {
	int count = CountProjectMarkers(nullptr, nullptr, nullptr);
	for (int i = 0; i < count; ++i) {
		double markerPos, regionEnd;
		bool isRegion;
		EnumProjectMarkers(i, &isRegion, &markerPos, &regionEnd, nullptr, nullptr);
		if (markerPos > end) {
			break;
		}
		if ((start <= markerPos && markerPos <= end) ||
				(isRegion && start <= regionEnd && regionEnd <= end)) {
			return true;
		}
	}
	return false;
}

bool isPassingItemEdge(double start, double end) {
	if (end - start > 0.020) {
		// The cursor jumped more than 20 ms. We only want to report when passing
		// close to an item edge.
		return false;
	}
	MediaTrack* track = GetLastTouchedTrack();
	if (!track) {
		return false;
	}
	int count = CountTrackMediaItems(track);
	for (int i = 0; i < count; ++i) {
		MediaItem* item = GetTrackMediaItem(track, i);
		double itemStart = *(double*)GetSetMediaItemInfo(item, "D_POSITION", nullptr);
		if (itemStart > end) {
			break;
		}
		double length = *(double*)GetSetMediaItemInfo(item, "D_LENGTH", nullptr);
		double itemEnd = itemStart + length;
		if ((start <= itemStart && itemStart <= end) ||
				(start <= itemEnd && itemEnd <= end)) {
			return true;
		}
	}
	return false;
}

} // namespace

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

void playSoundForCursorMovement() {
	if (!settings::playSounds) {
		return;
	}
	double cursor = GetCursorPosition();
	if (cursor == lastSoundCursorPos) {
		return; // No movement, nothing to do.
	}
	double start, end;
	if (cursor > lastSoundCursorPos) {
		// Moving forward.
		start = lastSoundCursorPos;
		end = cursor;
	} else {
		// Moving backward.
		start = cursor;
		end = lastSoundCursorPos;
	}
	if (isMarkerOrRegionBetween(start, end)) {
		playSound("marker.mp3");
	}
	if (isPassingItemEdge(start, end)) {
		playSound("item.mp3");
	}
	lastSoundCursorPos = cursor;
}
