/*
 * OSARA: Open Source Accessibility for the REAPER Application
 * MIDI Editor commands header
 * Author: James Teh <jamie@jantrid.net>
 * Copyright 2015-2017 NV Access Limited
 * License: GNU General Public License version 2.0
 */

#include "osara.h"

void cmdMidiMoveCursor(Command* command);
void cmdMidiToggleSelection(Command* command);
void cmdMidiMoveToNextChord(Command* command);
void cmdMidiMoveToPreviousChord(Command* command);
void cmdMidiMoveToNextChordKeepSel(Command* command);
void cmdMidiMoveToPreviousChordKeepSel(Command* command);
void cmdMidiMoveToNextNoteInChord(Command* command);
void cmdMidiMoveToPreviousNoteInChord(Command* command);
void cmdMidiMoveToNextNoteInChordKeepSel(Command* command);
void cmdMidiMoveToPreviousNoteInChordKeepSel(Command* command);
void cmdMidiMovePitchCursor(Command* command);
void cmdMidiInsertNote(Command* command);
void cmdMidiDeleteEvents(Command* command);
void cmdMidiSelectNotes(Command* command);
void cmdMidiMoveToNextItem(Command* command) ;
void cmdMidiMoveToPrevItem(Command* command) ;
void cmdMidiMoveToTrack(Command* command);
#ifdef _WIN32
void cmdFocusNearestMidiEvent(Command* command);
void cmdMidiFilterWindow(Command* command);
#endif

void cmdMidiMoveToNextCC(Command* command);
void cmdMidiMoveToPreviousCC(Command* command);
void cmdMidiMoveToNextCCKeepSel(Command* command);
void cmdMidiMoveToPreviousCCKeepSel(Command* command);
