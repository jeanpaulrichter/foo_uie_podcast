#ifndef DEF_H_DATABASE
#define DEF_H_DATABASE

#include <string>
#include <vector>
#include <sstream>
#include <iostream>
#include <fstream>
#include <queue>

#include <curl/curl.h>

#include <boost/thread.hpp>
#include <boost/thread/lock_guard.hpp> 
#include <boost/thread/scoped_thread.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/signals2.hpp>
#include <boost/iostreams/filtering_streambuf.hpp>
#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/filter/bzip2.hpp>

#include "uft8cpp/utf8.h"
#include "sqlite/sqlite3.h"
#include "tinyxml2/tinyxml2.h"


#ifdef UNICODE
#define RSS_UI_WSTRING
typedef std::wstring string;
typedef wchar_t tchar;
#else
typedef std::string string;
typedef char tchar;
#endif


namespace RSS
{
	// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

	class Exc : public std::exception
	{
	public:
		enum class Level { critical, unexpected };
		enum class Code { unspecified, sql_no_result };

		Exc() : _level(Level::unexpected) {};
		Exc(const char* w, Level l = Level::unexpected, Code c = Code::unspecified) : _msg(w), _level(l), _code(c) {};
		~Exc() throw() {};

		const char* what() const throw() { return _msg.c_str(); };
		Level getLevel() { return _level; };
		Code getCode() { return _code; };

	private:
		std::string		_msg;
		Level			_level;
		Code			_code;
	};

	// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------
	// SQL: manages excess to sqlite-database
	// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

	class SQL
	{
	public:

		/* SQL-Tables structure: TEXT-columns in order of enum (SQL_Feed_s,SQL_Item_s,SQL_Download_s) precede INTEGER-columns in order of enum (SQL_Item_i, SQL_Download_i) */
		enum class SQL_Feed_s : unsigned { url, title, homepage, author, description, image_url, last_published, last_checked, COUNT }; // last_checked has to be after last_published!
		enum class SQL_Feed_i : unsigned { retag, COUNT };
		enum class SQL_Item_s : unsigned { title, description, link, pubdate, url, download_time, COUNT };
		enum class SQL_Item_i : unsigned { length, filesize, feed_id, COUNT };
		enum class SQL_Download_s : unsigned { title, url, filename, COUNT};
		enum class SQL_Download_i : unsigned { filesize, item_id, status, COUNT };

		/* SQLite-Statements for use with sql_query(): return value can only be one integer (before COUNT_ROW) or rowid (before COUNT_INSERT) */
		enum class SQL_Query : unsigned  { count_tables, count_feeds_url, get_item_by_date, COUNT_ROW, insert_feed=3, insert_item, insert_download, COUNT_INSERT, delete_feed_items=6, 
											delete_feed, delete_download, delete_downloads, delete_items, update_feed, update_feed_info, update_feed_dates, update_feed_check, update_item_time, update_download,  
											update_downloads, vacuum, COUNT };

		/* SQLite-Statements for use with sql_row(), sql_column(), sql_reset() */
		enum class SQL_Query_s : unsigned  { select_feeds, select_new_items, select_downloads, select_items, select_feed, select_download_status, select_download_status_not, select_downloads_status_not, select_item_info, COUNT };

		/* SQLite-Statements to create the database */
		enum class SQL_Query_c : unsigned  { create_downloads, create_feeds, create_items, COUNT_TABLES, create_items_index1=3, create_feeds_index1, COUNT };

		class Lock
		{
		public:
			explicit Lock(SQL& obj, SQL_Query_s q) : obj_(obj), qs(q) { obj.sql_lock(qs); };
			~Lock() { obj_.sql_unlock(qs); }
			bool owns_lock(SQL const* l) const noexcept { return (l == &obj_); }
			unsigned id() { return (unsigned)qs; };

			Lock() = delete;
			Lock(Lock const&) = delete;
			Lock& operator=(Lock const&) = delete;
		private:
			SQL& obj_;
			SQL_Query_s qs;
		};

		SQL();
		~SQL();

	protected:
		bool			sql_isconnected();
		void			sql_connect(const char* path);	
		void			sql_close();

		bool			sql_query(SQL_Query q, const std::string* s=NULL, size_t s_num=0, const int* d=NULL, size_t d_num=0, int* ret=NULL);

		void			sql_bind(Lock& lock, const char* s);
		void			sql_bind(Lock& lock, int i);
		bool			sql_step(Lock& lock);
		int				sql_columnInt(Lock& lock);
		const char*		sql_columnString(Lock& lock);
		void			sql_reset(Lock& lock);

	private:
		void			sql_lock(SQL_Query_s q);
		void			sql_unlock(SQL_Query_s q);

		sqlite3*		sql_db;
		sqlite3_stmt*	sql_stmts_q[unsigned(SQL_Query::COUNT)];
		sqlite3_stmt*	sql_stmts_q_s[unsigned(SQL_Query_s::COUNT)];

