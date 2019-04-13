/*
 * OSARA: Open Source Accessibility for the REAPER Application
 * Parameters UI code
 * Author: James Teh <jamie@jantrid.net>
 * Copyright 2014-2017 NV Access Limited
 * License: GNU General Public License version 2.0
 */

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
#include <WDL/win32_utf8.h>
#include <WDL/db2val.h>
#include <WDL/wdltypes.h>
#include <reaper/reaper_plugin.h>
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

	virtual ~Param() = default;

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
	virtual ~ParamSource() = default;
	virtual string getTitle() = 0;
	virtual int getParamCount() = 0;
	virtual string getParamName(int param) = 0;
	virtual Param* getParam(int param) = 0;
};

class ReaperObjParamSource;

typedef struct {
	string displayName;
	string name;
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
		return data.makeParam(*this, data.name.c_str());
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
		format_timestr_pos(this->getValue(), out, sizeof(out), -1);
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
	string valText;

	void updateValueText() {
#ifdef _WIN32
		if (this->valText.empty()) {
			// No value text.
			accPropServices->ClearHwndProps(this->slider, OBJID_CLIENT, CHILDID_SELF, &PROPID_ACC_VALUE, 1);
			return;
		}

		// Set the slider's accessible value to this text.
		accPropServices->SetHwndPropStr(this->slider, OBJID_CLIENT, CHILDID_SELF,
			PROPID_ACC_VALUE, widen(this->valText).c_str());
		NotifyWinEvent(EVENT_OBJECT_VALUECHANGE, this->slider,
			OBJID_CLIENT, CHILDID_SELF);
#else // _WIN32
		// We can't set the slider's accessible value on Mac.
		outputMessage(this->valText);
#endif // _WIN32
	}

	void updateValue() {
		this->valText = this->param->getValueText(this->val);
		this->updateValueText();
		if (this->param->isEditable) {
			SetWindowText(this->valueEdit, this->param->getValueForEditing().c_str());
		}
	}

	void onParamChange() {
		if (this->param)
			delete this->param;
		int paramNum = this->visibleParams[ComboBox_GetCurSel(this->paramCombo)];
		this->param = this->source->getParam(paramNum);
		this->val = this->param->getValue();
		EnableWindow(this->valueEdit, this->param->isEditable);
		this->updateValue();
	}

	void onSliderChange(double newVal) {
		double step = this->param->step;
		if (newVal < val) {
			step = -step;
		}
		this->val = newVal;

		// If the value text (if any) doesn't change, the value change is insignificant.
		// Snap to the next change in value text.
		// todo: Optimise; perhaps a binary search?
		for (; this->param->min <= newVal && newVal <= this->param->max; newVal += step) {
			const string testText = this->param->getValueText(newVal);
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
		char rawText[30];
		if (GetDlgItemText(dialog, ID_PARAM_VAL_EDIT, rawText, sizeof(rawText)) == 0)
			return;
		this->param->setValueFromEdited(rawText);
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
			case WM_CLOSE:
				DestroyWindow(dialogHwnd);
				delete dialog;
				return TRUE;
		}
		return FALSE;
	}

	accelerator_register_t accelReg;
	static int translateAccel(MSG* msg, accelerator_register_t* accelReg) {
		// We handle key presses for the slider ourselves.
		ParamsDialog* dialog = (ParamsDialog*)accelReg->user;
		if (msg->message != WM_KEYDOWN || msg->hwnd != dialog->slider) {
			return 0;
		}
		double newVal = dialog->val;
		switch (msg->wParam) {
			case VK_UP:
			case VK_RIGHT:
				newVal += dialog->param->step;
				break;
			case VK_DOWN:
			case VK_LEFT:
				newVal -= dialog->param->step;
				break;
			case VK_PRIOR:
				newVal += dialog->param->largeStep;
				break;
			case VK_NEXT:
				newVal -= dialog->param->largeStep;
				break;
			case VK_HOME:
				newVal = dialog->param->max;
				break;
			case VK_END:
				newVal = dialog->param->min;
				break;
			default:
				return -1;
		}
		dialog->onSliderChange(newVal);
		return 1;
	}

	public:

	~ParamsDialog() {
		plugin_register("-accelerator", &this->accelReg);
		if (this->param)
			delete this->param;
		delete this->source;
	}

	bool shouldIncludeParam(string name) {
		if (filter.empty())
			return true;
		// Convert param name to lower case for match.
		transform(name.begin(), name.end(), name.begin(), ::tolower);
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
			const string name = source->getParamName(p);
			if (!this->shouldIncludeParam(name))
				continue;
			this->visibleParams.push_back(p);
			ComboBox_AddString(this->paramCombo, name.c_str());
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
		char rawText[100];
		GetDlgItemText(this->dialog, ID_PARAM_FILTER, rawText, sizeof(rawText));
		string text = rawText;
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
		SetWindowText(this->dialog, source->getTitle().c_str());
		this->paramCombo = GetDlgItem(this->dialog, ID_PARAM);
		WDL_UTF8_HookComboBox(this->paramCombo);
		this->slider = GetDlgItem(this->dialog, ID_PARAM_VAL_SLIDER);
		// We need to do exotic stuff with this slider that we can't support on Mac:
		// 1. Custom step values (TBM_SETLINESIZE, TBM_SETPAGESIZE).
		// 2. Down arrow moving left instead of right (TBS_DOWNISLEFT).
		// We also snap to changes in value text, which is even tricky on Windows.
		// Therefore, we just use the slider as a placeholder and handle key
		// presses ourselves.
		this->accelReg.translateAccel = &this->translateAccel;
		this->accelReg.isLocal = true;
		this->accelReg.user = (void*)this;
		plugin_register("accelerator", &this->accelReg);
		this->valueEdit = GetDlgItem(this->dialog, ID_PARAM_VAL_EDIT);
		this->updateParamList();
		ShowWindow(this->dialog, SW_SHOWNORMAL);
	}

};

