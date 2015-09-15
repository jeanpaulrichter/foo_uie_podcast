#ifndef H_DEF_FOO_UIE_PODCAST
#define H_DEF_FOO_UIE_PODCAST

#pragma once

#include "stdafx.h"
#include "database.h"
#include "panels.h"

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

class UIEInitQuit : public initquit
{
public:
	void on_init();
	void on_quit();
};

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

class UIEBase : public RSS::SQL
{
public:
	UIEBase();

	void Init();
	void Quit();
	void ShowFeedPlaylist(size_t index);
	void Log(const char* s);

private:
	void onDownloadComplete(const char* s, int item_id);
	void updateFeeds(size_t count);
	void updateDownloads(size_t count);
	void redrawDownloads();
	void highlightFeed(int index);

	boost::mutex				log_mutex;
	boost::mutex				on_download_mutex;
	int							cur_playlist_feed;
};

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

class UIEPanelFeeds : public UIEListViewPanel 
{
public:
	UIEPanelFeeds();
	~UIEPanelFeeds();

	const GUID & get_extension_guid() const;
	void get_name(pfc::string_base & out) const;

protected:
	static const GUID		guid;

private:
	void onItemClick(size_t index, size_t col);
	void onRClick(int index, int x, int y);
	void onColumnClick(int index);
	class_data & get_class_data()const;

	static cfg_lw_columns	columns;
	
	static const GUID		columns_guid;
	static const TCHAR*		columns_default_str[];
	static const int		columns_default_width[];
};

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

class UIEPanelDownloads : public UIEListViewPanel
{
public:
	UIEPanelDownloads();
	~UIEPanelDownloads();

	const GUID & get_extension_guid() const;
	void get_name(pfc::string_base & out) const;
	static int getStatusColumn() {
		return columns.getColumn(3);
	};

private:
	void onItemClick(size_t index, size_t col);
	void onRClick(int index, int x, int y);
	class_data & get_class_data()const;

	static cfg_lw_columns	columns;
	static const GUID		guid;
	static const GUID		columns_guid;
	static const TCHAR*		columns_default_str[];
	static const int		columns_default_width[];
};

#endif