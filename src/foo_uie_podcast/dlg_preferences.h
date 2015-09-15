#ifndef DEF_H_DLG_PREFERENCES
#define DEF_H_DLG_PREFERENCES

#include "stdafx.h"

class DlgPreference : public preferences_page_v3
{
public:

	DlgPreference();
	~DlgPreference();
	preferences_page_instance::ptr instantiate(HWND parent, preferences_page_callback::ptr callback);
	
	const char * get_name();
	GUID get_guid();
	GUID get_parent_guid();
	bool get_help_url(pfc::string_base & p_out);
	void reset();
	void apply();
	t_uint32 get_state();

private:
	static BOOL CALLBACK dlgProc(HWND hWnd, UINT Message, WPARAM wParam, LPARAM lParam);
	BOOL CALLBACK run_dlgProc(UINT message, WPARAM wParam, LPARAM lParam);

	HWND create(HWND p_parent);
	void set_output_folder();
	void add_log_string(std::vector<TCHAR>& buf, const TCHAR* s);

	static const GUID				m_guid;
	HWND							m_hwnd_parent;
	HWND							m_hwnd;
	preferences_page_callback::ptr	m_callback;
	bool							is_changed;
	bool							log_error;
};

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

class pref_page_instance_impl : public preferences_page_instance
{
public:
	pref_page_instance_impl(DlgPreference* dlg, HWND hwnd) : _dlg(dlg), _hwnd(hwnd) {};

	t_uint32 get_state() { return _dlg->get_state(); };
	HWND get_wnd() { return _hwnd; };
	void apply() { _dlg->apply(); };
	void reset() { _dlg->reset(); };

private:
	DlgPreference*	_dlg;
	HWND			_hwnd;
};

#endif