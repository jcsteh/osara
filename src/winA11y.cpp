/*
 * OSARA: Open Source Accessibility for the REAPER Application
 * Windows accessibility stuff: code
 * Author: James Teh <jamie@nvaccess.org>
 * Copyright 2014-2015 NV Access Limited
 * License: GNU General Public License version 2.0
 */

#define UNICODE
#include <windows.h>
#include <initguid.h>
#include <oleacc.h>
#include <Windowsx.h>
#include "osara.h"

using namespace std;

class OsaraAccessible : public IAccessible {

	private:
	ULONG _refCount;
	HWND _window;

	public:
	OsaraAccessible(HWND window) : _refCount(1), _window(window) {
	}

	// IUnknown

	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** object) {
		if (!object)
			return E_INVALIDARG;
		*object = NULL;
		if (iid == IID_IAccessible)
			*object = static_cast<IAccessible*>(this);
		else if (iid == IID_IDispatch)
			*object = static_cast<IDispatch*>(this);
		else if (iid == IID_IUnknown)
			*object = static_cast<IUnknown*>(this);
		else
			return E_NOINTERFACE;
		this->AddRef();
		return S_OK;
	}

	ULONG STDMETHODCALLTYPE AddRef() {
		return ++this->_refCount;
	}

	ULONG STDMETHODCALLTYPE Release() {
		if (this->_refCount > 0)
			--this->_refCount;
		if (this->_refCount == 0) {
			delete this;
			return 0;
		}
		return this->_refCount;
	}

	// IDispatch

	HRESULT STDMETHODCALLTYPE Invoke(DISPID dispIdMember, REFIID riid, LCID lcid, WORD wFlags, DISPPARAMS FAR* pDispParams, VARIANT FAR* pVarResult, EXCEPINFO FAR* pExcepInfo, unsigned int FAR* puArgErr) {
		return E_NOTIMPL;
	}

	HRESULT STDMETHODCALLTYPE  GetTypeInfoCount(UINT* count) {
		return E_NOTIMPL;
	}

	HRESULT STDMETHODCALLTYPE GetTypeInfo(UINT index, LCID lcid, ITypeInfo** ppTypeInfo) {
		return E_NOTIMPL;
	}

	HRESULT STDMETHODCALLTYPE IDispatch::GetIDsOfNames(const IID& riid, LPOLESTR* name,UINT x, LCID lcid, DISPID* dispID) {
		return E_NOTIMPL;
	}

	// IAccessible

	HRESULT STDMETHODCALLTYPE accDoDefaultAction(VARIANT id) {
		return E_NOTIMPL;
	}
	
	HRESULT STDMETHODCALLTYPE accHitTest(long left, long top, VARIANT* id) {
		return E_NOTIMPL;
	}

	HRESULT STDMETHODCALLTYPE accLocation(long* left, long* top, long* width, long* height, VARIANT child) {
		return E_NOTIMPL;
	}

	HRESULT STDMETHODCALLTYPE accNavigate(long dir, VARIANT start, VARIANT* end) {
		return E_NOTIMPL;
	}

	HRESULT STDMETHODCALLTYPE accSelect(long flags, VARIANT id) {
		return E_NOTIMPL;
	}

	HRESULT STDMETHODCALLTYPE get_accChild(VARIANT childId, IDispatch** child) {
		if (childId.lVal == CHILDID_SELF)
			return this->QueryInterface(IID_IDispatch, (void**)child);
		return E_INVALIDARG;
	}

	HRESULT STDMETHODCALLTYPE get_accChildCount(long* count) {
		*count = 0;
		return S_OK;
	}

	HRESULT STDMETHODCALLTYPE get_accDefaultAction(VARIANT id, BSTR* action) {
		return E_NOTIMPL;
	}

	HRESULT STDMETHODCALLTYPE get_accDescription(VARIANT id, BSTR* description) {
		return E_NOTIMPL;
	}

	HRESULT STDMETHODCALLTYPE get_accFocus(VARIANT* id) {
		return E_NOTIMPL;
	}

	HRESULT STDMETHODCALLTYPE get_accHelp(VARIANT id, BSTR* help) {
		return E_NOTIMPL;
	}

	HRESULT STDMETHODCALLTYPE get_accHelpTopic(BSTR* helpFile, VARIANT child, long* topic) {
		return E_NOTIMPL;
	}

	HRESULT STDMETHODCALLTYPE get_accKeyboardShortcut(VARIANT id, BSTR* shortcut) {
		return E_NOTIMPL;
	}

	wstring name;
	HRESULT STDMETHODCALLTYPE get_accName(VARIANT id, BSTR* name) {
		*name = SysAllocString(this->name.c_str());
		return S_OK;
	}

	HRESULT STDMETHODCALLTYPE get_accParent(IDispatch** parent) {
		return AccessibleObjectFromWindow(this->_window, OBJID_CLIENT, IID_IDispatch, (void**)parent);
	}

	HRESULT STDMETHODCALLTYPE get_accRole(VARIANT id, VARIANT* role) {
		role->vt = VT_I4;
		role->lVal = ROLE_SYSTEM_CLIENT;
		return S_OK;
	}

	HRESULT STDMETHODCALLTYPE get_accSelection(VARIANT* children) {
		return E_NOTIMPL;
	}

	HRESULT STDMETHODCALLTYPE get_accState(VARIANT id, VARIANT* state) {
		state->vt = VT_I4;
		state->lVal = STATE_SYSTEM_FOCUSED;
		return S_OK;
	}

	HRESULT STDMETHODCALLTYPE get_accValue(VARIANT id, BSTR* value) {
		return E_NOTIMPL;
	}

	HRESULT STDMETHODCALLTYPE put_accName(VARIANT id, BSTR name) {
		return E_NOTIMPL;
	}

	HRESULT STDMETHODCALLTYPE put_accValue(VARIANT id, BSTR value) {
		return E_NOTIMPL;
	}

};

OsaraAccessible* fakeFocusAcc = NULL;

WNDPROC realFocusWindowProc = NULL;
LRESULT CALLBACK fakeFocusWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	if (msg == WM_GETOBJECT && (DWORD)lParam == 1) {
		if (fakeFocusAcc)
			return LresultFromObject(IID_IAccessible, wParam, fakeFocusAcc);
	}
	return CallWindowProc(realFocusWindowProc, hwnd, msg, wParam, lParam);
}

HWND lastMessageHwnd = NULL;

void rawOutputMessage(const wstring& message) {
	HWND focus = GetFocus();
	if (focus != lastMessageHwnd) {
		if (realFocusWindowProc)
			SetWindowLongPtr(lastMessageHwnd, GWLP_WNDPROC, (LONG_PTR)realFocusWindowProc);
		if (fakeFocusAcc)
			fakeFocusAcc->Release();
		fakeFocusAcc = new OsaraAccessible(focus);
		realFocusWindowProc = (WNDPROC)SetWindowLongPtr(focus, GWLP_WNDPROC, (LONG_PTR)fakeFocusWindowProc);
	}
	fakeFocusAcc->name = message;
	// Fire a nameChange event so ATs will report this text.
	NotifyWinEvent(EVENT_OBJECT_NAMECHANGE, focus, 1, CHILDID_SELF);
	NotifyWinEvent(EVENT_OBJECT_FOCUS, focus, 1, CHILDID_SELF);
	lastMessageHwnd = focus;
}