class TrackParams: public ReaperObjParamSource {
	private:
	MediaTrack* track;

	void addSendParams(int category, const char* categoryName, const char* trackParam) {
		int count = GetTrackNumSends(track, category);
		for (int i = 0; i < count; ++i) {
			MediaTrack* sendTrack = (MediaTrack*)GetSetTrackSendInfo(this->track, category, i, trackParam, NULL);
			ostringstream dispPrefix;
			// Example display name: "1 Drums send volume"
			dispPrefix << (int)(size_t)GetSetMediaTrackInfo(sendTrack, "IP_TRACKNUMBER", NULL) << " ";
			char* trackName = (char*)GetSetMediaTrackInfo(sendTrack, "P_NAME", NULL);
			if (trackName)
				dispPrefix << trackName << " ";
			dispPrefix << categoryName << " ";
			ostringstream namePrefix;
			// The name prefix enables getSetValue to identify send parameters.
			// Example name: "s 0 0 D_VOL"
			namePrefix << "s " << category << " " << i << " ";
			this->params.insert(this->params.end(), {
				{dispPrefix.str() + "volume", namePrefix.str() + "D_VOL", ReaperObjVolParam::make},
				{dispPrefix.str() + "pan", namePrefix.str() + "D_PAN", ReaperObjPanParam::make},
				{dispPrefix.str() + "mute", namePrefix.str() + "B_MUTE", ReaperObjToggleParam::make}
			});
		}
	}

	public:
	TrackParams(MediaTrack* track): track(track) {
		this->params = {
			{"Volume", "D_VOL", ReaperObjVolParam::make},
			{"Pan", "D_PAN", ReaperObjPanParam::make},
			{"Mute", "B_MUTE", ReaperObjToggleParam::make}
		};
		this->addSendParams(0, "send", "P_DESTTRACK");
		this->addSendParams(-1, "receive", "P_SRCTRACK");
	}

	string getTitle() {
		return "Track Parameters";
	}

