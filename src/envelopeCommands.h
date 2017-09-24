/*
 * OSARA: Open Source Accessibility for the REAPER Application
 * Envelope commands header
 * Author: James Teh <jamie@jantrid.net>
 * Copyright 2015-2017 NV Access Limited, James Teh
 * License: GNU General Public License version 2.0
 */

#include "osara.h"

void postMoveEnvelopePoint(int command);
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
