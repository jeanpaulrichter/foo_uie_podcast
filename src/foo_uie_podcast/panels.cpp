#include "stdafx.h"
#include "panels.h"
#include "dlg_inputbox.h"
#include "mdecl.h"

// =========================================================================================================================================================================
//                    UIEPanelManager
// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

UIEPanelManager::UIEPanelManager(size_t panel_count) : count(panel_count)
{
	panels.resize(count);
	mutex.resize(count);
}

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

void UIEPanelManager::AddPanel(size_t id, HWND wnd)
{
	if (id < count)
	{
		boost::lock_guard<boost::mutex> lock(mutex[id]);
		panels[id].push_back(wnd);
	}
}

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

void UIEPanelManager::RemovePanel(size_t id, HWND wnd)
{
	if (id < count)
	{
		boost::lock_guard<boost::mutex> lock(mutex[id]);
		panels[id].remove(wnd);
	}
}

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

void UIEPanelManager::PostMsg(size_t id, int uMsg, WPARAM wParam, LPARAM lParam)
{
	if (id < count)
	{
		boost::lock_guard<boost::mutex> lock(mutex[id]);
		std::list<HWND>::const_iterator Iter;
		std::list<HWND>::const_iterator IterEnd;

		for (Iter = panels[id].begin(), IterEnd = panels[id].end(); Iter != IterEnd; ++Iter)
			::PostMessage(*Iter, uMsg, wParam, lParam);
	}
}

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

void UIEPanelManager::SetColumns(size_t id, cfg_lw_columns* columns)
{
	if (id < count)
	{
		boost::lock_guard<boost::mutex> lock(mutex[id]);
		std::list<HWND>::const_iterator Iter;
		std::list<HWND>::const_iterator IterEnd;

		for (Iter = panels[id].begin(), IterEnd = panels[id].end(); Iter != IterEnd; ++Iter) {
			LVCOLUMN c;
			c.mask = LVCF_TEXT | LVCF_WIDTH;
			size_t i;
			HWND header = ListView_GetHeader(*Iter);
			size_t cur_count = Header_GetItemCount(header);
			for (i = 0; i < columns->size() && i < cur_count; i++)
			{
				c.pszText = (TCHAR*)columns->getString(i);
				c.cx = columns->getWidth(i);
				ListView_SetColumn(*Iter, i, &c);
			}
			c.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT;
			c.fmt = LVCFMT_LEFT;
			for (; i < columns->size(); i++) {
				c.pszText = (TCHAR*)columns->getString(i);
				c.cx = columns->getWidth(i);
				uSendMessage(*Iter, LVM_INSERTCOLUMN, i, (LPARAM)&c);
			}
				
			for (; i < 6; i++)
				ListView_DeleteColumn(*Iter, i);
		}
	}
}

// =========================================================================================================================================================================
//                    cfg_lw_columns
// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

cfg_lw_columns::cfg_lw_columns(const GUID & id, size_t count, const TCHAR** def_strings, const int* def_widths) 
	: cfg_var(id), default_strings(def_strings), default_count(count)
{
	labels.resize(count);
	for (size_t i = 0; i < count; i++) {
		#ifdef UNICODE
		labels[i].resize(wcslen(def_strings[i]) + 1);
		wcscpy_s(&labels[i][0], labels[i].size(), def_strings[i]);
		#else
		labels[i].resize(strlen(def_strings[i]) + 1);
		strcpy_s(&labels[i][0], labels[i].size(), def_strings[i]);
		#endif
	}
	order.resize(count);
	widths.resize(count);
	visible.resize(count);
	for (size_t i = 0; i < count; i++)
	{
		order[i] = i;
		widths[i] = def_widths[i];
		visible[i] = true;
	}
}

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

const TCHAR* cfg_lw_columns::getString(size_t i) const
{
	insync(m_sync);
	if (i < order.size())
		return &labels[order[i]][0];
	return NULL;
}

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

