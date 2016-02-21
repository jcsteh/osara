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
#include <vector>
#include <algorithm>
#include <iomanip>
#ifdef _WIN32
#include <initguid.h>
#include <Windowsx.h>
#include <Commctrl.h>
#endif
#include <WDL/db2val.h>
#include "osara.h"
#include "resource.h"

using namespace std;

class ParamSource;

class Param {
	public:
	double min;
	double max;
	double step;
	double largeStep;
	bool isEditable;

	Param(): isEditable(false) {
	}

	virtual double getValue() = 0;
	virtual string getValueText(double value) = 0;
	virtual string getValueForEditing() {
		return "";
	}
	virtual void setValue(double value) = 0;
	virtual void setValueFromEdited(const string& text) {
	}
};

class ParamSource {
	public:
	virtual string getTitle() = 0;
	virtual int getParamCount() = 0;
	virtual string getParamName(int param) = 0;
	virtual Param* getParam(int param) = 0;
};

class ReaperObjParamSource;

typedef struct {
	const char* displayName;
	const char* name;
	Param*(*makeParam)(ReaperObjParamSource& source, const char* name);
} ReaperObjParamData;

class ReaperObjParam: public Param {
	protected:
	ReaperObjParamSource& source;
	const char* name;

	ReaperObjParam(ReaperObjParamSource& source, const char* name): Param(), source(source), name(name) {
	}

};

class ReaperObjParamSource: public ParamSource {
	protected:
	vector<ReaperObjParamData> params;

	public:

	virtual void* getSetValue(const char* name, void* newValue) = 0;

	int getParamCount() {
		return this->params.size();
	}

	string getParamName(int param) {
		return this->params[param].displayName;
	}

	Param* getParam(int param) {
		const ReaperObjParamData& data = this->params[param];
		return data.makeParam(*this, data.name);
	}

};

class ReaperObjToggleParam: public ReaperObjParam {

	public:
	ReaperObjToggleParam(ReaperObjParamSource& source, const char* name): ReaperObjParam(source, name) {
		this->min = 0;
		this->max = 1;
		this->step = 1;
		this->largeStep = 1;
	}

	double getValue() {
		return (double)*(bool*)this->source.getSetValue(this->name, NULL);
	}

	string getValueText(double value) {
		return value ? "on" : "off";
	}

	void setValue(double value) {
		bool val = (bool)value;
		this->source.getSetValue(this->name, (void*)&val);
	}

	static Param* make(ReaperObjParamSource& source, const char* name) {
		return new ReaperObjToggleParam(source, name);
	}

};

class ReaperObjVolParam: public ReaperObjParam {

	public:
	ReaperObjVolParam(ReaperObjParamSource& source, const char* name): ReaperObjParam(source, name) {
		this->min = 0;
		this->max = 4;
		this->step = 0.002;
		this->largeStep = 0.1;
		this->isEditable = true;
	}

	double getValue() {
		return *(double*)this->source.getSetValue(this->name, NULL);
	}

	string getValueText(double value) {
		char out[64];
		mkvolstr(out, value);
		return out;
	}

	string getValueForEditing() {
		return this->getValueText(this->getValue());
	}

	void setValue(double value) {
		this->source.getSetValue(this->name, (void*)&value);
	}

	void setValueFromEdited(const string& text) {
		if (text.compare(0, 4, "-inf") == 0) {
			this->setValue(0);
			return;
		}
		double db = atof(text.c_str());
		this->setValue(DB2VAL(db));
	}

	static Param* make(ReaperObjParamSource& source, const char* name) {
		return new ReaperObjVolParam(source, name);
	}

};

class ReaperObjPanParam: public ReaperObjParam {

	public:
	ReaperObjPanParam(ReaperObjParamSource& source, const char* name): ReaperObjParam(source, name) {
		this->min = -1;
		this->max = 1;
		this->step = 0.01;
		this->largeStep = 0.1;
		this->isEditable = true;
	}

	double getValue() {
		return *(double*)this->source.getSetValue(this->name, NULL);
	}

	string getValueText(double value) {
		char out[64];
		mkpanstr(out, value);
		return out;
	}

	string getValueForEditing() {
		return this->getValueText(this->getValue());
	}

	void setValue(double value) {
		this->source.getSetValue(this->name, (void*)&value);
	}

	void setValueFromEdited(const string& text) {
		this->setValue(parsepanstr(text.c_str()));
	}

	static Param* make(ReaperObjParamSource& source, const char* name) {
		return new ReaperObjPanParam(source, name);
	}

};

class ReaperObjLenParam: public ReaperObjParam {

	public:
	ReaperObjLenParam(ReaperObjParamSource& source, const char* name): ReaperObjParam(source, name) {
		this->min = 0;
		this->max = 500;
		this->step = 0.02;
		this->largeStep = 10;
		this->isEditable = true;
		resetTimeCache();
	}

	double getValue() {
		return *(double*)this->source.getSetValue(this->name, NULL);
	}

