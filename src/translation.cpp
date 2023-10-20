/*
 * OSARA: Open Source Accessibility for the REAPER Application
 * Translation code
 * Author: James Teh <jamie@jantrid.net>
 * Copyright 2021-2023 James Teh
 * License: GNU General Public License version 2.0
 */

#include <string>
#include <fstream>
#include <map>
#include <tinygettext/dictionary.hpp>
#include <tinygettext/po_parser.hpp>
#include "osara.h"
#include <WDL/win32_utf8.h>
#include "translation.h"

using namespace std;

// Maps REAPER language pack names to locale codes used by OSARA. There can
// be (and often are) multiple REAPER language packs per language.
map<string, string> REAPER_LANG_TO_CODE = {
		{"DE_(+SWS)", "de_DE"},					{"pt-BR", "pt_BR"},
		{"Reaper+SWS_CHSDOU", "zh_CN"}, {"REAPER_zh_CN_www.szzyyzz.com", "zh_CN"},
		{"REAPER_SWS_french", "fr_FR"}, {"Reaper5965_fr_sws_wip", "fr_FR"},
		{"REAPER_SWS_FRC", "fr_CA"},		{"Russian", "ru_RU"},
		{"Turkish", "tr_TR"},
};

tinygettext::Dictionary translationDict;

void initTranslation() {
	// Figure out which file name to load. We base it on the REAPER language
	// pack.
	char langpack[200];
	GetPrivateProfileString("REAPER", "langpack", "", langpack, sizeof(langpack), get_ini_file());
	if (langpack[0] == '\0' || langpack[0] == '<') {
		// No language pack.
		return;
	}
	// We can't use std::filesystem::path because it isn't supported until MacOS 10.15. Grrr!
	string name(langpack);
	// Strip .ReaperLangPack extension.
	auto extPos = name.rfind(".");
	if (extPos == string::npos) {
		return;
	}
	name.erase(extPos);
	// Check if we've mapped this REAPER language pack to a locale.
	const auto nameIt = REAPER_LANG_TO_CODE.find(name);
	if (nameIt != REAPER_LANG_TO_CODE.end()) {
		name = nameIt->second;
	}
	name += ".po";
	// OSARA translations are stored in osara/locale in the REAPER resource
	// directory.
	string path(GetResourcePath());
	path += "/osara/locale/";
	path += name;
#ifdef _WIN32
	// REAPER provides UTF-8 strings. However, on Windows, ifstream will
	// interpret a narrow (8 bit) string as an ANSI string. The easiest way to
	// deal with this is to convert the string to UTF-16, which Windows will
	// interpret correctly.
	ifstream input(widen(path));
#else
	ifstream input(path);
#endif
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
