
#include "stdafx.h"

pfnCustomProc g_CustomProc = NULL;
LPARAM g_CustomProcParam = 0;
TTBCtrl *g_ctrl = NULL;

void CALLBACK OnEventFire();

HWND hwndContactList = 0;

int nextButtonId = 200;

HANDLE hTTBModuleLoaded;
static WNDPROC buttonWndProc;

mir_cs csButtonsHook;

int sortfunc(const TopButtonInt *a, const TopButtonInt *b)
{
	return a->arrangedpos - b->arrangedpos;
}

LIST<TopButtonInt> Buttons(8, sortfunc);

TopButtonInt *idtopos(int id, int *pPos)
{
	for (int i = 0; i < Buttons.getCount(); i++)
		if (Buttons[i]->id == id) {
			if (pPos)
				*pPos = i;
			return Buttons[i];
		}

	if (pPos)
		*pPos = -1;
	return NULL;
}

//----- Service buttons -----
void InsertSBut(int i)
{
	TTBButton ttb = { 0 };
	ttb.hIconDn = (HICON)LoadImage(hInst, MAKEINTRESOURCE(IDI_RUN), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR);
	ttb.hIconUp = (HICON)LoadImage(hInst, MAKEINTRESOURCE(IDI_RUN), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR);
	ttb.dwFlags = TTBBF_VISIBLE | TTBBF_ISSBUTTON | TTBBF_INTERNAL;
	ttb.wParamDown = i;
	TTBAddButton((WPARAM)&ttb, 0);
}

void LoadAllSButs()
{
	//must be locked
	int cnt = db_get_b(0, TTB_OPTDIR, "ServiceCnt", 0);
	if (cnt > 0) {
		for (int i = 1; i <= cnt; i++)
		InsertSBut(i);
	}
}

//----- Launch buttons -----
INT_PTR LaunchService(WPARAM, LPARAM lParam)
{
	PROCESS_INFORMATION pi;
	STARTUPINFO si = {0};
	si.cb = sizeof(si);

	if (CreateProcess(NULL, Buttons[lParam]->ptszProgram, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);
	}

	return 0;
}

void InsertLBut(int i)
{
	TTBButton ttb = { 0 };
	ttb.hIconDn = (HICON)LoadImage(hInst, MAKEINTRESOURCE(IDI_RUN), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR);
	ttb.dwFlags = TTBBF_VISIBLE | TTBBF_ISLBUTTON | TTBBF_INTERNAL;
	ttb.name = LPGEN("Default");
	ttb.program = L"Execute Path";
	ttb.wParamDown = i;
	TTBAddButton((WPARAM)&ttb, 0);
}

void LoadAllLButs()
{
	//must be locked
	int cnt = db_get_b(0, TTB_OPTDIR, "LaunchCnt", 0);
	for (int i = 0; i < cnt; i++)
		InsertLBut(i);
}

//----- Separators -----
void InsertSeparator(int i)
{
	TTBButton ttb = { 0 };
	ttb.dwFlags = TTBBF_VISIBLE | TTBBF_ISSEPARATOR | TTBBF_INTERNAL;
	ttb.wParamDown = i;
	TTBAddButton((WPARAM)&ttb, 0);
}

void LoadAllSeparators()
{
	//must be locked
	int cnt = db_get_b(0, TTB_OPTDIR, "SepCnt", 0);
	for (int i = 0; i < cnt; i++)
		InsertSeparator(i);
}

int SaveAllButtonsOptions()
{
	int SeparatorCnt = 0;
	int LaunchCnt = 0;
	{
		mir_cslock lck(csButtonsHook);
		for (int i = 0; i < Buttons.getCount(); i++)
			Buttons[i]->SaveSettings(&SeparatorCnt, &LaunchCnt);
	}
	db_set_b(0, TTB_OPTDIR, "SepCnt", SeparatorCnt);
	db_set_b(0, TTB_OPTDIR, "LaunchCnt", LaunchCnt);
	return 0;
}

