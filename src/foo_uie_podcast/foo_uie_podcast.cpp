#include "stdafx.h"
#include "foo_uie_podcast.h"
#include "mdecl.h"
#include "dlg_inputbox.h"
#include "dlg_editfeed.h"
#include "resource.h"

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

//          Copyright Lars Viklund 2008 - 2011. (foo_wave_seekbar)
// Distributed under the Boost Software License, Version 1.0.
template <typename F>
void in_main_thread(F f)
{
	struct in_main : main_thread_callback
	{
		void callback_run() override
		{
			f();
		}

		in_main(F f) : f(f) {}
		F f;
	};

	static_api_ptr_t<main_thread_callback_manager>()->add_callback(new service_impl_t<in_main>(f));
}

// =========================================================================================================================================================================
//                    UIEBase
// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

EXTERN_C BOOL APIENTRY DllMain(HANDLE hModule, DWORD dwReason, LPVOID lpReserved)
{
	switch (dwReason) {
	case DLL_PROCESS_ATTACH:
	{
		#ifdef DEBUG
		_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
		#endif

		INITCOMMONCONTROLSEX icex;
		icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
		icex.dwICC = ICC_LISTVIEW_CLASSES;
		InitCommonControlsEx(&icex);

	}
		break;
	case DLL_PROCESS_DETACH:
		break;
	case DLL_THREAD_ATTACH:
	{
		bool ei = true; }
		break;
	case DLL_THREAD_DETACH:
		break;
	}

	return TRUE;
}

// =========================================================================================================================================================================
// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

void UIEInitQuit::on_init()
{
	main.Init();
}

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

void UIEInitQuit::on_quit()
{
	main.Quit();
}

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

static initquit_factory_t<UIEInitQuit> foo_initquit;

// =========================================================================================================================================================================
//                    UIEPlaylist
// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

UIEBase::UIEBase() : SQL(), cur_playlist_feed(-1)
{
}

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

void UIEBase::Init()
{
	try {
		
		std::string db_path = std::string(core_api::get_profile_path());
		boost::algorithm::replace_all(db_path, "\\", "/");
		db_path = db_path.substr(7) + "/configuration/" COMPONENT_DLL ".db";

		database.registerLog(boost::bind(&UIEBase::Log, this, _1));
		database.registerUpdateFeeds(boost::bind(&UIEBase::updateFeeds, this, _1));
		database.registerUpdateDownloads(boost::bind(&UIEBase::updateDownloads, this, _1));
		database.registerRedrawDownloads(boost::bind(&UIEBase::redrawDownloads, this));
		database.registerDownloadDone(boost::bind(&UIEBase::onDownloadComplete, this, _1, _2));
		database.registerHighlightFeed(boost::bind(&UIEBase::highlightFeed, this, _1));

		RSS::Options opt;
		opt.autoupdate_minutes = options::autoupdate_minutes;
		opt.download_range = options::download_days;
		opt.filename_mask = options::filename_mask;
		opt.output_dir = options::output_dir;
		opt.download_threads_count = options::download_threads;
		opt.flags = RSS::Options::AUTOUPDATE | RSS::Options::DL_RANGE | RSS::Options::FILENAME_MASK | RSS::Options::OUTPUT_DIR | RSS::Options::DL_THREADS;
		database.SetOptions(opt);
		database.SetFeedListSort(RSS::UI::FeedColumns(options::feedlist_sort_column.get_value()), options::feedlist_sort_order);

		sqlite3_config(SQLITE_CONFIG_MULTITHREAD);		
		database.Connect(db_path.c_str());
		sql_connect(db_path.c_str());
		
		updateFeeds(database.GetFeedCount());
		updateDownloads(database.GetDownloadCount());
		database.Update();
	}
	catch (RSS::Exc& exc) {
		Log(exc.what());
	}
}

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

void UIEBase::Quit()
{
	sql_close();
	database.Close();
}

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

