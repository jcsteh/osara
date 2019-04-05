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
class Surface : public IReaperControlSurface {
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
		if (!this->shouldHandleChange())
			return;
		// Calculate integer based transport state
		int TransportState = (int)play | ((int)pause << 1) | ((int)rec << 2);
		reportTransportState(TransportState);
	}

	virtual void SetRepeatState(bool repeat) override {
		if (!this->shouldHandleChange())
			return;
		reportRepeat(repeat);
	}

	virtual void SetSurfaceSolo(MediaTrack* track, bool solo) override {
		if (!this->shouldHandleChange())
			return;
		ostringstream s;
		s << (solo ? "soloed" : "unsoloed") << " ";
		s << formatTrackWithName(track);
		outputMessage(s);
	}

	virtual void SetSurfaceMute(MediaTrack* track, bool mute) override {
		if (!this->shouldHandleChange())
			return;
		ostringstream s;
		s << (mute ? "muted" : "unmuted") << " ";
		s << formatTrackWithName(track);
		outputMessage(s);
	}

	virtual void SetSurfaceRecArm(MediaTrack* track, bool arm) override {
		if (!this->shouldHandleChange())
			return;
		ostringstream s;
		s << (arm ? "armed" : "unarmed") << " ";
		s << formatTrackWithName(track);
		outputMessage(s);
	}

	private:
	bool shouldHandleChange() {
		DWORD now = GetTickCount();
		DWORD prevChangeTime = lastChangeTime;
		lastChangeTime = now;
		if (!isHandlingCommand &&
			// Only handle surface changes if the last change is 100ms or more ago.
			now - prevChangeTime >= 100
		)
			return true;
		return false;
	}
	DWORD lastChangeTime = 0;

};

IReaperControlSurface* createSurface() {
	return new Surface;
}
