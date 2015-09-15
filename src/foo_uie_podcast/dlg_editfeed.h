#ifndef DEF_H_DLG_EDITFEED
#define DEF_H_DLG_EDITFEED

#include "stdafx.h"
#include "mdecl.h"

class DlgEditfeed
{
public:

	DlgEditfeed(HINSTANCE hInst, HWND parent, RSS::UI::Feed& feed);
	void destroy();
	bool doDialog();

private:
	static BOOL CALLBACK dlgProc(HWND hWnd, UINT Message, WPARAM wParam, LPARAM lParam);
	BOOL CALLBACK run_dlgProc(UINT message, WPARAM wParam, LPARAM lParam);

	RSS::UI::Feed&		mfeed;
	HWND				hParent;
	HWND				hSelf;
	HINSTANCE			hInst;
};


#endif