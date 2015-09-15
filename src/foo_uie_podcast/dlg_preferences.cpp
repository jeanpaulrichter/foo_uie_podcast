#include "stdafx.h"
#include "dlg_preferences.h"
#include "resource.h"
#include "mdecl.h"

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

const GUID DlgPreference::m_guid = { 0x52b450ac, 0xc869, 0x4af4, { 0x81, 0x31, 0xef, 0x32, 0x32, 0x34, 0x11, 0x85 } };
// {52B450AC-C869-4AF4-8131-EF3232341185}

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

enum class Help : unsigned { output_folder, playlist1, playlist2, filename, autoupdate, dl_range, dl_threads, main, info };

static TCHAR* help_strings[] = { 
/* output_folder */	TEXT("Output-folder for downloaded podcasts."), 
/* playlist1	 */	TEXT("Name of the playlist that will consist of all downloaded podcasts. If you don't want this playlist to be created, \"uncheck\" the checkbox."),
/* playlist2	 */	TEXT("Name of the playlist that will show the content of selected podcast-feeds. If you don't want this playlist to be created, \"uncheck\" the checkbox."),
/* filename		 */	TEXT("Filename for downloaded podcasts. Supported tags: \n%filename% - original filename of download)\n%feedtitle% - title of feed\n%feedauthor% - author of feed\n%title% - title of podcast\n%ext% - extension (ie.: mp3)"),
/* autoupdate	 */	TEXT("Search for new podcasts in this intervall (minutes)"),
/* dl_range		 */	TEXT("Download all podcasts that were release in the past X days."),
/* dl_threads	 */	TEXT("Number of parallel downloads."),
/* main			 */	TEXT(COMPONENT_NAME)TEXT(" adds two columnUI-panels: feeds and downloads. You can change the column-order of both panels via drag-and-drop. Rightclick on the column-header to costumize it further. Rightclick on feeds-panel to add feeds, update etc. Rightclick on a feed to costumize it. If the 'Retag-Download'-Flag is set, downloaded files will be retaged using 'title' and 'author' of the feed plus 'title' and 'pubdate' of the item. Genre will be set to 'Podcast'."),
/* info			 */	TEXT("# ")TEXT(COMPONENT_NAME)TEXT(" (")TEXT(COMPONENT_DLL)TEXT(") ")TEXT(COMPONENT_VERSION_STR)TEXT("#\n")TEXT(COMPONENT_HELP_URL)
};

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

DlgPreference::DlgPreference()
{
}

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

DlgPreference::~DlgPreference()
{
}

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

preferences_page_instance::ptr DlgPreference::instantiate(HWND parent, preferences_page_callback::ptr callback)
{
	m_callback = callback;
	return new service_impl_t<pref_page_instance_impl>(this, create(parent));
}

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

HWND DlgPreference::create(HWND p_parent)
{
	m_hwnd_parent = p_parent;
	m_hwnd = CreateDialogParam(core_api::get_my_instance(), MAKEINTRESOURCE(IDD_PREFERENCES), p_parent, (DLGPROC)dlgProc, (LPARAM)this);
	return m_hwnd;
}

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

const char* DlgPreference::get_name() 
{
	return COMPONENT_NAME;
};

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

GUID DlgPreference::get_guid()
{
	return m_guid;
};

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

GUID DlgPreference::get_parent_guid()
{
	return guid_tools;
};

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

bool DlgPreference::get_help_url(pfc::string_base & p_out)
{
	p_out = COMPONENT_HELP_URL;
	return true;
};

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

BOOL CALLBACK DlgPreference::dlgProc(HWND hWnd, UINT Message, WPARAM wParam, LPARAM lParam)
{
	switch (Message)
	{
	case WM_INITDIALOG:
	{
		DlgPreference *pDlgPreference = (DlgPreference *)(lParam);
		pDlgPreference->m_hwnd = hWnd;
		::SetWindowLongPtr(hWnd, GWL_USERDATA, (long)lParam);
		pDlgPreference->run_dlgProc(Message, wParam, lParam);
		return TRUE;
	}

	default:
	{
		DlgPreference *pDlgPreference = reinterpret_cast<DlgPreference *>(::GetWindowLong(hWnd, GWL_USERDATA));
		if (!pDlgPreference)
			return FALSE;
		return pDlgPreference->run_dlgProc(Message, wParam, lParam);
	}

	}
	return FALSE;
}

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

