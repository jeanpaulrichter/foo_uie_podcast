#include "stdafx.h"
#include "database.h"

// =========================================================================================================================================================================
//                    SQL
// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

static const char* sql_query_str[] = {
/* count_tables			*/	"SELECT count(*) FROM sqlite_master WHERE type = 'table' AND name != 'android_metadata' AND name != 'sqlite_sequence';",
/* count_feeds_url		*/	"SELECT COUNT(*) FROM feeds WHERE url = ?001;",
/* get_item_by_date		*/	"SELECT rowid FROM items WHERE feed_id = ?002 AND pubdate = ?001",
/* insert_feed			*/	"INSERT INTO feeds (url, title, homepage, author, description, image_url, last_published, last_checked) VALUES (?001, ?002, ?003, ?004, ?005, ?006, ?007, ?008);",
/* insert_item			*/	"INSERT INTO items (title, description, link, pubdate, url, download_time, length, filesize, feed_id) VALUES (?001, ?002, ?003, ?004, ?005, ?006, ?007, ?008, ?009);",
/* insert_download		*/	"INSERT INTO downloads (title, url, filename, filesize, item_id, status) VALUES (?001, ?002, ?003, ?004, ?005, ?006);",
/* delete_feed_items	*/	"DELETE FROM items WHERE feed_id = ?001;",
/* delete_feed			*/	"DELETE FROM feeds WHERE rowid = ?001;",
/* delete_download		*/	"DELETE FROM downloads WHERE item_id = ?001;",
/* delete_downloads		*/	"DELETE FROM downloads;",
/* delete_items			*/	"DELETE FROM items;",
/* update_feed			*/	"UPDATE feeds SET url = ?001, title = ?002, homepage = ?003, author = ?004, description = ?005, image_url = ?006, last_published = ?007, last_checked = ?008 WHERE rowid = ?009;",
/* update_feed_info		*/	"UPDATE feeds SET title = ?001, homepage = ?002, author = ?003, retag = ?004 WHERE rowid = ?005;",
/* update_feed_dates	*/	"UPDATE feeds SET last_published = ?001, last_checked = ?002 WHERE rowid = ?003;",
/* update_feed_check	*/	"UPDATE feeds SET last_checked = ?001 WHERE rowid = ?002;",
/* update_item_time		*/	"UPDATE items SET download_time = datetime('now') WHERE rowid = ?001;",
/* update_download		*/	"UPDATE downloads SET status = ?001 WHERE item_id = ?002;",
/* update_downloads		*/	"UPDATE downloads SET status = ?001 WHERE status = ?002;",
/* vacuum				*/	"VACUUM;"
};

static const char* sql_query_c_str[] = {
/* create_downloads		*/	"CREATE TABLE downloads ( title TEXT, url TEXT, filename TEXT, filesize INTEGER, item_id INTEGER, status INTEGER );",
/* create_feeds			*/	"CREATE TABLE feeds ( url TEXT, title TEXT, homepage TEXT, author TEXT, description TEXT, image_url TEXT, last_published TEXT, last_checked TEXT, retag INTEGER DEFAULT 0 );",
/* create_items			*/	"CREATE TABLE items ( title TEXT, description TEXT, link TEXT, pubdate TEXT, url TEXT, download_time TEXT, length INTEGER, filesize INTEGER, feed_id INTEGER );",
/* create_items_index1	*/	"CREATE INDEX items_index1 ON items(feed_id, pubdate);",
/* create_feeds_index1	*/	"CREATE INDEX feeds_index1 ON feeds(title);"
};

static const char* sql_query_s_str[] = {
/* select_feeds					*/	"SELECT rowid, url, title, homepage, author, description, image_url, last_checked, last_published, retag, (SELECT Count(*) FROM items Where feed_id = feeds.rowid) as itemcount FROM feeds ORDER BY title ASC;",
/* select_new_items				*/	"SELECT items.rowid, items.title, items.url, feeds.title, feeds.author, filesize FROM items, feeds WHERE pubdate BETWEEN datetime('now', ?001) AND datetime('now', 'localtime') AND download_time = '' AND feed_id = feeds.rowid LIMIT 100;",
/* select_downloads				*/	"SELECT title, url, filename, filesize, item_id, status FROM downloads ORDER BY item_id ASC;",
/* select_items					*/	"SELECT title, description, link, pubdate, url, length, filesize FROM items WHERE feed_id = ?001 ORDER BY pubdate DESC;",
/* select_feed					*/	"SELECT url, title, homepage, author, description, image_url, last_published, last_checked FROM feeds WHERE rowid = ?001;",
/* select_download_status		*/	"SELECT title, url, filename FROM downloads WHERE status = ?001 AND item_id = ?002;",
/* select_download_status_not	*/	"SELECT filename FROM downloads WHERE status != ?001 AND item_id = ?002;",
/* select_downloads_status_not	*/	"SELECT filename, item_id FROM downloads WHERE status != ?001 ORDER BY item_id DESC;",
/* select_item_info				*/	"SELECT feeds.retag, feeds.title, feeds.author, items.title, items.pubdate FROM feeds, items WHERE items.rowid = ?001 AND feeds.rowid = items.feed_id;"
};

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

RSS::SQL::SQL() : sql_db(NULL)
{
	for (size_t i = 0; i < unsigned(SQL_Query::COUNT); i++)
		sql_stmts_q[i] = NULL;
	for (size_t i = 0; i < unsigned(SQL_Query_s::COUNT); i++) {
		sql_stmts_q_s[i] = NULL;
		sql_q_s_column[i] = -1;
		sql_q_s_bind[i] = 0;
	}
}

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

RSS::SQL::~SQL()
{
	sql_close();
}

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

bool RSS::SQL::sql_isconnected()
{
	return (sql_db != NULL);
}

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------
/* not thread-safe! */
void RSS::SQL::sql_connect(const char* path)
{
	int				i;
	int				rc;
	char			err_str[60];
	sqlite3_stmt*	t_stmt = NULL;

	if (sql_db != NULL)
		throw Exc("database already connected");

	// ---------------- Open database-file
	rc = sqlite3_open_v2(path, &sql_db, SQLITE_OPEN_NOMUTEX | SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL);
	if (rc) 
	{
		sqlite3_close(sql_db);
		sql_db = NULL;
		_snprintf_s(err_str, _TRUNCATE, "failed to open database: error code %d", rc);
		throw Exc(err_str);
	}

	try {
		// ---------------- Prepare count tables statement
		rc = sqlite3_prepare_v2(sql_db, sql_query_str[unsigned(SQL_Query::count_tables)], -1, &sql_stmts_q[unsigned(SQL_Query::count_tables)], NULL);
		if (rc != SQLITE_OK) {
			sqlite3_finalize(sql_stmts_q[unsigned(SQL_Query::count_tables)]);
			sql_stmts_q[unsigned(SQL_Query::count_tables)] = NULL;
			_snprintf_s(err_str, _TRUNCATE, "failed to compile sql_query_str 0: error code %d", rc);
			throw Exc(err_str, Exc::Level::critical);
		}
		// ---------------- Create tables if nessecary
		if (!sql_query(SQL_Query::count_tables, NULL, 0, NULL, 0, &rc) || rc != int(SQL_Query_c::COUNT_TABLES))
		{
			for (i = 0; i < int(SQL_Query_c::COUNT); i++)
			{
				if (sqlite3_prepare_v2(sql_db, sql_query_c_str[i], -1, &t_stmt, NULL) != SQLITE_OK)
					throw Exc("failed to compile create-table statments");
				if (sqlite3_step(t_stmt) != SQLITE_DONE)
					throw Exc("failed to create tables");
				sqlite3_finalize(t_stmt);
				t_stmt = NULL;
			}
		}

		// ---------------- Prepare statements
		// ------ Query
		for (i = 0; i < int(SQL_Query::COUNT); i++) {
			if (i == int(SQL_Query::count_tables))
				continue;
			rc = sqlite3_prepare_v2(sql_db, sql_query_str[i], -1, &sql_stmts_q[i], NULL);
			if (SQLITE_OK != rc)
				break;
		}
		if (i != int(SQL_Query::COUNT)) {
			sqlite3_finalize(sql_stmts_q[i]);
			sql_stmts_q[i] = NULL;
			_snprintf_s(err_str, _TRUNCATE, "failed to compile sql_query_str %d: error code %d", i, rc);
			throw Exc(err_str, Exc::Level::critical);
		}
		// ------ Query_s
		for (i = 0; i < int(SQL_Query_s::COUNT); i++) {
			rc = sqlite3_prepare_v2(sql_db, sql_query_s_str[i], -1, &sql_stmts_q_s[i], NULL);
			if (SQLITE_OK != rc)
				break;
		}
		if (i != int(SQL_Query_s::COUNT)) {
			sqlite3_finalize(sql_stmts_q_s[i]);
			sql_stmts_q_s[i] = NULL;
			_snprintf_s(err_str, _TRUNCATE, "failed to compile sql_query_s_str %d: error code %d", i, rc);
			throw Exc(err_str, Exc::Level::critical);
		}
	}
	catch (Exc& exc) 
	{
		if (t_stmt)
			sqlite3_finalize(t_stmt);
		for (i = 0; i < int(SQL_Query::COUNT); i++) {
			sqlite3_finalize(sql_stmts_q[i]);
			sql_stmts_q[i] = NULL;
		}
		for (i = 0; i < int(SQL_Query_s::COUNT); i++) {
			sqlite3_finalize(sql_stmts_q_s[i]);
			sql_stmts_q_s[i] = NULL;
		}
		sqlite3_close(sql_db);
		sql_db = NULL;
		throw exc;
	}
}

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------
/* not thread-safe! */
void RSS::SQL::sql_close()
{
	// ---------------- Clear statements & close db:
	if (sql_db) {
		for (unsigned i = 0; i < unsigned(SQL_Query::COUNT); i++) {
			sqlite3_finalize(sql_stmts_q[i]);
			sql_stmts_q[i] = NULL;
		}
		for (unsigned i = 0; i < unsigned(SQL_Query_s::COUNT); i++) {
			sqlite3_finalize(sql_stmts_q_s[i]);
			sql_stmts_q_s[i] = NULL;
		}
		sqlite3_close(sql_db);
		sql_db = NULL;
	}
}

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------
/*     executes SQL_Query, expects std::string-array 's' of size 's_num' and integer-array 'd' of size 'd_num' as binds. 
       '*ret' has to be specified in case return value is expected. returns true on success                                                  */
bool RSS::SQL::sql_query(SQL_Query q, const std::string* s, size_t s_num, const int* d, size_t d_num, int* ret)
{
	if (q == SQL_Query::COUNT)
		return false;

	// ---------------- Lock
	boost::lock_guard<boost::mutex> lock(sql_query_mutex[unsigned(q)]);

	char	err_string[70];
	int		err_code;
	bool	success = true;
	int		arg_i = 1;

	try {
		// ---------------- Bind Strings
		if (s != NULL) {
			for (size_t i = 0; i < s_num; i++) {
				err_code = sqlite3_bind_text(sql_stmts_q[(unsigned)q], arg_i, s[i].c_str(), s[i].size(), NULL);
				if (err_code != SQLITE_OK) {
					_snprintf_s(err_string, _TRUNCATE, "sql error (SQLQ %d): bind, str-id: %d, error: %d", (int)q, (int)i, err_code);
					throw Exc(err_string, Exc::Level::critical);
				}
				arg_i++;
			}
		}
		// ---------------- Bind Integers
		if (d != NULL) {
			for (size_t i = 0; i < d_num; i++) {
				err_code = sqlite3_bind_int(sql_stmts_q[(unsigned)q], arg_i, d[i]);
				if (err_code != SQLITE_OK) {
					_snprintf_s(err_string, _TRUNCATE, "sql error (SQLQ %d): bind, integer-id: %d, error: %d", (int)q, (int)i, err_code);
					throw Exc(err_string, Exc::Level::critical);
				}
				arg_i++;
			}
		}
		// ---------------- Step
		err_code = sqlite3_step(sql_stmts_q[(unsigned)q]);
		// --------- expect return
		if (q < SQL_Query::COUNT_ROW)
		{			
			if (err_code == SQLITE_ROW) {
				if (ret)
					*ret = sqlite3_column_int(sql_stmts_q[(unsigned)q], 0);
			}
			else if (err_code == SQLITE_DONE) {
				success = false;
			}
		}
		// -------- insert/update
		else {
			if (err_code == SQLITE_DONE) {
				if (q < SQL_Query::COUNT_INSERT && ret != NULL)
					*ret = (int)sqlite3_last_insert_rowid(sql_db);
			}
			else if(err_code == SQLITE_ROW) {
				_snprintf_s(err_string, _TRUNCATE, "sql error (SQLQ %d): unexpected SQLITE_ROW", (int)q);
				throw Exc(err_string);
			}
		}
		if (err_code != SQLITE_DONE && err_code != SQLITE_ROW) {
			_snprintf_s(err_string, _TRUNCATE, "sql error (SQLQ %d): step, error: %d", (int)q, err_code);
			throw Exc(err_string, Exc::Level::critical);
		}
		// ---------------- Reset statement
		sqlite3_reset(sql_stmts_q[(unsigned)q]);

	}
	catch (Exc& exc) {
		sqlite3_reset(sql_stmts_q[(unsigned)q]);
		throw exc;
	}
	catch (std::exception& exc) {
		sqlite3_reset(sql_stmts_q[(unsigned)q]);
		throw exc;
	}
	return success;
}

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

