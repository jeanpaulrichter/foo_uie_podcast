#ifndef DEF_H_DECL
#define DEF_H_DECL

#include "stdafx.h"
#include "foo_uie_podcast.h"
#include "panels.h"

#define COMPONENT_NAME "podcastsCUI"
#define COMPONENT_DLL "foo_uie_podcast.dll"
#define COMPONENT_VERSION_STR "1.0"
#define COMPONENT_VERSION 1.0
#define COMPONENT_HELP_URL "http://www.cerberus-design.de/foo_uie_podcast/readme.txtjo"

namespace options {

	extern cfg_string		filename_mask;
	extern cfg_string		output_dir;
	extern cfg_string		playlist1;
	extern cfg_string		playlist2;
	extern cfg_bool			playlist1_use;
	extern cfg_bool			playlist2_use;
	extern cfg_int			autoupdate_minutes;
	extern cfg_int			download_days;
	extern cfg_int			download_threads;
	extern cfg_bool			feedlist_sort_order;
	extern cfg_int			feedlist_sort_column;
};

enum class PanelID : unsigned { feeds = 0, downloads, COUNT };

extern RSS::Database			database;
extern UIEBase					main;
extern UIEPanelManager			panelmanager;

#endif