static bool nameexists(const char *name)
{
	if (name == NULL)
		return false;

	for (int i = 0; i < Buttons.getCount(); i++)
		if (!mir_strcmp(Buttons[i]->pszName, name))
			return true;

	return false;
}

static void Icon2button(TTBButton *but, HANDLE &hIcoLib, HICON &hIcon, bool bIsUp)
{
	HANDLE hSrc = bIsUp ? but->hIconHandleUp : but->hIconHandleDn;
	if (hSrc == NULL) {
		hIcoLib = NULL, hIcon = NULL;
		return;
	}

	hIcoLib = IcoLib_IsManaged((HICON)hSrc);
	if (!hIcoLib) {
		char buf[256];
		mir_snprintf(buf, "toptoolbar_%s%s", but->name, bIsUp ? (but->hIconDn ? "%s_up" : "%s") : "%s_dn");

		SKINICONDESC sid = { 0 };
		sid.section.a = "Toolbar";
		sid.pszName = buf;
		sid.defaultFile.a = NULL;
		mir_snprintf(buf, "%s%s", but->name, bIsUp ? "" : " (pressed)");
		sid.description.a = buf;
		sid.hDefaultIcon = bIsUp ? but->hIconUp : but->hIconDn;
		hIcoLib = IcoLib_AddIcon(&sid);
	}

	hIcon = IcoLib_GetIconByHandle(hIcoLib);
}

TopButtonInt *CreateButton(TTBButton *but)
{
	TopButtonInt *b = new TopButtonInt;
	b->id = nextButtonId++;

	b->dwFlags = but->dwFlags;

	b->wParamUp = but->wParamUp;
	b->lParamUp = but->lParamUp;
	b->wParamDown = but->wParamDown;
	b->lParamDown = but->lParamDown;

	if (!(b->dwFlags & TTBBF_ISSEPARATOR)) {
		b->bPushed = (but->dwFlags & TTBBF_PUSHED) ? TRUE : FALSE;

		if (but->dwFlags & TTBBF_ISLBUTTON) {
			b->ptszProgram = mir_wstrdup(but->program);
			b->pszService = mir_strdup(TTB_LAUNCHSERVICE);
		}
		else {
			b->ptszProgram = NULL;
			b->pszService = mir_strdup(but->pszService);
		}

		b->pszName = mir_strdup(but->name);

		Icon2button(but, b->hIconHandleUp, b->hIconUp, true);
		Icon2button(but, b->hIconHandleDn, b->hIconDn, false);

		b->ptszTooltipUp = mir_a2u(but->pszTooltipUp);
		b->ptszTooltipDn = mir_a2u(but->pszTooltipDn);
	}
	return b;
}