void UIEBase::ShowFeedPlaylist(size_t index)
{
	if (!options::playlist2_use || !sql_isconnected())
		return;

	try {
		static_api_ptr_t<playlist_manager>  playlist_api;
		static_api_ptr_t<metadb>			metadb_api;
		static_api_ptr_t<metadb_io_v3>		meta_db_io_api;

		t_size playlist_index = playlist_api->find_or_create_playlist(options::playlist2);

		if (cur_playlist_feed != (int)index)
		{
			int feed_id = database.GetFeedID(index);
			pfc::list_t<metadb_handle_ptr>	f_list;
			file_info_impl					f_info;
			t_filestats						f_stats;
			std::string						feed_strings[unsigned(SQL_Feed_s::COUNT)];
			
			{				
				SQL::Lock sql_lock(*this, SQL_Query_s::select_feed);
				sql_bind(sql_lock, feed_id);
				if (sql_step(sql_lock)) {
					for (size_t i = 0; i < (size_t)SQL_Feed_s::COUNT; i++)
						feed_strings[i].assign(sql_columnString(sql_lock));
				}
				sql_reset(sql_lock);
			}

			f_info.meta_set("Artist", feed_strings[unsigned(SQL_Feed_s::author)].c_str());
			f_info.meta_set("Album", feed_strings[unsigned(SQL_Feed_s::title)].c_str());

			{
				SQL::Lock sql_lock(*this, SQL_Query_s::select_items);
				sql_bind(sql_lock, feed_id);
				while (sql_step(sql_lock)) {
					metadb_handle_ptr t;
					f_info.meta_set("Title", sql_columnString(sql_lock));
					f_info.meta_set("Comment", sql_columnString(sql_lock));
					f_info.meta_set("Link", sql_columnString(sql_lock));
					f_info.meta_set("Date", sql_columnString(sql_lock));
					metadb_api->handle_create(t, make_playable_location(sql_columnString(sql_lock), 0));
					f_info.set_length(sql_columnInt(sql_lock));
					f_stats.m_size = sql_columnInt(sql_lock);

					meta_db_io_api->hint_async(t, f_info, f_stats, true);
					f_list.add_item(t);
				}
			}
			playlist_api->playlist_clear(playlist_index);
			playlist_api->playlist_add_items(playlist_index, f_list, bit_array_false());
			cur_playlist_feed = (int)index;
		}
		playlist_api->set_active_playlist(playlist_index);
	}
	catch (RSS::Exc& exc) {
		Log(exc.what());
	}
}

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

void UIEBase::Log(const char* s)
{
	boost::lock_guard<boost::mutex> lock(log_mutex);
	console::formatter() << "foo_uie_podcast: " << s;
}

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

void UIEBase::onDownloadComplete(const char* path, int item_id)
{
	boost::lock_guard<boost::mutex> lock(on_download_mutex);

	abort_callback_impl				abort;
	static_api_ptr_t<metadb>		metadb_api;
	static_api_ptr_t<metadb_io_v3>	meta_db_io_api;

	metadb_handle_ptr				fhandle;
	t_filestats						fstats;		
	file_info_impl					finfo;		
	pfc::list_t<metadb_handle_ptr>	flist;
	std::string						fpath(path);

	// --------------- RSSDatabase uses only '/' , but foobar wants '\'
	boost::algorithm::replace_all(fpath, "/", "\\");
	metadb_api->handle_create(fhandle, make_playable_location(fpath.c_str(), 0));

	// --------------- Read tags
	try {
		input_info_reader::ptr reader;
		input_entry::g_open_for_info_read(reader, NULL, fhandle->get_path(), abort);
		reader->get_info(fhandle->get_subsong_index(), finfo, abort);
		fstats = reader->get_file_stats(abort);
	}
	catch (...) {
		Log("failed to read tags");
	}

	// --------------- Write tags
	try
	{
		SQL::Lock sql_lock(*this, SQL_Query_s::select_item_info);
		sql_bind(sql_lock, item_id);
		if (sql_step(sql_lock))
		{
			if (sql_columnInt(sql_lock) == 1)
			{
				input_info_writer::ptr writer;

				finfo.meta_set("ALBUM", sql_columnString(sql_lock));
				finfo.meta_set("ARTIST", sql_columnString(sql_lock));
				finfo.meta_set("TITLE", sql_columnString(sql_lock));
				finfo.meta_set("DATE", sql_columnString(sql_lock));
				finfo.meta_set("GENRE", "Podcast");

				input_entry::g_open_for_info_write(writer, NULL, fhandle->get_path(), abort);
				writer->set_info(fhandle->get_subsong_index(), finfo, abort);
			}
		}
		sql_reset(sql_lock);
	}
	catch (RSS::Exc& exc) 
	{
		Log(exc.what());
	}
	catch (...) {
		Log("failed to write tags");
	}

	// --------------- Add to playlist
	if (options::playlist1_use)
	{
		try {
			meta_db_io_api->hint_async(fhandle, finfo, fstats, true);
			flist.add_item(fhandle);

			in_main_thread([flist]()
			{
				static_api_ptr_t<playlist_manager>  playlist_api;
				t_size playlist_index = playlist_api->find_or_create_playlist(options::playlist1);
				playlist_api->playlist_add_items(playlist_index, flist, bit_array_false());
			});
		}
		catch (...) {
			Log("failed to add download to playlist");
		}
	}
}

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

