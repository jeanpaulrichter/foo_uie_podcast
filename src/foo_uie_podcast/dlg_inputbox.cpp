
#include "stdafx.h"
#include "dlg_inputbox.h"
#include "resource.h"

DlgInputbox::DlgInputbox(HINSTANCE hInst, HWND parent)
{
	this->hInst = hInst;
	hParent = parent;
};

void DlgInputbox::destroy()
{
	::DestroyWindow(hSelf);
};

bool DlgInputbox::doDialog(const TCHAR* caption, const TCHAR* text)
{
	if (caption) {
		#ifdef UNICODE
		size_t cap_len = wcslen(caption);
		_caption.resize(cap_len + 1);
		wcscpy_s(&_caption[0], cap_len + 1, caption);
		#else
		size_t cap_len = strlen(caption);
		_caption.resize(cap_len + 1);
		strcpy(&_caption[0], cap_len + 1, caption);
		#endif
	}
	else {
		_caption.resize(1);
		_caption[0] = 0;
	}
	if (text) {
		#ifdef UNICODE
		size_t text_len = wcslen(text);
		_text.resize(text_len + 1);
		wcscpy_s(&_text[0], text_len + 1, text);
		#else
		size_t text_len = strlen(text);
		_text.resize(text_len + 1);
		strcpy(&_text[0], text_len + 1, text);
		#endif
	}
	else {
		_text.resize(1);
		_text[0] = 0;
	}

	if (DialogBoxParam(hInst, MAKEINTRESOURCE(IDD_INPUTBOX), hParent, (DLGPROC)dlgProc, (LPARAM)this) == IDOK)
		return true;
	return false;
}

BOOL CALLBACK DlgInputbox::dlgProc(HWND hWnd, UINT Message, WPARAM wParam, LPARAM lParam)
{
	switch (Message)
	{
	case WM_INITDIALOG:
	{
		DlgInputbox *pDlgInputbox = (DlgInputbox *)(lParam);
		pDlgInputbox->hSelf = hWnd;
		::SetWindowLongPtr(hWnd, GWL_USERDATA, (long)lParam);
		pDlgInputbox->run_dlgProc(Message, wParam, lParam);
		return TRUE;
	}

	default:
	{
		DlgInputbox *pDlgInputbox = reinterpret_cast<DlgInputbox *>(::GetWindowLong(hWnd, GWL_USERDATA));
		if (!pDlgInputbox)
			return FALSE;
		return pDlgInputbox->run_dlgProc(Message, wParam, lParam);
	}

	}
	return FALSE;
}

BOOL CALLBACK DlgInputbox::run_dlgProc(UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_INITDIALOG:
	{
		::SetWindowText(hSelf, &_caption[0]);
		::SetDlgItemText(hSelf, IDC_INPUTBOX_EDIT, &_text[0]);
		::SendDlgItemMessage(hSelf, IDC_INPUTBOX_EDIT, EM_LIMITTEXT, 255, 0);

		return TRUE;

	}

	case WM_COMMAND:
	{
		switch (LOWORD(wParam))
		{
		case IDOK: {
			buffer.resize(255);
			size_t len = ::GetDlgItemText(hSelf, IDC_INPUTBOX_EDIT, &buffer[0], 255);
			buffer.resize(len + 1);

			if (!buffer.size() || buffer[0]==0)
				return 0;

			EndDialog(hSelf, IDOK);
			return TRUE; 
		}

		case IDCANCEL:
			EndDialog(hSelf, IDCANCEL);
			return TRUE;

		default:
			break;
		}
		break;
	}
	}
	return FALSE;
}

const TCHAR* DlgInputbox::getString()
{
	return &buffer[0];
}