BOOL CALLBACK DlgPreference::run_dlgProc(UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{

	case WM_INITDIALOG:
	{
		::SendDlgItemMessage(m_hwnd, IDC_PREF_PLAYLIST1, EM_LIMITTEXT, 60, 0);
		::SendDlgItemMessage(m_hwnd, IDC_PREF_PLAYLIST2, EM_LIMITTEXT, 60, 0);
		::SendDlgItemMessage(m_hwnd, IDC_PREF_FILENAMES, EM_LIMITTEXT, 100, 0);

		pfc::stringcvt::string_os_from_utf8 os_string_temp(options::output_dir);
		::SetDlgItemText(m_hwnd, IDC_PREF_OUTPUTDIR, const_cast<TCHAR*>(os_string_temp.get_ptr()));
		os_string_temp = pfc::stringcvt::string_os_from_utf8(options::playlist1);
		::SetDlgItemText(m_hwnd, IDC_PREF_PLAYLIST1, const_cast<TCHAR*>(os_string_temp.get_ptr()));
		os_string_temp = pfc::stringcvt::string_os_from_utf8(options::playlist2);
		::SetDlgItemText(m_hwnd, IDC_PREF_PLAYLIST2, const_cast<TCHAR*>(os_string_temp.get_ptr()));
		os_string_temp = pfc::stringcvt::string_os_from_utf8(options::filename_mask);
		::SetDlgItemText(m_hwnd, IDC_PREF_FILENAMES, const_cast<TCHAR*>(os_string_temp.get_ptr()));

		CheckDlgButton(m_hwnd, IDC_PREF_PLAYLIST1_CHECK, (options::playlist1_use) ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(m_hwnd, IDC_PREF_PLAYLIST2_CHECK, (options::playlist2_use) ? BST_CHECKED : BST_UNCHECKED);

		::SendDlgItemMessage(m_hwnd, IDC_PREF_AUTOUPDATE_SPIN, UDM_SETRANGE, true, (LPARAM)MAKELONG(120, 5));
		::SendDlgItemMessage(m_hwnd, IDC_PREF_AUTOUPDATE_SPIN, UDM_SETBUDDY, (WPARAM)GetDlgItem(m_hwnd, IDC_PREF_AUTOUPDATE), 0);
		::SendDlgItemMessage(m_hwnd, IDC_PREF_AUTOUPDATE_SPIN, UDM_SETPOS32, 0, options::autoupdate_minutes);

		::SendDlgItemMessage(m_hwnd, IDC_PREF_DRANGE_SPIN, UDM_SETRANGE, true, (LPARAM)MAKELONG(31, 1));
		::SendDlgItemMessage(m_hwnd, IDC_PREF_DRANGE_SPIN, UDM_SETBUDDY, (WPARAM)GetDlgItem(m_hwnd, IDC_PREF_DRANGE), 0);
		::SendDlgItemMessage(m_hwnd, IDC_PREF_DRANGE_SPIN, UDM_SETPOS32, 0, options::download_days);

		::SendDlgItemMessage(m_hwnd, IDC_PREF_DTHREADS_SPIN, UDM_SETRANGE, true, (LPARAM)MAKELONG(6, 1));
		::SendDlgItemMessage(m_hwnd, IDC_PREF_DTHREADS_SPIN, UDM_SETBUDDY, (WPARAM)GetDlgItem(m_hwnd, IDC_PREF_DTHREADS), 0);
		::SendDlgItemMessage(m_hwnd, IDC_PREF_DTHREADS_SPIN, UDM_SETPOS32, 0, options::download_threads);

		::SetDlgItemText(m_hwnd, IDC_PREF_STATICBOX, TEXT(COMPONENT_NAME));

		log_error = false;
		is_changed = false;
		return false;
	}

		case WM_COMMAND:
	{
		if (HIWORD(wParam) == EN_SETFOCUS) {
			log_error = false;
			switch (LOWORD(wParam)) {
			case IDC_PREF_OUTPUTDIR:
				::SetDlgItemText(m_hwnd, IDC_PREF_LOG, help_strings[unsigned(Help::output_folder)]);
				break;
			case IDC_PREF_PLAYLIST1:
				::SetDlgItemText(m_hwnd, IDC_PREF_LOG, help_strings[unsigned(Help::playlist1)]);
				break;
			case IDC_PREF_PLAYLIST2:
				::SetDlgItemText(m_hwnd, IDC_PREF_LOG, help_strings[unsigned(Help::playlist2)]);
				break;
			case IDC_PREF_FILENAMES:
				::SetDlgItemText(m_hwnd, IDC_PREF_LOG, help_strings[unsigned(Help::filename)]);
				break;
			case IDC_PREF_AUTOUPDATE:
				::SetDlgItemText(m_hwnd, IDC_PREF_LOG, help_strings[unsigned(Help::autoupdate)]);
				break;
			case IDC_PREF_DRANGE:
				::SetDlgItemText(m_hwnd, IDC_PREF_LOG, help_strings[unsigned(Help::dl_range)]);
				break;
			case IDC_PREF_DTHREADS:
				::SetDlgItemText(m_hwnd, IDC_PREF_LOG, help_strings[unsigned(Help::dl_threads)]);
				break;
			}
		}
		else if (HIWORD(wParam) == EN_CHANGE) {
			if (!is_changed) {
				is_changed = true;
				m_callback->on_state_changed();
			}
		}
		else if (HIWORD(wParam) == BN_CLICKED) {
			if (LOWORD(wParam) == IDC_PREF_BROWSE) {
				set_output_folder();
			}
			else {
				log_error = false;
				if (LOWORD(wParam) == IDC_PREF_HELP) {
					::SetDlgItemText(m_hwnd, IDC_PREF_LOG, help_strings[unsigned(Help::main)]);
				}
				else if (LOWORD(wParam) == IDC_PREF_INFO) {
					::SetDlgItemText(m_hwnd, IDC_PREF_LOG, help_strings[unsigned(Help::info)]);
				}
				else {
					if (LOWORD(wParam) == IDC_PREF_PLAYLIST1_CHECK)
						::SetDlgItemText(m_hwnd, IDC_PREF_LOG, help_strings[unsigned(Help::playlist1)]);
					else if (LOWORD(wParam) == IDC_PREF_PLAYLIST2_CHECK)
						::SetDlgItemText(m_hwnd, IDC_PREF_LOG, help_strings[unsigned(Help::playlist2)]);
					if (!is_changed) {
						is_changed = true;
						m_callback->on_state_changed();
					}
				}
			}
		}
	}
	case WM_CTLCOLORSTATIC:
		if ((HWND)lParam == GetDlgItem(m_hwnd, IDC_PREF_LOG))
		{
			if (log_error) {
				SetBkMode((HDC)wParam, TRANSPARENT);
				SetTextColor((HDC)wParam, RGB(200, 80, 80));
				return (BOOL)GetSysColorBrush(COLOR_MENU);
			}
			else {
				SetBkMode((HDC)wParam, TRANSPARENT);
				SetTextColor((HDC)wParam, RGB(80, 80, 80));
				return (BOOL)GetSysColorBrush(COLOR_MENU);
			}
		}
		break;
	}
	return FALSE;
}

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

void DlgPreference::add_log_string(std::vector<TCHAR>& buf, const TCHAR* s)
{
	size_t cur_size = buf.size();
	if (cur_size == 0)
		cur_size++;
	#ifdef UNICODE
	size_t s_len = wcslen(s);
	buf.resize(cur_size + s_len);
	wcscpy_s(&buf[cur_size - 1], s_len + 1, s);
	#else
	size_t s_len = strlen(s);
	buf.resize(cur_size + s_len);
	strcpy_s(&buf[cur_size - 1], s_len + 1, s);
	#endif
	buf[cur_size + s_len - 1] = 0;
}

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

void DlgPreference::set_output_folder()
{

	IFileDialog *pfd = NULL;
	DWORD dwFlags;

	try {
		if (!SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pfd))))
			throw std::exception();
		if (!SUCCEEDED(pfd->GetOptions(&dwFlags)))
			throw std::exception();
		if (!SUCCEEDED(pfd->SetOptions(dwFlags | FOS_FORCEFILESYSTEM | FOS_PICKFOLDERS | FOS_PATHMUSTEXIST)))
			throw std::exception();
		if (!SUCCEEDED(pfd->Show(NULL)))
			throw std::exception();

		IShellItem *psiResult;
		if (SUCCEEDED(pfd->GetResult(&psiResult)))
		{
			PWSTR pszFilePath = NULL;
			if (SUCCEEDED(psiResult->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath)))
			{
				::SetDlgItemText(m_hwnd, IDC_PREF_OUTPUTDIR, pszFilePath);
				CoTaskMemFree(pszFilePath);
			}
			psiResult->Release();
		}
	}
	catch (std::exception&) {
		if (pfd)
			pfd->Release();
	}
}

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