void UIEBase::updateFeeds(size_t count)
{
	panelmanager.PostMsg(size_t(PanelID::feeds), LVM_SETITEMCOUNT, count, LVSICF_NOSCROLL);
	panelmanager.PostMsg(size_t(PanelID::feeds), LVM_REDRAWITEMS, 0, count);
}

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

void UIEBase::updateDownloads(size_t count)
{
	panelmanager.PostMsg(size_t(PanelID::downloads), LVM_SETITEMCOUNT, count, LVSICF_NOSCROLL);
}

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

void UIEBase::redrawDownloads()
{
	panelmanager.PostMsg(size_t(PanelID::downloads), LVM_REDRAWITEMS, 0, 100);
}

void UIEBase::highlightFeed(int index)
{
	panelmanager.PostMsg(size_t(PanelID::feeds), WM_UIELW_HIGHLIGHT_ITEM, index, 0);
	panelmanager.PostMsg(size_t(PanelID::feeds), LVM_REDRAWITEMS, 0, index);
}

// =========================================================================================================================================================================
//                    UIEPanelFeeds
// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

// {54B190D1-F930-4D1A-87D6-5DF5317826A4}
const GUID UIEPanelFeeds::guid =
{ 0x54b190d1, 0xf930, 0x4d1a, { 0x87, 0xd6, 0x5d, 0xf5, 0x31, 0x78, 0x26, 0xa4 } };

// {4C1E76D0-6C85-46FD-915E-89CB3E79D3D4}
const GUID UIEPanelFeeds::columns_guid =
{ 0x4c1e76d0, 0x6c85, 0x46fd, { 0x91, 0x5e, 0x89, 0xcb, 0x3e, 0x79, 0xd3, 0xd4 } };

const TCHAR* UIEPanelFeeds::columns_default_str[] = { TEXT("title"), TEXT("author"), TEXT("url"), TEXT("homepage"), TEXT("last published"), TEXT("last checked"), TEXT("items") };
const int UIEPanelFeeds::columns_default_width[] = { 300, 200, 200, 100, 100, 100, 50 };

cfg_lw_columns UIEPanelFeeds::columns(columns_guid, 7, columns_default_str, columns_default_width);

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

UIEPanelFeeds::UIEPanelFeeds() : UIEListViewPanel(size_t(PanelID::feeds), columns, boost::bind(&RSS::Database::GetFeedCount, &database), boost::bind(&RSS::Database::GetFeedString, &database, _1, _2))
{}

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

UIEPanelFeeds::~UIEPanelFeeds()
{}

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

const GUID & UIEPanelFeeds::get_extension_guid() const
{
	return guid;
}

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

void UIEPanelFeeds::get_name(pfc::string_base & out) const
{
	out.set_string(COMPONENT_NAME": feeds");
}

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

UIEPanelFeeds::class_data & UIEPanelFeeds::get_class_data() const
{
	__implement_get_class_data(_T("{54B190D1-F930-4D1A-87D6-5DF5317826A4}"), true);
}

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

void UIEPanelFeeds::onItemClick(size_t index, size_t col)
{
	main.ShowFeedPlaylist(index);
}

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

