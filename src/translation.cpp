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