void RSS::SQL::sql_bind(Lock& lock, const char* s)
{
	if (!lock.owns_lock(this))
		throw Exc("sql_bind: locking error", Exc::Level::critical);

	unsigned id = lock.id();
	sql_q_s_bind[id]++;
	int err_code = sqlite3_bind_text(sql_stmts_q_s[id], sql_q_s_bind[id], s, strlen(s), NULL);

	if (err_code != SQLITE_OK) {
		sql_reset(lock);
		char err_string[70];
		_snprintf_s(err_string, _TRUNCATE, "sql error (SQL_Query_s %d): bind, error: %d", id, err_code);
		throw Exc(err_string, Exc::Level::critical);
	}
}

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

void RSS::SQL::sql_bind(Lock& lock, int i)
{
	if (!lock.owns_lock(this))
		throw Exc("sql_bind: locking error", Exc::Level::critical);
	unsigned id = lock.id();
	sql_q_s_bind[id]++;
	int err_code = sqlite3_bind_int(sql_stmts_q_s[id], sql_q_s_bind[id], i);

	if (err_code != SQLITE_OK) {
		sql_reset(lock);
		char err_string[70];
		_snprintf_s(err_string, _TRUNCATE, "sql error (SQL_Query_s %d): bind, error: %d", id, err_code);
		throw Exc(err_string, Exc::Level::critical);
	}
}

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

bool RSS::SQL::sql_step(Lock& lock)
{
	if (!lock.owns_lock(this))
		throw Exc("sql_step: locking error", Exc::Level::critical);

	int		err_code;
	unsigned id = lock.id();

	err_code = sqlite3_step(sql_stmts_q_s[id]);
	if (err_code == SQLITE_DONE) {
		sql_reset(lock);
		return false;
	}
	else if (err_code == SQLITE_ROW) {
		sql_q_s_column[id] = -1;
		return true;
	}
	else {
		sql_reset(lock);
		char	err_string[70];
		_snprintf_s(err_string, _TRUNCATE, "sql error (SQLQ_s %d): step, error: %d", id, err_code);
		throw Exc(err_string, Exc::Level::critical);
	}
	return false;
}

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

int	RSS::SQL::sql_columnInt(Lock& lock)
{
	if (!lock.owns_lock(this))
		throw Exc("sql_bind: locking error", Exc::Level::critical);
	unsigned id = lock.id();
	sql_q_s_column[id]++;
	return sqlite3_column_int(sql_stmts_q_s[id], sql_q_s_column[id]);
}

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

const char*	RSS::SQL::sql_columnString(Lock& lock)
{
	if (!lock.owns_lock(this))
		throw Exc("sql_columnString: locking error", Exc::Level::critical);
	unsigned id = lock.id();
	sql_q_s_column[id]++;
	return (const char*)sqlite3_column_text(sql_stmts_q_s[id], sql_q_s_column[id]);
}

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

void RSS::SQL::sql_reset(Lock& lock)
{
	if (!lock.owns_lock(this))
		throw Exc("sql_reset: locking error", Exc::Level::critical);
	unsigned id = lock.id();
	sqlite3_reset(sql_stmts_q_s[id]);
	sql_q_s_column[id] = -1;
	sql_q_s_bind[id] = 0;
}

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

void RSS::SQL::sql_lock(SQL_Query_s q)
{
	sql_query_s_mutex[unsigned(q)].lock();
}

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

void RSS::SQL::sql_unlock(SQL_Query_s q)
{
	sql_query_s_mutex[unsigned(q)].unlock();
}

// =========================================================================================================================================================================
//                    RSS::Database
// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

RSS::Database::Database() : SQL(), download_ready(false)
{
}

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

RSS::Database::~Database()
{
	Close();
}

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

bool RSS::Database::IsConnected()
{
	return sql_isconnected();
}

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

void RSS::Database::Connect(const char* path)
{
	try {
		sql_connect(path);

		// --------------- Setup ui feed-list
		std::string										feed_strings[unsigned(SQL_Feed_s::COUNT)];
		RSS::UI::Feed									ui_feed;

		{
			boost::lock_guard<boost::mutex> lock(list.feeds_mutex);
			{
				SQL::Lock sql_lock(*this, SQL_Query_s::select_feeds);
				while (sql_step(sql_lock))
				{
					try {
						ui_feed.id = sql_columnInt(sql_lock);
						for (size_t i = 0; i < size_t(SQL_Feed_s::COUNT); i++)
							feed_strings[i].assign(sql_columnString(sql_lock));
						ui_feed.retag = (sql_columnInt(sql_lock) == 1) ? true : false;
						int item_count = sql_columnInt(sql_lock);
						getFeedUIStrings(feed_strings, ui_feed.strings, item_count);
						
						list.feeds.push_back(ui_feed);
					}
					catch (Exc& exc) {
						func.log(exc.what());
					}
				}
				sql_reset(sql_lock);
			}
			list.feeds.sort(FeedCompare(list.feeds_order_column, list.feeds_order_up));
			list.feedsVector.reserve(list.feeds.size());
			for (RSS::UI::FeedList::iterator it = list.feeds.begin(); it != list.feeds.end(); ++it)
				list.feedsVector.push_back(it);
			func.feed_list_update(list.feeds.size());
		}

		// --------------- Init old downloads
		std::string				t_download_s[unsigned(SQL::SQL_Download_s::COUNT)];
		int						t_download_i[unsigned(SQL::SQL_Download_i::COUNT)];
		size_t					t_downloads_count = 0;
		size_t					t_downloads_active = 0;

		{
			SQL::Lock sql_lock(*this, SQL_Query_s::select_downloads);
			while (sql_step(sql_lock))
			{
				for (size_t i = 0; i < size_t(SQL_Download_s::COUNT); i++)
					t_download_s[i].assign(sql_columnString(sql_lock));
				for (size_t i = 0; i < size_t(SQL_Download_i::COUNT); i++)
					t_download_i[i] = sql_columnInt(sql_lock);

				RSS::UI::Download t_ui_download;
				getDownloadUIStrings(t_download_s, t_ui_download.strings);
				getUIStatusString(RSS::Net::Status(t_download_i[unsigned(SQL_Download_i::status)]), t_ui_download.strings.back(), false);
				{
					boost::lock_guard<boost::mutex> lock(list.downloads_mutex);
					list.downloads.insert(std::pair<int, RSS::UI::Download>(t_download_i[int(SQL_Download_i::item_id)], t_ui_download));
				}
				t_downloads_count++;
				if (t_download_i[unsigned(SQL_Download_i::status)] == (int)RSS::Net::Status::downloading)
				{
					boost::lock_guard<boost::mutex> lock(download_mutex);
					downloads.push_back(t_download_i[int(SQL_Download_i::item_id)]);
					t_downloads_active++;
				}
			}
			sql_reset(sql_lock);
		}
		func.download_list_update(t_downloads_count);
		if (t_downloads_active)
		{
			boost::lock_guard<boost::mutex> lock(download_mutex);
			download_ready = true;
		}
		download_ready_cond.notify_all();

		// --------------- Start download threads
		size_t a_threads;
		{
			boost::lock_guard<boost::mutex> lock(options.mutex);
			a_threads = options.dl_threads_count;
		}
		{
			boost::lock_guard<boost::mutex> lock(download_threads_item_mutex);
			download_threads_item.resize(a_threads);
			for (size_t i = 0; i < a_threads; i++)
				download_threads_item[i] = -1;
		}

		download_threads.resize(a_threads);
		for (size_t i = 0; i < a_threads; i++) {
			download_threads[i] = boost::thread(&RSS::Database::download, this, i);
		}

		// --------------- Start autoupdate thread
		{
			boost::lock_guard<boost::mutex> lock(autoupdate_status_mutex);
			autoupdate_status = THREAD_SHUTDOWN;
		}
		thread_autoupdate.interrupt();
		thread_autoupdate.join();
		{
			boost::lock_guard<boost::mutex> lock(autoupdate_status_mutex);
			autoupdate_status = THREAD_AUTOUPDATE_WAIT;
		}
		thread_autoupdate = boost::thread(&RSS::Database::autoUpdate, this);
	}
	catch (Exc& exc) {
		func.log(exc.what());
	}
}

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

void RSS::Database::Close()
{
	// --------------- Shutdown threads
	{
		boost::lock_guard<boost::mutex> lock(autoupdate_status_mutex);
		autoupdate_status = THREAD_SHUTDOWN;
	}
	thread_autoupdate.interrupt();
	thread_update.interrupt();
	thread_addfeed.interrupt();

	{
		boost::lock_guard<boost::mutex> lock(download_threads_item_mutex);
		for (size_t i = 0; i < download_threads.size(); i++) {
			download_threads_item[i] = THREAD_SHUTDOWN;
			download_threads[i].interrupt();
		}
	}

	thread_autoupdate.join();
	thread_update.join();
	thread_addfeed.join();	
	for (size_t i = 0; i < download_threads.size(); i++)
		download_threads[i].join();

	sql_close();
}

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

void RSS::Database::AddFeed(const char* url)
{
	if (!url || strlen(url) < 10) {
		func.log("feed url not valid");
		return;
	}
	if (!sql_isconnected())
		return;

	thread_addfeed.join();
	thread_addfeed = boost::thread(&RSS::Database::doAddFeed, this, std::string(url));
}

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

void RSS::Database::DeleteFeed(int feed_id)
{
	try {
		boost::unique_lock<boost::mutex> lock(update_mutex, boost::try_to_lock);
		if (!lock.owns_lock()) {
			func.log("cannot delete feed: update in progress.");
			return;
		}

		sql_query(SQL_Query::delete_feed_items, NULL, 0, &feed_id, 1, NULL);
		sql_query(SQL_Query::delete_feed, NULL, 0, &feed_id, 1, NULL);
		{
			boost::lock_guard<boost::mutex> lock(list.feeds_mutex);
			for (size_t i = 0; i < list.feedsVector.size(); i++) {
				if (list.feedsVector[i]->id == feed_id) {
					list.feeds.erase(list.feedsVector[i]);
					list.feedsVector.erase(list.feedsVector.begin() + i);
					break;
				}
			}
			func.feed_list_update(list.feeds.size());
			func.log("feed deleted.");
		}
	}
	catch (Exc& exc) {
		func.log(exc.what());
	}
}

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

void RSS::Database::Update(bool rebuild)
{
	try {
		thread_update.join();
		thread_update = boost::thread(&RSS::Database::updateFeeds, this, rebuild);
	}
	catch (boost::thread_interrupted&) 	{}
}

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

bool RSS::Database::UpdateInProgress()
{
	boost::unique_lock<boost::mutex> lock(update_mutex, boost::try_to_lock);
	return !lock.owns_lock();
}