int ArrangeButtons()
{
	mir_cslock lck(csButtonsHook);

	RECT rcClient;
	GetClientRect(g_ctrl->hWnd, &rcClient);
	int nBarSize = rcClient.right - rcClient.left;
	if (nBarSize == 0)
		return 0;

	int nLineCount = 0;
	int i, nextX = 0, y = 0;
	int nButtonCount = 0;

	for (i = 0; i < Buttons.getCount(); i++)
		if (Buttons[i]->hwnd)
			nButtonCount++;

	if (nButtonCount == 0)
		return 0;

	HDWP hdwp = BeginDeferWindowPos(nButtonCount);

	bool bWasButttonBefore;
	int nUsedWidth, iFirstButtonId = 0, iLastButtonId = 0;

	do {
		nLineCount++;
		bWasButttonBefore = false;
		nUsedWidth = 0;

		for (i = iFirstButtonId; i < Buttons.getCount(); i++) {
			TopButtonInt *b = Buttons[i];
			if (b->hwnd == NULL)
				continue;

			int width = 0;
			if (b->isVisible())
				width = b->isSep() ? SEPWIDTH + 2 : g_ctrl->nButtonWidth + (bWasButttonBefore ? g_ctrl->nButtonSpace : 0);
			if (nUsedWidth + width > nBarSize)
				break;

			nUsedWidth += width;
			iLastButtonId = i + 1;
			bWasButttonBefore = !b->isSep();
		}

		for (i = iFirstButtonId; i < iLastButtonId; i++) {
			TopButtonInt *b = Buttons[i];
			if (b->hwnd == NULL)
				continue;

			bool bOldVisible = IsWindowVisible(b->hwnd) != 0;
			if (bOldVisible != b->isVisible())
				g_ctrl->bOrderChanged = TRUE;

			if (b->isVisible()) {
				if (NULL != b->hwnd) /* Wine fix. */
					hdwp = DeferWindowPos(hdwp, b->hwnd, NULL, nextX, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_SHOWWINDOW);
				if (b->isSep())
					nextX += SEPWIDTH + 2;
				else
					nextX += g_ctrl->nButtonWidth + g_ctrl->nButtonSpace;
			} else {
				if (NULL != Buttons[i]->hwnd) /* Wine fix. */
					hdwp = DeferWindowPos(hdwp, Buttons[i]->hwnd, NULL, nextX, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_HIDEWINDOW);
			}
		}

		if (iFirstButtonId == iLastButtonId)
			break;

		iFirstButtonId = iLastButtonId;
		y += g_ctrl->nButtonHeight + g_ctrl->nButtonSpace;
		nextX = 0;
		if (g_ctrl->bSingleLine)
			break;
	} while (iFirstButtonId < Buttons.getCount() && y >= 0 && (g_ctrl->bAutoSize || (y + g_ctrl->nButtonHeight <= rcClient.bottom - rcClient.top)));

	for (i = iLastButtonId; i < Buttons.getCount(); i++) {
		if (NULL != Buttons[i]->hwnd) /* Wine fix. */
			hdwp = DeferWindowPos(hdwp, Buttons[i]->hwnd, NULL, nextX, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_HIDEWINDOW);
	}

	if (hdwp)
		EndDeferWindowPos(hdwp);

	return (g_ctrl->nButtonHeight + g_ctrl->nButtonSpace) * nLineCount - g_ctrl->nButtonSpace;
}

/////////////////////////////////////////////////////////////////////////////////////////
// Toolbar services

// wparam = (TTBButton*)lpTTBButton
// lparam = hLangpack
INT_PTR TTBAddButton(WPARAM wParam, LPARAM lParam)
{
	if (wParam == 0)
		return -1;

	TTBButton *but = (TTBButton *)wParam;
	if (!(but->dwFlags & TTBBF_ISLBUTTON) && nameexists(but->name))
		return -1;

	TopButtonInt *b = CreateButton(but);
	b->hLangpack = (int)lParam;
	b->LoadSettings();
	b->CreateWnd();
	if (b->hwnd == NULL) {
		delete b;
		return -1;
	}
	mir_cslock lck(csButtonsHook);
	Buttons.insert(b);

	g_ctrl->bOrderChanged = TRUE;
	ArrangeButtons();
	AddToOptions(b);
	return b->id;
}

// wparam = (HANDLE)hTTButton
// lparam = 0
INT_PTR TTBRemoveButton(WPARAM wParam, LPARAM)
{
	mir_cslock lck(csButtonsHook);

	int idx;
	TopButtonInt *b = idtopos(wParam, &idx);
	if (b == NULL)
		return -1;

	RemoveFromOptions(b->id);

	Buttons.remove(idx);
	delete b;

	g_ctrl->bOrderChanged = TRUE;
	ArrangeButtons();
	return 0;
}

// wparam = hTTBButton
// lparam = state
INT_PTR TTBSetState(WPARAM wParam, LPARAM lParam)
{
	mir_cslock lck(csButtonsHook);

	TopButtonInt *b = idtopos(wParam);
	if (b == NULL)
		return -1;

	b->bPushed = (lParam & TTBST_PUSHED) != 0;
	b->SetBitmap();
	return 0;
}

// wparam = hTTBButton
// lparam = 0
//return = state
INT_PTR TTBGetState(WPARAM wParam, LPARAM)
{
	mir_cslock lck(csButtonsHook);
	TopButtonInt *b = idtopos(wParam);
	if (b == NULL)
		return -1;

	return ((b->bPushed == TRUE) ? TTBST_PUSHED : 0);
}