	string getValueText(double value) {
		static string lastText;
		string text = formatTime(value, TF_RULER, true);
		if (text.empty()) {
			// formatTime returned nothing because value produced the same value text as the last call.
			// Therefore, we cache the text and return it here.
			return lastText;
		}
		lastText = text;
		return text;
	}

	string getValueForEditing() {
		char out[64];
		format_timestr_pos(this->getValue(), out, ARRAYSIZE(out), -1);
		return out;
	}

	void setValue(double value) {
		this->source.getSetValue(this->name, (void*)&value);
	}

	void setValueFromEdited(const string& text) {
		this->setValue(parse_timestr_pos(text.c_str(), -1));
	}

	static Param* make(ReaperObjParamSource& source, const char* name) {
		return new ReaperObjLenParam(source, name);
	}

};

#ifdef _WIN32

class ParamsDialog {
	private:
	ParamSource* source;
	HWND dialog;
	HWND paramCombo;
	HWND slider;
	HWND valueEdit;
	int paramCount;
	string filter;
	vector<int> visibleParams;
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

	void updateValue() {
		double sliderVal = (this->val - this->param->min) / this->param->step;
		// This should be very close to an integer, but we can get values like 116.999...
		// Casting to int just truncates the decimal.
		// Nudge the number a bit to compensate for this.
		// nearbyint would be better, but MSVC doesn't have this.
		sliderVal += sliderVal > 0 ? 0.1 : -0.1;
		SendMessage(this->slider, TBM_SETPOS, TRUE,
			(int)sliderVal);
		this->valText = this->param->getValueText(this->val);
		this->updateValueText();
		if (this->param->isEditable) {
			SendMessage(this->valueEdit, WM_SETTEXT, 0,
				(LPARAM)widen(this->param->getValueForEditing()).c_str());
		}
	}

	void onParamChange() {
		if (this->param)
			delete this->param;
		int paramNum = this->visibleParams[ComboBox_GetCurSel(this->paramCombo)];
		this->param = this->source->getParam(paramNum);
		this->val = this->param->getValue();
		this->sliderRange = (int)((this->param->max - this->param->min) / this->param->step);
		SendMessage(this->slider, TBM_SETRANGE, TRUE, MAKELPARAM(0, this->sliderRange));
		SendMessage(this->slider, TBM_SETLINESIZE, 0, 1);
		SendMessage(this->slider, TBM_SETPAGESIZE, 0,
			(int)(this->param->largeStep / this->param->step));
		EnableWindow(this->valueEdit, this->param->isEditable);
		this->updateValue();
	}

