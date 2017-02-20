/*
 * OSARA: Open Source Accessibility for the REAPER Application
 * MIDI Editor commands header
 * Author: James Teh <jamie@nvaccess.org>
 * Copyright 2015-2017 NV Access Limited
 * License: GNU General Public License version 2.0
 */

#include "osara.h"

void cmdMidiMoveCursor(Command* command);
void cmdMidiMoveToNextChord(Command* command);
void cmdMidiMoveToPreviousChord(Command* command);
void cmdMidiMoveToNextNoteInChord(Command* command);
void cmdMidiMoveToPreviousNoteInChord(Command* command);
void cmdMidiMoveToNote(Command* command);
void cmdMidiMovePitchCursor(Command* command);
void cmdMidiDeleteEvents(Command* command);
#ifdef _WIN32
void cmdFocusNearestMidiEvent(Command* command);
#endif
