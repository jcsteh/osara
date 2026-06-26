/*
 * OSARA: Open Source Accessibility for the REAPER Application
 * MIDI Editor commands header
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
void previewNotesOff(bool sendNoteOff=true);

// This must be called when playback starts, as otherwise, pending note off
// messages for OSARA MIDI preview might interfere with MIDI playback.
// It must also be called when canceling MIDI note preview explicitly, e.g. when not to wait on the timer to elapse.
// Returns true when previewDoneTimer was set at the time of calling the function, false otherwise.
bool cancelPendingMidiPreviewNotesOff();

int getItemPPQ(MediaItem* item);

void cmdMidiMoveCursor(int command);
void cmdMidiToggleSelection(int command);
void cmdMidiMoveToNextChord(int command);
void cmdMidiMoveToPreviousChord(int command);
void cmdMidiMoveToNextChordKeepSel(int command);
void cmdMidiMoveToPreviousChordKeepSel(int command);
void cmdMidiMoveToHigherNoteInChord(int command);
void cmdMidiMoveToLowerNoteInChord(int command);
void cmdMidiMoveToHigherNoteInChordKeepSel(int command);
void cmdMidiMoveToLowerNoteInChordKeepSel(int command);
void postMidiMovePitchCursor(int command);
void cmdMidiInsertCC(int command);
void cmdMidiInsertNote(int command);
void cmdMidiDeleteEvents(int command);
void cmdMidiPasteEvents(int command);
void postMidiCopyEvents(int command);
void postMidiSelectNotes(int command);
void postMidiSelectCCs(int command);
void postMidiSelectEvents(int command);
void cmdMidiToggleSelCC (int command) ;
void cmdMidiMoveToNextCC(int command);
void cmdMidiMoveToPreviousCC(int command);
void cmdMidiMoveToNextCCKeepSel(int command);
void cmdMidiMoveToPreviousCCKeepSel(int command);
void cmdMidiMoveToNextItem(int command) ;
void cmdMidiMoveToPrevItem(int command) ;
void cmdMidiMoveToTrack(int command);
void cmdMidiSelectSamePitchStartingInTimeSelection(int command) ;
void cmdMidiNoteSplitOrJoin(int command);
#ifdef _WIN32
void cmdFocusNearestMidiEvent(int command);
void cmdMidiFilterWindow(int command);
void maybeHandleEventListItemFocus(HWND hwnd, long childId);
void toggleListViewItemSelection(HWND list);
#endif

void postMidiChangeVelocity(int command);
void postMidiChangeLength(int command);
void postMidiChangePitch(int command);
void postMidiMovePosition(int command);
void postMidiMoveStart(int command);
void postMidiChangeCCValue(int command);
void postMidiSwitchCCLane(int command);
void postToggleMidiInputsAsStepInput(int command);
void postToggleFunctionKeysAsStepInput(int command);
void postMidiToggleMute(int command);
void postMidiToggleSnap(int command);
void postMidiChangeZoom(int command);
int countSelectedEvents(MediaItem_Take* take);
const std::string getMidiNoteName(MediaTrack* track, int pitch, int channel);
int midiStepTranslateAccel(MSG* msg, accelerator_register_t* accelReg);
