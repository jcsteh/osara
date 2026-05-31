# OSARA: Open Source Accessibility for the REAPER Application
# Shared helpers for translatable NSIS installer strings
# Copyright 2026 James Teh
# License: GNU General Public License version 2.0

import re


RE_INSTALLER_STRING = re.compile(
	r'^\s*!insertmacro OSARA_LANG_STRING (?P<name>\w+) "(?P<msgid>.*)"\s*$'
)

RE_LITERAL_DOLLAR = re.compile(
	r'\$(?!\\[rnt"]|\$|INSTDIR\b)'
)


def unescapeNsiString(text):
	return text.replace(r'$\"', '"')


def escapeNsiString(text):
	text = text.replace("\r\n", "\n").replace("\r", "\n").replace("\n", r'$\r$\n')
	text = RE_LITERAL_DOLLAR.sub("$$", text)
	return text.replace('"', r'$\"')