const TCHAR* cfg_lw_columns::getDefaultString(size_t i) const
{
	insync(m_sync);
	if (i < default_count)
		return default_strings[i];
	return NULL;
}

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

void cfg_lw_columns::setString(size_t i, const TCHAR* str)
{
	insync(m_sync);
	if (i >= order.size() || !str)
		return;
	#ifdef UNICODE
	size_t len = wcslen(str);
	labels[order[i]].resize(len + 1);
	wcscpy_s(&labels[order[i]][0], len + 1, str);
	#else
	size_t len = strlen(str);
	labels[order[i]].resize(len + 1);
	strcpy_s(&labels[order[i]][0], len + 1, str);
	#endif
}

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

size_t cfg_lw_columns::size() const
{
	insync(m_sync);
	return order.size();
}

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

size_t cfg_lw_columns::maxsize() const
{
	insync(m_sync);
	return default_count;
}

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

size_t cfg_lw_columns::getOrder(size_t i)
{
	if (i < order.size())
		return order[i];
	else
		return 0;
}

int cfg_lw_columns::getColumn(size_t index)
{
	for (size_t i = 0; i < order.size(); i++)
		if (order[i] == index)
			return (int)i;
	return -1;
}

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

void cfg_lw_columns::setWidth(size_t i, int w)
{
	insync(m_sync);
	if (i < order.size())
		widths[order[i]] = w;
}

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

int cfg_lw_columns::getWidth(size_t i)
{
	insync(m_sync);
	if (i < order.size())
		return widths[order[i]];
	return 100;
}

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

void cfg_lw_columns::change_order(size_t from, size_t to)
{
	insync(m_sync);
	if (from < order.size() && from != to) {
		if (to >= order.size())
			to = order.size() - 1;
		size_t x = order[from];
		if (from > to) {
			for (size_t i = from; i > to; i--)
				order[i] = order[i - 1];
			order[to] = x;
		}
		else {
			for (size_t i = from; i < to; i++)
				order[i] = order[i + 1];
			order[to] = x;
		}
	}
}

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

void cfg_lw_columns::change_visible(size_t i)
{
	insync(m_sync);
	if (i >= visible.size())
		return;
	if (visible[i]) {
		visible[i] = false;
		for (size_t x = 0; x < order.size(); x++) {
			if (order[x] == i) {
				order.erase(order.begin() + x);
				break;
			}
		}
	}
	else {
		order.push_back(i);
		visible[i] = true;
	}
}

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

bool cfg_lw_columns::isVisible(size_t index)
{
	if (index < visible.size())
		return visible[index];
	else
		return false;
}

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

void cfg_lw_columns::get_data_raw(stream_writer * p_stream, abort_callback & p_abort)
{
	insync(m_sync);
	size_t tsize = order.size();
	p_stream->write_lendian_t(tsize, p_abort);
	for (size_t i = 0; i < order.size(); i++)
		p_stream->write_lendian_t(order[i], p_abort);

	for (size_t i = 0; i < default_count; i++)
	{
		p_stream->write_lendian_t(widths[i], p_abort);
		tsize = labels[i].size();
		p_stream->write_lendian_t(tsize, p_abort);
		p_stream->write(&labels[i][0], tsize * sizeof(TCHAR), p_abort);
	}
}

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

void cfg_lw_columns::set_data_raw(stream_reader * p_stream, t_size p_sizehint, abort_callback & p_abort)
{
	size_t tsize;
	p_stream->read_lendian_t(tsize, p_abort);
	order.resize(tsize);
	for (size_t i = 0; i < visible.size(); i++)
		visible[i] = false;
	for (size_t i = 0; i < order.size(); i++) {
		p_stream->read_lendian_t(order[i], p_abort);
		visible[order[i]] = true;
	}

	for (size_t i = 0; i < default_count; i++)
	{
		p_stream->read_lendian_t(widths[i], p_abort);
		p_stream->read_lendian_t(tsize, p_abort);
		labels[i].resize(tsize);
		p_stream->read(&labels[i][0], tsize * sizeof(TCHAR), p_abort);
	}
}