void RSS::Database::PauseDownload(int item_id)
{	
	if (item_id < 0) {
		int ti[2] = { int(RSS::Net::Status::paused), int(RSS::Net::Status::downloading) };
		sql_query(SQL_Query::update_downloads, NULL, 0, ti, 2);
		{
			boost::lock_guard<boost::mutex> lock(download_threads_item_mutex);
			for (size_t i = 0; i < download_threads_item.size(); i++) {
				if (download_threads_item[i] >= 0) {
					download_threads_item[i] = THREAD_PAUSE_DOWNLOAD;
					download_threads[i].interrupt();
				}
			}
		}
		{
			boost::lock_guard<boost::mutex> lock(list.downloads_mutex);
			RSS::UI::DownloadMap::iterator iter = list.downloads.begin();
			RSS::UI::DownloadMap::iterator iterEnd = list.downloads.end();;
			while(iter != iterEnd) {
				if(iter->second.strings.back().at(0) == 0)
					getUIStatusString(RSS::Net::Status::paused, iter->second.strings.back(), false);
				++iter;
			}
			func.download_list_redraw();
		}
	}
	else {
		int ti[2] = { int(RSS::Net::Status::paused), item_id };
		sql_query(SQL_Query::update_download, NULL, 0, ti, 2);
		{
			boost::lock_guard<boost::mutex> lock(download_threads_item_mutex);
			for (size_t i = 0; i < download_threads_item.size(); i++) {
				if (download_threads_item[i] == item_id) {
					download_threads_item[i] = THREAD_PAUSE_DOWNLOAD;
					download_threads[i].interrupt();
					break;
				}
			}
		}
		{
			boost::lock_guard<boost::mutex> lock(list.downloads_mutex);
			RSS::UI::DownloadMap::iterator iter = list.downloads.find(item_id);
			RSS::UI::DownloadMap::iterator iterEnd = list.downloads.end();;
			if (iter != iterEnd) {
				if (iter->second.strings.back().at(0) == 0)
					getUIStatusString(RSS::Net::Status::paused, iter->second.strings.back());
			}
			func.download_list_redraw();
		}
	}
}

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

void RSS::Database::ResumeDownload(int item_id)
{
	int download_count = 0;
	// --------------------------------------- Resume all downloads -----------------------------------------------------
	if (item_id < 0) 
	{
		try {
			SQL::Lock sql_lock(*this, SQL_Query_s::select_downloads_status_not);
			sql_bind(sql_lock, int(RSS::Net::Status::downloading));
			while (sql_step(sql_lock))
			{
				std::string filename(sql_columnString(sql_lock));
				item_id = sql_columnInt(sql_lock);
				download_count++;
				{
					boost::lock_guard<boost::mutex> lock(download_mutex);
					if (downloads.size() == 0 || downloads.front() > item_id)
						downloads.push_front(item_id);
				}
			}
			int ti[2] = { int(RSS::Net::Status::downloading), int(RSS::Net::Status::paused) };
			sql_query(SQL_Query::update_downloads, NULL, 0, ti, 2);
		}
		catch (Exc& exc) {
			func.log(exc.what());
		}
		{
			boost::lock_guard<boost::mutex> lock(list.downloads_mutex);
			RSS::UI::DownloadMap::iterator iter = list.downloads.begin();
			RSS::UI::DownloadMap::iterator iterEnd= list.downloads.end();
			while(iter != iterEnd) {
				iter->second.strings.back()[0] = 0;
				++iter;
			}
		}
	}
	// --------------------------------------- Resume specific download -----------------------------------------------------
	else {
		try {
			SQL::Lock sql_lock(*this, SQL_Query_s::select_download_status_not);
			sql_bind(sql_lock, int(RSS::Net::Status::downloading));
			sql_bind(sql_lock, item_id);
			if (sql_step(sql_lock))
			{
				std::string filename(sql_columnString(sql_lock));
				download_count++;
				{
					boost::lock_guard<boost::mutex> lock(download_mutex);
					if (downloads.size() == 0 || downloads.front() > item_id)
						downloads.push_front(item_id);
				}
				int ti[2] = { int(RSS::Net::Status::downloading), item_id };
				sql_query(SQL_Query::update_download, NULL, 0, ti, 2);
				{
					boost::lock_guard<boost::mutex> lock(list.downloads_mutex);
					RSS::UI::DownloadMap::iterator iter = list.downloads.find(item_id);
					if (iter != list.downloads.end())
						iter->second.strings.back()[0] = 0;
				}
			}
			sql_reset(sql_lock);
		}
		catch (Exc& exc) {
			func.log(exc.what());
		}
	}
	if (download_count) {
		{
			boost::unique_lock<boost::mutex> lock(download_mutex);
			download_ready = true;
		}
		download_ready_cond.notify_all();
		func.download_list_redraw();
	}
}

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

void RSS::Database::CancelDownload(int item_id)
{
	// --------------------------------------- cancel specific download -----------------------------------------------------
	if (item_id >= 0) 
	{
		// ---------------- check active downloads
		boost::lock_guard<boost::mutex> lock(download_threads_item_mutex);
		for (size_t i = 0; i < download_threads_item.size(); i++) {
			if (download_threads_item[i] >= 0 && download_threads_item[i] == item_id) {
				download_threads_item[i] = THREAD_ABORT_DOWNLOAD;
				download_threads[i].interrupt();
				return;
			}
		}
		// ---------------- check nonactive downloads
		try {
			char		str_msg[200];
			std::string output_path;
			{
				boost::lock_guard<boost::mutex> lock(options.mutex);
				output_path = options.output_dir;
			}
			{
				SQL::Lock sql_lock(*this, SQL_Query_s::select_download_status_not);
				sql_bind(sql_lock, int(RSS::Net::Status::downloading));
				sql_bind(sql_lock, item_id);

				if (sql_step(sql_lock))
				{
					std::string filename(sql_columnString(sql_lock));
					std::string path = output_path + '/' + filename + ".part";
					std::remove(path.c_str());
					sql_query(SQL_Query::delete_download, NULL, 0, &item_id, 1, NULL);
					sql_query(SQL_Query::update_item_time, NULL, 0, &item_id, 1, NULL);

					_snprintf_s(str_msg, _TRUNCATE, "download of '%s' canceled", filename.c_str());
					func.log(str_msg);
				}
				sql_reset(sql_lock);
			}
			{
				boost::lock_guard<boost::mutex> lock(list.downloads_mutex);
				list.downloads.erase(item_id);
				func.download_list_update(list.downloads.size());
			}
		}
		catch (Exc& exc) {
			func.log(exc.what());
		}
	}
	// --------------------------------------- cancel all downloads -----------------------------------------------------
	else {
		// ---------------- clear the quere
		{
			boost::lock_guard<boost::mutex> lock(download_mutex);
			download_ready = false;
			std::deque<int> empty;
			std::swap(downloads, empty);
		}
		// ---------------- kill active downloads
		{
			boost::lock_guard<boost::mutex> lock(download_threads_item_mutex);
			for (size_t i = 0; i < download_threads_item.size(); i++) {
				if (download_threads_item[i] >= 0) {
					download_threads_item[i] = THREAD_ABORT_DOWNLOAD;
					download_threads[i].interrupt();
				}
			}
		}
		// ---------------- kill nonactive downloads
		try {
			std::string output_path;
			char		str_msg[200];
			{
				boost::lock_guard<boost::mutex> lock(options.mutex);
				output_path = options.output_dir;
			}
			{
				SQL::Lock sql_lock(*this, SQL_Query_s::select_downloads_status_not);
				sql_bind(sql_lock, int(RSS::Net::Status::downloading));
				while (sql_step(sql_lock))
				{
					std::string filename(sql_columnString(sql_lock));
					item_id = sql_columnInt(sql_lock);
					std::string path = output_path + '/' + filename + ".part";
					std::remove(path.c_str());
					_snprintf_s(str_msg, _TRUNCATE, "download of '%s' canceled", filename.c_str());
					func.log(str_msg);
				}
				sql_reset(sql_lock);
				sql_query(SQL_Query::delete_downloads);
			}
			{
				boost::lock_guard<boost::mutex> lock(list.downloads_mutex);
				list.downloads.clear();
				func.download_list_update(list.downloads.size());
			}			
		}
		catch (Exc& exc) {
			func.log(exc.what());
		}
	}
}

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

int RSS::Database::SetOptions(const RSS::Options& o)
{
	int flags = 0;

	// --------- autoupdate minutes
	if ((o.flags & RSS::Options::AUTOUPDATE) == RSS::Options::AUTOUPDATE) {
		if (o.autoupdate_minutes >= 3) {
			{
				boost::lock_guard<boost::mutex> lock(options.mutex);
				options.autoupdate_minutes = boost::posix_time::minutes(o.autoupdate_minutes);
			}
			flags |= RSS::Options::AUTOUPDATE;
			{
				boost::lock_guard<boost::mutex> lock(autoupdate_status_mutex);
				if (autoupdate_status == THREAD_AUTOUPDATE_WAIT) {
					autoupdate_status = THREAD_AUTOUPDATE_RESTART;
					thread_autoupdate.interrupt();
				}
			}			
		}
	}
	// --------- download range days
	if ((o.flags & RSS::Options::DL_RANGE) == RSS::Options::DL_RANGE) {
		if (o.download_range >= 1) {
			{
				boost::lock_guard<boost::mutex> lock(options.mutex);
				options.dl_range_days = o.download_range;
			}
			flags |= RSS::Options::DL_RANGE;
		}
	}
	// --------- filename mask
	if ((o.flags & RSS::Options::FILENAME_MASK) == RSS::Options::FILENAME_MASK) {
		if (o.filename_mask.size() > 3) {
			{
				boost::lock_guard<boost::mutex> lock(options.mutex);
				options.filename_mask.assign(o.filename_mask);
			}
			flags |= RSS::Options::FILENAME_MASK;
		}
	}
	// --------- output dir
	if ((o.flags & RSS::Options::OUTPUT_DIR) == RSS::Options::OUTPUT_DIR) {
		if (GetDownloadCount() == 0) {
			std::string temp(o.output_dir);
			boost::algorithm::replace_all(temp, "\\", "/");
			if (temp.back() == '/')
				temp.resize(temp.size() - 1);
			if (temp.size() > 2) {
				{
					boost::lock_guard<boost::mutex> lock(options.mutex);
					options.output_dir.assign(temp);
				}
				flags |= RSS::Options::OUTPUT_DIR;
			}
		}
	}
	// --------- download threads
	if ((o.flags & RSS::Options::DL_THREADS) == RSS::Options::DL_THREADS) {
		if (o.download_threads_count >= 1 && o.download_threads_count < 10)
		{
			if (IsConnected()) {
				bool wait_for_downloads = false;
				size_t cur_count;
				{
					boost::lock_guard<boost::mutex> lock(options.mutex);
					cur_count = options.dl_threads_count;
				}
				if (o.download_threads_count != cur_count)
				{
					boost::lock_guard<boost::mutex> lock(download_threads_item_mutex);

					if (o.download_threads_count > cur_count)
					{
						download_threads_item.resize(o.download_threads_count);
						download_threads.resize(o.download_threads_count);
						for (size_t i = cur_count; i < o.download_threads_count; i++) {
							download_threads_item[i] = -1;
							download_threads[i] = boost::thread(&RSS::Database::download, this, i);
						}
					}
					else {
						for (size_t i = 0; i < download_threads_item.size(); i++) {
							if (download_threads_item[i] >= 0) {
								wait_for_downloads = true;
								break;
							}
						}
						if (!wait_for_downloads) {
							for (size_t i = o.download_threads_count; i < cur_count; i++) {
								download_threads_item[i] = THREAD_SHUTDOWN;
								download_threads[i].interrupt();
								download_threads[i].join();
							}
							download_threads_item.resize(o.download_threads_count);
							download_threads.resize(o.download_threads_count);
						}
					}
					if (!wait_for_downloads) {
						{
							boost::lock_guard<boost::mutex> lock(options.mutex);
							options.dl_threads_count = o.download_threads_count;
						}
						flags |= RSS::Options::DL_THREADS;
					}
				}
			}
			else {
				options.dl_threads_count = o.download_threads_count;
				flags |= RSS::Options::DL_THREADS;
			}			
		}
	}
	// --------- 
	return flags;
}

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

