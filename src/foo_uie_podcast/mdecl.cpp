
#include "stdafx.h"
#include "mdecl.h"

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

DECLARE_COMPONENT_VERSION( COMPONENT_NAME, COMPONENT_VERSION_STR,
"compiled: " __DATE__ "\n"
"with Panel API version: " UI_EXTENSION_VERSION
);

VALIDATE_COMPONENT_FILENAME(COMPONENT_DLL);

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

RSS::Database			database;
UIEBase					main;
UIEPanelManager			panelmanager(2);

// -------------------------------------------------------------------------------------------------------------------------------------------------------------------------

// {851E6A5E-E04A-4AEF-9938-F8DE937BD62F}
static const GUID filename_mask_gui = { 0x851e6a5e, 0xe04a, 0x4aef, { 0x99, 0x38, 0xf8, 0xde, 0x93, 0x7b, 0xd6, 0x2f } };
// {5DDD528A-AD48-482C-A1EC-DE46D2A897E9}
static const GUID output_dir_gui = { 0x5ddd528a, 0xad48, 0x482c, { 0xa1, 0xec, 0xde, 0x46, 0xd2, 0xa8, 0x97, 0xe9 } };
// {8440276F-F9F8-4D1F-A27E-D28271B34638}
static const GUID autoupdate_minutes_gui = { 0x8440276f, 0xf9f8, 0x4d1f, { 0xa2, 0x7e, 0xd2, 0x82, 0x71, 0xb3, 0x46, 0x38 } };
// {68396BBA-740F-41F3-A57D-3E2107BFECD3}
static const GUID download_days_gui = { 0x68396bba, 0x740f, 0x41f3, { 0xa5, 0x7d, 0x3e, 0x21, 0x7, 0xbf, 0xec, 0xd3 } };
// {BC0EA238-44C8-42B0-B702-8B25C0E1EEA1}
static const GUID playlist1_gui = { 0xbc0ea238, 0x44c8, 0x42b0, { 0xb7, 0x2, 0x8b, 0x25, 0xc0, 0xe1, 0xee, 0xa1 } };
// {2FA5FB71-4BC0-4CA7-9D7E-06F4D7C47869}
static const GUID playlist2_gui = { 0x2fa5fb71, 0x4bc0, 0x4ca7, { 0x9d, 0x7e, 0x6, 0xf4, 0xd7, 0xc4, 0x78, 0x69 } };
// {05A67835-FA20-4AC3-8432-4B465AB5E50F}
static const GUID download_threads_gui = { 0x5a67835, 0xfa20, 0x4ac3, { 0x84, 0x32, 0x4b, 0x46, 0x5a, 0xb5, 0xe5, 0xf } };
// {52F2A2DF-5388-4F21-8D6F-DD21BC7604BF}
static const GUID playlist1_use_gui = { 0x52f2a2df, 0x5388, 0x4f21, { 0x8d, 0x6f, 0xdd, 0x21, 0xbc, 0x76, 0x4, 0xbf } };
// {9D5499AA-FF2A-4D1E-9A20-C33ADED1B4F7}
static const GUID playlist2_use_gui = { 0x9d5499aa, 0xff2a, 0x4d1e, { 0x9a, 0x20, 0xc3, 0x3a, 0xde, 0xd1, 0xb4, 0xf7 } };
// {6399649B-5E74-473C-A4B6-F8E83A2D0D6D}
static const GUID feedlist_sort_order_gui = { 0x6399649b, 0x5e74, 0x473c, { 0xa4, 0xb6, 0xf8, 0xe8, 0x3a, 0x2d, 0xd, 0x6d } };
// {F9E0FF58-30FF-4D7C-A51D-9898BA4B7250}
static const GUID feedlist_sort_column_gui = { 0xf9e0ff58, 0x30ff, 0x4d7c, { 0xa5, 0x1d, 0x98, 0x98, 0xba, 0x4b, 0x72, 0x50 } };


cfg_string		options::filename_mask(filename_mask_gui, "%filename%");
cfg_string		options::output_dir(output_dir_gui, "C:\\Users\\Public\\Music\\Podcasts");
cfg_string		options::playlist1(playlist1_gui, "#Podcast::new");
cfg_string		options::playlist2(playlist2_gui, "#Podcast::feed");
cfg_bool		options::playlist1_use(playlist1_use_gui, true);
cfg_bool		options::playlist2_use(playlist2_use_gui, true);
cfg_bool		options::feedlist_sort_order(feedlist_sort_order_gui, true);

cfg_int			options::autoupdate_minutes(autoupdate_minutes_gui, 30);
cfg_int			options::download_days(download_days_gui, 7);
cfg_int			options::download_threads(download_threads_gui, 3);
cfg_int			options::feedlist_sort_column(feedlist_sort_column_gui, 0);
