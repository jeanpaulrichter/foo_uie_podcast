#ifndef DEF_H_LISTVIEWPANEL
#define DEF_H_LISTVIEWPANEL

#include "stdafx.h"

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

class cfg_lw_columns : public cfg_var
{
public:
	cfg_lw_columns(const GUID & id, size_t count, const TCHAR** def_strings, const int* def_widths);
	const TCHAR* getString(size_t i) const;
	const TCHAR* getDefaultString(size_t i) const;
	void setString(size_t i, const TCHAR* str);
	size_t size() const;
	size_t maxsize() const;
	size_t getOrder(size_t i);
	int getColumn(size_t index);
	void setWidth(size_t i, int w);
	int getWidth(size_t i);
	void change_order(size_t from, size_t to);
	void change_visible(size_t i);
	bool isVisible(size_t index);	

private:
	void get_data_raw(stream_writer * p_stream, abort_callback & p_abort);
	void set_data_raw(stream_reader * p_stream, t_size p_sizehint, abort_callback & p_abort);

	mutable critical_section		m_sync;
	std::vector<std::vector<TCHAR>> labels;
	std::vector<int>				widths;
	std::vector<bool>				visible;
	std::vector<size_t>				order;
	size_t							default_count;
	const TCHAR**					default_strings;
};

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

class UIEPanelManager
{
public:
	UIEPanelManager(size_t panel_count);
	void AddPanel(size_t id, HWND wnd);
	void RemovePanel(size_t id, HWND wnd);
	void PostMsg(size_t id, int uMsg, WPARAM wParam, LPARAM lParam);
	void SetColumns(size_t id, cfg_lw_columns* columns);

private:
	std::vector<std::list<HWND>>	panels;
	std::deque<boost::mutex>		mutex;
	size_t							count;
};

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

#define WM_UIELW_HIGHLIGHT_ITEM (WM_USER + 0x0001)

class UIEListViewPanel : public uie::container_ui_extension
{
public:
	UIEListViewPanel(size_t id, cfg_lw_columns& columns, 
						boost::function<size_t()> getcount, 
						boost::function<const TCHAR*(size_t,size_t)> getstring);
	~UIEListViewPanel();

	virtual const GUID & get_extension_guid() const =0;
	virtual void get_name(pfc::string_base & out) const =0;
	void get_category(pfc::string_base & out)const;
	unsigned get_type() const;

protected:
	virtual class_data & get_class_data()const =0;
	virtual void onItemClick(size_t index, size_t col) {};
	virtual void onRClick(int index, int x, int y) {};
	virtual void onColumnClick(int index) {};

	HINSTANCE			m_hinstance;
	HWND				m_hwnd_list;
	HWND				m_hwnd_header;

	static LRESULT WINAPI list_proc(HWND wnd, UINT msg, WPARAM wp, LPARAM lp);
	static LRESULT WINAPI header_proc(HWND wnd, UINT msg, WPARAM wp, LPARAM lp);

private:
	LRESULT on_message(HWND wnd, UINT msg, WPARAM wp, LPARAM lp);
	LRESULT on_list(HWND wnd, UINT msg, WPARAM wp, LPARAM lp);
	LRESULT on_header(HWND wnd, UINT msg, WPARAM wp, LPARAM lp);


	size_t				m_id;
	WNDPROC				m_listproc;
	WNDPROC				m_headerproc;
	int					m_drag_column_id;	
	HMENU				m_column_menu;
	cfg_lw_columns&		m_columns;
	int					m_highlight_item;

	boost::function<size_t()>							f_getcount;
	boost::function<const TCHAR*(size_t, size_t)>		f_getstring;
};



#endif