const TCHAR* RSS::Database::GetFeedString(size_t i, size_t col)
{
	boost::lock_guard<boost::mutex> lock(list.feeds_mutex);
	if (i < list.feedsVector.size() && col < unsigned(RSS::UI::FeedColumns::COUNT))
		return &list.feedsVector[i]->strings[col][0];
	static TCHAR t[] = TEXT("?");
	return t;
}

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

const TCHAR* RSS::Database::GetDownloadString(size_t i, size_t col)
{
	boost::lock_guard<boost::mutex> lock(list.downloads_mutex);
	static TCHAR t[] = TEXT("?");

	if (col < unsigned(RSS::UI::DownloadColumns::COUNT))
	{
		RSS::UI::DownloadMap::iterator Iter = list.downloads.begin();
		RSS::UI::DownloadMap::iterator IterEnd = list.downloads.end();

		for (size_t x = 0; x < i; x++) {
			if (Iter == IterEnd)
				break;
			Iter++;
		}
		if (Iter != IterEnd)
			return &Iter->second.strings[col][0];
	}
	return t;
}

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

size_t RSS::Database::GetFeedCount()
{
	boost::lock_guard<boost::mutex> lock(list.feeds_mutex);
	return list.feedsVector.size();
}

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

size_t RSS::Database::GetDownloadCount()
{
	boost::lock_guard<boost::mutex> lock(list.downloads_mutex);
	size_t l = list.downloads.size();
	return l;
}

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

int RSS::Database::GetFeedID(size_t index)
{
	boost::lock_guard<boost::mutex> lock(list.feeds_mutex);
	size_t id = (index < list.feedsVector.size()) ? list.feedsVector[index]->id : -1;
	return id;
}

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

bool RSS::Database::GetDownloadInfo(size_t list_index, int& item_id, bool& is_active)
{
	item_id = -1;
	is_active = false;

	{
		boost::lock_guard<boost::mutex> lock(list.downloads_mutex);
		UI::DownloadMap::iterator Iter = list.downloads.begin();
		UI::DownloadMap::iterator IterEnd = list.downloads.end();

		for (size_t x = 0; x < list_index; x++) {
			if (Iter == IterEnd)
				break;
			Iter++;
		}
		if (Iter != IterEnd) {
			item_id = Iter->first;
		}
	}

	if (item_id >= 0) 
	{
		SQL::Lock sql_lock(*this, SQL::SQL_Query_s::select_download_status_not);
		sql_bind(sql_lock, int(Net::Status::downloading));
		sql_bind(sql_lock, item_id);
		is_active = !sql_step(sql_lock);
		sql_reset(sql_lock);

		return true;
	} else {
		return false;
	}
}

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

void RSS::Database::SetFeedListSort(RSS::UI::FeedColumns column, bool up)
{
	boost::lock_guard<boost::mutex> lock(list.feeds_mutex);
	if (column != RSS::UI::FeedColumns::COUNT)
		list.feeds_order_column = column;
	list.feeds_order_up = up;
}

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

void RSS::Database::SortFeedList(RSS::UI::FeedColumns column, bool up)
{
	boost::lock_guard<boost::mutex> lock(list.feeds_mutex);
	if (column != RSS::UI::FeedColumns::COUNT)
		list.feeds_order_column = column;
	list.feeds_order_up = up;

	list.feeds.sort(FeedCompare(list.feeds_order_column, list.feeds_order_up));
	list.feedsVector.clear();
	list.feedsVector.reserve(list.feeds.size());
	for (RSS::UI::FeedList::iterator it = list.feeds.begin(); it != list.feeds.end(); ++it)
		list.feedsVector.push_back(it);
	func.feed_list_update(list.feeds.size());
}

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

bool RSS::Database::GetFeedInfo(size_t index, RSS::UI::Feed& feed)
{
	boost::lock_guard<boost::mutex> lock(list.feeds_mutex);
	if (index >= list.feedsVector.size())
		return false;
	feed.id = list.feedsVector[index]->id;
	feed.retag = list.feedsVector[index]->retag;
	for (size_t i = 0; i < unsigned(RSS::UI::FeedColumns::COUNT); i++)
		feed.strings[i].assign(list.feedsVector[index]->strings[i].begin(), list.feedsVector[index]->strings[i].end());
	return true;
}

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

bool RSS::Database::SetFeedInfo(const RSS::UI::Feed& feed)
{
	try {
		size_t findex = 0;
		{
			boost::lock_guard<boost::mutex> lock(list.feeds_mutex);
			for (; findex < list.feedsVector.size(); findex++)
				if (list.feedsVector[findex]->id == feed.id)
					break;
			if (findex >= list.feedsVector.size())
				return false;
		}

		std::string fs[3];
		int fi[2] = { (feed.retag ? 1 : 0), feed.id };

		#ifdef RSS_UI_WSTRING
		const tchar* ss = feed.strings[unsigned(RSS::UI::FeedColumns::title)].c_str();
		size_t slen = wcslen(feed.strings[unsigned(RSS::UI::FeedColumns::title)].c_str());
		utf8::utf16to8(ss, ss + slen, back_inserter(fs[0]));
		ss = feed.strings[unsigned(RSS::UI::FeedColumns::homepage)].c_str();
		slen = wcslen(feed.strings[unsigned(RSS::UI::FeedColumns::homepage)].c_str());
		utf8::utf16to8(ss, ss + slen, back_inserter(fs[1]));
		ss = feed.strings[unsigned(RSS::UI::FeedColumns::author)].c_str();
		slen = wcslen(feed.strings[unsigned(RSS::UI::FeedColumns::author)].c_str());
		utf8::utf16to8(ss, ss + slen, back_inserter(fs[2]));

		#else
		fs[0].assign(feed.strings[unsigned(RSS::UI::FeedColumns::title)]);
		fs[1].assign(feed.strings[unsigned(RSS::UI::FeedColumns::homepage)]);
		fs[2].assign(feed.strings[unsigned(RSS::UI::FeedColumns::author)]);
		#endif

		if (sql_query(SQL_Query::update_feed_info, fs, 3, fi, 2)) 
		{
			boost::lock_guard<boost::mutex> lock(list.feeds_mutex);
			list.feedsVector[findex]->retag = feed.retag;
			list.feedsVector[findex]->strings[unsigned(RSS::UI::FeedColumns::title)].assign(feed.strings[unsigned(RSS::UI::FeedColumns::title)].begin(), feed.strings[unsigned(RSS::UI::FeedColumns::title)].end());
			list.feedsVector[findex]->strings[unsigned(RSS::UI::FeedColumns::author)].assign(feed.strings[unsigned(RSS::UI::FeedColumns::author)].begin(), feed.strings[unsigned(RSS::UI::FeedColumns::author)].end());
			list.feedsVector[findex]->strings[unsigned(RSS::UI::FeedColumns::homepage)].assign(feed.strings[unsigned(RSS::UI::FeedColumns::homepage)].begin(), feed.strings[unsigned(RSS::UI::FeedColumns::homepage)].end());

			list.feeds.sort(FeedCompare(list.feeds_order_column, list.feeds_order_up));
			list.feedsVector.clear();
			list.feedsVector.reserve(list.feeds.size());
			for (RSS::UI::FeedList::iterator it = list.feeds.begin(); it != list.feeds.end(); ++it)
				list.feedsVector.push_back(it);
			func.feed_list_update(list.feeds.size());
			return true;
		}
	}
	catch (Exc& exc) {
		func.log(exc.what());
	}
	catch (utf8::exception& exc) {
		func.log(exc.what());
	}
	return false;
}

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

RSS::Signals::Conn RSS::Database::registerLog(const RSS::Signals::StringSlotType& t)
{
	return func.log.connect(t);
}

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

RSS::Signals::Conn RSS::Database::registerUpdateFeeds(const RSS::Signals::UnsignedSlotType& t)
{
	return func.feed_list_update.connect(t);
}

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

RSS::Signals::Conn RSS::Database::registerUpdateDownloads(const RSS::Signals::UnsignedSlotType& t)
{
	return func.download_list_update.connect(t);
}

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

RSS::Signals::Conn RSS::Database::registerRedrawDownloads(const RSS::Signals::VoidSlotType& t)
{
	return func.download_list_redraw.connect(t);
}

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

RSS::Signals::Conn RSS::Database::registerDownloadDone(const RSS::Signals::StringIntSlotType& t)
{
	return func.download_done.connect(t);
}

RSS::Signals::Conn RSS::Database::registerHighlightFeed(const RSS::Signals::IntSlotType& t)
{
	return func.highlight_feed.connect(t);
}

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

void RSS::Database::doAddFeed(std::string url)
{
	// ---------------- Lock
	boost::unique_lock<boost::mutex> lock(update_mutex, boost::try_to_lock);
	if (!lock.owns_lock()) {
		func.log("cannot add feed: update in progress.");
		return;
	}

	try {
		int							feed_id;
		std::string					feed_strings[unsigned(SQL_Feed_s::COUNT)];
		std::ostringstream			buffer;
		RSS::Net::HttpHeader		s_header;
		char						msg[60];
		tinyxml2::XMLDocument		doc;
		tinyxml2::XMLElement*		xml_el;
		tinyxml2::XMLError			xml_err;

		// ---------------- Check if feed already exists:
		feed_strings[unsigned(SQL_Feed_s::url)].assign(url);
		int query_ret = 0;
		sql_query(SQL_Query::count_feeds_url, &feed_strings[unsigned(SQL_Feed_s::url)], 1, NULL, 0, &query_ret);
		if (query_ret > 0)
			throw Exc("feed already in database");

		// ---------------- Get feed-data:		
		getFeedData(url.c_str(), &buffer, &s_header);

		if (s_header.enc == RSS::Net::HttpHeader::Encoding::gzip
			|| s_header.enc == RSS::Net::HttpHeader::Encoding::bzip2) {
			std::stringstream out;
			boost::iostreams::filtering_streambuf<boost::iostreams::input> in;
			if (s_header.enc == RSS::Net::HttpHeader::Encoding::gzip)
				in.push(boost::iostreams::gzip_decompressor());
			else
				in.push(boost::iostreams::bzip2_decompressor());
			const std::string& tmp1 = buffer.str();
			in.push(boost::iostreams::array_source(tmp1.data(), tmp1.size()));
			boost::iostreams::copy(in, out);
			const std::string& tmp2 = out.str();
			xml_err = doc.Parse(tmp2.c_str(), tmp2.size());
		}
		else {
			const std::string& tmp = buffer.str();
			xml_err = doc.Parse(tmp.c_str(), tmp.size());
		}

		if (xml_err != tinyxml2::XML_NO_ERROR)
			throw Exc("invalid rss-feed: failed to parse xml");
		if ((xml_el = doc.FirstChildElement("rss")) == NULL)
			throw Exc("invalid rss-feed: <rss> not found");
		if ((xml_el = xml_el->FirstChildElement("channel")) == NULL)
			throw Exc("invalid rss-feed: <channel> not found");

		feed_strings[(unsigned)SQL_Feed_s::last_checked] = s_header.date;
		feed_strings[(unsigned)SQL_Feed_s::last_published] = s_header.last_modified;

		// ---------------- Get feed-strings
		parseFeed(xml_el, feed_strings);

		// ---------------- Update database
		sql_query(SQL_Query::insert_feed, feed_strings, (size_t)SQL_Feed_s::COUNT, NULL, 0, &feed_id);
		size_t item_count = updateItems(feed_id, xml_el);

		// ---------------- Update list
		{
			boost::lock_guard<boost::mutex> lock(list.feeds_mutex);
			RSS::UI::Feed	feed_ui;
			feed_ui.id = feed_id;
			feed_ui.retag = false;
			getFeedUIStrings(feed_strings, feed_ui.strings, item_count);
			list.feeds.push_back(feed_ui);
			list.feeds.sort(FeedCompare(list.feeds_order_column, list.feeds_order_up));
			list.feedsVector.clear();
			list.feedsVector.reserve(list.feeds.size());
			for (RSS::UI::FeedList::iterator it = list.feeds.begin(); it != list.feeds.end(); ++it)
				list.feedsVector.push_back(it);
			func.feed_list_update(list.feeds.size());
		}

		// ---------------- Log		
		_snprintf_s(msg, _TRUNCATE, "added feed '%s'", feed_strings[(unsigned)SQL_Feed_s::title].c_str());
		func.log(msg);
	}
	catch (Exc& exc) {
		func.log(exc.what());
	}
	catch (boost::thread_interrupted&) {
		// thread interrupted: exit
	}
}

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