	void* getSetValue(const char* name, void* newValue) {
		istringstream nameStream(name);
		string category;
		nameStream >> category;
		if (category.compare("s") == 0) {
			// Send.
			// Extract required arguments from the name.
			int category, index;
			string param;
			nameStream >> category >> index >> param;
			return GetSetTrackSendInfo(this->track, category, index, param.c_str(), newValue);
		}
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

// The FX functions in the REAPER API are the same for tracks and takes
// except for the prefix (TrackFX_*/TakeFX_*)
// and the first argument type (MediaTrack*/MediaItem_Take*).
// Deal with the type using templates
// and with the prefix by passing it and fetching the functions dynamically.
template<typename ReaperObj>
class FxParam;

template<typename ReaperObj>
class FxParams: public ParamSource {
	friend class FxParam<ReaperObj>;

	private:
	ReaperObj* obj;
	int fx;
	int (*_GetNumParams)(ReaperObj*, int);
	bool (*_GetParamName)(ReaperObj*, int, int, char*, int);
	double (*_GetParam)(ReaperObj*, int, int, double*, double*);
	bool (*_SetParam)(ReaperObj*, int, int, double);
	bool (*_FormatParamValue)(ReaperObj*, int, int, double, char*, int);

	public:

	FxParams(ReaperObj* obj, const string& apiPrefix, int fx): obj(obj), fx(fx) {
		// Get functions.
		*(void**)&this->_GetNumParams = plugin_getapi((apiPrefix + "_GetNumParams").c_str());
		*(void**)&this->_GetParamName = plugin_getapi((apiPrefix + "_GetParamName").c_str());
		*(void**)&this->_GetParam = plugin_getapi((apiPrefix + "_GetParam").c_str());
		*(void**)&this->_SetParam = plugin_getapi((apiPrefix + "_SetParam").c_str());
		*(void**)&this->_FormatParamValue = plugin_getapi((apiPrefix + "_FormatParamValue").c_str());
	}

	string getTitle() {
		return "FX Parameters";
	}

	int getParamCount() {
		return this->_GetNumParams(this->obj, this->fx);
	}

	string getParamName(int param) {
		char name[256];
		this->_GetParamName(this->obj, this->fx, param, name, sizeof(name));
		// Append the parameter number to facilitate efficient navigation
		// and to ensure reporting where two consecutive parameters have the same name (#32).
		ostringstream ns;
		ns << name << " (" << param + 1 << ")";
		return ns.str();
	}

	Param* getParam(int param);
};

template<typename ReaperObj>
class FxParam: public Param {
	private:
	FxParams<ReaperObj>& source;
	int param;

	public:

	FxParam(FxParams<ReaperObj>& source, int param): Param(), source(source), param(param) {
		this->source._GetParam(source.obj, source.fx, param, &this->min, &this->max);
		this->step = (this->max - this->min) / 1000;
		this->largeStep = this->step * 20;
		this->isEditable = true;
	}

	double getValue() {
		return this->source._GetParam(this->source.obj, this->source.fx, this->param, NULL, NULL);
	}

	string getValueText(double value) {
		char text[50];
		if (this->source._FormatParamValue(this->source.obj, this->source.fx, param, value, text, sizeof(text)))
			return text;
		return "";
	}

	string getValueForEditing() {
		ostringstream s;
		s << fixed << setprecision(4);
		s << this->source._GetParam(this->source.obj, this->source.fx, this->param, NULL, NULL);
		return s.str();
	}
	
	void setValue(double value) {
		this->source._SetParam(this->source.obj, this->source.fx, this->param, value);
	}

	void setValueFromEdited(const string& text) {
		this->setValue(atof(text.c_str()));
	}

};

template<typename ReaperObj>
Param* FxParams<ReaperObj>::getParam(int param) {
	return new FxParam<ReaperObj>(*this, param);
}

typedef vector<pair<int, string>> FxList;

FxList listFx(MediaTrack* track) {
	FxList fxList;
	char rawName[256];
	const int count = TrackFX_GetCount(track);
	for (int index = 0; index < count; ++index) {
		TrackFX_GetFXName(track, index, rawName, sizeof(rawName));
		fxList.push_back({index, rawName});
	}
	const int recCount = TrackFX_GetRecCount(track);
	if (recCount == 0) {
		return fxList;
	}
	string suffix;
	if (track == GetMasterTrack(0)) {
		suffix = " [monitor]";
	} else {
		suffix = " [input]";
	}
	for (int index = 0; index < recCount; ++index) {
		const int rawIndex = 0x1000000 + index;
		TrackFX_GetFXName(track, rawIndex, rawName, sizeof(rawName));
		string name = rawName + suffix;
		fxList.push_back({rawIndex, name});
	}
	return fxList;
}

FxList listFx(MediaItem_Take* track) {
	FxList fxList;
	char rawName[256];
	const int count = TakeFX_GetCount(track);
	for (int index = 0; index < count; ++index) {
		TakeFX_GetFXName(track, index, rawName, sizeof(rawName));
		fxList.push_back({index, rawName});
	}
	return fxList;
}

template<typename ReaperObj>
void fxParams_begin(ReaperObj* obj, const string& apiPrefix) {
	const auto fxList = listFx(obj);
	const int fxCount = fxList.size();
	int fx = -1;
	if (fxCount == 0) {
		outputMessage("No FX");
		return;
	} else if (fxCount == 1)
		fx = fxList[0].first;
	else {
		// Present a menu of effects.
		HMENU effects = CreatePopupMenu();
		MENUITEMINFO itemInfo;
		itemInfo.cbSize = sizeof(MENUITEMINFO);
		for (int f = 0; f < fxCount; ++f) {
			itemInfo.fMask = MIIM_TYPE | MIIM_ID;
			itemInfo.fType = MFT_STRING;
			itemInfo.wID = fxList[f].first + 1;
			itemInfo.dwTypeData = (char*)fxList[f].second.c_str();
			itemInfo.cch = fxList[f].second.length();
			InsertMenuItem(effects, f, true, &itemInfo);
		}
		fx = TrackPopupMenu(effects, TPM_NONOTIFY | TPM_RETURNCMD, 0, 0, 0, mainHwnd, NULL) - 1;
		DestroyMenu(effects);
		if (fx == -1)
			return; // Cancelled.
	}

	FxParams<ReaperObj>* source = new FxParams<ReaperObj>(obj, apiPrefix, fx);
	new ParamsDialog(source);
}

void cmdFxParamsFocus(Command* command) {
	switch (fakeFocus) {
		case FOCUS_TRACK: {
			MediaTrack* track = GetLastTouchedTrack();
			if (!track)
				return;
			fxParams_begin(track, "TrackFX");
			break;
		}
		case FOCUS_ITEM: {
			MediaItem* item = GetSelectedMediaItem(0, 0);
			if (!item)
				return;
			MediaItem_Take* take = GetActiveTake(item);
			if (!take)
				return;
			fxParams_begin(take, "TakeFX");
			break;
		}
		default:
			break;
	}
}

void cmdFxParamsMaster(Command* command) {
	fxParams_begin(GetMasterTrack(0), "TrackFX");
}