INT_PTR TTBGetOptions(WPARAM wParam, LPARAM lParam)
{
	mir_cslock lck(csButtonsHook);
	TopButtonInt *b = idtopos(wParam);
	if (b == NULL)
		return -1;

	switch (LOWORD(wParam)) {
	case TTBO_FLAGS: {
		INT_PTR retval = b->dwFlags & (~TTBBF_PUSHED);
		if (b->bPushed)
			retval |= TTBBF_PUSHED;
		return retval;
	}
	case TTBO_TIPNAME:
		return (INT_PTR)b->ptszTooltip;

	case TTBO_ALLDATA:
		if (lParam) {
			TTBButton *lpTTB = (TTBButton*)lParam;
			lpTTB->dwFlags = b->dwFlags & (~TTBBF_PUSHED);
			if (b->bPushed)
				lpTTB->dwFlags |= TTBBF_PUSHED;

			lpTTB->hIconDn = b->hIconDn;
			lpTTB->hIconUp = b->hIconUp;

			lpTTB->lParamUp = b->lParamUp;
			lpTTB->wParamUp = b->wParamUp;
			lpTTB->lParamDown = b->lParamDown;
			lpTTB->wParamDown = b->wParamDown;

			if (b->dwFlags & TTBBF_ISLBUTTON)
				replaceStrW(lpTTB->program, b->ptszProgram);
			else
				replaceStr(lpTTB->pszService, b->pszService);

			return (INT_PTR)lpTTB;
		}
		else
			return -1;

	default:
		return -1;
	}
}

INT_PTR TTBSetOptions(WPARAM wParam, LPARAM lParam)
{
	mir_cslock lck(csButtonsHook);
	TopButtonInt *b = idtopos(HIWORD(wParam));
	if (b == NULL)
		return -1;

	switch (LOWORD(wParam)) {
	case TTBO_FLAGS: {
		if (b->dwFlags == lParam)
			return 0;

		DWORD retval = b->CheckFlags(lParam);

		if (retval & TTBBF_PUSHED)
			b->SetBitmap();
		if (retval & TTBBF_VISIBLE) {
			ArrangeButtons();
			b->SaveSettings(0, 0);
		}

		return 1;
	}
	case TTBO_TIPNAME:
		if (lParam == 0)
			return -1;

		replaceStrW(b->ptszTooltip, TranslateW(_A2T((LPCSTR)lParam)));
		SendMessage(b->hwnd, BUTTONADDTOOLTIP, (WPARAM)b->ptszTooltip, BATF_UNICODE);
		return 1;

	case TTBO_ALLDATA:
		if (lParam) {
			TTBButton *lpTTB = (TTBButton*)lParam;
			DWORD retval = b->CheckFlags(lpTTB->dwFlags);

			bool changed = false;
			if (b->hIconUp != lpTTB->hIconUp) {
				b->hIconUp = lpTTB->hIconUp;
				changed = true;
			}
			if (b->hIconDn != lpTTB->hIconDn) {
				b->hIconDn = lpTTB->hIconDn;
				changed = true;
			}
			if (changed)
				b->SetBitmap();

			if (retval & TTBBF_VISIBLE) {
				ArrangeButtons();
				b->SaveSettings(0, 0);
			}

			if (b->dwFlags & TTBBF_ISLBUTTON)
				replaceStrW(b->ptszProgram, lpTTB->program);
			else
				replaceStr(b->pszService, lpTTB->pszService);

			b->lParamUp = lpTTB->lParamUp;
			b->wParamUp = lpTTB->wParamUp;
			b->lParamDown = lpTTB->lParamDown;
			b->wParamDown = lpTTB->wParamDown;

			return 1;
		}
		else
			return 0;

	default:
		return -1;
	}
}

/////////////////////////////////////////////////////////////////////////////////////////
// Reload all icons from their icolib handles

