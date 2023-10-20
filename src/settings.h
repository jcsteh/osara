/*
 * OSARA: Open Source Accessibility for the REAPER Application
 * Setting definitions
 * Author: James Teh <jamie@jantrid.net>
 * Copyright 2022-2023 James Teh
 * License: GNU General Public License version 2.0
 */

/*
 * This file defines the various OSARA configuration settings. Most code that
 * wants to access settings should include config.h instead. This file is
 * included by config code which defines macros to perform an operation for
 * each setting; e.g. loading configuration.
 */

// Usage: BoolSetting(name, sectionId, displayName, defaultValue)
// sectionId identifies the section where the toggle action will appear.
BoolSetting(reportScrub, MAIN_SECTION, "Report position when &scrubbing", true)
BoolSetting(reportTimeMovementWhilePlaying, MAIN_SECTION, "Report time movement during playback/recording", true)
BoolSetting(reportFullTimeMovement, MAIN_SECTION, "Report f&ull time for time movement commands", false)
BoolSetting(
		moveFromPlayCursor, MAIN_SECTION, "&Move relative to the play cursor for time movement commands during playback",
		false
)
BoolSetting(reportMarkersWhilePlaying, MAIN_SECTION, "Report mar&kers and regions during playback", false)
BoolSetting(reportTransport, MAIN_SECTION, "Report &transport state (play, record, etc.)", true)
BoolSetting(reportTrackNumbers, MAIN_SECTION, "&Report track numbers", true)
BoolSetting(reportFx, MAIN_SECTION, "Report &FX when moving to tracks/takes", false)
BoolSetting(reportPositionMIDI, MIDI_EDITOR_SECTION, "Report &position when navigating chords in MIDI editor", true)
BoolSetting(reportNotes, MIDI_EDITOR_SECTION, "Report MIDI &notes in MIDI editor", true)
BoolSetting(reportSurfaceChanges, MAIN_SECTION, "Report changes made via &control surfaces", false)
