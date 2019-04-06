/*
 * OSARA: Open Source Accessibility for the REAPER Application
 * UIAutomation code
 * Author: James Teh <jamie@jantrid.net>
 * Copyright 2019 NV Access Limited, James Teh
 * License: GNU General Public License version 2.0
 */

#include "osara.h"
#include <UIAutomation.h>

HWND UIAWnd = 0;
const char* WINDOW_CLASS_NAME = "OSARA_UIA_WND";

LRESULT CALLBACK UIAWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	return DefWindowProc(hwnd, msg, wParam, lParam);
}

WNDCLASSEX getWindowClass() {
	WNDCLASSEX windowClass = {};
	windowClass.cbSize = sizeof(WNDCLASSEX);
	windowClass.lpfnWndProc = UIAWndProc;
	windowClass.hInstance = pluginHInstance;
	windowClass.lpszClassName = WINDOW_CLASS_NAME;
	return windowClass;
}

WNDCLASSEX windowClass;

bool initializeUIA() {
	windowClass = getWindowClass();
	if (!RegisterClassEx(&windowClass)) {
		return false;
	}
	HWND UIAWnd = CreateWindowEx(
		0,
		WINDOW_CLASS_NAME,
		NULL,
		WS_DISABLED,
		CW_USEDEFAULT,
		SW_SHOWNA,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		mainHwnd,
		0,
		pluginHInstance,
		nullptr
	);
	if (!UIAWnd) {
		return false;
	}
	ShowWindow(UIAWnd, SW_SHOWNA);
	return true;;
}


bool trminateUIA() {
	ShowWindow(UIAWnd, SW_HIDE);
	if (!DestroyWindow(UIAWnd)) {
		return false;
	}
	if (!UnregisterClass(WINDOW_CLASS_NAME, pluginHInstance)) {
		return false;
	}
	return true;
}