void RSS::Database::autoUpdate()
{
	while (true) {
		try {

			boost::posix_time::minutes sleeptime(30);
			{
				boost::lock_guard<boost::mutex> lock(options.mutex);
				sleeptime = options.autoupdate_minutes;
			}
			{
				boost::lock_guard<boost::mutex> lock(autoupdate_status_mutex);
				autoupdate_status = THREAD_AUTOUPDATE_WAIT;
			}
			boost::this_thread::sleep(sleeptime);
			{
				boost::lock_guard<boost::mutex> lock(autoupdate_status_mutex);
				autoupdate_status = THREAD_AUTOUPDATE_INPROGRESS;
			}
			updateFeeds(false);
		}
		catch (Exc& exc) {
			func.log(exc.what());
		}
		catch (boost::thread_interrupted&) {
			boost::lock_guard<boost::mutex> lock(autoupdate_status_mutex);
			if(autoupdate_status == THREAD_SHUTDOWN)
				break;
		}
	}
}

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

void RSS::Database::updateFeeds(bool rebuild)
{
	// ---------------- Lock (if update in progress: skip)
	boost::unique_lock<boost::mutex> lock(update_mutex, boost::try_to_lock);
	if (!lock.owns_lock())
		return;
	if (!sql_isconnected())
		return;
	
	try {		

		RSS::Net::HttpHeader		s_header;
		std::string					feed_strings[unsigned(SQL_Feed_s::COUNT)];
		std::vector<int>			feed_ids;
		char						msg[60];

		if (rebuild) {
			func.log("rebuilding database...");
			sql_query(SQL_Query::delete_items);
		}

		{
			boost::lock_guard<boost::mutex> lock(list.feeds_mutex);
			for (size_t i = 0; i < list.feedsVector.size(); i++)
				feed_ids.push_back(list.feedsVector[i]->id);
		}

		// ---------------- Loop through feeds
		for (size_t feed_index = 0; feed_index < feed_ids.size(); feed_index++)
		{
			try {
				func.highlight_feed((int)feed_index);

				{
					SQL::Lock sql_lock(*this, SQL_Query_s::select_feed);
					sql_bind(sql_lock, feed_ids[feed_index]);
					sql_step(sql_lock);
					for (size_t i = 0; i < (size_t)SQL_Feed_s::COUNT; i++)
						feed_strings[i].assign(sql_columnString(sql_lock));
					sql_reset(sql_lock);
				}

				// ---------------- Check if feed got modified
				if (rebuild || isModifiedSince(feed_strings[(unsigned)SQL_Feed_s::url].c_str(), feed_strings[(unsigned)SQL_Feed_s::last_checked], &s_header))
				{
					// ---------------- Get feed data
					std::ostringstream		buffer;
					getFeedData(feed_strings[(unsigned)SQL_Feed_s::url].c_str(), &buffer, &s_header);

					boost::this_thread::interruption_point();

					feed_strings[(unsigned)SQL_Feed_s::last_checked] = s_header.date;
					feed_strings[(unsigned)SQL_Feed_s::last_published] = s_header.last_modified;

					// ---------------- Reparse feed
					tinyxml2::XMLDocument doc;
					tinyxml2::XMLElement*	xml_el;
					tinyxml2::XMLError		xml_err;

					if (s_header.enc == RSS::Net::HttpHeader::Encoding::gzip
						|| s_header.enc == RSS::Net::HttpHeader::Encoding::bzip2) {
						std::stringstream out;
						boost::iostreams::filtering_streambuf<boost::iostreams::input> in;
						if (s_header.enc == RSS::Net::HttpHeader::Encoding::gzip)
							in.push(boost::iostreams::gzip_decompressor());
						else
							in.push(boost::iostreams::bzip2_decompressor());
						const std::string& tmp1 = buffer.str();
						in.push(boost::iostreams::array_source(tmp1.data(), tmp1.size()));
						boost::iostreams::copy(in, out);
						const std::string& tmp2 = out.str();
						xml_err = doc.Parse(tmp2.c_str(), tmp2.size());
					}
					else {
						const std::string& tmp = buffer.str();
						xml_err = doc.Parse(tmp.c_str(), tmp.size());
					}

					if (xml_err != tinyxml2::XML_NO_ERROR)
						throw Exc("invalid rss-feed: failed to parse xml");
					if ((xml_el = doc.FirstChildElement("rss")) == NULL)
						throw Exc("invalid rss-feed: <rss> not found");
					if ((xml_el = xml_el->FirstChildElement("channel")) == NULL)
						throw Exc("invalid rss-feed: <channel> not found");

					parseFeed(xml_el, feed_strings, !rebuild);

					if (rebuild)
						sql_query(SQL_Query::update_feed, feed_strings, (size_t)SQL_Feed_s::COUNT, &feed_ids[feed_index], 1, NULL);
					else
						sql_query(SQL_Query::update_feed_dates, &feed_strings[unsigned(SQL_Feed_s::last_published)], 2, &feed_ids[feed_index], 1, NULL);

					// ---------------- Items
					size_t icount = updateItems(feed_ids[feed_index], xml_el);

					{
						boost::lock_guard<boost::mutex> lock(list.feeds_mutex);
						getFeedUIStrings(feed_strings, list.feedsVector[feed_index]->strings, icount, !rebuild);
					}

					_snprintf_s(msg, _TRUNCATE, "'%s' updated", feed_strings[(unsigned)SQL_Feed_s::title].c_str());
					func.log(msg);
				}
				else {
					// ---------------- Feed not modified: update last_checked
					_snprintf_s(msg, _TRUNCATE, "'%s' not modified", feed_strings[(unsigned)SQL_Feed_s::title].c_str());
					func.log(msg);
					feed_strings[unsigned(SQL_Feed_s::last_checked)].assign(s_header.date);
					sql_query(SQL_Query::update_feed_check, &feed_strings[unsigned(SQL_Feed_s::last_checked)], 1, &feed_ids[feed_index], 1, NULL);
				}
				boost::this_thread::interruption_point();
			}
			catch (Exc& exc)
			{
				_snprintf_s(msg, _TRUNCATE, "error while updating '%s': %s", feed_strings[(unsigned)SQL_Feed_s::title].c_str(), exc.what());
				func.log(msg);
			}
		}
		func.highlight_feed(-1);

		// ---------------- Vacuum
		if (rebuild) {
			sql_query(SQL_Query::vacuum);
		}

		// ---------------- Send list-update msg
		{
			boost::lock_guard<boost::mutex> lock(list.feeds_mutex);
			if (list.feedsVector.size() == 0)
				return;
			if (rebuild) {
				list.feeds.sort(FeedCompare(list.feeds_order_column, list.feeds_order_up));
				list.feedsVector.clear();
				list.feedsVector.reserve(list.feeds.size());
				for (RSS::UI::FeedList::iterator it = list.feeds.begin(); it != list.feeds.end(); ++it)
					list.feedsVector.push_back(it);
			}
			func.feed_list_update(list.feeds.size());
		}

		char			t_str_days[] = "-00 days";
		bool			enable_downloads = true;
		std::string		t_filename_mask;

		// ------------- Get options
		{
			boost::lock_guard<boost::mutex> lock(options.mutex);
			if (!options.output_dir.size())
				enable_downloads = false;
			t_filename_mask.assign(options.filename_mask);
			if (!t_filename_mask.size())
				enable_downloads = false;

			if (options.dl_range_days < 10) {
				t_str_days[2] = '0' + options.dl_range_days;
			}
			else {
				t_str_days[1] = '0' + char(options.dl_range_days / 10);
				t_str_days[2] = '0' + char(options.dl_range_days % 10);
			}
		}

		if (!enable_downloads) 
		{
			func.log("downloads disabled: please check options");
		} else {

			std::string					t_download_s[unsigned(SQL::SQL_Download_s::COUNT)];
			int							t_download_i[unsigned(SQL::SQL_Download_i::COUNT)];
			RSS::UI::Download			t_ui_download;
			size_t						new_downloads_count = 0;

			{
				SQL::Lock sql_lock(*this, SQL_Query_s::select_new_items);
				sql_bind(sql_lock, t_str_days);

				while (sql_step(sql_lock))
				{
					// ----------- setup t_download
					t_download_i[unsigned(SQL_Download_i::item_id)] = sql_columnInt(sql_lock);
					t_download_s[unsigned(SQL_Download_s::title)].assign(sql_columnString(sql_lock));
					t_download_s[unsigned(SQL_Download_s::url)].assign(sql_columnString(sql_lock));
					std::string feed_title(sql_columnString(sql_lock));
					std::string feed_author(sql_columnString(sql_lock));
					std::string t_url_filename = t_download_s[unsigned(SQL_Download_s::url)].substr(t_download_s[unsigned(SQL_Download_s::url)].find_last_of('/') + 1);
					std::string t_extension = t_url_filename.substr(t_url_filename.find_last_of('.') + 1);
					std::string t_filename(t_filename_mask);
					boost::algorithm::replace_all(t_filename, "%filename%", t_url_filename);
					boost::algorithm::replace_all(t_filename, "%feedtitle%", feed_title);
					boost::algorithm::replace_all(t_filename, "%feedauthor%", feed_author);
					boost::algorithm::replace_all(t_filename, "%ext%", t_extension);
					boost::algorithm::replace_all(t_filename, "%title%", t_download_s[unsigned(SQL_Download_s::title)]);
					t_filename.erase(std::remove_if(t_filename.begin(), t_filename.end(), [](unsigned char x) {return !(std::isalnum(x) || x == ' ' || x == '.' || x == '-' || x == '_'); }), t_filename.end());
					if (t_filename.size() < 3)
						t_download_s[unsigned(SQL_Download_s::filename)].assign(t_url_filename);
					else
						t_download_s[unsigned(SQL_Download_s::filename)].assign(t_filename);
					t_download_i[unsigned(SQL_Download_i::filesize)] = sql_columnInt(sql_lock);
					t_download_i[unsigned(SQL_Download_i::status)] = int(RSS::Net::Status::downloading);

					// ------------ add to ui-download-list
					getDownloadUIStrings(t_download_s, t_ui_download.strings);
					{
						boost::lock_guard<boost::mutex> lock(list.downloads_mutex);
						RSS::UI::DownloadMap::iterator iter = list.downloads.find(t_download_i[unsigned(SQL_Download_i::item_id)]);
						if (iter != list.downloads.end()) // should not happen
							continue;

						list.downloads.insert(std::pair<int, RSS::UI::Download>(t_download_i[unsigned(SQL_Download_i::item_id)], t_ui_download));
					}

					// ----------- add to queure
					{
						boost::unique_lock<boost::mutex> lock(download_mutex);
						downloads.push_back(t_download_i[unsigned(SQL_Download_i::item_id)]);
					}

					// ----------- add to database
					sql_query(SQL_Query::update_item_time, NULL, 0, &t_download_i[unsigned(SQL_Download_i::item_id)], 1);
					sql_query(SQL_Query::insert_download, t_download_s, size_t(SQL_Download_s::COUNT), t_download_i, size_t(SQL_Download_i::COUNT));

					new_downloads_count++;
				}
				sql_reset(sql_lock);
			}

			if (new_downloads_count) 
			{
				{
					boost::unique_lock<boost::mutex> lock(download_mutex);
					download_ready = true;
				}
				{
					boost::lock_guard<boost::mutex> lock(list.downloads_mutex);
					func.download_list_update(list.downloads.size());
				}

				download_ready_cond.notify_all();
				_snprintf_s(msg, _TRUNCATE, "%d items to download", new_downloads_count);
				func.log(msg);
			}
			else {
				func.log("no new items to download");
			}
		}
	}
	catch (Exc& exc)
	{
		func.log(exc.what());
	}
}

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

