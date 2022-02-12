/*
 * OSARA: Open Source Accessibility for the REAPER Application
 * Envelope commands header
 * Author: James Teh <jamie@jantrid.net>
 * Copyright 2015-2022 NV Access Limited, James Teh
 * License: GNU General Public License version 2.0
 */
#include <optional>
#include "osara.h"

// Keeps track of the automation item the user last moved to.
// -1 means no automation item,
// which means use the envelope itself for stuff related to envelope points.
extern int currentAutomationItem;
int getEnvelopePointAtCursor();
void postMoveEnvelopePoint(int command);
int countEnvelopePointsIncludingAutoItems(TrackEnvelope* envelope);
void cmdhDeleteEnvelopePointsOrAutoItems(int command, bool checkPoints=true, bool checkItems=true);
void cmdDeleteEnvelopePoints(Command* command);
void cmdInsertEnvelopePoint(Command* command);
void cmdSelectNextEnvelope(Command* command);
void cmdSelectPreviousEnvelope(Command* command);
void cmdMoveToNextEnvelopePoint(Command* command);
void cmdMoveToPrevEnvelopePoint(Command* command);
void cmdMoveToNextEnvelopePointKeepSel(Command* command);
void cmdMoveToPrevEnvelopePointKeepSel(Command* command);
void moveToAutomationItem(int direction, bool clearSelection=true, bool select=true);
bool toggleCurrentAutomationItemSelection();
std::optional <bool> toggleCurrentEnvelopePointSelection();
void reportCopiedEnvelopePointsOrAutoItems();
void postToggleTrackVolumeEnvelope(int command);
void postToggleTrackPanEnvelope(int command);
void cmdToggleTrackEnvelope(Command* command);
void postSelectMultipleEnvelopePoints(int command);
void cmdMoveSelEnvelopePoints(Command* command);