		int				sql_q_s_column[unsigned(SQL_Query_s::COUNT)];
		int				sql_q_s_bind[unsigned(SQL_Query_s::COUNT)];
		boost::mutex	sql_query_s_mutex[unsigned(SQL_Query_s::COUNT)];
		boost::mutex	sql_query_mutex[unsigned(SQL_Query::COUNT)];
	};

	// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------
	// all kinds of strucs, typedefs and enums
	// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

	// --------------------------- callback -----------------------------------------
	namespace Signals
	{
		typedef boost::signals2::signal<void(const char*)>	String;
		typedef boost::signals2::signal<void(const char*, int)>  StringInt;
		typedef boost::signals2::signal<void()>				Void;
		typedef boost::signals2::signal<void(size_t)>		Unsigned;
		typedef boost::signals2::signal<void(int)>			Int;

		typedef Void::slot_type								VoidSlotType;
		typedef String::slot_type							StringSlotType;
		typedef Unsigned::slot_type							UnsignedSlotType;
		typedef StringInt::slot_type						StringIntSlotType;
		typedef Int::slot_type								IntSlotType;
		typedef boost::signals2::connection					Conn;

		struct List
		{
			String		log;
			Unsigned	feed_list_update;
			Unsigned	download_list_update;
			Void		download_list_redraw;
			StringInt	download_done;
			Int			highlight_feed;
		};
	};

	// --------------------------- Lists of feeds/downloads for UI -------------------------------------
	namespace UI
	{
		enum class FeedColumns : unsigned { title, author, url, homepage, last_published, last_checked, itemcount, COUNT };
		enum class DownloadColumns : unsigned { title, filename, url, status, COUNT }; // status has to be the last [ see RSSDatabase::getDownloadUIStrings() ]

		/* negativ value means prepare buffer of value*-1 */
		static const int FeedColumnIndex[int(FeedColumns::COUNT)] = { int(SQL::SQL_Feed_s::title), int(SQL::SQL_Feed_s::author), int(SQL::SQL_Feed_s::url),
			int(SQL::SQL_Feed_s::homepage), int(SQL::SQL_Feed_s::last_published), int(SQL::SQL_Feed_s::last_checked), -10 };
		static const int DownloadColumnIndex[int(DownloadColumns::COUNT)] = { int(SQL::SQL_Download_s::title), int(SQL::SQL_Download_s::filename),
			int(SQL::SQL_Download_s::url), -60 };

		typedef std::array<string, unsigned(FeedColumns::COUNT)>		FeedStrings;
		typedef std::array<string, unsigned(DownloadColumns::COUNT)>	DownloadStrings;

		struct Download {
			//int				thread_id;
			DownloadStrings		strings;
		};

		struct Feed {
			int					id;
			bool				retag;
			FeedStrings			strings;
		};
		
		typedef std::list<Feed>					FeedList;
		typedef std::vector<FeedList::iterator>	FeedVector;
		typedef std::map<int, Download>			DownloadMap;
	};

	// --------------------------- Options [ RSSDatabase::SetOptions() ] ------------------------------
	struct Options
	{
		Options() : flags(0) {};
		enum { DL_THREADS=1, DL_RANGE=2, AUTOUPDATE=4, OUTPUT_DIR=8, FILENAME_MASK=16 };

		size_t						download_threads_count;
		int							download_range;
		int							autoupdate_minutes;
		std::string					output_dir;
		std::string					filename_mask;
		int							flags;
	};

	// --------------------------- Net -----------------------------------------
	namespace Net
	{
		struct HttpHeader
		{
			enum class Encoding : unsigned { gzip, bzip2, deflate, unknown };
			HttpHeader() : code(0), enc(Encoding::unknown) {};
			void clear() { code = 0; last_modified.clear(); date.clear(); enc = Encoding::unknown;  };

			int			code;
			std::string last_modified;
			std::string date;
			Encoding	enc;
		};

		enum class Status : unsigned { downloading, paused, error };

		struct FileData
		{
			FileData() : ibytes(0), file(NULL) {};

			std::fstream*	file;
			long long		ibytes;
		};

		struct ProgressData
		{
			double					lasttime;
			curl_off_t				offset;
			tchar*					strbuf;
			CURL*					curl;
			RSS::Signals::Void*		func_update_ui;
		};
	};

	// --------------------------- just to group some variables together --------------------
	namespace S {
		struct List
		{
			RSS::UI::FeedList				feeds;
			RSS::UI::FeedVector				feedsVector;
			RSS::UI::DownloadMap			downloads;
			boost::mutex					feeds_mutex;
			boost::mutex					downloads_mutex;

			RSS::UI::FeedColumns			feeds_order_column;
			bool							feeds_order_up;
		};

		struct Options
		{
			Options() : dl_threads_count(3), dl_range_days(7), autoupdate_minutes(30) {};
			size_t							dl_threads_count;
			int								dl_range_days;
			boost::posix_time::minutes		autoupdate_minutes;
			std::string						output_dir;
			std::string						filename_mask;
			boost::mutex					mutex;
		};
	};

