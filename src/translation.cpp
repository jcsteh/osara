/*
 * OSARA: Open Source Accessibility for the REAPER Application
 * Translation code
 * Author: James Teh <jamie@jantrid.net>
 * Copyright 2021 James Teh
 * License: GNU General Public License version 2.0
 */

#include <string>
#include <fstream>
#include <tinygettext/dictionary.hpp>
#include <tinygettext/po_parser.hpp>
#include "osara.h"
#include <WDL/win32_utf8.h>

using namespace std;

tinygettext::Dictionary translationDict;

void initTranslation() {
	// Figure out which file name to load. We base it on the REAPER language
	// pack.
	char langpack[200];
	GetPrivateProfileString("REAPER", "langpack", "", langpack, sizeof(langpack),
		get_ini_file());
	if (langpack[0] == '\0' || langpack[0] == '<') {
		// No language pack.
		return;
	}
	// We can't use std::filesystem::path because it isn't supported until MacOS 10.15. Grrr!
	string name(langpack);
	// Replace .ReaperLangPack extension with .po.
	auto extPos = name.rfind(".");
	if (extPos == string::npos) {
		return;
	}
	name.erase(extPos);
	name += ".po";
	// OSARA translations are stored in osara/locale in the REAPER resource
	// directory.
	string path(GetResourcePath());
	path += "/osara/locale/";
	path += name;
	ifstream input(path);
	tinygettext::POParser::parse(path, input, translationDict);
}

BOOL CALLBACK translateWindow(HWND hwnd, LPARAM lParam) {
	auto context = (char*)lParam;
	char text[500] = "\0";
	GetWindowText(hwnd, text, sizeof(text));
	if (!text[0]) {
		return true;
	}
	string translated = translate_ctxt(context, text);
	if (translated == text) {
		// No translation.
		return true;
	}
	SetWindowText(hwnd, translated.c_str());
	return true;
}

void translateDialog(HWND dialog) {
	char context[100];
	// We use the caption of the dialog as the translation context.
	GetWindowText(dialog, context, sizeof(context));
	auto lParam = (LPARAM)context;
	// Translate dialog title.
	translateWindow(dialog, lParam);
	// Translate controls.
	EnumChildWindows(dialog, translateWindow, lParam);
}