size_t RSS::Database::updateItems(int feed_id, tinyxml2::XMLElement* xml_channel)
{
	size_t						item_count = 0;
	int							item_id;
	std::string					item_strings[unsigned(SQL_Item_s::COUNT)];
	int							item_ints[unsigned(SQL_Item_i::COUNT)];

	item_ints[unsigned(SQL_Item_i::feed_id)] = feed_id;

	// ---------------- Loop through items
	tinyxml2::XMLElement* xml_el_item = xml_channel->FirstChildElement("item");
	while (true)
	{
		if (xml_el_item == NULL)
			break;
		boost::this_thread::interruption_point();

		// ---------------- Parse item
		if (parseItem(xml_el_item, item_strings, item_ints)) {
			try {
				if(!sql_query(SQL_Query::get_item_by_date, &item_strings[unsigned(SQL_Item_s::pubdate)], 1, &feed_id, 1))
					sql_query(SQL_Query::insert_item, item_strings, (size_t)SQL_Item_s::COUNT, item_ints, (size_t)SQL_Item_i::COUNT, &item_id);
			}
			catch (Exc& exc) {
				func.log(exc.what());
			}
		}
		item_count++;
		xml_el_item = xml_el_item->NextSiblingElement("item");
	}
	return item_count;
}

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

void RSS::Database::download(size_t thread_id)
{
	while (true)
	{
		CURL*					conn = NULL;
		std::fstream			f;
		int						item_id = -1;
		char					str_error[CURL_ERROR_SIZE];
		char					str_msg[CURL_ERROR_SIZE + 50];
		std::string				t_path;
		std::string				t_download[unsigned(SQL::SQL_Download_s::COUNT)];

		try {
			// ----------------------------- set cur-item id
			{
				boost::lock_guard<boost::mutex> lock(download_threads_item_mutex);
				download_threads_item[thread_id] = -1;
			}

			// ----------------------------- Wait for new download-data
			
			item_id = -1;
			{
				boost::unique_lock<boost::mutex> lock(download_mutex);
				while (!download_ready)
				{
					download_ready_cond.wait(lock);
				}

				item_id = downloads.front();
				downloads.pop_front();

				if (!downloads.size())
					download_ready = false;
			}

			// -----------------------------------------------------------------------------		

			CURLcode				code;
			std::string				path;
			RSS::Net::ProgressData	progress_data;
			RSS::Net::FileData		file_data;
			RSS::UI::Download		t_ui_download;

			// ----------------------------- set cur-item id
			{
				boost::lock_guard<boost::mutex> lock(download_threads_item_mutex);
				download_threads_item[thread_id] = item_id;
			}

			// ----------------------------- get data/check if active
			{
				SQL::Lock sql_lock(*this, SQL_Query_s::select_download_status);
				sql_bind(sql_lock, int(RSS::Net::Status::downloading));
				sql_bind(sql_lock, item_id);
				if (!sql_step(sql_lock)) {
					sql_reset(sql_lock);
					continue;
				}
				for (size_t i = 0; i < size_t(SQL::SQL_Download_s::COUNT); i++)
					t_download[i].assign(sql_columnString(sql_lock));
				sql_reset(sql_lock);
			}

			// ----------------------------- Get output path
			{
				boost::lock_guard<boost::mutex> lock(options.mutex);
				t_path = options.output_dir + '/' + t_download[unsigned(SQL_Download_s::filename)] + ".part";
			}

			// ----------------------------- Open ouput path
			f.open(t_path.c_str(), std::ios::out | std::ios::binary | std::ios::ate | std::ios::app);
			if (!f.is_open()) {
				_snprintf_s(str_msg, _TRUNCATE, "failed to open %s", t_path.c_str());
				throw Exc(str_msg);
			}

			// ---------------- Init connection
			if ((conn = curl_easy_init()) == NULL)
				throw Exc("curl_easy_init failed", Exc::Level::critical);

			// ----------------------------- Prepare thread_data & progress_data
			file_data.file = &f;
			file_data.ibytes = static_cast<long long>(f.tellg());
			progress_data.func_update_ui = &func.download_list_redraw;
			progress_data.lasttime = 0;
			progress_data.strbuf = NULL;
			progress_data.offset = file_data.ibytes;
			progress_data.curl = conn;

			{
				boost::lock_guard<boost::mutex> lock(list.downloads_mutex);
				RSS::UI::DownloadMap::iterator iter = list.downloads.find(item_id);
				if (iter != list.downloads.end()) {
					progress_data.strbuf = &iter->second.strings.back()[0];
				}
			}

			// ---------------- Set curl options
			if (curl_easy_setopt(conn, CURLOPT_ERRORBUFFER, str_error) != CURLE_OK)
				throw Exc("curl: failed to set error buffer", Exc::Level::critical);

			if (curl_easy_setopt(conn, CURLOPT_URL, t_download[unsigned(SQL_Download_s::url)].c_str()) != CURLE_OK) {
				_snprintf_s(str_msg, _TRUNCATE, "curl: failed to set URL [%s]", str_error);
				throw Exc(str_msg, Exc::Level::critical);
			}
			if (curl_easy_setopt(conn, CURLOPT_RESUME_FROM, file_data.ibytes) != CURLE_OK)	{
				_snprintf_s(str_msg, _TRUNCATE, "curl: failed to set resume from [%s]", str_error);
				throw Exc(str_msg, Exc::Level::critical);
			}
			if (curl_easy_setopt(conn, CURLOPT_FOLLOWLOCATION, 1L) != CURLE_OK)	{
				_snprintf_s(str_msg, _TRUNCATE, "curl: failed to set redirect option [%s]", str_error);
				throw Exc(str_msg, Exc::Level::critical);
			}
			if (curl_easy_setopt(conn, CURLOPT_USERAGENT, "foo_uie_podcast/1.0") != CURLE_OK)	{
				_snprintf_s(str_msg, _TRUNCATE, "curl: failed to set user agent [%s]", str_error);
				throw Exc(str_msg, Exc::Level::critical);
			}
			if (curl_easy_setopt(conn, CURLOPT_WRITEFUNCTION, func_write_file) != CURLE_OK) {
				_snprintf_s(str_msg, _TRUNCATE, "curl: failed to set writer [%s]", str_error);
				throw Exc(str_msg, Exc::Level::critical);
			}
			if (curl_easy_setopt(conn, CURLOPT_WRITEDATA, &file_data) != CURLE_OK) {
				_snprintf_s(str_msg, _TRUNCATE, "curl: failed to set write data [%s]", str_error);
				throw Exc(str_msg, Exc::Level::critical);
			}
			if (curl_easy_setopt(conn, CURLOPT_XFERINFOFUNCTION, &func_progress) != CURLE_OK) {
				_snprintf_s(str_msg, _TRUNCATE, "curl: failed to set progress func [%s]", str_error);
				throw Exc(str_msg, Exc::Level::critical);
			}
			if (curl_easy_setopt(conn, CURLOPT_XFERINFODATA, &progress_data) != CURLE_OK) {
				_snprintf_s(str_msg, _TRUNCATE, "curl: failed to set progress data [%s]", str_error);
				throw Exc(str_msg, Exc::Level::critical);
			}
			if (curl_easy_setopt(conn, CURLOPT_NOPROGRESS, 0L) != CURLE_OK) {
				_snprintf_s(str_msg, _TRUNCATE, "curl: failed to set noprogress to 0 [%s]", str_error);
				throw Exc(str_msg, Exc::Level::critical);
			}
			if (curl_easy_setopt(conn, CURLOPT_LOW_SPEED_LIMIT, 300) != CURLE_OK) {
				_snprintf_s(str_msg, _TRUNCATE, "curl: failed to set low speed limit [%s]", str_error);
				throw Exc(str_msg, Exc::Level::critical);
			}
			if (curl_easy_setopt(conn, CURLOPT_LOW_SPEED_TIME, 5) != CURLE_OK) {
				_snprintf_s(str_msg, _TRUNCATE, "curl: failed to set low speed time [%s]", str_error);
				throw Exc(str_msg, Exc::Level::critical);
			}

			// ---------------- Log start download
			if (file_data.ibytes > 0)
				_snprintf_s(str_msg, _TRUNCATE, "resuming download of '%s'...", t_download[unsigned(SQL_Download_s::filename)].c_str());
			else
				_snprintf_s(str_msg, _TRUNCATE, "starting download of '%s'...", t_download[unsigned(SQL_Download_s::filename)].c_str());
			func.log(str_msg);

			// ---------------- Perform download
			code = curl_easy_perform(conn);

			// ---------------- Cleanup
			curl_easy_cleanup(conn);
			conn = NULL;
			f.close();
			
			if (code != CURLE_OK)
			{
				_snprintf_s(str_msg, _TRUNCATE, "download of '%s' failed: %s", t_download[unsigned(SQL_Download_s::filename)].c_str(), str_error);
				throw Exc(str_msg);
			}

			path = t_path.substr(0, t_path.size() - 5);
			if (std::rename(t_path.c_str(), path.c_str()) != 0)
			{
				// ---------------- Rename failed. perhaps file already exits, try some other names:
				bool successful_rename = false;
				std::string tpath = path.substr(0, path.size() - 5);
				size_t p = tpath.find_last_of('.');
				if (p != std::string::npos && p > 5 && p < tpath.size())
				{
					std::string extension = tpath.substr(p);
					tpath = tpath.substr(0, p);
					std::string temp = "( )";
					for (size_t i = 0; i < 5; i++) {
						temp[1] = '1' + char(i);
						path = tpath + temp + extension;
						if (std::rename(path.c_str(), path.c_str()) == 0) {
							successful_rename = true;
							break;
						}
					}
				}
				if (!successful_rename) {
					_snprintf_s(str_msg, _TRUNCATE, "failed to rename file: %s", t_download[unsigned(SQL_Download_s::filename)].c_str());
					func.log(str_msg);
				}
			}

			// ---------------- Success
			_snprintf_s(str_msg, _TRUNCATE, "download of '%s' complete", t_download[unsigned(SQL_Download_s::filename)].c_str());
			func.log(str_msg);

			// ---------------- Update database
			sql_query(SQL_Query::delete_download, NULL, 0, &item_id, 1, NULL);
			sql_query(SQL_Query::update_item_time, NULL, 0, &item_id, 1, NULL);

			{
				// ---------------- Delete Download from UI-List
				boost::lock_guard<boost::mutex> lock(list.downloads_mutex);
				list.downloads.erase(item_id);
				func.download_list_update(list.downloads.size());
			}

			func.download_done(path.c_str(), item_id);
		}
		// =============================== The download failed:
		catch (Exc& exc) {
			// -------------------- Cleanup
			if (conn)
				curl_easy_cleanup(conn);
			f.close();
			func.log(exc.what());
			// -------------------- Update UI list:
			{
				boost::lock_guard<boost::mutex> lock(list.downloads_mutex);
				RSS::UI::DownloadMap::iterator iter = list.downloads.find(item_id);
				if (iter != list.downloads.end()) {
					getUIStatusString(RSS::Net::Status::error, iter->second.strings.back(), false);
				}
			}
			func.download_list_redraw();
			// -------------------- Update database (download)
			try {
				int ti[2] = { int(RSS::Net::Status::error), item_id };
				sql_query(SQL_Query::update_download, NULL, 0, ti, 2, NULL);
			}
			catch (Exc& exc) {
				func.log(exc.what());
			}
		}
		catch (boost::thread_interrupted&)
		{
			// -------------------- Cleanup
			if (conn)
				curl_easy_cleanup(conn);
			f.close();
			// ------------------ Check what we should do
			int action;
			{
				boost::lock_guard<boost::mutex> lock(download_threads_item_mutex);
				action = download_threads_item[thread_id];
			}
			if (action == THREAD_SHUTDOWN)
				break;
			if (item_id == -1)
				continue;
			
			try {
				// ------------------ Download was paused:
				if (action == THREAD_PAUSE_DOWNLOAD)
				{
					// -------------------- Update UI list:
					{
						boost::lock_guard<boost::mutex> lock(list.downloads_mutex);
						RSS::UI::DownloadMap::iterator iter = list.downloads.find(item_id);
						if (iter != list.downloads.end()) {
							getUIStatusString(RSS::Net::Status::paused, iter->second.strings.back());
						}
						func.download_list_redraw();
					}
					_snprintf_s(str_msg, _TRUNCATE, "download of '%s' paused", t_download[unsigned(SQL_Download_s::filename)].c_str());
					func.log(str_msg);
				}
				// ------------------ Download was aborted
				else {
					if (std::remove(t_path.c_str()) != 0) {
						_snprintf_s(str_msg, _TRUNCATE, "failed to delete: %s", t_path.c_str());
						func.log(str_msg);
					}

					// ---------------- Update database
					sql_query(SQL_Query::delete_download, NULL, 0, &item_id, 1, NULL);
					sql_query(SQL_Query::update_item_time, NULL, 0, &item_id, 1, NULL);

					{
						// ---------------- Delete Download from UI-List
						boost::lock_guard<boost::mutex> lock(list.downloads_mutex);
						list.downloads.erase(item_id);
						func.download_list_update(list.downloads.size());
					}
					_snprintf_s(str_msg, _TRUNCATE, "download of '%s' canceled", t_download[unsigned(SQL_Download_s::filename)].c_str());
					func.log(str_msg);
				}
			}
			catch (Exc& exc) {
				func.log(exc.what());
			}
		}
	}
}

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

