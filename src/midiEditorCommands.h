/*
 * OSARA: Open Source Accessibility for the REAPER Application
 * MIDI Editor commands header
 * Author: James Teh <jamie@jantrid.net>
 * Copyright 2015-2023 NV Access Limited, James Teh
 * License: GNU General Public License version 2.0
 */

#include "osara.h"

const int MIDI_EDITOR_SECTION = 32060;

// Stops the notes currently being previewed.
// If sendNoteOff is true, note off messages are sent immediately.
// This is used to silence the preview.
// If sendNoteOff is false, note off messages are queued but not yet sent.
// This is used when stopping a note preview that is immediately followed by a new preview.
void previewNotesOff(bool sendNoteOff = true);

// This must be called when playback starts, as otherwise, pending note off
// messages for OSARA MIDI preview might interfere with MIDI playback.
// It must also be called when canceling MIDI note preview explicitly, e.g. when not to wait on the timer to elapse.
// Returns true when previewDoneTimer was set at the time of calling the function, false otherwise.
bool cancelPendingMidiPreviewNotesOff();

int getItemPPQ(MediaItem* item);

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
void postMidiSelectEvents(int command);
void cmdMidiToggleSelCC(Command* command);
void cmdMidiMoveToNextCC(Command* command);
void cmdMidiMoveToPreviousCC(Command* command);
void cmdMidiMoveToNextCCKeepSel(Command* command);
void cmdMidiMoveToPreviousCCKeepSel(Command* command);
void cmdMidiMoveToNextItem(Command* command);
void cmdMidiMoveToPrevItem(Command* command);
void cmdMidiMoveToTrack(Command* command);
void cmdMidiSelectSamePitchStartingInTimeSelection(Command* command);
void cmdMidiNoteSplitOrJoin(Command* command);
#ifdef _WIN32
void cmdFocusNearestMidiEvent(Command* command);
void cmdMidiFilterWindow(Command* command);
void maybeHandleEventListItemFocus(HWND hwnd, long childId);
#endif

void postMidiChangeVelocity(int command);
void postMidiChangeLength(int command);
void postMidiChangePitch(int command);
void postMidiMoveStart(int command);
void postMidiChangeCCValue(int command);
void postMidiSwitchCCLane(int command);
void postToggleMidiInputsAsStepInput(int command);
void postToggleFunctionKeysAsStepInput(int command);
void postMidiToggleSnap(int command);
void postMidiChangeZoom(int command);