	void onSliderChange() {
		int sliderVal = SendMessage(this->slider, TBM_GETPOS, 0, 0);
		double newVal = sliderVal * this->param->step + this->param->min;
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
				break;
			}
		}
		this->param->setValue(this->val);
		this->updateValue();
	}

	void onValueEdited() {
		WCHAR rawText[30];
		if (GetDlgItemText(dialog, ID_PARAM_VAL_EDIT, rawText, ARRAYSIZE(rawText)) == 0)
			return;
		this->param->setValueFromEdited(narrow(rawText));
		this->val = this->param->getValue();
		this->updateValue();
	}

	static INT_PTR CALLBACK dialogProc(HWND dialogHwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
		ParamsDialog* dialog = (ParamsDialog*)GetWindowLongPtr(dialogHwnd, GWLP_USERDATA);
		switch (msg) {
			case WM_COMMAND:
				if (LOWORD(wParam) == ID_PARAM && HIWORD(wParam) == CBN_SELCHANGE) {
					dialog->onParamChange();
					return TRUE;
				} else if (LOWORD(wParam) == ID_PARAM_FILTER && HIWORD(wParam) == EN_KILLFOCUS) {
					dialog->onFilterChange();
					return TRUE;
				} else if (LOWORD(wParam) == ID_PARAM_VAL_EDIT && HIWORD(wParam) ==EN_KILLFOCUS) {
					dialog->onValueEdited();
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

	bool shouldIncludeParam(string name) {
		if (filter.empty())
			return true;
		// Convert param name to lower case for match.
		transform(name.begin(), name.end(), name.begin(), tolower);
		return name.find(filter) != string::npos;
	}

	void updateParamList() {
		int prevSelParam;
		if (this->visibleParams.empty())
			prevSelParam = -1;
		else
			prevSelParam = this->visibleParams[ComboBox_GetCurSel(this->paramCombo)];
		this->visibleParams.clear();
		// Use the first item if the previously selected param gets filtered out.
		int newComboSel = 0;
		ComboBox_ResetContent(this->paramCombo);
		for (int p = 0; p < this->paramCount; ++p) {
			string& name = source->getParamName(p);
			if (!this->shouldIncludeParam(name))
				continue;
			this->visibleParams.push_back(p);
			ComboBox_AddString(this->paramCombo, widen(name).c_str());
			if (p == prevSelParam)
				newComboSel = this->visibleParams.size() - 1;
		}
		ComboBox_SetCurSel(this->paramCombo, newComboSel);
		if (this->visibleParams.empty()) {
			EnableWindow(this->slider, FALSE);
			return;
		}
		EnableWindow(this->slider, TRUE);
		this->onParamChange();
	}

	void onFilterChange() {
		WCHAR rawText[100];
		GetDlgItemText(this->dialog, ID_PARAM_FILTER, rawText, ARRAYSIZE(rawText));
		string& text = narrow(rawText);
		if (this->filter.compare(text) == 0)
			return; // No change.
		this->filter = text;
		this->updateParamList();
	}

	ParamsDialog(ParamSource* source): source(source), param(NULL) {
		this->paramCount = source->getParamCount();
		if (this->paramCount == 0) {
			delete this;
			return;
		}
		this->dialog = CreateDialog(pluginHInstance, MAKEINTRESOURCE(ID_PARAMS_DLG), mainHwnd, ParamsDialog::dialogProc);
		SetWindowLongPtr(this->dialog, GWLP_USERDATA, (LONG_PTR)this);
		SetWindowText(this->dialog, widen(source->getTitle()).c_str());
		this->paramCombo = GetDlgItem(this->dialog, ID_PARAM);
		this->slider = GetDlgItem(this->dialog, ID_PARAM_VAL_SLIDER);
		this->valueEdit = GetDlgItem(this->dialog, ID_PARAM_VAL_EDIT);
		this->updateParamList();
		ShowWindow(this->dialog, SW_SHOWNORMAL);
	}

};

#endif

class TrackParams: public ReaperObjParamSource {
	private:
	MediaTrack* track;

	public:
	TrackParams(MediaTrack* track): track(track) {
		this->params = {
			{"Volume", "D_VOL", ReaperObjVolParam::make},
			{"Pan", "D_PAN", ReaperObjPanParam::make},
			{"Mute", "B_MUTE", ReaperObjToggleParam::make}
		};
	}

	string getTitle() {
		return "Track Parameters";
	}

	void* getSetValue(const char* name, void* newValue) {
		return GetSetMediaTrackInfo(this->track, name, newValue);
	}

};

class ItemParams: public ReaperObjParamSource {
	private:
	MediaItem* item;

	public:
	ItemParams(MediaItem* item): item(item) {
		this->params.push_back({"Item volume", "D_VOL", ReaperObjVolParam::make});
		// #74: Only add take parameters if there *is* a take. There isn't for empty items.
		if (GetActiveTake(item)) {
			this->params.insert(this->params.end(), {
				{"Take volume", "t:D_VOL", ReaperObjVolParam::make},
				{"Take pan", "t:D_PAN", ReaperObjPanParam::make},
			});
		}
		this->params.insert(this->params.end(), {
			{"Mute", "B_MUTE", ReaperObjToggleParam::make},
			{"Fade in length", "D_FADEINLEN", ReaperObjLenParam::make},
			{"Fade out length", "D_FADEOUTLEN", ReaperObjLenParam::make}
		});
	}

	string getTitle() {
		return "Item Parameters";
	}

	void* getSetValue(const char* name, void* newValue) {
		if (strncmp(name, "t:", 2) == 0) {
			// Take property.
			name = &name[2];
			MediaItem_Take* take = GetActiveTake(this->item);
			return GetSetMediaItemTakeInfo(take, name, newValue);
		}
		return GetSetMediaItemInfo(this->item, name, newValue);
	}

};

#ifdef _WIN32

void cmdParamsFocus(Command* command) {
	ParamSource* source;
	switch (fakeFocus) {
		case FOCUS_TRACK: {
			MediaTrack* track = GetLastTouchedTrack();
			if (!track)
				return;
			source = new TrackParams(track);
			break;
		}
		case FOCUS_ITEM: {
			MediaItem* item = GetSelectedMediaItem(0, 0);
			if (!item)
				return;
			source = new ItemParams(item);
			break;
		}
		default:
			return;
	}
	new ParamsDialog(source);
}

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

	TrackFxParam(TrackFxParams& source, int param): Param(), source(source), param(param) {
		TrackFX_GetParam(source.track, source.fx, param, &this->min, &this->max);
		this->step = (this->max - this->min) / 1000;
		this->largeStep = this->step * 20;
		this->isEditable = true;
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

	string getValueForEditing() {
		ostringstream s;
		s << fixed << setprecision(4);
		s << TrackFX_GetParam(this->source.track, this->source.fx, this->param, NULL, NULL);
		return s.str();
	}
	
	void setValue(double value) {
		TrackFX_SetParam(this->source.track, this->source.fx, this->param, value);
	}

	void setValueFromEdited(const string& text) {
		this->setValue(atof(text.c_str()));
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
	if (fxCount == 0) {
		outputMessage("No FX");
		return;
	} else if (fxCount == 1)
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
