/*
 * OSARA: Open Source Accessibility for the REAPER Application
 * UI Automation header
 * Copyright 2019-2025 Leonard de Ruijter, James Teh
 * License: GNU General Public License version 2.0
 */

#pragma once

#include <atlcomcli.h>
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

// A UIA provider for a slider with a text value. This is needed because slider
// controls are normally numeric and can't expose a text value. This provider
// implements the Value pattern. It also provides MSAA support for backwards
// compatibility.
class TextSliderUiaProvider : public UiaProvider, public IValueProvider {
	public:
	// Create an instance of this provider for a given slider control HWND and set
	//it up so that it responds to UIA clients which query this control.
	static CComPtr<TextSliderUiaProvider> create(HWND hwnd);

	// Set the value of the slider to expose to clients. Call this when the value
	// changes.
	void setValue(std::string value);

	// Fire UIA and MSAA events indicating that the value has changed.
	void fireValueChange();

	// IUnknown methods
	ULONG STDMETHODCALLTYPE AddRef() override {
		return UiaProvider::AddRef();
	}

	ULONG STDMETHODCALLTYPE Release() override {
		return UiaProvider::Release();
	}

	HRESULT STDMETHODCALLTYPE QueryInterface(_In_ REFIID riid,
		_Outptr_ void** ppInterface) override;

	// IRawElementProviderSimple methods
	HRESULT STDMETHODCALLTYPE GetPatternProvider(PATTERNID patternId,
		_Outptr_result_maybenull_ IUnknown** pRetVal) override;

	// IValueProvider methods
	HRESULT STDMETHODCALLTYPE SetValue(__RPC__in LPCWSTR val) override;
	HRESULT STDMETHODCALLTYPE get_Value(
		__RPC__deref_out_opt BSTR* pRetVal) override;
	HRESULT STDMETHODCALLTYPE get_IsReadOnly(__RPC__out BOOL* pRetVal) override;

	protected:
	long getControlType() const final {
		return UIA_SliderControlTypeId;
	}

	bool isFocusable() const final {
		return true;
	}

	private:
	TextSliderUiaProvider(HWND hwnd) : UiaProvider(hwnd) {}

	static LRESULT CALLBACK subclassProc(HWND hwnd, UINT msg, WPARAM wParam,
		LPARAM lParam, UINT_PTR subclass, DWORD_PTR data);

	std::string sliderValue;
};
