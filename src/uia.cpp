/*
 * OSARA: Open Source Accessibility for the REAPER Application
 * UI Automation code
 * Copyright 2019-2023 Leonard de Ruijter, James Teh
 * License: GNU General Public License version 2.0
 */

#include <uiautomation.h>
#include <ole2.h>
#include <tlhelp32.h>
#include <atlcomcli.h>
#include <memory>
#include <utility>
#include "osara.h"
#include "config.h"

using namespace std;

HWND uiaWnd = nullptr;
const char* WINDOW_CLASS_NAME = "REAPEROSARANotificationWND";

// Some UIA functions aren't available in earlier versions of Windows, so we
// must fetch those at runtime. Otherwise, OSARA will fail to load. This class
// handles loading/freeing the dll and getting the required functions.
class UiaCore {
	private:
	HMODULE dll = LoadLibraryA("UIAutomationCore.dll");

	template<typename FuncType> FuncType* getFunc(const char* funcName) {
		return (FuncType*)GetProcAddress(this->dll, funcName);
	}

	public:
	~UiaCore() {
		FreeLibrary(this->dll);
	}

	decltype(UiaRaiseNotificationEvent)* RaiseNotificationEvent =
			getFunc<decltype(UiaRaiseNotificationEvent)>("UiaRaiseNotificationEvent");
	decltype(UiaDisconnectProvider)* DisconnectProvider =
			getFunc<decltype(UiaDisconnectProvider)>("UiaDisconnectProvider");
	decltype(UiaDisconnectAllProviders)* DisconnectAllProviders =
			getFunc<decltype(UiaDisconnectAllProviders)>("UiaDisconnectAllProviders");
};

unique_ptr<UiaCore> uiaCore;

// Provider code based on Microsoft's uiautomationSimpleProvider example.
class UiaProvider : public IRawElementProviderSimple {
	public:
	UiaProvider(_In_ HWND hwnd) : refCount(0), controlHWnd(hwnd) {}

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
			*ppInterface = static_cast<IRawElementProviderSimple*>(this);
		} else if (riid == __uuidof(IRawElementProviderSimple)) {
			*ppInterface = static_cast<IRawElementProviderSimple*>(this);
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
	virtual ~UiaProvider() {}

	ULONG refCount; // Ref Count for this COM object
	HWND controlHWnd; // The HWND for the control.
};

CComPtr<IRawElementProviderSimple> uiaProvider;

LRESULT CALLBACK uiaWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch (msg) {
		case WM_GETOBJECT:
			if (static_cast<long>(lParam) == static_cast<long>(UiaRootObjectId) && uiaProvider) {
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
	uiaCore = make_unique<UiaCore>();
	// If UiaRaiseNotificationEvent is available, UiaDisconnectProvider and
	// UiaDisconnectAllProviders will also be available, so we don't need to
	// check those.
	if (!uiaCore->RaiseNotificationEvent) {
		uiaCore = nullptr;
		return false;
	}
	windowClass = getWindowClass();
	if (!RegisterClassEx(&windowClass)) {
		return false;
	}
	uiaWnd = CreateWindowEx(
			// Make it transparent because it has to have width/height.
			WS_EX_TRANSPARENT, WINDOW_CLASS_NAME, "Reaper OSARA Notifications", WS_CHILD | WS_DISABLED, 0, 0,
			// UIA notifications fail if the window has 0 width/height.
			1, 1, mainHwnd, 0, pluginHInstance, nullptr
	);
	if (!uiaWnd) {
		return false;
	}
	ShowWindow(uiaWnd, SW_SHOWNA);
	// Constructor  initializes refcount to 0, assignment to a CComPtr
	// takes it to 1.
	uiaProvider = new UiaProvider(uiaWnd);
	return true;
}

bool terminateUia() {
	if (uiaProvider) {
		// Null out uiaProvider so it can't be returned by WM_GETOBJECT during
		// disconnection.
		CComPtr<IRawElementProviderSimple> tmpProv = std::move(uiaProvider);
		uiaProvider = nullptr;
		uiaCore->DisconnectProvider(tmpProv);
	}
	ShowWindow(uiaWnd, SW_HIDE);
	if (!DestroyWindow(uiaWnd)) {
		return false;
	}
	if (!UnregisterClass(WINDOW_CLASS_NAME, pluginHInstance)) {
		return false;
	}
	uiaCore->DisconnectAllProviders();
	uiaCore = nullptr;
	return true;
}

bool shouldUseUiaNotifications() {
	static const bool cachedResult = []() -> bool {
		if (!uiaWnd) {
			// Not available (requires Windows 10 fall creators update or above).
			return false;
		}
		const char setting = GetExtState(CONFIG_SECTION, "uiaNotificationEvents")[0];
		if (setting == '0') {
			return false; // Force disable.
		} else if (setting == '2') {
			return true; // Force enable.
		}
		// Setting not present or '1' means auto.
		// Several screen readers ignore or don't support UIA notification events.
		// First check for screen readers with in-process dlls.
		if (GetModuleHandleA("jhook.dll") // JAWS
				|| GetModuleHandleA("dolwinhk.dll") // Dolphin
		) {
			return false;
		}
		return true;
	}();
	return cachedResult;
}

bool sendUiaNotification(const string& message, bool interrupt) {
	if (!UiaClientsAreListening() || message.empty()) {
		return true;
	}
	return (
			uiaCore->RaiseNotificationEvent(
					uiaProvider, NotificationKind_Other,
					interrupt ? NotificationProcessing_MostRecent : NotificationProcessing_All,
					SysAllocString(widen(message).c_str()), SysAllocString(L"REAPER_OSARA")
			) == S_OK
	);
}

void resetUia() {
	ShowWindow(uiaWnd, SW_HIDE);
	ShowWindow(uiaWnd, SW_SHOWNA);
}