void UIEPanelFeeds::onRClick(int index, int x, int y)
{
	HMENU menu = NULL;

	if (index >= 0) 
	{
		menu = LoadMenu(m_hinstance, MAKEINTRESOURCE(IDR_MENU_FEED));
		ListView_SetItemState(m_hwnd_list, index, LVIS_SELECTED, LVIS_SELECTED);
		EnableMenuItem(menu, ID_MENU_FEED_DELETE, (database.UpdateInProgress() ? MF_GRAYED : MF_ENABLED));
	}
	else {
		menu = LoadMenu(m_hinstance, MAKEINTRESOURCE(IDR_MENU_MAIN));
		bool updating = database.UpdateInProgress();
		EnableMenuItem(menu, ID_MENU_FEED_ADD, (updating ? MF_GRAYED : MF_ENABLED));
		EnableMenuItem(menu, ID_MENU_UPDATE, (updating ? MF_GRAYED : MF_ENABLED));
		EnableMenuItem(menu, ID_MENU_REBUILD, (updating ? MF_GRAYED : MF_ENABLED));
	}
	if (!menu)
		return;

	HMENU submenu = GetSubMenu(menu, 0);
	int cmd = TrackPopupMenu(submenu, TPM_LEFTBUTTON | TPM_NONOTIFY | TPM_RETURNCMD, x, y, 0, m_hwnd_list, 0);

	switch (cmd) {
	case ID_MENU_FEED_EDIT:
	{
		RSS::UI::Feed tfeed;
		database.GetFeedInfo(index, tfeed);
		DlgEditfeed dlg(m_hinstance, m_hwnd_list, tfeed);
		if (dlg.doDialog()) {
			database.SetFeedInfo(tfeed);
		}
	} break;
		
	case ID_MENU_FEED_ADD:
	{
		DlgInputbox dlg(m_hinstance, m_hwnd_list);
		if (dlg.doDialog(TEXT("Add feed"))) 
		{
			const TCHAR* s = dlg.getString();
			#ifdef UNICODE
			int buf_size = WideCharToMultiByte(CP_UTF8, 0, s, -1, NULL, 0, NULL, false);
			if (buf_size > 0) {
				std::string buffer;
				buffer.resize(size_t(buf_size));
				WideCharToMultiByte(CP_UTF8, 0, s, -1, &buffer[0], buf_size, NULL, false);
				database.AddFeed(buffer.c_str());
			}
			#else
			database.AddFeed(s);
			#endif
		}
	} break;

	case ID_MENU_FEED_DELETE:
	{
		TCHAR tmsg[100] = TEXT("Delete '");
		_tcsncpy_s(tmsg + 8, 92, database.GetFeedString(index, 0), _TRUNCATE);
		int tmsg_len = _tcslen(tmsg);
		if (tmsg_len < 97) {
			tmsg[tmsg_len] = '\'';
			tmsg[tmsg_len + 1] = '?';
			tmsg[tmsg_len + 2] = 0;
		}
		int msgboxID = MessageBox(m_hwnd_list, tmsg, TEXT("delete feed"), MB_ICONQUESTION | MB_YESNO | MB_APPLMODAL);
		if (msgboxID == IDYES) {
			database.DeleteFeed(database.GetFeedID(index));
		}
	} break;

	case ID_MENU_UPDATE:
		database.Update();
		break;

	case ID_MENU_REBUILD:
	{
		int msgboxID = MessageBox(m_hwnd_list, TEXT("rebuild the database? (all edits and information\n\rabout previous downloads will be lost!)"), TEXT("rebuild"), MB_ICONQUESTION | MB_YESNO | MB_APPLMODAL);
		if (msgboxID == IDYES) {
			database.Update(true);
		}
		
	} break;
	}
	DestroyMenu(menu);
}

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

void UIEPanelFeeds::onColumnClick(int index)
{
	if (index == options::feedlist_sort_column) {
		options::feedlist_sort_order = !options::feedlist_sort_order;
	}
	else {
		options::feedlist_sort_column = index;
	}
	database.SortFeedList(RSS::UI::FeedColumns(index), options::feedlist_sort_order);
}

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

uie::window_factory<UIEPanelFeeds> g_feeds_window_factory;

// =========================================================================================================================================================================
//                    UIEPanelDownloads
// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