void RSS::Database::getFeedData(const char* url, std::ostringstream* buffer, RSS::Net::HttpHeader* s_header)
{
	CURL*		conn = NULL;
	CURLcode	code;
	char		str_error[CURL_ERROR_SIZE];
	char		str_error_msg[CURL_ERROR_SIZE + 50];

	s_header->clear();

	try {
		// ---------------- Init connection
		if ((conn = curl_easy_init()) == NULL)
			throw Exc("curl_easy_init failed", Exc::Level::critical);

		// ---------------- Set curl options
		if (curl_easy_setopt(conn, CURLOPT_ERRORBUFFER, str_error) != CURLE_OK)
			throw Exc("curl: failed to set error buffer", Exc::Level::critical);

		if (curl_easy_setopt(conn, CURLOPT_URL, url) != CURLE_OK) {
			_snprintf_s(str_error_msg, _TRUNCATE, "curl: failed to set URL [%s]", str_error);
			throw Exc(str_error_msg, Exc::Level::critical);
		}
		if (curl_easy_setopt(conn, CURLOPT_FOLLOWLOCATION, 1L) != CURLE_OK)	{
			_snprintf_s(str_error_msg, _TRUNCATE, "curl: failed to set redirect option [%s]", str_error);
			throw Exc(str_error_msg, Exc::Level::critical);
		}
		if (curl_easy_setopt(conn, CURLOPT_USERAGENT, "foo_uie_podcast/1.0") != CURLE_OK)	{
			_snprintf_s(str_error_msg, _TRUNCATE, "curl: failed to set user agent [%s]", str_error);
			throw Exc(str_error_msg, Exc::Level::critical);
		}
		if (curl_easy_setopt(conn, CURLOPT_WRITEFUNCTION, func_write_data) != CURLE_OK) {
			_snprintf_s(str_error_msg, _TRUNCATE, "curl: failed to set writer [%s]", str_error);
			throw Exc(str_error_msg, Exc::Level::critical);
		}
		if (curl_easy_setopt(conn, CURLOPT_HEADERFUNCTION, func_write_header) != CURLE_OK) {
			_snprintf_s(str_error_msg, _TRUNCATE, "curl: failed to set header writer [%s]", str_error);
			throw Exc(str_error_msg, Exc::Level::critical);
		}
		if (curl_easy_setopt(conn, CURLOPT_WRITEDATA, buffer) != CURLE_OK) {
			_snprintf_s(str_error_msg, _TRUNCATE, "curl: failed to set write data [%s]", str_error);
			throw Exc(str_error_msg, Exc::Level::critical);
		}
		if (curl_easy_setopt(conn, CURLOPT_HEADERDATA, s_header) != CURLE_OK)	{
			_snprintf_s(str_error_msg, _TRUNCATE, "curl: failed to set header data [%s]", str_error);
			throw Exc(str_error_msg, Exc::Level::critical);
		}
		if (curl_easy_setopt(conn, CURLOPT_LOW_SPEED_LIMIT, 300) != CURLE_OK) {
			_snprintf_s(str_error_msg, _TRUNCATE, "curl: failed to set low speed limit [%s]", str_error);
			throw Exc(str_error_msg, Exc::Level::critical);
		}
		if (curl_easy_setopt(conn, CURLOPT_LOW_SPEED_TIME, 5) != CURLE_OK) {
			_snprintf_s(str_error_msg, _TRUNCATE, "curl: failed to set low speed time [%s]", str_error);
			throw Exc(str_error_msg, Exc::Level::critical);
		}
		if (curl_easy_setopt(conn, CURLOPT_XFERINFOFUNCTION, &func_progress) != CURLE_OK) {
			_snprintf_s(str_error_msg, _TRUNCATE, "curl: failed to set progress func [%s]", str_error);
			throw Exc(str_error_msg, Exc::Level::critical);
		}
		if (curl_easy_setopt(conn, CURLOPT_XFERINFODATA, NULL) != CURLE_OK) {
			_snprintf_s(str_error_msg, _TRUNCATE, "curl: failed to set progress data [%s]", str_error);
			throw Exc(str_error_msg, Exc::Level::critical);
		}

		code = curl_easy_perform(conn);
		curl_easy_cleanup(conn);
		conn = NULL;

		if (code != CURLE_OK)
		{
			_snprintf_s(str_error_msg, _TRUNCATE, "Download failed: %s", str_error);
			throw Exc(str_error_msg);
		}

		format_date(s_header->date);
		format_date(s_header->last_modified);
	}
	catch (Exc& exc) {
		if (conn)
			curl_easy_cleanup(conn);
		throw exc;
	}
	catch (boost::thread_interrupted& exc) {
		if (conn)
			curl_easy_cleanup(conn);
		throw exc;
	}
}

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

bool RSS::Database::isModifiedSince(const char* url, const std::string& date, RSS::Net::HttpHeader* s_header)
{
	CURL*				conn = NULL;
	CURLcode			code;
	char				str_error[CURL_ERROR_SIZE];
	char				str_error_msg[CURL_ERROR_SIZE + 50];
	struct curl_slist*	header_list = NULL;
	std::string			header_str = "If-Modified-Since: " + date;

	s_header->clear();

	try {
		// ---------------- Init connection
		if ((conn = curl_easy_init()) == NULL)
			throw Exc("curl_easy_init failed", Exc::Level::critical);

		// ---------------- Set curl options
		header_list = curl_slist_append(header_list, header_str.c_str());
		if (curl_easy_setopt(conn, CURLOPT_HTTPHEADER, header_list) != CURLE_OK) {
			_snprintf_s(str_error_msg, _TRUNCATE, "curl: failed to set http-header [%s]", str_error);
			throw Exc(str_error_msg, Exc::Level::critical);
		}
		if (curl_easy_setopt(conn, CURLOPT_ERRORBUFFER, str_error) != CURLE_OK)
			throw Exc("Failed to set error buffer", Exc::Level::critical);

		if (curl_easy_setopt(conn, CURLOPT_URL, url) != CURLE_OK) {
			_snprintf_s(str_error_msg, _TRUNCATE, "curl: failed to set URL [%s]", str_error);
			throw Exc(str_error_msg, Exc::Level::critical);
		}
		if (curl_easy_setopt(conn, CURLOPT_FOLLOWLOCATION, 1L) != CURLE_OK)	{
			_snprintf_s(str_error_msg, _TRUNCATE, "curl: failed to set CURLOPT_FOLLOWLOCATION [%s]", str_error);
			throw Exc(str_error_msg, Exc::Level::critical);
		}
		if (curl_easy_setopt(conn, CURLOPT_USERAGENT, "foo_uie_podcast/1.0") != CURLE_OK)	{
			_snprintf_s(str_error_msg, _TRUNCATE, "curl: failed to set user agent [%s]", str_error);
			throw Exc(str_error_msg, Exc::Level::critical);
		}
		if (curl_easy_setopt(conn, CURLOPT_NOBODY, 1) != CURLE_OK) {
			_snprintf_s(str_error_msg, _TRUNCATE, "curl: failed to set CURLOPT_NOBODY [%s]", str_error);
			throw Exc(str_error_msg, Exc::Level::critical);
		}
		if (curl_easy_setopt(conn, CURLOPT_HEADERFUNCTION, func_write_header) != CURLE_OK) {
			_snprintf_s(str_error_msg, _TRUNCATE, "curl: failed to set header function [%s]", str_error);
			throw Exc(str_error_msg, Exc::Level::critical);
		}
		if (curl_easy_setopt(conn, CURLOPT_HEADERDATA, s_header) != CURLE_OK)	{
			_snprintf_s(str_error_msg, _TRUNCATE, "curl: failed to set header data [%s]", str_error);
			throw Exc(str_error_msg, Exc::Level::critical);
		}
		if (curl_easy_setopt(conn, CURLOPT_LOW_SPEED_LIMIT, 300) != CURLE_OK) {
			_snprintf_s(str_error_msg, _TRUNCATE, "curl: failed to set low speed limit [%s]", str_error);
			throw Exc(str_error_msg, Exc::Level::critical);
		}
		if (curl_easy_setopt(conn, CURLOPT_LOW_SPEED_TIME, 5) != CURLE_OK) {
			_snprintf_s(str_error_msg, _TRUNCATE, "curl: failed to set low speed time [%s]", str_error);
			throw Exc(str_error_msg, Exc::Level::critical);
		}
		if (curl_easy_setopt(conn, CURLOPT_XFERINFOFUNCTION, &func_progress) != CURLE_OK) {
			_snprintf_s(str_error_msg, _TRUNCATE, "curl: failed to set progress func [%s]", str_error);
			throw Exc(str_error_msg, Exc::Level::critical);
		}
		if (curl_easy_setopt(conn, CURLOPT_XFERINFODATA, NULL) != CURLE_OK) {
			_snprintf_s(str_error_msg, _TRUNCATE, "curl: failed to set progress data [%s]", str_error);
			throw Exc(str_error_msg, Exc::Level::critical);
		}

		code = curl_easy_perform(conn);
		curl_easy_cleanup(conn);
		conn = NULL;
		curl_slist_free_all(header_list);
		header_list = NULL;

		if (code != CURLE_OK)
		{
			_snprintf_s(str_error_msg, _TRUNCATE, "Failed to check if modified: %s", str_error);
			throw Exc(str_error_msg);
		}
		format_date(s_header->date);
		format_date(s_header->last_modified);
		return (s_header->code != 304);
	}
	catch (Exc& exc) {
		if (conn)
			curl_easy_cleanup(conn);
		if (header_list)
			curl_slist_free_all(header_list);
		throw exc;
	}
	catch (boost::thread_interrupted& exc) {
		if (conn)
			curl_easy_cleanup(conn);
		if (header_list)
			curl_slist_free_all(header_list);
		throw exc;
	}
}

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

int RSS::Database::func_write_data(char *data, size_t size, size_t nmemb, std::ostringstream *data_buf)
{
	boost::this_thread::interruption_point();

	data_buf->write(data, size*nmemb);
	return size * nmemb;
}

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