// =========================================================================================================================================================================
//                    UIEListViewPanel
// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------


UIEListViewPanel::UIEListViewPanel(size_t id, cfg_lw_columns& columns, boost::function<size_t()> getcount,
	boost::function<const TCHAR*(size_t, size_t)> getstring)
		: m_hwnd_list(NULL), m_hwnd_header(NULL), m_drag_column_id(-1), m_columns(columns), m_id(id), f_getcount(getcount), f_getstring(getstring), m_column_menu(NULL), m_highlight_item(-1)
{}

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

UIEListViewPanel::~UIEListViewPanel()
{}

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

void UIEListViewPanel::get_category(pfc::string_base & out) const
{
	out.set_string("Panels");
}

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

unsigned UIEListViewPanel::get_type() const
{
	return uie::type_panel;
}

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

LRESULT UIEListViewPanel::on_message(HWND wnd, UINT msg, WPARAM wp, LPARAM lp)
{

	switch (msg)
	{
	case WM_CREATE:
	{
		m_hinstance = core_api::get_my_instance();
		m_hwnd_list = CreateWindowEx(0, WC_LISTVIEW, _T("listviewpanel"),
			WS_CHILD | WS_VISIBLE | LVS_OWNERDATA | LVS_SINGLESEL | LVS_SHOWSELALWAYS | LVS_REPORT | LVS_NOSORTHEADER | LVS_EX_FULLROWSELECT, 0, 0, 0, 0, wnd, HMENU(0), m_hinstance, NULL);

		if (m_hwnd_list)
		{
			ListView_SetExtendedListViewStyleEx(m_hwnd_list, LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT);
			ListView_SetExtendedListViewStyleEx(m_hwnd_list, LVS_EX_DOUBLEBUFFER, LVS_EX_DOUBLEBUFFER);

			panelmanager.AddPanel(m_id, m_hwnd_list);

			// ----------- Setup message handlers
			SetWindowLongPtr(m_hwnd_list, GWL_USERDATA, (LPARAM)(this));
			m_listproc = (WNDPROC)SetWindowLongPtr(m_hwnd_list, GWL_WNDPROC, (LPARAM)(list_proc));
			m_hwnd_header = ListView_GetHeader(m_hwnd_list);
			SetWindowLongPtr(m_hwnd_header, GWL_USERDATA, (LPARAM)(this));
			m_headerproc = (WNDPROC)SetWindowLongPtr(m_hwnd_header, GWL_WNDPROC, (LPARAM)(header_proc));

			// ----------- Setup columns
			RECT rect = { 0, 0, 0, 0 };
			LVCOLUMN data = {};
			data.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT;
			data.fmt = LVCFMT_LEFT;
			for (size_t i = 0; i < m_columns.size(); i++) 
			{
				rect.right = m_columns.getWidth(i);
				MapDialogRect(wnd, &rect);
				data.cx = rect.right;
				data.pszText = (TCHAR *)m_columns.getString(i);
				uSendMessage(m_hwnd_list, LVM_INSERTCOLUMN, i, (LPARAM)&data);
			}

			// ------------ Create columns menu
			m_column_menu = CreatePopupMenu();
			AppendMenu(m_column_menu, MF_STRING, 1, TEXT("Edit label"));
			AppendMenu(m_column_menu, MF_SEPARATOR, NULL, NULL);
			for (size_t i = 0; i < m_columns.maxsize(); i++)
				AppendMenu(m_column_menu, MF_STRING, 2+i, m_columns.getDefaultString(i));

			::SendMessage(m_hwnd_list, LVM_SETITEMCOUNT, f_getcount(), LVSICF_NOSCROLL);
		}
	} break;

	case WM_SIZE:
		SetWindowPos(m_hwnd_list, 0, 0, 0, LOWORD(lp), HIWORD(lp), SWP_NOZORDER);
		break;
	case WM_DESTROY:
		panelmanager.RemovePanel(m_id, m_hwnd_list);
		if (m_column_menu)
			DestroyMenu(m_column_menu);
		m_column_menu = NULL;		
		m_hwnd_list = NULL;
		m_hwnd_header = NULL;
		break;
	case WM_NOTIFY:
		if (((LPNMHDR)lp)->hwndFrom == m_hwnd_list)
			return on_list(wnd, msg, wp, lp);
	}
	return DefWindowProc(wnd, msg, wp, lp);
}

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