	// --------------------------- used to sort feed-list --------------------------
	class FeedCompare
	{
	public:
		FeedCompare(UI::FeedColumns column, bool desc = true);
		bool operator()(const UI::Feed& first, const RSS::UI::Feed& second);
	private:
		size_t					column;
		bool					desc;
		std::greater<string>	great;
		std::less<string>		less;
	};

	// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------
	// Database: manages database of rss-feeds and provides strings of current feeds/downloads for UI
	// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

	class Database : SQL
	{
	public:
		Database();
		~Database();

		bool IsConnected();
		void Connect(const char* path);
		void Close();
		void AddFeed(const char* url);
		void DeleteFeed(int feed_id);
		void Update(bool rebuild = false);
		bool UpdateInProgress();
		void PauseDownload(int item_id);
		void ResumeDownload(int item_id);
		void CancelDownload(int item_id);
		int SetOptions(const RSS::Options& o);
		void ImportOPML(const std::string path);
		void ExportOPML(const std::string path);

		// ---------------------- ui
		const tchar*	GetFeedString(size_t i, size_t col);
		const tchar*	GetDownloadString(size_t i, size_t col);
		size_t			GetFeedCount();
		size_t			GetDownloadCount();
		int				GetFeedID(size_t index);
		bool			GetDownloadInfo(size_t list_index, int& item_id, bool& is_active);
		void			SetFeedListSort(RSS::UI::FeedColumns column, bool up);
		void			SortFeedList(RSS::UI::FeedColumns column, bool up);
		bool			GetFeedInfo(size_t index, RSS::UI::Feed& feed);
		bool			SetFeedInfo(const RSS::UI::Feed& feed);

		// ---------------------- register signals
		RSS::Signals::Conn registerLog(const RSS::Signals::StringSlotType& t);
		RSS::Signals::Conn registerUpdateFeeds(const RSS::Signals::UnsignedSlotType& t);
		RSS::Signals::Conn registerUpdateDownloads(const RSS::Signals::UnsignedSlotType& t);
		RSS::Signals::Conn registerRedrawDownloads(const RSS::Signals::VoidSlotType& t);
		RSS::Signals::Conn registerDownloadDone(const RSS::Signals::StringIntSlotType& t);
		RSS::Signals::Conn registerHighlightFeed(const RSS::Signals::IntSlotType& t);

	private:
		// ---------------------- main
		void doAddFeeds();
		void autoUpdate();
		void updateFeeds(bool rebuild);
		size_t updateItems(int feed_id, tinyxml2::XMLElement* xml_channel);
		void download(size_t thread_id);
		void getFeedData(const char* url, std::ostringstream* buffer, RSS::Net::HttpHeader* s_header);
		bool isModifiedSince(const char* url, const std::string& date, RSS::Net::HttpHeader* s_header);

		// ---------------------- curl
		static int func_write_data(char *data, size_t size, size_t nmemb, std::ostringstream *data_buf);
		static int func_write_header(char *data, size_t size, size_t nmemb, RSS::Net::HttpHeader* header);
		static int func_write_file(char* data, size_t size, size_t nmemb, RSS::Net::FileData* s);
		static int func_progress(void* p, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow);

		// ---------------------- help
		void parseFeed(tinyxml2::XMLElement* xml_channel, std::string* str, bool onlydate = false);
		bool parseItem(tinyxml2::XMLElement* xml_item, std::string* str, int* ints);
		void parseOPML(tinyxml2::XMLElement* xml_body);
		void getFeedUIStrings(const std::string* s, UI::FeedStrings& s_ui, size_t item_count, bool onlydate = false);
		void getDownloadUIStrings(const std::string* s, UI::DownloadStrings& s_ui);
		void getUIStatusString(Net::Status status, string& str, bool offset = true);
		bool format_date(std::string& str);

		enum {	THREAD_SHUTDOWN = -1, 
				THREAD_ABORT_DOWNLOAD = -2, 
				THREAD_PAUSE_DOWNLOAD = -3, 
				THREAD_AUTOUPDATE_INPROGRESS = -4, 
				THREAD_AUTOUPDATE_RESTART = -5, 
				THREAD_AUTOUPDATE_WAIT = -6 
			 };

		boost::thread						thread_autoupdate;
		boost::thread						thread_addfeed;
		boost::thread						thread_update;
		std::vector<boost::thread>			download_threads;
		std::vector<int>					download_threads_item;
		boost::mutex						download_threads_item_mutex;
		int									autoupdate_status;
		boost::mutex						autoupdate_status_mutex;
		boost::mutex						update_mutex;

		std::deque<int>						downloads;
		boost::condition_variable			download_ready_cond;
		boost::mutex						download_mutex;
		bool								download_ready;

		RSS::S::List						list;
		RSS::S::Options						options;
		RSS::Signals::List					func;

		std::vector<std::string>			new_feed_urls;
	};

	// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------
};

#endif