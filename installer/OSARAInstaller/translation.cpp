/*
 * OSARA Installer: Translation implementation
 * Copyright 2025 OSARA Project
 * License: GNU General Public License version 2.0
 */

#include <string>
#include <fstream>
#include <algorithm>
#include <tinygettext/po_parser.hpp>
#include <CoreFoundation/CoreFoundation.h>

#include "swell.h"
#include "installer_app.h"

#include "../../src/translation.h"

using namespace std;

// Use the same dictionary name as main OSARA for consistency
// Note: This is a separate instance for the installer since we use different locale detection
tinygettext::Dictionary translationDict;

void initTranslation() {
	// For the installer, we'll use system locale detection
	// since we don't have access to REAPER's language pack setting

	string localeCode;

	// Use the macOS system preferred-language list rather than $LANG, which
	// is typically unset (or "C") when the app is launched from Finder/Dock.
	CFArrayRef preferredLangs = CFLocaleCopyPreferredLanguages();
	if (preferredLangs && CFArrayGetCount(preferredLangs) > 0) {
		CFStringRef lang = (CFStringRef)CFArrayGetValueAtIndex(preferredLangs, 0);
		char buf[64] = {};
		if (CFStringGetCString(lang, buf, sizeof(buf), kCFStringEncodingUTF8)) {
			localeCode = buf;
			// CFLocale returns BCP 47 tags (e.g. "de-DE"); convert hyphens to
			// underscores to match the POSIX locale format we use for filenames.
			std::replace(localeCode.begin(), localeCode.end(), '-', '_');
		}
	}
	if (preferredLangs)
		CFRelease(preferredLangs);

	// macOS uses script subtags for Chinese (zh_Hans / zh_Hant) rather than
	// region codes (zh_CN / zh_TW).  Map them explicitly so the exact-match
	// and fallback logic below find the right .po file.
	if (localeCode == "zh_Hans" ||
	    localeCode.rfind("zh_Hans_", 0) == 0)
		localeCode = "zh_CN";
	else if (localeCode == "zh_Hant" ||
	         localeCode.rfind("zh_Hant_", 0) == 0)
		localeCode = "zh_TW";

	if (localeCode.empty()) {
		// Could not determine locale; fall back to English.
		return;
	}

	// Try to load the appropriate .po file
	// First try the full locale (e.g., "de_DE")
	string poFileName = localeCode + ".po";
	string resourcePath = g_installer ? g_installer->GetResourcePath() : "";
	string poPath;

	if (!resourcePath.empty()) {
		poPath = resourcePath + "/locale/" + poFileName;
	} else {
		poPath = "locale/" + poFileName;
	}

	ifstream input(poPath);
	if (!input.is_open()) {
		// Fall back to the base language code (e.g., "de" from "de_DE", or "de" as-is).
		// Derive the fallback filename by scanning kLocaleFiles rather than maintaining
		// a separate hardcoded map; adding a new locale to kLocaleFiles automatically
		// makes it available here without any second update.
		size_t underscorePos = localeCode.find('_');
		string langCode = (underscorePos != string::npos)
			? localeCode.substr(0, underscorePos)
			: localeCode;

		// "no" is the macOS BCP-47 tag for Norwegian; our file uses the more
		// specific "nb" subtag.  Map it explicitly before the general scan.
		const string searchCode = (langCode == "no") ? "nb" : langCode;

		string fallbackFileName;
		for (const string& locale : kLocaleFiles) {
			// Each entry looks like "de_DE.po" — extract the language prefix.
			size_t sep = locale.find('_');
			string fileLC = (sep != string::npos)
				? locale.substr(0, sep)
				: locale.substr(0, locale.find('.'));
			if (fileLC == searchCode) {
				fallbackFileName = locale;
				break;
			}
		}

		if (fallbackFileName.empty()) {
			// No matching language found; fall back to English.
			return;
		}
		poFileName = fallbackFileName;

		if (!resourcePath.empty()) {
			poPath = resourcePath + "/locale/" + poFileName;
		} else {
			poPath = "locale/" + poFileName;
		}

		input.open(poPath);
	}

	if (input.is_open()) {
		try {
			tinygettext::POParser::parse(poPath, input, translationDict);
		} catch (const exception& e) {
			// If parsing fails, just continue without translations
		}
	}
	// If we can't load any translation file, we'll just use English strings
}

// Helper function for translating individual windows (same as main OSARA)
BOOL CALLBACK translateWindow(HWND hwnd, LPARAM lParam) {
	const char* context = reinterpret_cast<const char*>(lParam);
	int len = GetWindowTextLength(hwnd);
	if (len <= 0)
		return true;
	string text(static_cast<size_t>(len) + 1, '\0');
	GetWindowText(hwnd, &text[0], len + 1);
	text.resize(static_cast<size_t>(len));
	string translated = translationDict.translate_ctxt(context, text.c_str());
	if (translated == text) {
		// No translation.
		return true;
	}
	SetWindowText(hwnd, translated.c_str());
	return true;
}

// Main dialog translation function (same approach as main OSARA)
void translateDialog(HWND dialog) {
	// We use the caption of the dialog as the translation context.
	int len = GetWindowTextLength(dialog);
	string context(static_cast<size_t>(len) + 1, '\0');
	GetWindowText(dialog, &context[0], len + 1);
	context.resize(static_cast<size_t>(len));
	auto lParam = reinterpret_cast<LPARAM>(context.c_str());
	// Translate dialog title.
	translateWindow(dialog, lParam);
	// Translate controls.
	EnumChildWindows(dialog, translateWindow, lParam);
}