LRESULT UIEListViewPanel::on_list(HWND wnd, UINT msg, WPARAM wp, LPARAM lp)
{
	switch (msg)
	{
	case WM_UIELW_HIGHLIGHT_ITEM:
		m_highlight_item = wp;
		return 0;

	case WM_LBUTTONDOWN:
	{
		LPNMITEMACTIVATE	item = (LPNMITEMACTIVATE)lp;
		LVHITTESTINFO		hinfo;
		hinfo.pt.x = LOWORD(lp);
		hinfo.pt.y = HIWORD(lp);
		if (ListView_SubItemHitTest(m_hwnd_list, &hinfo) >= 0) 
		{
			ListView_SetItemState(m_hwnd_list, hinfo.iItem, LVIS_SELECTED, LVIS_SELECTED);
			onItemClick(size_t(hinfo.iItem), size_t(hinfo.iSubItem));
		}
		return 0;
	} break;

	case WM_RBUTTONDOWN:
	{
		LPNMITEMACTIVATE	item = (LPNMITEMACTIVATE)lp;
		LVHITTESTINFO		hinfo;
		RECT				rc;
		hinfo.pt.x = LOWORD(lp);
		hinfo.pt.y = HIWORD(lp);
		ListView_SubItemHitTest(m_hwnd_list, &hinfo);
		GetWindowRect(m_hwnd_list, &rc);
		onRClick(hinfo.iItem, hinfo.pt.x + rc.left, hinfo.pt.y + rc.top);
		return 0;
	} break;

	case WM_NOTIFY:
		switch (((LPNMHDR)lp)->code)
		{
		case LVN_GETDISPINFO:
		{
			LV_DISPINFO *lpdi = (LV_DISPINFO *)lp;

			if (lpdi->item.mask & LVIF_TEXT)
			{
				_tcsncpy_s(lpdi->item.pszText, lpdi->item.cchTextMax,
					f_getstring(lpdi->item.iItem, m_columns.getOrder(lpdi->item.iSubItem)), _TRUNCATE);
			}

			if (lpdi->item.mask & LVIF_IMAGE)
			{
				lpdi->item.iImage = 0;
			}
		} return 0;

		case LVN_ODCACHEHINT:
			return 0;
		case LVN_ODFINDITEM:
			return 0;

		case HDN_ENDTRACK:
		{
			LPNMHEADER info = (LPNMHEADER)lp;
			size_t i = size_t(info->iItem);
			m_columns.setWidth(i, ListView_GetColumnWidth(m_hwnd_list, i));
		} break;

		case NM_CUSTOMDRAW:
		{
			LPNMLVCUSTOMDRAW  lplvcd = (LPNMLVCUSTOMDRAW)lp;

			switch (lplvcd->nmcd.dwDrawStage)
			{
			case CDDS_PREPAINT:
				return CDRF_NOTIFYITEMDRAW;

			case CDDS_ITEMPREPAINT:
				if (lplvcd->nmcd.dwItemSpec == m_highlight_item)
				{
					lplvcd->clrTextBk = RGB(170, 190, 210);
				}
				else {
					lplvcd->clrTextBk = CLR_DEFAULT;
				}
				break;
			}
		} break;

		}
	}
	return CallWindowProc(m_listproc, wnd, msg, wp, lp);
}

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

