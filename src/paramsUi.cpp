/*
 * OSARA: Open Source Accessibility for the REAPER Application
 * Parameters UI code
 * Author: James Teh <jamie@nvaccess.org>
 * Copyright 2014-2015 NV Access Limited
 * License: GNU General Public License version 2.0
 */

#define UNICODE
#include <windows.h>
#include <string>
#include <sstream>
#ifdef _WIN32
#include <initguid.h>
#include <Windowsx.h>
#include <Commctrl.h>
#endif
#include "osara.h"
#include "resource.h"

using namespace std;

class ParamSource;

class Param {
	public:
	double min;
	double max;
	double step;

	virtual double getValue() = 0;
	virtual string getValueText(double value) = 0;
	virtual void setValue(double value) = 0;
};

class ParamSource {
	public:
	ParamSource() {};
	virtual string getTitle() = 0;
	virtual int getParamCount() = 0;
	virtual string getParamName(int param) = 0;
	virtual Param* getParam(int param) = 0;
};

#ifdef _WIN32

class ParamsDialog {
	private:
	ParamSource* source;
	HWND dialog;
	HWND paramCombo;
	HWND slider;
	Param* param;
	double val;
	int sliderRange;
	string valText;

	void updateValueText() {
		if (this->valText.empty()) {
			// No value text.
			accPropServices->ClearHwndProps(this->slider, OBJID_CLIENT, CHILDID_SELF, &PROPID_ACC_VALUE, 1);
			return;
		}

		// Set the slider's accessible value to this text.
		accPropServices->SetHwndPropStr(slider, OBJID_CLIENT, CHILDID_SELF, PROPID_ACC_VALUE,
			widen(this->valText).c_str());
	}

	void updateSlider() {
		SendMessage(this->slider, TBM_SETPOS, TRUE,
			(int)((this->val - this->param->min) / this->param->step));
		this->valText = this->param->getValueText(this->val);
		this->updateValueText();
	}

	void onParamChange() {
		if (this->param)
			delete this->param;
		this->param = this->source->getParam(ComboBox_GetCurSel(this->paramCombo));
		this->val = this->param->getValue();
		this->sliderRange = (int)((this->param->max - this->param->min) / this->param->step);
		SendMessage(this->slider, TBM_SETRANGE, TRUE, MAKELPARAM(0, this->sliderRange));
		SendMessage(this->slider, TBM_SETLINESIZE, 0, 1);
		this->updateSlider();
	}

	void onSliderChange() {
		int sliderVal = SendMessage(this->slider, TBM_GETPOS, 0, 0);
		double newVal = sliderVal * this->param->step + this->param->min;
		this->param->setValue(newVal);
		if (newVal == this->val)
			return; // This is due to our own snapping call (below).
		int step = (newVal > this->val) ? 1 : -1;
		this->val = newVal;

		// If the value text (if any) doesn't change, the value change is insignificant.
		// Snap to the next change in value text.
		// todo: Optimise; perhaps a binary search?
		for (; 0 <= sliderVal && sliderVal <= this->sliderRange; sliderVal += step) {
			// Continually adding to a float accumulates inaccuracy,
			// so calculate the value from scratch each time.
			newVal = sliderVal * this->param->step + this->param->min;
			string& testText = this->param->getValueText(newVal);
			if (testText.empty())
				break; // Formatted values not supported.
			if (testText.compare(this->valText) != 0) {
				// The value text is different, so this change is significant.
				// Snap to this value.
				this->val = newVal;
				this->updateSlider();
				break;
			}
		}
	}

	static INT_PTR CALLBACK dialogProc(HWND dialogHwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
		ParamsDialog* dialog = (ParamsDialog*)GetWindowLongPtr(dialogHwnd, GWLP_USERDATA);
		switch (msg) {
			case WM_COMMAND:
				if (LOWORD(wParam) == ID_PARAM && HIWORD(wParam) == CBN_SELCHANGE) {
					dialog->onParamChange();
					return TRUE;
				} else if (LOWORD(wParam) == IDCANCEL) {
					DestroyWindow(dialogHwnd);
					delete dialog;
					return TRUE;
				}
				break;
			case WM_HSCROLL:
				if ((HWND)lParam == dialog->slider) {
					dialog->onSliderChange();
					return TRUE;
				}
				break;
			case WM_CLOSE:
				DestroyWindow(dialogHwnd);
				delete dialog;
				return TRUE;
		}
		return FALSE;
	}

	public:

