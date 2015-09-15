
#include "stdafx.h"
#include "dlg_editfeed.h"
#include "resource.h"

DlgEditfeed::DlgEditfeed(HINSTANCE hInst, HWND parent, RSS::UI::Feed& feed) :mfeed(feed)
{
	this->hInst = hInst;
	hParent = parent;
};

void DlgEditfeed::destroy()
{
	::DestroyWindow(hSelf);
};

bool DlgEditfeed::doDialog()
{
	if (DialogBoxParam(hInst, MAKEINTRESOURCE(IDD_EDITFEED), hParent, (DLGPROC)dlgProc, (LPARAM)this) == IDOK)
		return true;
	return false;
}

BOOL CALLBACK DlgEditfeed::dlgProc(HWND hWnd, UINT Message, WPARAM wParam, LPARAM lParam)
{
	switch (Message)
	{
	case WM_INITDIALOG:
	{
		DlgEditfeed *pDlgEditfeed = (DlgEditfeed *)(lParam);
		pDlgEditfeed->hSelf = hWnd;
		::SetWindowLongPtr(hWnd, GWL_USERDATA, (long)lParam);
		pDlgEditfeed->run_dlgProc(Message, wParam, lParam);
		return TRUE;
	}

	default:
	{
		DlgEditfeed *pDlgEditfeed = reinterpret_cast<DlgEditfeed *>(::GetWindowLong(hWnd, GWL_USERDATA));
		if (!pDlgEditfeed)
			return FALSE;
		return pDlgEditfeed->run_dlgProc(Message, wParam, lParam);
	}

	}
	return FALSE;
}

BOOL CALLBACK DlgEditfeed::run_dlgProc(UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_INITDIALOG:
	{
		::SetDlgItemText(hSelf, IDC_EDITFEED_TITLE, mfeed.strings[unsigned(RSS::UI::FeedColumns::title)].c_str());
		::SetDlgItemText(hSelf, IDC_EDITFEED_AUTHOR, mfeed.strings[unsigned(RSS::UI::FeedColumns::author)].c_str());
		::SetDlgItemText(hSelf, IDC_EDITFEED_HOMEPAGE, mfeed.strings[unsigned(RSS::UI::FeedColumns::homepage)].c_str());
		::SetDlgItemText(hSelf, IDC_EDITFEED_URL, mfeed.strings[unsigned(RSS::UI::FeedColumns::url)].c_str());
		::SendDlgItemMessage(hSelf, IDC_EDITFEED_TITLE, EM_LIMITTEXT, 300, 0);
		::SendDlgItemMessage(hSelf, IDC_EDITFEED_AUTHOR, EM_LIMITTEXT, 300, 0);
		::SendDlgItemMessage(hSelf, IDC_EDITFEED_HOMEPAGE, EM_LIMITTEXT, 300, 0);
		CheckDlgButton(hSelf, IDC_EDITFEED_RETAG, (mfeed.retag) ? BST_CHECKED : BST_UNCHECKED);
		return TRUE;

	}

	case WM_COMMAND:
	{
		switch (LOWORD(wParam))
		{
		case IDOK: 
		{
			// ---------------- title
			int str_len;
			str_len = GetWindowTextLength(GetDlgItem(hSelf, IDC_EDITFEED_TITLE));
			if (str_len <= 0)
				return 0;
			mfeed.strings[unsigned(RSS::UI::FeedColumns::title)].resize(str_len+1);
			::GetDlgItemText(hSelf, IDC_EDITFEED_TITLE, &mfeed.strings[unsigned(RSS::UI::FeedColumns::title)][0], str_len+1);
			mfeed.strings[unsigned(RSS::UI::FeedColumns::title)].pop_back();
			// ---------------- author
			str_len = GetWindowTextLength(GetDlgItem(hSelf, IDC_EDITFEED_AUTHOR));
			if (str_len <= 0)
				return 0;
			mfeed.strings[unsigned(RSS::UI::FeedColumns::author)].resize(str_len+1);
			::GetDlgItemText(hSelf, IDC_EDITFEED_AUTHOR, &mfeed.strings[unsigned(RSS::UI::FeedColumns::author)][0], str_len+1);
			mfeed.strings[unsigned(RSS::UI::FeedColumns::author)].pop_back();
			// ---------------- homepage
			str_len = GetWindowTextLength(GetDlgItem(hSelf, IDC_EDITFEED_HOMEPAGE));
			if (str_len <= 0)
				return 0;
			mfeed.strings[unsigned(RSS::UI::FeedColumns::homepage)].resize(str_len + 1);
			::GetDlgItemText(hSelf, IDC_EDITFEED_HOMEPAGE, &mfeed.strings[unsigned(RSS::UI::FeedColumns::homepage)][0], str_len+1);
			mfeed.strings[unsigned(RSS::UI::FeedColumns::homepage)].pop_back();

			mfeed.retag = ::SendDlgItemMessage(hSelf, IDC_EDITFEED_RETAG, BM_GETCHECK, 0, 0) ? true : false;

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