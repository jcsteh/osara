/*
 * OSARA: Open Source Accessibility for the REAPER Application
 * UIAutomation code
 * Author: James Teh <jamie@jantrid.net>
 * Copyright 2019 NV Access Limited, James Teh
 * License: GNU General Public License version 2.0
 */

#include "osara.h"
#include <UIAutomation.h>
#include <ole2.h>

using namespace std;

HWND UIAWnd = 0;
const char* WINDOW_CLASS_NAME = "REAPEROSARANotificationWND";
typedef HRESULT(WINAPI *UiaRaiseNotificationEvent_funcType)(
	_In_ IRawElementProviderSimple* provider,
	NotificationKind notificationKind,
	NotificationProcessing notificationProcessing,
	_In_opt_ BSTR displayString,
	_In_ BSTR activityId
);
HMODULE UIAutomationCore = nullptr;
UiaRaiseNotificationEvent_funcType UiaRaiseNotificationEvent_ptr = nullptr;

// Provider code based on Microsoft's UIAutomationSimpleProvider example.
class UIAProviderImpl : public IRawElementProviderSimple
{
public:
	UIAProviderImpl(_In_ HWND hwnd): controlHWnd(hwnd) {}

	// IUnknown methods
	ULONG STDMETHODCALLTYPE AddRef() {
		return InterlockedIncrement(&m_refCount);
	}

	ULONG STDMETHODCALLTYPE Release() {
		long val = InterlockedDecrement(&m_refCount);
		if (val == 0) {
			delete this;
		}
		return val;
	}

	HRESULT STDMETHODCALLTYPE QueryInterface(_In_ REFIID riid, _Outptr_ void** ppInterface) {
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
	HRESULT STDMETHODCALLTYPE get_ProviderOptions(_Out_ ProviderOptions* pRetVal) {
		*pRetVal = ProviderOptions_ServerSideProvider | ProviderOptions_UseComThreading;
		return S_OK;
	}

	HRESULT STDMETHODCALLTYPE GetPatternProvider(PATTERNID patternId, _Outptr_result_maybenull_ IUnknown** pRetVal) {
		// We do not support any pattern.
		*pRetVal = NULL;
		return S_OK;
	}

	HRESULT STDMETHODCALLTYPE GetPropertyValue(PROPERTYID propertyId, _Out_ VARIANT* pRetVal) {
		switch (propertyId) {
			case UIA_ControlTypePropertyId:
				// Stop Narrator from ever speaking this as a window
				pRetVal->vt = VT_I4;
				pRetVal->lVal = UIA_CustomControlTypeId;
				break;
			case UIA_IsControlElementPropertyId:
			case UIA_IsContentElementPropertyId:
			case UIA_IsKeyboardFocusablePropertyId:
				pRetVal->vt = VT_BOOL;
				pRetVal->boolVal = VARIANT_FALSE;
				break;
			case UIA_ProviderDescriptionPropertyId:
				pRetVal->vt = VT_BSTR;
				pRetVal->bstrVal = SysAllocString(L"REAPER OSARA");
				break;
			default:
				pRetVal->vt = VT_EMPTY;
		}
		return S_OK;
	}

	HRESULT STDMETHODCALLTYPE get_HostRawElementProvider(IRawElementProviderSimple** pRetVal) {
		return UiaHostProviderFromHwnd(controlHWnd, pRetVal);
	}

private:
	virtual ~UIAProviderImpl() {}
	// Ref Counter for this COM object
	ULONG m_refCount;
	HWND controlHWnd; // The HWND for the control.
};

IRawElementProviderSimple* UIAProvider = nullptr;

LRESULT CALLBACK UIAWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch (msg) {
		case WM_GETOBJECT:
			if (static_cast<long>(lParam) == static_cast<long>(UiaRootObjectId)) {
				if (!UIAProvider) {
					UIAProvider = new UIAProviderImpl(UIAWnd);
				}
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
	UIAutomationCore = LoadLibraryA("UIAutomationCore.dll");
	if (!UIAutomationCore) {
		return false;
	}
	UiaRaiseNotificationEvent_ptr = (UiaRaiseNotificationEvent_funcType)GetProcAddress(UIAutomationCore, "UiaRaiseNotificationEvent");
	if (!UiaRaiseNotificationEvent_ptr) {
		return false;
	}
	windowClass = getWindowClass();
	if (!RegisterClassEx(&windowClass)) {
		return false;
	}
	UIAWnd = CreateWindowEx(
		0,
		WINDOW_CLASS_NAME,
		"Reaper OSARA Notifications",
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
	delete UIAProvider;
	if (!DestroyWindow(UIAWnd)) {
		return false;
	}
	if (!UnregisterClass(WINDOW_CLASS_NAME, pluginHInstance)) {
		return false;
	}
	UiaRaiseNotificationEvent_ptr = (UiaRaiseNotificationEvent_funcType)GetProcAddress(UIAutomationCore, "UiaRaiseNotificationEvent");
	if (UiaRaiseNotificationEvent_ptr) {
		UiaRaiseNotificationEvent_ptr = nullptr;
	}
	if (UIAutomationCore) {
		FreeLibrary(UIAutomationCore);
		UIAutomationCore = nullptr;
	}
	return true;
}

bool sendUIANotification(const string& message) {
	if (message.empty()) {
		return true; // Silently do not send empty messages as Narrator announces those.
	}
	return (UiaRaiseNotificationEvent_ptr(
		UIAProvider,
		NotificationKind_Other,
		NotificationProcessing_MostRecent,
		SysAllocString(widen(message).c_str()),
		SysAllocString(L"Reaper_OSARA")
	) == S_OK);
}
