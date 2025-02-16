/*
 * OSARA: Open Source Accessibility for the REAPER Application
 * UI Automation header
 * Copyright 2019-2025 Leonard de Ruijter, James Teh
 * License: GNU General Public License version 2.0
 */

#pragma once

#include <uiautomation.h>

bool initializeUia();
bool hasTriedToInitializeUia();
bool terminateUia();
bool shouldUseUiaNotifications();
bool sendUiaNotification(const std::string& message, bool interrupt = true);
void resetUia();

class UiaProvider : public IRawElementProviderSimple {
	public:
	UiaProvider(_In_ HWND hwnd): controlHWnd(hwnd), refCount(0) {}

	// IUnknown methods
	ULONG STDMETHODCALLTYPE AddRef() override;
	ULONG STDMETHODCALLTYPE Release() override;
	HRESULT STDMETHODCALLTYPE QueryInterface(_In_ REFIID riid,
		_Outptr_ void** ppInterface) override;

	// IRawElementProviderSimple methods
	HRESULT STDMETHODCALLTYPE get_ProviderOptions(
		_Out_ ProviderOptions* pRetVal) final;
	HRESULT STDMETHODCALLTYPE GetPatternProvider(PATTERNID patternId,
		_Outptr_result_maybenull_ IUnknown** pRetVal) override;
	HRESULT STDMETHODCALLTYPE GetPropertyValue(PROPERTYID propertyId,
		_Out_ VARIANT* pRetVal) override;
	HRESULT STDMETHODCALLTYPE get_HostRawElementProvider(
		IRawElementProviderSimple** pRetVal) final;

	protected:
	virtual ~UiaProvider() = default;

	virtual long getControlType() const {
		// Stop Narrator from ever speaking this as a window.
		return UIA_CustomControlTypeId;
	}

	virtual bool isFocusable() const {
		return false;
	}

	HWND controlHWnd; // The HWND for the control.

	private:
	ULONG refCount; // Ref Count for this COM object
};
