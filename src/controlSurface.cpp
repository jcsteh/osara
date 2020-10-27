/*
 * OSARA: Open Source Accessibility for the REAPER Application
 * Control surface code
 * Author: James Teh <jamie@jantrid.net>
 * Copyright 2019 NV Access Limited, James Teh, Leonard de Ruijter
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
			// parm3 is supposedly normalized value, but it seems to be incorrect in
			// some cases.
			double value = TrackFX_GetParam(track, fx, param, nullptr, nullptr);
			ostringstream s;
			char chunk[256];
			TrackFX_GetFXName(track, fx, chunk, sizeof(chunk));
			s << chunk << " ";
			TrackFX_GetParamName(track, fx, param, chunk, sizeof(chunk));
			s << chunk << " ";
			TrackFX_FormatParamValue(track, fx, param, value, chunk,
				sizeof(chunk));
			if (chunk[0]) {
				s << chunk;
			} else {
				s << value;
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

};

IReaperControlSurface* createSurface() {
	return new Surface;
}