int RSS::Database::func_write_header(char *data, size_t size, size_t nmemb, RSS::Net::HttpHeader* header)
{
	boost::this_thread::interruption_point();

	if (data[0] == 'H' && data[1] == 'T' && data[2] == 'T' && data[3] == 'P') {
		header->code = atoi(data + 9);
	}
	else if (data[0] == 'D' && data[1] == 'a' && data[2] == 't' && data[3] == 'e') {
		header->date.assign(data + 6, size*nmemb - 6);
	}
	else if (data[0] == 'L' && data[1] == 'a' && data[2] == 's' && data[3] == 't' && data[4] == '-' && data[5] == 'M' && data[6] == 'o' && data[7] == 'd') {
		header->last_modified.assign(data + 15, size*nmemb - 15);
	}
	else if ((size*nmemb) > 17 && strncmp(data, "Content-Encoding", 16) == 0) {
		if ((size*nmemb) >= 22 && strncmp(data + 18, "gzip", 4) == 0)
			header->enc = RSS::Net::HttpHeader::Encoding::gzip;
		else if ((size*nmemb) >= 23 && strncmp(data + 18, "bzip2", 5) == 0)
			header->enc = RSS::Net::HttpHeader::Encoding::bzip2;
		else if ((size*nmemb) >= 25 && strncmp(data + 18, "deflate", 7) == 0)
			header->enc = RSS::Net::HttpHeader::Encoding::deflate;
		else
			header->enc = RSS::Net::HttpHeader::Encoding::unknown;
	}
	return size * nmemb;
}

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

int RSS::Database::func_write_file(char* data, size_t size, size_t nmemb, RSS::Net::FileData* s)
{
	boost::this_thread::interruption_point();

	s->file->write(data, size * nmemb);
	s->ibytes += size * nmemb;

	return size * nmemb;
}

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

int RSS::Database::func_progress(void *p, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow)
{
	boost::this_thread::interruption_point();
	if (p != NULL) {
		double curtime = 0;
		struct RSS::Net::ProgressData* myp = (struct RSS::Net::ProgressData *)p;
		curl_easy_getinfo(myp->curl, CURLINFO_TOTAL_TIME, &curtime);
		if ((curtime - myp->lasttime) >= 1) {
			myp->lasttime = curtime;
			if (dltotal > 0) {
				double mbnow = static_cast<double>(dlnow + myp->offset) / 1048576;
				double mbtotal = static_cast<double>(dltotal + myp->offset) / 1048576;
				double speed;
				curl_easy_getinfo(myp->curl, CURLINFO_SPEED_DOWNLOAD, &speed);
				speed = speed / 1000;

				#ifdef RSS_UI_WSTRING
				swprintf_s(myp->strbuf, 50, TEXT("%.2f/%.2f MB (%d kb/s)"), mbnow, mbtotal, int(speed));
				#else
				sprintf_s(myp->strbuf, 50, TEXT("%.2f/%.2f MB (%d kb/s)"), kbnow, kbtotal, int(speed));
				#endif
				myp->func_update_ui->operator()();
			}
		}
	}
	return 0;
}

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

void RSS::Database::parseFeed(tinyxml2::XMLElement* xml_channel, std::string* str, bool onlydate)
{
	tinyxml2::XMLElement*	xml_el;
	std::string				tstr;
	const char*				tpstr;

	if (!onlydate) {
		// ----------------- title
		xml_el = xml_channel->FirstChildElement("title");
		if (xml_el && (tpstr = xml_el->GetText()) != NULL)
			str[(unsigned)SQL_Feed_s::title] = std::string(tpstr);
		if (str[(unsigned)SQL_Feed_s::title].size() == 0)
			throw Exc("invalid feed: title missing");
		// ----------------- link
		xml_el = xml_channel->FirstChildElement("link");
		str[(unsigned)SQL_Feed_s::homepage] = (xml_el && (tpstr = xml_el->GetText())) ? std::string(tpstr) : "";
		// ----------------- author
		xml_el = xml_channel->FirstChildElement("itunes:author");
		str[(unsigned)SQL_Feed_s::author] = (xml_el && (tpstr = xml_el->GetText())) ? std::string(tpstr) : "unknown";
		// ----------------- description
		xml_el = xml_channel->FirstChildElement("description");
		str[(unsigned)SQL_Feed_s::description] = (xml_el && (tpstr = xml_el->GetText())) ? std::string(tpstr) : "-";
		// ----------------- itunes:image
		xml_el = xml_channel->FirstChildElement("itunes:image");
		str[(unsigned)SQL_Feed_s::image_url] = (xml_el && (tpstr = xml_el->Attribute("href")) != NULL) ? std::string(tpstr) : "";
	}

	// ----------------- lastBuildDate
	xml_el = xml_channel->FirstChildElement("lastBuildDate");
	str[(unsigned)SQL_Feed_s::last_published] = (xml_el && (tpstr = xml_el->GetText()) != NULL) ? std::string(tpstr) : "";
	format_date(str[(unsigned)SQL_Feed_s::last_published]);

}

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

bool RSS::Database::parseItem(tinyxml2::XMLElement* xml_item, std::string* str, int* ints)
{
	tinyxml2::XMLElement*	xml_el;
	std::string				tstr;
	const char*				tpstr;

	// ----------------- title
	xml_el = xml_item->FirstChildElement("title");
	str[(unsigned)SQL_Item_s::title] = (xml_el && (tpstr = xml_el->GetText()) != NULL) ? std::string(tpstr) : "unknown title";
	// ----------------- link
	xml_el = xml_item->FirstChildElement("link");
	str[(unsigned)SQL_Item_s::link] = (xml_el && (tpstr = xml_el->GetText()) != NULL) ? std::string(tpstr) : "";
	// ----------------- description
	xml_el = xml_item->FirstChildElement("description");
	str[(unsigned)SQL_Item_s::description] = (xml_el && (tpstr = xml_el->GetText()) != NULL) ? std::string(tpstr) : "-";
	// ----------------- pubDate
	xml_el = xml_item->FirstChildElement("pubDate");
	str[(unsigned)SQL_Item_s::pubdate] = (xml_el && (tpstr = xml_el->GetText()) != NULL) ? std::string(tpstr) : "";
	format_date(str[(unsigned)SQL_Item_s::pubdate]);
	// ----------------- url & length
	xml_el = xml_item->FirstChildElement("enclosure");
	if (xml_el != NULL) {
		str[(unsigned)SQL_Item_s::url] = ((tpstr = xml_el->Attribute("url")) != NULL) ? std::string(tpstr) : "";
		ints[(unsigned)SQL_Item_i::filesize] = xml_el->IntAttribute("length");
	}
	// ----------------- 
	str[(unsigned)SQL_Item_s::download_time] = "";
	// ----------------- itunes:duration
	xml_el = xml_item->FirstChildElement("itunes:duration");
	std::string temp = (xml_el && (tpstr = xml_el->GetText()) != NULL) ? std::string(tpstr) : "";
	ints[(unsigned)SQL_Item_i::length] = 0;
	int oi = temp.size() - 1;
	int oe = oi + 1;
	int i = 0;
	while (oi >= 0) {
		while (oi >= 0 && temp[oi] != ':')
			oi--;
		if (oi + 1 < oe) {
			int t = std::atoi(temp.substr(oi + 1, oe - oi - 1).c_str());
			ints[(unsigned)SQL_Item_i::length] += t * static_cast<int>(std::pow(60, i));
			i++;
			oe = oi;
			oi--;
		}
	}

	// -----------------check title & url
	if (str[(unsigned)SQL_Item_s::title].size() == 0 || str[(size_t)SQL_Item_s::url].size() == 0)
		return false; // error message?
	else
		return true;
}

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

void RSS::Database::getFeedUIStrings(const std::string* s, RSS::UI::FeedStrings& s_ui, size_t item_count, bool onlydate)
{
	for (size_t i = 0; i < size_t(UI::FeedColumns::COUNT); i++)
	{
		if (RSS::UI::FeedColumnIndex[i] < 0) {
			s_ui[i].resize(-RSS::UI::FeedColumnIndex[i]);
			s_ui[i][0] = 0;
		}
		else {
			if (onlydate && i != size_t(UI::FeedColumns::last_published) && i != size_t(UI::FeedColumns::last_checked))
				continue;
			try {
				#ifdef RSS_UI_WSTRING
				const char* ss = s[RSS::UI::FeedColumnIndex[i]].c_str();
				size_t slen = s[RSS::UI::FeedColumnIndex[i]].size();
				s_ui[i].clear();
				s_ui[i].reserve(slen);
				utf8::utf8to16(ss, ss + slen, back_inserter(s_ui[i]));
				#else
				s_ui[i].assign(s[RSS::UI::FeedColumnIndex[i]]);
				#endif
			}
			catch (utf8::exception& exc) {
				s_ui[i].resize(2);
				s_ui[i][0] = '?'; s_ui[i][1] = 0;
				func.log(exc.what());
			}
		}
	}
	#ifdef RSS_UI_WSTRING
	swprintf_s(&s_ui[unsigned(UI::FeedColumns::itemcount)][0], s_ui[unsigned(UI::FeedColumns::itemcount)].size(), TEXT("%d"), item_count);
	#else
	sprintf_s(&s_ui[unsigned(UI::FeedColumns::itemcount)][0], s_ui[unsigned(UI::FeedColumns::itemcount)].size(), "%d", item_count);
	#endif
}

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

void RSS::Database::getDownloadUIStrings(const std::string* s, RSS::UI::DownloadStrings& s_ui)
{
	for (size_t i = 0; i < size_t(UI::DownloadColumns::COUNT); i++) 
	{
		if (UI::DownloadColumnIndex[i] < 0) {
			s_ui[i].resize(-UI::DownloadColumnIndex[i]);
			s_ui[i][0] = 0;
		}
		else {
			try {
				#ifdef RSS_UI_WSTRING
				const char* ss = s[UI::DownloadColumnIndex[i]].c_str();
				size_t slen = s[UI::DownloadColumnIndex[i]].size();
				s_ui[i].clear();
				s_ui[i].reserve(slen);
				utf8::utf8to16(ss, ss + slen, back_inserter(s_ui[i]));
				#else
				s_ui[i].assign(s[RSS::UI::DownloadColumnIndex[i]]);
				#endif
			}
			catch (utf8::exception& exc) {
				s_ui[i].resize(2);
				s_ui[i][0] = '?'; s_ui[i][1] = 0;
				func.log(exc.what());
			}
		}
	}
}

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

void RSS::Database::getUIStatusString(RSS::Net::Status status, string& str, bool offset)
{
	static const TCHAR* tstr[] = { TEXT("[error]"), TEXT("[paused]") };
	static const size_t	tlen[]{ 7, 8 };
	int					index = 0;
	size_t				ioffset = 0;

	switch (status) {
		case Net::Status::error: index = 0; break;
		case Net::Status::paused: index = 1; break;
	}
	
	if (offset) {
		for (; (int)ioffset < (UI::DownloadColumnIndex[unsigned(UI::DownloadColumns::status)] * -1); ioffset++) {
			if (str[ioffset] == 0)
				break;
		}
	}
	if (int(ioffset + tlen[index] + 1) < (UI::DownloadColumnIndex[unsigned(UI::DownloadColumns::status)] * -1)) {
		for (size_t i = 0; i < tlen[index]; i++)
			str[ioffset + i] = tstr[index][i];
		str[ioffset + tlen[index]] = 0;
	}
}

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

bool RSS::Database::format_date(std::string& str)
{
	time_t timeinfo = curl_getdate(str.c_str(), NULL);
	if (timeinfo != -1)
	{
		struct tm ptm;
		char buffer[64];
		if (gmtime_s(&ptm, &timeinfo) == 0 && strftime(buffer, 64, "%Y-%m-%d %H:%M:%S", &ptm) != 0)
		{
			str.assign(buffer);
			return true;
		}
	}
	return false;
}

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

RSS::FeedCompare::FeedCompare(RSS::UI::FeedColumns column, bool desc)
{
	if (column == RSS::UI::FeedColumns::COUNT)
		this->column = unsigned(RSS::UI::FeedColumns::title);
	else
		this->column = unsigned(column);
	this->desc = desc;
};

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

bool RSS::FeedCompare::operator()(const RSS::UI::Feed& first, const RSS::UI::Feed& second)
{
	return (desc) ? great(first.strings[column], second.strings[column]) : less(first.strings[column], second.strings[column]);
};
