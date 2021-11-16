/*
 * OSARA: Open Source Accessibility for the REAPER Application
 * Translation header
 * Author: James Teh <jamie@jantrid.net>
 * Copyright 2021 James Teh
 * License: GNU General Public License version 2.0
 */

#pragma once

#include <string>
#include <tinygettext/dictionary.hpp>
#include <fmt/core.h>
#include <fmt/format.h>

extern tinygettext::Dictionary translationDict;

#define translate(msg) translationDict.translate(msg)
#define translate_ctxt(context, msg) \
	translationDict.translate_ctxt(context, msg)
#define translate_plural(msg, msgPlural, num) \
	translationDict.translate_plural(msg, msgPlural, num)

// Catch exceptions from fmt::format due to errors in translations so we can
// fail gracefully instead of crashing.
template<typename FormatStr, typename... Args>
std::string format(FormatStr&& formatStr, Args&&... args) {
	try {
		return fmt::format(formatStr, std::forward<Args>(args)...);
	} catch (fmt::format_error) {
		return fmt::format("error in format string: {}", formatStr);
	}
}
