/*
 * OSARA Installer: Translation implementation
 * Copyright 2025 OSARA Project
 * License: GNU General Public License version 2.0
 */

#include <string>
#include <fstream>
#include <tinygettext/po_parser.hpp>

#include "swell.h"
#include <mach-o/dyld.h>

#include "../../src/translation.h"

using namespace std;

// Forward declaration
string GetResourcePath();

// Use the same dictionary name as main OSARA for consistency
// Note: This is a separate instance for the installer since we use different locale detection
extern tinygettext::Dictionary translationDict;
tinygettext::Dictionary translationDict;

void initTranslation() {
	// For the installer, we'll use system locale detection
	// since we don't have access to REAPER's language pack setting
	
	string localeCode;
	
	// Get macOS system locale
	const char* lang = getenv("LANG");
	if (lang) {
		string locale(lang);
		// Extract just the language_COUNTRY part (before any dot or @)
		size_t dotPos = locale.find('.');
		if (dotPos != string::npos) {
			locale = locale.substr(0, dotPos);
		}
		size_t atPos = locale.find('@');
		if (atPos != string::npos) {
			locale = locale.substr(0, atPos);
		}
		localeCode = locale;
	}
	
	if (localeCode.empty() || localeCode == "C" || localeCode == "POSIX") {
		// No locale or default C locale, use English (no translation needed)
		return;
	}
	
	// Try to load the appropriate .po file
	// First try the full locale (e.g., "de_DE")
	string poFileName = localeCode + ".po";
	string resourcePath = GetResourcePath();
	string poPath;
	
	if (!resourcePath.empty()) {
		poPath = resourcePath + "/locale/" + poFileName;
	} else {
		poPath = "locale/" + poFileName;
	}
	
	ifstream input(poPath);
	if (!input.is_open()) {
		// Try just the language part (e.g., "de" from "de_DE")
		size_t underscorePos = localeCode.find('_');
		if (underscorePos != string::npos) {
			string langCode = localeCode.substr(0, underscorePos);
			// Map common language codes to our available locales
			if (langCode == "de") poFileName = "de_DE.po";
			else if (langCode == "es") poFileName = "es_ES.po";
			else if (langCode == "fr") poFileName = "fr_FR.po";
			else if (langCode == "pt") poFileName = "pt_BR.po";
			else if (langCode == "ru") poFileName = "ru_RU.po";
			else if (langCode == "tr") poFileName = "tr_TR.po";
			else if (langCode == "zh") poFileName = "zh_CN.po";
			else if (langCode == "nb" || langCode == "no") poFileName = "nb_NO.po";
			else {
				// No matching language, use English
				return;
			}
			
			if (!resourcePath.empty()) {
				poPath = resourcePath + "/locale/" + poFileName;
			} else {
				poPath = "locale/" + poFileName;
			}
			
			input.open(poPath);
		}
	}
	
	if (input.is_open()) {
		try {
			tinygettext::POParser::parse(poPath, input, translationDict);
		} catch (const exception& e) {
			// If parsing fails, just continue without translations
			// Could log this error in a real implementation
		}
	}
	// If we can't load any translation file, we'll just use English strings
}

// Helper function for translating individual windows (same as main OSARA)
BOOL CALLBACK translateWindow(HWND hwnd, LPARAM lParam) {
	auto context = (char*)lParam;
	char text[500] = "\0";
	GetWindowText(hwnd, text, sizeof(text));
	if (!text[0]) {
		return true;
	}
	string translated = translationDict.translate_ctxt(context, text);
	if (translated == text) {
		// No translation.
		return true;
	}
	SetWindowText(hwnd, translated.c_str());
	return true;
}

// Main dialog translation function (same approach as main OSARA)
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

// Helper function to get resource path (reused from installer_app.cpp)
string GetResourcePath() {
	// On macOS, resources are in the app bundle
	// Get the path to the current executable
	char path[1024];
	uint32_t size = sizeof(path);
	if (_NSGetExecutablePath(path, &size) == 0) {
		string exePath(path);
		
		// Navigate from Contents/MacOS/OSARAInstaller to Contents/Resources
		size_t pos = exePath.find("/Contents/MacOS/");
		if (pos != string::npos) {
			return exePath.substr(0, pos) + "/Contents/Resources";
		}
	}
	
	return "../OSARAInstaller/Resources"; // Fallback for command line execution
}
