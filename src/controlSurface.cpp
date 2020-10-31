/*
 * OSARA: Open Source Accessibility for the REAPER Application
 * Control surface code
 * Copyright 2019-2020 NV Access Limited, James Teh, Leonard de Ruijter
 * License: GNU General Public License version 2.0
 */

#include <string>
#include <sstream>
#include "osara.h"

using namespace std;

/*** A control surface to obtain certain info that can only be retrieved that way.
 */
class Surface: public IReaperControlSurface {
	public:
	virtual const char* GetTypeString() override {
		return "OSARA";
	}

	virtual const char* GetDescString() override {
		return "OSARA";
	}

	virtual const char* GetConfigString() override {
		return "";
	}

	virtual void SetPlayState(bool play, bool pause, bool rec) override {
		if (!this->shouldHandleChange()) {
			return;
		}
		// Calculate integer based transport state
		int TransportState = (int)play | ((int)pause << 1) | ((int)rec << 2);
		reportTransportState(TransportState);
	}

	virtual void SetRepeatState(bool repeat) override {
		if (!this->shouldHandleChange()) {
			return;
		}
		reportRepeat(repeat);
	}

	virtual int Extended(int call, void* parm1, void* parm2, void* parm3) override {
		if (call == CSURF_EXT_SETFXPARAM) {
			if (!this->shouldHandleChange()) {
				return 0; // Unsupported.
			}
			auto track = (MediaTrack*)parm1;
			int fx = *(int*)parm2 >> 16;
			int param = *(int*)parm2 & 0xFFFF;
			double normVal = *(double*)parm3;
			ostringstream s;
			char chunk[256];
			// Don't report the effect name if we're changing the same effect.
			bool different = track != this->lastTrack || fx != this->lastFx;
			if (different) {
				TrackFX_GetFXName(track, fx, chunk, sizeof(chunk));
				s << chunk << " ";
			}
			this->lastTrack = track;
			this->lastFx = fx;
			different |= param != this->lastFxParam;
			if (different) {
				TrackFX_GetParamName(track, fx, param, chunk, sizeof(chunk));
				s << chunk << " ";
			}
			this->lastFxParam = param;
			TrackFX_FormatParamValueNormalized(track, fx, param, normVal, chunk,
				sizeof(chunk));
			if (chunk[0]) {
				s << chunk;
			} else {
				s << normVal;
			}
			outputMessage(s);
		}
		return 0; // Unsupported.
	}

	private:
	bool shouldHandleChange() {
		DWORD now = GetTickCount();
		DWORD prevChangeTime = lastChangeTime;
		lastChangeTime = now;
		if (!isHandlingCommand &&
			// Only handle surface changes if the last change is 100ms or more ago.
			now - prevChangeTime >= 100
		) {
			return true;
		}
		return false;
	}
	DWORD lastChangeTime = 0;

	MediaTrack* lastTrack = nullptr;
	int lastFx = 0;
	int lastFxParam = 0;
};

IReaperControlSurface* createSurface() {
	return new Surface;
}
