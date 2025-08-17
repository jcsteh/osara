# OSARA: Open Source Accessibility for the REAPER Application
# Utility to build Configuration dialog resource
# Copyright 2022-2023 James Teh
# License: GNU General Public License version 2.0

import re

RE_BOOL_SETTING = re.compile(r'^BoolSetting\(\s*(?P<name>[^,]+),\s*(?P<sectionId>[^,]+),\s*(?P<displayName>"[^"]+"),\s*(?P<default>true|false)\s*\)$')
def makeConfigRc(target, source, env):
	out = open(target[0].path, "wt", encoding="UTF-8")
	resourceH = open(source[0].path, "rt", encoding="UTF-8")
	for line in resourceH:
		if line.startswith("#define ID_CONFIG_DLG "):
			cid = int(line.strip().rsplit(" ")[-1])
			break
	out.write(
"""#include <windows.h>
{cid} DIALOGEX 250, 125, 500, 500
	CAPTION "OSARA Configuration"
BEGIN
""".format(cid=cid))
	y = 6
	settingsH = open(source[1].path, "rt", encoding="UTF-8")
	setting = None
	for line in settingsH:
		line = line.strip()
		if line.startswith("BoolSetting("):
			setting = ""
		elif setting is None:
			continue
		setting += line
		if not line.endswith(")"):
			continue # Setting is split across multiple lines.
		m = RE_BOOL_SETTING.match(setting)
		cid += 1
		out.write(
			'\tCONTROL {displayName}, {cid}, "Button", BS_AUTOCHECKBOX | WS_TABSTOP, 10, {y}, 490, 14\n'
			.format(displayName=m.group("displayName"), cid=cid, y=y))
		y += 20
	out.write('\tDEFPUSHBUTTON "OK", IDOK, 10, {y}, 30, 14\n'
		'\tPUSHBUTTON "Cancel", IDCANCEL, 137, {y}, 40, 14\n'
		.format(y=y))
	out.write('END\n')
