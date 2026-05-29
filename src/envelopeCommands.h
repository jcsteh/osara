/*
 * OSARA: Open Source Accessibility for the REAPER Application
 * Envelope commands header
 * Copyright 2015-2023 NV Access Limited, James Teh
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
void cmdDeleteEnvelopePoints(int command);
void cmdInsertEnvelopePoint(int command);
void cmdSelectNextEnvelope(int command);
void cmdSelectPreviousEnvelope(int command);
void cmdMoveToNextEnvelopePoint(int command);
void cmdMoveToPrevEnvelopePoint(int command);
void cmdMoveToNextEnvelopePointKeepSel(int command);
void cmdMoveToPrevEnvelopePointKeepSel(int command);
void moveToAutomationItem(int direction, bool clearSelection=true, bool select=true);
bool toggleCurrentAutomationItemSelection();
std::optional <bool> toggleCurrentEnvelopePointSelection();
void reportCopiedEnvelopePointsOrAutoItems();
void cmdToggleTrackEnvelope(int command);
void cmdToggleTakeEnvelope(int command);
void postSelectMultipleEnvelopePoints(int command);
void cmdMoveSelEnvelopePoints(int command);
void cmdCycleEnvelopePointShape(int command);
void cmdToggleVolumeEnvelope(int command);
void cmdTogglePanEnvelope(int command);
void cmdToggleMuteEnvelope(int command);
void cmdTogglePreFXPanOrTakePitchEnvelope(int command);
void cmdToggleLastTouchedEnvelope(int command);
void cmdInsertAutoItem(int command);
void cmdDeleteAutoItems(int command);
void cmdAddAutoItems(int command);
void cmdGlueAutoItems(int command);
