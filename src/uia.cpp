/*
 * OSARA: Open Source Accessibility for the REAPER Application
 * UI Automation code
 * Copyright 2019-2020 Leonard de Ruijter, James Teh
 * License: GNU General Public License version 2.0
 */

#include <uiautomation.h>
#include <ole2.h>
#include <atlcomcli.h>
#include <optional>
#include "osara.h"

using namespace std;

HWND uiaWnd = nullptr;
const char* WINDOW_CLASS_NAME = "REAPEROSARANotificationWND";
typedef HRESULT(WINAPI *UiaRaiseNotificationEvent_funcType)(
	_In_ IRawElementProviderSimple* provider,
	NotificationKind notificationKind,
	NotificationProcessing notificationProcessing,
	_In_opt_ BSTR displayString,
	_In_ BSTR activityId
);
HMODULE uiAutomationCore = nullptr;
UiaRaiseNotificationEvent_funcType uiaRaiseNotificationEvent_ptr = nullptr;

// Provider code based on Microsoft's uiautomationSimpleProvider example.
class UiaProviderImpl : public IRawElementProviderSimple {
	public:
	UiaProviderImpl(_In_ HWND hwnd): controlHWnd(hwnd) {}

	// IUnknown methods
	ULONG STDMETHODCALLTYPE AddRef() {
		return InterlockedIncrement(&refCount);
	}

	ULONG STDMETHODCALLTYPE Release() {
		long val = InterlockedDecrement(&refCount);
		if (val == 0) {
			delete this;
		}
		return val;
	}

	HRESULT STDMETHODCALLTYPE QueryInterface(_In_ REFIID riid, _Outptr_ void** ppInterface) {
		if (ppInterface) {
			return E_INVALIDARG;
		}
		if (riid == __uuidof(IUnknown)) {
			*ppInterface =static_cast<IRawElementProviderSimple*>(this);
		} else if (riid == __uuidof(IRawElementProviderSimple)) {
			*ppInterface =static_cast<IRawElementProviderSimple*>(this);
		} else {
			*ppInterface = nullptr;
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
	virtual ~UiaProviderImpl() {
		UiaDisconnectProvider(this);
	}

	ULONG refCount; // Ref Count for this COM object
	HWND controlHWnd; // The HWND for the control.
};

CComPtr<IRawElementProviderSimple> uiaProvider;

LRESULT CALLBACK uiaWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch (msg) {
		case WM_GETOBJECT:
			if (static_cast<long>(lParam) == static_cast<long>(UiaRootObjectId)) {
				return UiaReturnRawElementProvider(hwnd, wParam, lParam, uiaProvider);
			}
			return 0;
		default:
			return DefWindowProc(hwnd, msg, wParam, lParam);
	}
}

WNDCLASSEX getWindowClass() {
	WNDCLASSEX windowClass = {};
	windowClass.cbSize = sizeof(WNDCLASSEX);
	windowClass.lpfnWndProc = uiaWndProc;
	windowClass.hInstance = pluginHInstance;
	windowClass.lpszClassName = WINDOW_CLASS_NAME;
	return windowClass;
}

WNDCLASSEX windowClass;

bool initializeUia() {
	uiAutomationCore = LoadLibraryA("UIAutomationCore.dll");
	if (!uiAutomationCore) {
		return false;
	}
	uiaRaiseNotificationEvent_ptr = (UiaRaiseNotificationEvent_funcType)GetProcAddress(uiAutomationCore, "UiaRaiseNotificationEvent");
	if (!uiaRaiseNotificationEvent_ptr) {
		return false;
	}
	windowClass = getWindowClass();
	if (!RegisterClassEx(&windowClass)) {
		return false;
	}
	uiaWnd = CreateWindowEx(
		// Make it transparent because it has to have width/height.
		WS_EX_TRANSPARENT,
		WINDOW_CLASS_NAME,
		"Reaper OSARA Notifications",
		WS_CHILD | WS_DISABLED,
		0,
		0,
		// UIA notifications fail if the window has 0 width/height.
		1,
		1,
		mainHwnd,
		0,
		pluginHInstance,
		nullptr
	);
	if (!uiaWnd) {
		return false;
	}
	ShowWindow(uiaWnd, SW_SHOWNA);
	uiaProvider = new UiaProviderImpl(uiaWnd);
	return true;
}

bool terminateUia() {
	if (uiaProvider) {
		uiaProvider = nullptr;
	}
	ShowWindow(uiaWnd, SW_HIDE);
	if (!DestroyWindow(uiaWnd)) {
		return false;
	}
	if (!UnregisterClass(WINDOW_CLASS_NAME, pluginHInstance)) {
		return false;
	}
	UiaDisconnectAllProviders();
	uiaRaiseNotificationEvent_ptr = nullptr;
	if (uiAutomationCore) {
		FreeLibrary(uiAutomationCore);
		uiAutomationCore = nullptr;
	}
	return true;
}

bool shouldUseUiaNotifications() {
	static optional<bool> cachedResult;
	if (!cachedResult) {
		// Don't use for JAWS because JAWS ignores these events in REAPER.
		cachedResult.emplace(!GetModuleHandleA("jhook.dll"));
	}
	return *cachedResult;
}

bool sendUiaNotification(const string& message, bool interrupt) {
	if (!UiaClientsAreListening() || message.empty()) {
		return true;
	}
	return (uiaRaiseNotificationEvent_ptr(
		uiaProvider,
		NotificationKind_Other,
		interrupt ? NotificationProcessing_MostRecent : NotificationProcessing_All,
		SysAllocString(widen(message).c_str()),
		SysAllocString(L"REAPER_OSARA")
	) == S_OK);
}