void DlgPreference::reset()
{
	::SetDlgItemText(m_hwnd, IDC_PREF_OUTPUTDIR, TEXT("C:\\Users\\Public\\Music\\Podcasts"));
	::SetDlgItemText(m_hwnd, IDC_PREF_PLAYLIST1, TEXT("Podcasts::new"));
	::SetDlgItemText(m_hwnd, IDC_PREF_PLAYLIST2, TEXT("Podcasts::feed"));
	::SetDlgItemText(m_hwnd, IDC_PREF_FILENAMES, TEXT("%filename%"));

	::SendDlgItemMessage(m_hwnd, IDC_PREF_AUTOUPDATE_SPIN, UDM_SETPOS32, 0, 30);
	::SendDlgItemMessage(m_hwnd, IDC_PREF_DRANGE_SPIN, UDM_SETPOS32, 0, 7);
	::SendDlgItemMessage(m_hwnd, IDC_PREF_DTHREADS_SPIN, UDM_SETPOS32, 0, 3);

	CheckDlgButton(m_hwnd, IDC_PREF_PLAYLIST1_CHECK, BST_CHECKED);
	CheckDlgButton(m_hwnd, IDC_PREF_PLAYLIST2_CHECK, BST_CHECKED);
}

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

void DlgPreference::apply()
{
	TCHAR tbuf[MAX_PATH];
	std::vector<TCHAR> logbuf;
	logbuf.reserve(255);
	RSS::Options opt;

	// ------------------- playlist1
	if (GetWindowTextLength(::GetDlgItem(m_hwnd, IDC_PREF_PLAYLIST1)) > 0)
	{
		::GetDlgItemText(m_hwnd, IDC_PREF_PLAYLIST1, tbuf, MAX_PATH);
		options::playlist1 = pfc::stringcvt::string_utf8_from_os(tbuf);
	}
	else {
		add_log_string(logbuf, TEXT("[ERROR] playlist [main]: please enter valid name.\n"));
	}
	// ------------------- playlist2
	if (GetWindowTextLength(::GetDlgItem(m_hwnd, IDC_PREF_PLAYLIST2)) > 0)
	{
		::GetDlgItemText(m_hwnd, IDC_PREF_PLAYLIST2, tbuf, MAX_PATH);
		options::playlist2 = pfc::stringcvt::string_utf8_from_os(tbuf);
	}
	else {
		add_log_string(logbuf, TEXT("[ERROR] playlist [temp]: please enter valid name.\n"));
	}
	// ------------------- output dir
	::GetDlgItemText(m_hwnd, IDC_PREF_OUTPUTDIR, tbuf, MAX_PATH);
	DWORD dwAttrib = GetFileAttributes(tbuf);
	if (dwAttrib != INVALID_FILE_ATTRIBUTES && (dwAttrib & FILE_ATTRIBUTE_DIRECTORY))
	{
		opt.output_dir = pfc::stringcvt::string_utf8_from_os(tbuf);
		if (opt.output_dir.compare(options::output_dir) != 0) {
			opt.flags |= RSS::Options::OUTPUT_DIR;
		}
	}
	else {
		add_log_string(logbuf, TEXT("[ERROR] output-path: please choose valid path.\n"));
	}
	// ------------------- filenamemask
	if (GetWindowTextLength(::GetDlgItem(m_hwnd, IDC_PREF_FILENAMES)) > 0)
	{
		::GetDlgItemText(m_hwnd, IDC_PREF_FILENAMES, tbuf, MAX_PATH);
		opt.filename_mask = pfc::stringcvt::string_utf8_from_os(tbuf);
		if (opt.filename_mask.compare(options::filename_mask) != 0) {
			opt.flags |= RSS::Options::FILENAME_MASK;
		}
	}
	else {
		add_log_string(logbuf, TEXT("[ERROR] filenames: please enter valid filename mask.\n"));
	}
	// ------------------- Autoupdate
	opt.autoupdate_minutes = ::SendDlgItemMessage(m_hwnd, IDC_PREF_AUTOUPDATE_SPIN, UDM_GETPOS32, 0, 0);
	if (opt.autoupdate_minutes != options::autoupdate_minutes)
		opt.flags |= RSS::Options::AUTOUPDATE;
	// ------------------- Download-Range
	opt.download_range = ::SendDlgItemMessage(m_hwnd, IDC_PREF_DRANGE_SPIN, UDM_GETPOS32, 0, 0);
	if (opt.download_range != options::download_days)
		opt.flags |= RSS::Options::DL_RANGE;
	// ------------------- Download-Threads
	opt.download_threads_count = ::SendDlgItemMessage(m_hwnd, IDC_PREF_DTHREADS_SPIN, UDM_GETPOS32, 0, 0);
	if (opt.download_threads_count != options::download_threads)
		opt.flags |= RSS::Options::DL_THREADS;

	int ret = database.SetOptions(opt);

	if ((opt.flags & RSS::Options::OUTPUT_DIR) == RSS::Options::OUTPUT_DIR) {
		if ((ret & RSS::Options::OUTPUT_DIR) == RSS::Options::OUTPUT_DIR)
			options::output_dir.set_string(opt.output_dir.c_str());
		else
			add_log_string(logbuf, TEXT("[ERROR] you cannot change the output-dir when there are active downloads."));
	}
	if ((opt.flags & RSS::Options::FILENAME_MASK) == RSS::Options::FILENAME_MASK) {
		if ((ret & RSS::Options::FILENAME_MASK) == RSS::Options::FILENAME_MASK)
			options::filename_mask.set_string(opt.filename_mask.c_str());
		else
			add_log_string(logbuf, TEXT("[ERROR] failed to set filename-mask."));
	}
	if ((opt.flags & RSS::Options::DL_RANGE) == RSS::Options::DL_RANGE) {
		if ((ret & RSS::Options::DL_RANGE) == RSS::Options::DL_RANGE)
			options::download_days = opt.download_range;
		else
			add_log_string(logbuf, TEXT("[ERROR] failed to set download-range."));
	}
	if ((opt.flags & RSS::Options::AUTOUPDATE) == RSS::Options::AUTOUPDATE) {
		if ((ret & RSS::Options::AUTOUPDATE) == RSS::Options::AUTOUPDATE)
			options::autoupdate_minutes = opt.autoupdate_minutes;
		else
			add_log_string(logbuf, TEXT("[ERROR] failed to set autoupdate-time."));
	}
	if ((opt.flags & RSS::Options::DL_THREADS) == RSS::Options::DL_THREADS) {
		if ((ret & RSS::Options::DL_THREADS) == RSS::Options::DL_THREADS)
			options::download_threads = opt.download_threads_count;
		else
			add_log_string(logbuf, TEXT("[ERROR] download threads: please wait for current downloads to finish."));
	}
	options::playlist1_use = ::SendDlgItemMessage(m_hwnd, IDC_PREF_PLAYLIST1_CHECK, BM_GETCHECK, 0, 0) ? true : false;
	options::playlist2_use = ::SendDlgItemMessage(m_hwnd, IDC_PREF_PLAYLIST2_CHECK, BM_GETCHECK, 0, 0) ? true : false;

	if (logbuf.size() > 0)
	{
		log_error = true;
		::SetDlgItemText(m_hwnd, IDC_PREF_LOG, &logbuf[0]);
		is_changed = true;
		m_callback->on_state_changed();
	}
	else {
		::SetDlgItemText(m_hwnd, IDC_PREF_LOG, TEXT(""));
		is_changed = false;
		m_callback->on_state_changed();
	}
}

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

t_uint32 DlgPreference::get_state()
{
	t_uint32 state = preferences_state::resettable;
	if (is_changed) state |= preferences_state::changed;
	return state;
}

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

preferences_page_factory_t<DlgPreference> pref_factory;

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------