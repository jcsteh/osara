/*
 * OSARA: Open Source Accessibility for the REAPER Application
 * Linux accessibility announcement bridge
 * Copyright 2026 robbie Murray
 * License: GNU General Public License version 2.0
 */

#include "linuxa11y_wrapper.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
#include <windows.h>
#pragma clang diagnostic pop

namespace LinuxA11y {
	using AnnounceFn = void (*)(const char* message, int interrupt);
	static AnnounceFn announceFn = nullptr;

	bool init() {
		announceFn = reinterpret_cast<AnnounceFn>(
			SWELL_ExtendedAPI("ACCESSIBILITY_ANNOUNCER", nullptr));
		return announceFn != nullptr;
	}

	void announce(const std::string& message, bool interrupt) {
		if (announceFn) {
			announceFn(message.c_str(), interrupt ? 1 : 0);
		}
	}

	void destroy() {
		announceFn = nullptr;
	}
}
