/*
 * OSARA: Open Source Accessibility for the REAPER Application
 * UIAutomation code
 * Author: James Teh <jamie@jantrid.net>
 * Copyright 2019 NV Access Limited, James Teh
 * License: GNU General Public License version 2.0
 */

#include "osara.h"
#include <ole2.h>

using namespace std;

HWND UIAWnd = 0;
const char* WINDOW_CLASS_NAME = "OSARA_UIA_WND";

// Provider code based on Microsoft's UIAutomationSimpleProvider example.
class UIAProviderImpl : public IRawElementProviderSimple
{
public:
	UIAProviderImpl(HWND hwnd) {}

	// IUnknown methods
	IFACEMETHODIMP_(ULONG) AddRef() {
		return InterlockedIncrement(&m_refCount);
	}

	IFACEMETHODIMP_(ULONG) Release() {
		long val = InterlockedDecrement(&m_refCount);
		if (val == 0) {
			delete this;
		}
		return val;
	}

	IFACEMETHODIMP QueryInterface(REFIID riid, void** ppInterface) {
		if (riid == __uuidof(IUnknown)) {
			*ppInterface =static_cast<IUnknown*>(this);
		} else if (riid == __uuidof(IRawElementProviderSimple)) {
			*ppInterface =static_cast<IRawElementProviderSimple*>(this);
		} else {
			*ppInterface = NULL;
			return E_NOINTERFACE;
		}
		(static_cast<IUnknown*>(*ppInterface))->AddRef();
		return S_OK;
	}

	// IRawElementProviderSimple methods
	IFACEMETHODIMP get_ProviderOptions(ProviderOptions * pRetVal) {
		*pRetVal = ProviderOptions_ServerSideProvider;
		return S_OK;
	}

	IFACEMETHODIMP GetPatternProvider(PATTERNID patternId, IUnknown** pRetVal) {
		// We do not support any pattern.
		*pRetVal = NULL;
		return S_OK;
	}

	IFACEMETHODIMP GetPropertyValue(PROPERTYID propertyId, VARIANT * pRetVal) {
		// We do not implement any property.
		pRetVal->vt = VT_EMPTY;
		return S_OK;
	}

	IFACEMETHODIMP get_HostRawElementProvider(IRawElementProviderSimple ** pRetVal) {
		return UiaHostProviderFromHwnd(m_controlHWnd, pRetVal);
	}

private:
	virtual ~UIAProviderImpl() {}
	// Ref Counter for this COM object
	ULONG m_refCount;
	HWND m_controlHWnd; // The HWND for the control.
};

IRawElementProviderSimple* UIAProvider = nullptr;

LRESULT CALLBACK UIAWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch (msg) {
		case WM_GETOBJECT:
			if (static_cast<long>(lParam) == static_cast<long>(UiaRootObjectId)) {
				return UiaReturnRawElementProvider(hwnd, wParam, lParam, UIAProvider);
			}
			return 0;
		default:
			return DefWindowProc(hwnd, msg, wParam, lParam);
	}
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
	UIAProvider = new UIAProviderImpl(UIAWnd);
	ShowWindow(UIAWnd, SW_SHOWNA);
	return true;;
}

bool trminateUIA() {
	ShowWindow(UIAWnd, SW_HIDE);
	delete UIAProvider;
	if (!DestroyWindow(UIAWnd)) {
		return false;
	}
	if (!UnregisterClass(WINDOW_CLASS_NAME, pluginHInstance)) {
		return false;
	}
	return true;
}
