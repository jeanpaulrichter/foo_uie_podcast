#ifndef DEF_H_DLG_INPUTBOX
#define DEF_H_DLG_INPUTBOX

#include "stdafx.h"

class DlgInputbox
{
public:

	DlgInputbox(HINSTANCE hInst, HWND parent);
	void destroy();
	bool doDialog(const TCHAR* caption=NULL, const TCHAR* text=NULL);
	const TCHAR* getString();

private:
	static BOOL CALLBACK dlgProc(HWND hWnd, UINT Message, WPARAM wParam, LPARAM lParam);
	BOOL CALLBACK run_dlgProc(UINT message, WPARAM wParam, LPARAM lParam);

	std::vector<TCHAR>	_caption;
	std::vector<TCHAR>	_text;
	std::vector<TCHAR>	buffer;
	HWND				hParent;
	HWND				hSelf;
	HINSTANCE			hInst;
};



#endif