	~ParamsDialog() {
		if (this->param)
			delete this->param;
		delete this->source;
	}

	ParamsDialog(ParamSource* source): source(source), param(NULL) {
		int nParams = source->getParamCount();
		if (nParams == 0) {
			delete this;
			return;
		}
		this->dialog = CreateDialog(pluginHInstance, MAKEINTRESOURCE(ID_PARAMS_DLG), mainHwnd, ParamsDialog::dialogProc);
		SetWindowLongPtr(this->dialog, GWLP_USERDATA, (LONG_PTR)this);
		SetWindowText(this->dialog, widen(source->getTitle()).c_str());
		this->paramCombo = GetDlgItem(this->dialog, ID_PARAM);
		this->slider = GetDlgItem(this->dialog, ID_PARAM_VAL_SLIDER);
		// Populate the parameter list.
		for (int p = 0; p < nParams; ++p) {
			string& name = source->getParamName(p);
			ComboBox_AddString(this->paramCombo, widen(name).c_str());
		}
		ComboBox_SetCurSel(this->paramCombo, 0); // Select the first initially.
		this->onParamChange();
		ShowWindow(this->dialog, SW_SHOWNORMAL);
	}

};

#endif

class TrackFxParam;

class TrackFxParams: public ParamSource {
	friend class TrackFxParam;

	private:
	MediaTrack* track;
	int fx;

	public:

	TrackFxParams(MediaTrack* track, int fx): track(track), fx(fx) {
	}

	string getTitle() {
		return "FX Parameters";
	}

	int getParamCount() {
		return TrackFX_GetNumParams(this->track, this->fx);
	}

	string getParamName(int param) {
		char name[256];
		TrackFX_GetParamName(this->track, this->fx, param, name, sizeof(name));
		// Append the parameter number to facilitate efficient navigation
		// and to ensure reporting where two consecutive parameters have the same name (#32).
		ostringstream ns;
		ns << name << " (" << param + 1 << ")";
		return ns.str();
	}

	Param* getParam(int param);
};

class TrackFxParam: public Param {
	private:
	TrackFxParams& source;
	int param;

	public:

	TrackFxParam(TrackFxParams& source, int param): source(source), param(param) {
		TrackFX_GetParam(source.track, source.fx, param, &this->min, &this->max);
		this->step = (this->max - this->min) / 1000;
	}

	double getValue() {
		return TrackFX_GetParam(this->source.track, this->source.fx, this->param, NULL, NULL);
	}

	string getValueText(double value) {
		char text[50];
		if (TrackFX_FormatParamValue(this->source.track, this->source.fx, param, value, text, sizeof(text)))
			return text;
		return "";
	}

	void setValue(double value) {
		TrackFX_SetParam(this->source.track, this->source.fx, this->param, value);
	}

};

Param* TrackFxParams::getParam(int param) {
	return new TrackFxParam(*this, param);
}

#ifdef _WIN32

void fxParams_begin(MediaTrack* track) {
	char name[256];

	int fxCount = TrackFX_GetCount(track);
	int fx;
	if (fxCount == 0)
		return;
	else if (fxCount == 1)
		fx = 0;
	else {
		// Present a menu of effects.
		HMENU effects = CreatePopupMenu();
		MENUITEMINFO itemInfo;
		itemInfo.cbSize = sizeof(MENUITEMINFO);
		for (int f = 0; f < fxCount; ++f) {
			TrackFX_GetFXName(track, f, name, sizeof(name));
			itemInfo.fMask = MIIM_FTYPE | MIIM_ID | MIIM_STRING;
			itemInfo.fType = MFT_STRING;
			itemInfo.wID = f + 1;
			itemInfo.dwTypeData = (wchar_t*)widen(name).c_str();
			itemInfo.cch = ARRAYSIZE(name);
			InsertMenuItem(effects, f, false, &itemInfo);
		}
		fx = TrackPopupMenu(effects, TPM_NONOTIFY | TPM_RETURNCMD, 0, 0, 0, mainHwnd, NULL) - 1;
		DestroyMenu(effects);
		if (fx == -1)
			return; // Cancelled.
	}

	TrackFxParams* source = new TrackFxParams(track, fx);
	new ParamsDialog(source);
}

void cmdFxParamsCurrentTrack(Command* command) {
	MediaTrack* currentTrack = GetLastTouchedTrack();
	if (!currentTrack)
		return;
	fxParams_begin(currentTrack);
}

void cmdFxParamsMaster(Command* command) {
	fxParams_begin(GetMasterTrack(0));
}

#endif
