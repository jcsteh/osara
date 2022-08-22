/*
 * OSARA: Open Source Accessibility for the REAPER Application
 * Setting definitions
 * Author: James Teh <jamie@jantrid.net>
 * Copyright 2022 James Teh
 * License: GNU General Public License version 2.0
 */

/*
 * This file defines the various OSARA configuration settings. Most code that
 * wants to access settings should include config.h instead. This file is
 * included by config code which defines macros to perform an operation for
 * each setting; e.g. loading configuration.
 */

// Usage: BoolSetting(name, displayName, defaultValue)
BoolSetting(reportScrub,
	"Report position when &scrubbing",
	true)
BoolSetting(reportTimeMovementWhilePlaying,
	"Report time movement during playback/recording",
	true)
BoolSetting(moveFromPlayCursor,
	"&Move relative to the play cursor for time movement commands during playback",
	false)
BoolSetting(reportMarkersWhilePlaying,
	"Report mar&kers during playback",
	false)
BoolSetting(reportTransport,
	"Report &transport state (play, record, etc.)",
	true)
BoolSetting(reportTrackNumbers,
	"&Report track numbers",
	true)
BoolSetting(reportFx,
	"Report &FX when moving to tracks/takes",
	false)
BoolSetting(reportNotes,
	"Report MIDI &notes in MIDI editor",
	true)
#ifdef _WIN32
BoolSetting(editCursorFollowsEventListFocus,
	"Edit cursor follows focus in MIDI &event list",
	false)
#endif
BoolSetting(reportSurfaceChanges,
	"Report changes made via &control surfaces",
	false)
