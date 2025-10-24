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
			cid = dialogCid = int(line.strip().rsplit(" ")[-1])
			break
	lines = []
	lines.append("#include <windows.h>\n")
	# We can't generate the DIALOGEX header yet because we don't know the height
	# yet. Add a placeholder here and generate that below.
	lines.append(None)
	lines.append('\tCAPTION "OSARA Configuration"\n')
	lines.append('BEGIN\n')
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
		displayName = m.group("displayName")
		lines.append(
			f'\tCONTROL {displayName}, {cid}, "Button", BS_AUTOCHECKBOX | WS_TABSTOP, 10, {y}, 390, 14\n')
		y += 20
	lines.append(f'\tDEFPUSHBUTTON "OK", IDOK, 10, {y}, 30, 14\n'
		f'\tPUSHBUTTON "Cancel", IDCANCEL, 137, {y}, 40, 14\n')
	y += 20
	# We know the height now. Generate the DIALOGEX header.
	lines[1] = f"{dialogCid} DIALOGEX 250, 125, 400, {y}"
	lines.append('END\n')
	out.writelines(lines)
