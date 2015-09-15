#ifndef DEF_H_STDAFX
#define DEF_H_STDAFX

#pragma comment(lib, "wldap32.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "Shlwapi.lib")

#pragma comment(linker,"\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#include <SDKDDKVer.h>

#include <vector>
#include <Windows.h>
#include <string>
#include <windowsx.h>
#include <string>
#include <boost/thread.hpp>
#include <list>
#include <deque>
#include <shobjidl.h>

#include "../foobar2000/SDK/foobar2000.h"
#include "../foobar2000/columns_ui-sdk/ui_extension.h"

#endif