/*

Jabber Protocol Plugin for Miranda NG

Copyright (c) 2002-04  Santithorn Bunchua
Copyright (c) 2005-12  George Hazan
Copyright (�) 2012-17 Miranda NG project

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include "stdafx.h"
#include "jabber_iq.h"
#include "jabber_caps.h"

static INT_PTR CALLBACK JabberChangePasswordDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam);

INT_PTR __cdecl CJabberProto::OnMenuHandleChangePassword(WPARAM, LPARAM)
{
	if (IsWindow(m_hwndJabberChangePassword))
		SetForegroundWindow(m_hwndJabberChangePassword);
	else
		m_hwndJabberChangePassword = CreateDialogParam(hInst, MAKEINTRESOURCE(IDD_CHANGEPASSWORD), NULL, JabberChangePasswordDlgProc, (LPARAM)this);

	return 0;
}

static INT_PTR CALLBACK JabberChangePasswordDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	CJabberProto *ppro = (CJabberProto*)GetWindowLongPtr(hwndDlg, GWLP_USERDATA);
	switch (msg) {
	case WM_INITDIALOG:
		ppro = (CJabberProto*)lParam;
		SetWindowLongPtr(hwndDlg, GWLP_USERDATA, (LONG_PTR)lParam);

		Window_SetIcon_IcoLib(hwndDlg, g_GetIconHandle(IDI_KEYS));
		TranslateDialogDefault(hwndDlg);
		if (ppro->m_bJabberOnline && ppro->m_ThreadInfo != NULL) {
			wchar_t text[1024];
			mir_snwprintf(text, TranslateT("Set New Password for %s@%S"), ppro->m_ThreadInfo->conn.username, ppro->m_ThreadInfo->conn.server);
			SetWindowText(hwndDlg, text);
		}
		return TRUE;
	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDOK:
			if (ppro->m_bJabberOnline && ppro->m_ThreadInfo != NULL) {
				wchar_t newPasswd[512], text[512];
				GetDlgItemText(hwndDlg, IDC_NEWPASSWD, newPasswd, _countof(newPasswd));
				GetDlgItemText(hwndDlg, IDC_NEWPASSWD2, text, _countof(text));
				if (mir_wstrcmp(newPasswd, text)) {
					MessageBox(hwndDlg, TranslateT("New password does not match."), TranslateT("Change Password"), MB_OK|MB_ICONSTOP|MB_SETFOREGROUND);
					break;
				}
				GetDlgItemText(hwndDlg, IDC_OLDPASSWD, text, _countof(text));
				if (mir_wstrcmp(text, ppro->m_ThreadInfo->conn.password)) {
					MessageBox(hwndDlg, TranslateT("Current password is incorrect."), TranslateT("Change Password"), MB_OK|MB_ICONSTOP|MB_SETFOREGROUND);
					break;
				}
				ppro->m_ThreadInfo->tszNewPassword = mir_wstrdup(newPasswd);

				XmlNodeIq iq(ppro->AddIQ(&CJabberProto::OnIqResultSetPassword, JABBER_IQ_TYPE_SET, _A2T(ppro->m_ThreadInfo->conn.server)));
				HXML q = iq << XQUERY(JABBER_FEAT_REGISTER);
				q << XCHILD(L"username", ppro->m_ThreadInfo->conn.username);
				q << XCHILD(L"password", newPasswd);
				ppro->m_ThreadInfo->send(iq);
			}
			DestroyWindow(hwndDlg);
			break;
		case IDCANCEL:
			DestroyWindow(hwndDlg);
			break;
		}
		break;
	case WM_CLOSE:
		DestroyWindow(hwndDlg);
		break;
	case WM_DESTROY:
		ppro->m_hwndJabberChangePassword = NULL;
		Window_FreeIcon_IcoLib(hwndDlg);
		break;
	}

	return FALSE;
}