int OnIconChange(WPARAM, LPARAM)
{
	mir_cslock lck(csButtonsHook);
	for (int i = 0; i < Buttons.getCount(); i++) {
		TopButtonInt *b = Buttons[i];
		if (!b->hIconHandleUp && !b->hIconHandleDn)
			continue;

		if (b->hIconHandleUp) {
			IcoLib_ReleaseIcon(b->hIconUp);
			b->hIconUp = IcoLib_GetIconByHandle(b->hIconHandleUp);
		}
		if (b->hIconHandleDn) {
			IcoLib_ReleaseIcon(b->hIconDn);
			b->hIconDn = IcoLib_GetIconByHandle(b->hIconHandleDn);
		}
		DestroyWindow(b->hwnd);
		b->CreateWnd();
	}

	if (g_ctrl->hWnd) {
		g_ctrl->bOrderChanged = true;
		PostMessage(g_ctrl->hWnd, TTB_UPDATEFRAMEVISIBILITY, 0, 0);
	}

	return 0;
}

static int OnBGChange(WPARAM, LPARAM)
{
	LoadBackgroundOptions();
	return 0;
}

static INT_PTR TTBSetCustomProc(WPARAM wParam, LPARAM lParam)
{
	g_CustomProc = (pfnCustomProc)wParam;
	g_CustomProcParam = lParam;
	return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////
// Adds buttons of plugins being loaded. lParam = HINSTANCE

int OnPluginLoad(WPARAM, LPARAM lParam)
{
	CallPluginEventHook((HINSTANCE)lParam, hTTBModuleLoaded, 0, 0);
	if (g_ctrl->hWnd && g_ctrl->bOrderChanged)
		PostMessage(g_ctrl->hWnd, TTB_UPDATEFRAMEVISIBILITY, 0, 0);
	return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////
// Removes buttons of plugins being unloaded. lParam = HINSTANCE

int OnPluginUnload(WPARAM, LPARAM lParam)
{
	int lang = GetPluginLangByInstance((HINSTANCE)lParam);
	if (lang) {
		bool bNeedUpdate = false;
		mir_cslock lck(csButtonsHook);

		for (int i = Buttons.getCount() - 1; i >= 0; i--)
			if (Buttons[i]->hLangpack == lang) {
				TTBRemoveButton(Buttons[i]->id, 0);
				bNeedUpdate = true;
			}

		if (bNeedUpdate) {
			ArrangeButtons();
			if (g_ctrl->hWnd)
				PostMessage(g_ctrl->hWnd, TTB_UPDATEFRAMEVISIBILITY, 0, 0);
		}
	}
	return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////

static int OnModulesLoad(WPARAM, LPARAM)
{
	LoadAllSeparators();
	LoadAllLButs();

	ArrangeButtons();

	Miranda_WaitOnHandle(OnEventFire);

	if (HookEvent(ME_BACKGROUNDCONFIG_CHANGED, OnBGChange)) {
		char buf[256];
		mir_snprintf(buf, "%s/%s", LPGEN("TopToolBar background"), TTB_OPTDIR);
		CallService(MS_BACKGROUNDCONFIG_REGISTER, (WPARAM)buf, 0);
	}
	return 0;
}

static int OnShutdown(WPARAM, LPARAM)
{
	if (g_ctrl) {
		if (g_ctrl->hFrame) {
			CallService(MS_CLIST_FRAMES_REMOVEFRAME, (WPARAM)g_ctrl->hFrame, 0);
			g_ctrl->hFrame = 0;
		}
		if (g_ctrl->hWnd) {
			DestroyWindow(g_ctrl->hWnd);
			g_ctrl->hWnd = NULL;
		}
	}

	return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////

static LRESULT CALLBACK TTBButtonWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	LRESULT lResult = buttonWndProc(hwnd, msg, wParam, lParam);

	if (msg == WM_NCCREATE) {
		TopButtonInt *p = (TopButtonInt *)((CREATESTRUCT *)lParam)->lpCreateParams;
		if (g_CustomProc)
			g_CustomProc((HANDLE)p->id, hwnd, g_CustomProcParam);
	}

	return lResult;
}

int LoadToolbarModule()
{
	if (!ServiceExists(MS_CLIST_FRAMES_ADDFRAME)) {
		if (!db_get_b(NULL, TTB_OPTDIR, "WarningDone", 0))
			MessageBox(0, TranslateT("Frames service has not been found, so plugin will be disabled.\nTo run it you need to install and / or enable contact list plugin that supports it:\n- Modern contact list\n- Clist nicer\nYou can get them at https://wiki.miranda-ng.org/Download"), TranslateT("TopToolBar"), 0);
		db_set_b(NULL, TTB_OPTDIR, "WarningDone", 1);
		return 1;
	}

	g_ctrl = (TTBCtrl *)mir_calloc(sizeof(TTBCtrl));
	g_ctrl->nButtonHeight = db_get_dw(0, TTB_OPTDIR, "BUTTHEIGHT", DEFBUTTHEIGHT);
	g_ctrl->nButtonWidth = db_get_dw(0, TTB_OPTDIR, "BUTTWIDTH", DEFBUTTWIDTH);
	g_ctrl->nButtonSpace = db_get_dw(0, TTB_OPTDIR, "BUTTGAP", DEFBUTTGAP);
	g_ctrl->nLastHeight = db_get_dw(0, TTB_OPTDIR, "LastHeight", DEFBUTTHEIGHT);

	g_ctrl->bFlatButtons = db_get_b(0, TTB_OPTDIR, "UseFlatButton", true);
	g_ctrl->bSingleLine = db_get_b(0, TTB_OPTDIR, "SingleLine", true);
	g_ctrl->bAutoSize = db_get_b(0, TTB_OPTDIR, "AutoSize", true);

	db_unset(NULL, TTB_OPTDIR, "WarningDone");

	hBmpSeparator = LoadBitmap(hInst, MAKEINTRESOURCE(IDB_SEP));

	HookEvent(ME_SYSTEM_MODULELOAD, OnPluginLoad);
	HookEvent(ME_SYSTEM_MODULEUNLOAD, OnPluginUnload);
	HookEvent(ME_SYSTEM_MODULESLOADED, OnModulesLoad);
	HookEvent(ME_SYSTEM_PRESHUTDOWN, OnShutdown);
	HookEvent(ME_SKIN2_ICONSCHANGED, OnIconChange);
	HookEvent(ME_OPT_INITIALISE, TTBOptInit);

	hTTBModuleLoaded = CreateHookableEvent(ME_TTB_MODULELOADED);

	CreateServiceFunction("TopToolBar/AddButton", TTBAddButton);
	CreateServiceFunction(MS_TTB_REMOVEBUTTON, TTBRemoveButton);

	CreateServiceFunction(MS_TTB_SETBUTTONSTATE, TTBSetState);
	CreateServiceFunction(MS_TTB_GETBUTTONSTATE, TTBGetState);

	CreateServiceFunction(MS_TTB_GETBUTTONOPTIONS, TTBGetOptions);
	CreateServiceFunction(MS_TTB_SETBUTTONOPTIONS, TTBSetOptions);

	CreateServiceFunction(TTB_LAUNCHSERVICE, LaunchService);

	CreateServiceFunction("TopToolBar/SetCustomProc", TTBSetCustomProc);

	buttonWndProc = (WNDPROC)CallService("Button/GetWindowProc", 0, 0);
	WNDCLASSEX wc = {0};
	wc.cbSize = sizeof(wc);
	wc.lpszClassName = TTB_BUTTON_CLASS;
	wc.lpfnWndProc = TTBButtonWndProc;
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.cbWndExtra = sizeof(void *);
	wc.hbrBackground = 0;
	wc.style = CS_GLOBALCLASS;
	RegisterClassEx(&wc);
	return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////

int UnloadToolbarModule()
{
	DestroyHookableEvent(hTTBModuleLoaded);

	DeleteObject(hBmpSeparator);

	for (int i = 0; i < Buttons.getCount(); i++)
		delete Buttons[i];

	mir_free(g_ctrl);
	return 0;
}