// {EDA327FC-187C-4CA6-ADFA-B449E0AF3810}
const GUID UIEPanelDownloads::guid = { 0xeda327fc, 0x187c, 0x4ca6, { 0xad, 0xfa, 0xb4, 0x49, 0xe0, 0xaf, 0x38, 0x10 } };

// {01643CA3-01E3-481A-88D2-16F513C5D0D9}
const GUID UIEPanelDownloads::columns_guid = { 0x1643ca3, 0x1e3, 0x481a, { 0x88, 0xd2, 0x16, 0xf5, 0x13, 0xc5, 0xd0, 0xd9 } };

const TCHAR* UIEPanelDownloads::columns_default_str[] = { TEXT("title"), TEXT("filename"), TEXT("url"), TEXT("status") };
const int UIEPanelDownloads::columns_default_width[] = { 250, 200, 200, 150 };

cfg_lw_columns UIEPanelDownloads::columns(columns_guid, 4, columns_default_str, columns_default_width);

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

UIEPanelDownloads::UIEPanelDownloads() : UIEListViewPanel(size_t(PanelID::downloads), columns, boost::bind(&RSS::Database::GetDownloadCount, &database), boost::bind(&RSS::Database::GetDownloadString, &database, _1, _2))
{}

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

UIEPanelDownloads::~UIEPanelDownloads()
{}

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

const GUID & UIEPanelDownloads::get_extension_guid() const
{
	return guid;
}

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

UIEPanelDownloads::class_data & UIEPanelDownloads::get_class_data() const
{
	__implement_get_class_data(_T("{EDA327FC-187C-4CA6-ADFA-B449E0AF3810}"), true);
}

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

void UIEPanelDownloads::get_name(pfc::string_base & out) const
{
	out.set_string(COMPONENT_NAME": downloads");
}

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

void UIEPanelDownloads::onItemClick(size_t index, size_t col)
{

}

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

void UIEPanelDownloads::onRClick(int index, int x, int y)
{
	if (index >= 0)
		ListView_SetItemState(m_hwnd_list, index, LVIS_SELECTED, LVIS_SELECTED);

	HMENU	menu = LoadMenu(m_hinstance, MAKEINTRESOURCE(IDR_MENU_DOWNLOAD));
	bool	active = false;
	int		item_id = -1;

	if (index >= 0) {
		database.GetDownloadInfo(index, item_id, active);
	}

	if (item_id == -1) {
		EnableMenuItem(menu, ID_MENU_DL_PAUSE, MF_GRAYED);
		EnableMenuItem(menu, ID_MENU_DL_RESUME, MF_GRAYED);
		EnableMenuItem(menu, ID_MENU_DL_CANCEL, MF_GRAYED);
		if (database.GetDownloadCount() == 0) {
			EnableMenuItem(menu, ID_MENU_DL_PAUSEALL, MF_GRAYED);
			EnableMenuItem(menu, ID_MENU_DL_RESUMEALL, MF_GRAYED);
			EnableMenuItem(menu, ID_MENU_DL_CANCELALL, MF_GRAYED);
		}
	}
	else {
		EnableMenuItem(menu, ID_MENU_DL_PAUSE, active ? MF_ENABLED : MF_GRAYED);
		EnableMenuItem(menu, ID_MENU_DL_RESUME, !active ? MF_ENABLED : MF_GRAYED);
	}

	HMENU submenu = GetSubMenu(menu, 0);
	int cmd = TrackPopupMenu(submenu, TPM_LEFTBUTTON | TPM_NONOTIFY | TPM_RETURNCMD, x, y, 0, m_hwnd_list, 0);

	switch (cmd) {
	case ID_MENU_DL_PAUSE:
		database.PauseDownload(item_id);
		break;
	case ID_MENU_DL_RESUME:
		database.ResumeDownload(item_id);
		break;
	case ID_MENU_DL_CANCEL:
		database.CancelDownload(item_id);
		break;
	case ID_MENU_DL_PAUSEALL:
		database.PauseDownload(-1);
		break;
	case ID_MENU_DL_RESUMEALL:
		database.ResumeDownload(-1);
		break;
	case ID_MENU_DL_CANCELALL:
		database.CancelDownload(-1);
		break;
	}

	DestroyMenu(menu);
}

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

uie::window_factory<UIEPanelDownloads> g_downloads_window_factory;

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------
