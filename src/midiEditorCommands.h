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
void postMidiMovePitchCursor(int command);
void cmdMidiInsertNote(Command* command);
void cmdMidiDeleteEvents(Command* command);
void postMidiSelectNotes(int command);
void cmdMidiMoveToNextCC(Command* command);
void cmdMidiMoveToPreviousCC(Command* command);
void cmdMidiMoveToNextCCKeepSel(Command* command);
void cmdMidiMoveToPreviousCCKeepSel(Command* command);
void cmdMidiMoveToNextItem(Command* command) ;
void cmdMidiMoveToPrevItem(Command* command) ;
void cmdMidiMoveToTrack(Command* command);
void cmdMidiSelectSamePitchStartingInTimeSelection(Command* command) ;
void cmdMidiNoteSplitOrJoin(Command* command);
#ifdef _WIN32
void cmdFocusNearestMidiEvent(Command* command);
void cmdMidiFilterWindow(Command* command);
#endif

void postMidiChangeVelocity(int command);
void postMidiChangeLength(int command);
void postMidiChangePitch(int command);
void postMidiMoveStart(int command);
void postMidiChangeCCValue(int command);
void postMidiSwitchCCLane(int command);

// This should be called when playback starts, as otherwise, pending note off
// messages for OSARA MIDI preview might interfere with MIDI playback.
void cancelMidiPreviewNotesOff();
