/*
 * OSARA: Open Source Accessibility for the REAPER Application
 * Translation header
 * Author: James Teh <jamie@jantrid.net>
 * Copyright 2021-2023 James Teh
 * License: GNU General Public License version 2.0
 */

#pragma once

#include <string>
#include <tinygettext/dictionary.hpp>
#include <fmt/core.h>
#include <fmt/format.h>

extern tinygettext::Dictionary translationDict;

void initTranslation();
void translateDialog(HWND dialog);

template<typename S> auto translate(S msg) {
	return translationDict.translate(msg);
}

template<typename S> auto translate_ctxt(S context, S msg) {
	return translationDict.translate_ctxt(context, msg);
}

template<typename S, typename N> auto translate_plural(S msg, S msgPlural, N num) {
	return translationDict.translate_plural(msg, msgPlural, num);
}

// This function is used to mark a string as translatable without actually
// translating it. This is useful for strings in compile time data structures.
constexpr auto _t(auto msg) {
	return msg;
}

// Catch exceptions from fmt::format due to errors in translations so we can
// fail gracefully instead of crashing.
template<typename FormatStr, typename... Args> std::string format(FormatStr&& formatStr, Args&&... args) {
	try {
		return fmt::format(formatStr, std::forward<Args>(args)...);
	} catch (fmt::format_error) {
		return fmt::format("error in format string: {}", formatStr);
	}
}