LRESULT UIEListViewPanel::on_header(HWND wnd, UINT msg, WPARAM wp, LPARAM lp)
{

	switch (msg)
	{
	case WM_RBUTTONDOWN:
	{
		RECT rc;
		HDHITTESTINFO info;
		info.pt.x = LOWORD(lp);
		info.pt.y = HIWORD(lp);
		size_t num_visible = 0;
		size_t last_visible = 0;
		int id = SendMessage(m_hwnd_header, HDM_HITTEST, 0, (LPARAM)&info);
		GetWindowRect(m_hwnd_header, &rc);

		EnableMenuItem(m_column_menu, 1, (id != -1) ? MF_ENABLED : MF_GRAYED);
		for (size_t i = 0; i < m_columns.maxsize(); i++) {
			EnableMenuItem(m_column_menu, 2+i, MF_ENABLED);
			if (m_columns.isVisible(i)) {
				CheckMenuItem(m_column_menu, 2 + i, MF_CHECKED);
				num_visible++;
				last_visible = i;
			}
			else {
				CheckMenuItem(m_column_menu, 2 + i, MF_UNCHECKED);
			}
		}
		if (num_visible == 1)
			EnableMenuItem(m_column_menu, 2 + last_visible, MF_GRAYED);

		int cmd = TrackPopupMenu(m_column_menu, TPM_LEFTBUTTON | TPM_NONOTIFY | TPM_RETURNCMD, info.pt.x + rc.left, info.pt.y + rc.top, 0, m_hwnd_header, 0);

		if (cmd >= 2 && cmd - 2 < (int)m_columns.maxsize()) {
			m_columns.change_visible(size_t(cmd - 2));
			panelmanager.SetColumns(m_id, &m_columns);
		}
		else if (cmd == 1) {
			DlgInputbox dlg(core_api::get_my_instance(), m_hwnd_list);
			if (dlg.doDialog(TEXT("change column caption"), m_columns.getString(id))) {
				m_columns.setString(id, dlg.getString());
				panelmanager.SetColumns(m_id, &m_columns);
			}
		}
		return 0;
	}
		break;
	case WM_LBUTTONUP:
	{
		if (m_drag_column_id >= 0) {
			HDHITTESTINFO info;
			info.pt.x = LOWORD(lp);
			info.pt.y = HIWORD(lp);
			int id = SendMessage(m_hwnd_header, HDM_HITTEST, 0, (LPARAM)&info);

			if (id != m_drag_column_id) {
				m_columns.change_order(size_t(m_drag_column_id), size_t(id));
				panelmanager.SetColumns(m_id, &m_columns);
			}
			else if (id >= 0) {
				onColumnClick(int(m_columns.getOrder(id)));
			}
			m_drag_column_id = -1;
		}
	}
		break;

	case WM_LBUTTONDOWN:
	{
		HDHITTESTINFO info;
		info.pt.x = LOWORD(lp);
		info.pt.y = HIWORD(lp);
		int id = SendMessage(m_hwnd_header, HDM_HITTEST, 0, (LPARAM)&info);
		if (id >= 0 && (info.flags & HHT_ONDIVIDER) != HHT_ONDIVIDER) {
			HCURSOR c = LoadCursor(NULL, IDC_HAND);
			SetCursor(c);
			m_drag_column_id = id;
		}

	}
		break;
	}
	return CallWindowProc(m_headerproc, wnd, msg, wp, lp);
}

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

LRESULT WINAPI UIEListViewPanel::list_proc(HWND wnd, UINT msg, WPARAM wp, LPARAM lp)
{
	UIEListViewPanel* p_this = reinterpret_cast<UIEListViewPanel*>(GetWindowLongPtr(wnd, GWL_USERDATA));
	LRESULT rv = p_this ? p_this->on_list(wnd, msg, wp, lp) : DefWindowProc(wnd, msg, wp, lp);
	return rv;
}

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

LRESULT WINAPI UIEListViewPanel::header_proc(HWND wnd, UINT msg, WPARAM wp, LPARAM lp)
{
	UIEListViewPanel* p_this = reinterpret_cast<UIEListViewPanel*>(GetWindowLongPtr(wnd, GWL_USERDATA));
	LRESULT rv = p_this ? p_this->on_header(wnd, msg, wp, lp) : DefWindowProc(wnd, msg, wp, lp);
	return rv;
}