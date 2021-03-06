/*

Miranda NG: the free IM client for Microsoft* Windows*

Copyright (�) 2012-17 Miranda NG project (https://miranda-ng.org),
Copyright (c) 2000-12 Miranda IM project,
all portions of this codebase are copyrighted to the people
listed in contributors.txt.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

#include "stdafx.h"

#include "plugins.h"
#include "profilemanager.h"
#include "langpack.h"
#include "netlib.h"

void LoadExtraIconsModule();

extern bool bModulesLoadedFired;

static int sttComparePluginsByName(const pluginEntry* p1, const pluginEntry* p2)
{
	return mir_wstrcmpi(p1->pluginname, p2->pluginname);
}

LIST<pluginEntry>
	pluginList(10, sttComparePluginsByName),
	servicePlugins(5, sttComparePluginsByName),
	clistPlugins(5, sttComparePluginsByName);

/////////////////////////////////////////////////////////////////////////////////////////

MUUID miid_last = MIID_LAST;
MUUID miid_srmm = MIID_SRMM;
MUUID miid_clist = MIID_CLIST;
MUUID miid_database = MIID_DATABASE;
MUUID miid_protocol = MIID_PROTOCOL;
MUUID miid_servicemode = MIID_SERVICEMODE;
MUUID miid_crypto = MIID_CRYPTO;
MUUID miid_ssl = MIID_SSL;

/////////////////////////////////////////////////////////////////////////////////////////

#define MAX_MIR_VER ULONG_MAX

static BOOL bModuleInitialized = FALSE;

wchar_t  mirandabootini[MAX_PATH];
static DWORD mirandaVersion;
static int sttFakeID = -100;
static HANDLE hPluginListHeap = nullptr;
static int askAboutIgnoredPlugins;

static pluginEntry *plugin_freeimg, *plugin_crshdmp, *serviceModePlugin, *plugin_ssl;

#define PLUGINDISABLELIST "PluginDisable"

/////////////////////////////////////////////////////////////////////////////////////////
// basic functions

bool hasMuuid(const MUUID* p, const MUUID& uuid)
{
	if (p == nullptr)
		return false;

	for (int i = 0; p[i] != miid_last; i++)
		if (p[i] == uuid)
			return true;

	return false;
}

bool hasMuuid(const BASIC_PLUGIN_INFO& bpi, const MUUID& uuid)
{
	if (bpi.Interfaces)
		return hasMuuid(bpi.Interfaces, uuid);

	return false;
}

/////////////////////////////////////////////////////////////////////////////////////////
// banned plugins

static const MUUID pluginBannedList[] =
{
	{ 0x9d6c3213, 0x02b4, 0x4fe1, { 0x92, 0xe6, 0x52, 0x6d, 0xe2, 0x4f, 0x8d, 0x65 } },  // old chat
	{ 0x240a91dc, 0x9464, 0x457a, { 0x97, 0x87, 0xff, 0x1e, 0xa8, 0x8e, 0x77, 0xe3 } },  // old clist
	{ 0x657fe89b, 0xd121, 0x40c2, { 0x8a, 0xc9, 0xb9, 0xfa, 0x57, 0x55, 0xb3, 0x0c } },  // old srmm
	{ 0x112f7d30, 0xcd19, 0x4c74, { 0xa0, 0x3b, 0xbf, 0xbb, 0x76, 0xb7, 0x5b, 0xc4 } },  // extraicons
	{ 0x72765a6f, 0xb017, 0x42f1, { 0xb3, 0x0f, 0x5e, 0x09, 0x41, 0x27, 0x3a, 0x3f } },  // flashavatars
	{ 0x1394a3ab, 0x2585, 0x4196, { 0x8f, 0x72, 0x0e, 0xae, 0xc2, 0x45, 0x0e, 0x11 } },  // db3x
	{ 0x28ff9b91, 0x3e4d, 0x4f1c, { 0xb4, 0x7c, 0xc6, 0x41, 0xb0, 0x37, 0xff, 0x40 } },  // dbx_mmap_sa
	{ 0x28f45248, 0x8c9c, 0x4bee, { 0x93, 0x07, 0x7b, 0xcf, 0x3e, 0x12, 0xbf, 0x99 } },  // dbx_tree
	{ 0x4c4a27cf, 0x5e64, 0x4242, { 0xa3, 0x32, 0xb9, 0x8b, 0x08, 0x24, 0x3e, 0x89 } },  // metacontacts
	{ 0x9c448c61, 0xfc3f, 0x42f9, { 0xb9, 0xf0, 0x4a, 0x30, 0xe1, 0xcf, 0x86, 0x71 } },  // skypekit based skype
	{ 0x49c2cf54, 0x7898, 0x44de, { 0xbe, 0x3a, 0x6d, 0x2e, 0x4e, 0xf9, 0x00, 0x79 } },  // firstrun
	{ 0x0ca63eee, 0xeb2c, 0x4aed, { 0xb3, 0xd0, 0xbc, 0x8e, 0x6e, 0xb3, 0xbf, 0xb8 } },  // stdurl
	{ 0x0aa7bfea, 0x1fc7, 0x45f0, { 0x90, 0x6e, 0x2a, 0x46, 0xb6, 0xe1, 0x19, 0xcf } },  // yahoo
	{ 0x2f3fe8b9, 0x7327, 0x4008, { 0xa6, 0x0d, 0x93, 0xf0, 0xf4, 0xf7, 0xf0, 0xf1 } },  // yahoogroups
	{ 0xf0fdf73a, 0x753d, 0x499d, { 0x8d, 0xba, 0x33, 0x6d, 0xb7, 0x9c, 0xdd, 0x41 } },  // advancedautoaway
	{ 0xa5bb1b7a, 0xb7cd, 0x4cbb, { 0xa7, 0xdb, 0xce, 0xb4, 0xeb, 0x71, 0xda, 0x49 } },  // keepstatus
	{ 0x4b733944, 0x5a70, 0x4b52, { 0xab, 0x2d, 0x68, 0xb1, 0xef, 0x38, 0xff, 0xe8 } },  // startupstatus
	{ 0x9d6c3213, 0x02b4, 0x4fe1, { 0x92, 0xe6, 0x52, 0x6d, 0xe1, 0x4f, 0x8d, 0x65 } }   // stdchat
};

static bool isPluginBanned(const MUUID& u1)
{
	for (int i = 0; i < _countof(pluginBannedList); i++)
		if (pluginBannedList[i] == u1)
			return true;

	return false;
}

/////////////////////////////////////////////////////////////////////////////////////////
// default plugins

static MuuidReplacement pluginDefault[] =
{
	{ MIID_UIUSERINFO, L"stduserinfo",   nullptr }, // 0
	{ MIID_SREMAIL,    L"stdemail",      nullptr }, // 1
	{ MIID_SRAUTH,     L"stdauth",       nullptr }, // 2
	{ MIID_SRFILE,     L"stdfile",       nullptr }, // 3
	{ MIID_UIHELP,     L"stdhelp",       nullptr }, // 4
	{ MIID_UIHISTORY,  L"stduihist",     nullptr }, // 5
	{ MIID_IDLE,       L"stdidle",       nullptr }, // 6
	{ MIID_AUTOAWAY,   L"stdautoaway",   nullptr }, // 7
	{ MIID_USERONLINE, L"stduseronline", nullptr }, // 8
	{ MIID_SRAWAY,     L"stdaway",       nullptr }, // 9
	{ MIID_CLIST,      L"stdclist",      nullptr }, // 10
	{ MIID_SRMM,       L"stdmsg",        nullptr }  // 11
};

int getDefaultPluginIdx(const MUUID &muuid)
{
	for (int i = 0; i < _countof(pluginDefault); i++)
		if (muuid == pluginDefault[i].uuid)
			return i;

	return -1;
}

int LoadStdPlugins()
{
	for (int i = 0; i < _countof(pluginDefault); i++) {
		if (pluginDefault[i].pImpl)
			continue;

		if (!LoadCorePlugin(pluginDefault[i]))
			return 1;
	}

	if (pluginDefault[11].pImpl == nullptr)
		MessageBox(nullptr, TranslateT("No messaging plugins loaded. Please install/enable one of the messaging plugins, for instance, \"StdMsg.dll\""), L"Miranda NG", MB_OK | MB_ICONWARNING);

	return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////
// global functions

char* GetPluginNameByInstance(HINSTANCE hInstance)
{
	if (pluginList.getCount() == 0)
		return nullptr;

	for (int i = 0; i < pluginList.getCount(); i++) {
		pluginEntry *p = pluginList[i];
		if (p->bpi.pluginInfo && p->bpi.hInst == hInstance)
			return p->bpi.pluginInfo->shortName;
	}
	return nullptr;
}

MIR_APP_DLL(int) GetPluginLangByInstance(HINSTANCE hInstance)
{
	if (pluginList.getCount() == 0)
		return 0;

	for (int i = 0; i < pluginList.getCount(); i++) {
		pluginEntry *p = pluginList[i];
		if (p->bpi.pluginInfo && p->bpi.hInst == hInstance)
			return p->hLangpack;
	}
	return 0;
}

MIR_APP_DLL(int) GetPluginLangId(const MUUID &uuid, int _hLang)
{
	if (uuid == miid_last)
		return --sttFakeID;

	for (int i = 0; i < pluginList.getCount(); i++) {
		pluginEntry *p = pluginList[i];
		if (!p->bpi.hInst)
			continue;

		if (p->bpi.pluginInfo->uuid == uuid)
			return p->hLangpack = (_hLang) ? _hLang : --sttFakeID;
	}

	return 0;
}

MIR_APP_DLL(int) IsPluginLoaded(const MUUID &uuid)
{
	for (int i = 0; i < pluginList.getCount(); i++) {
		pluginEntry *p = pluginList[i];
		if (!p->bpi.hInst)
			continue;

		if (p->bpi.pluginInfo->uuid == uuid)
			return true;
	}

	return false;
}

static bool validInterfaceList(MUUID *piface)
{
	if (piface == nullptr)
		return true;

	if (miid_last == piface[0])
		return false;

	return true;
}

static int checkPI(BASIC_PLUGIN_INFO* bpi, PLUGININFOEX* pi)
{
	if (pi == nullptr)
		return FALSE;

	if (bpi->InfoEx == nullptr || pi->cbSize != sizeof(PLUGININFOEX))
		return FALSE;

	if (!validInterfaceList(bpi->Interfaces) || isPluginBanned(pi->uuid))
		return FALSE;

	if (pi->shortName == nullptr || pi->description == nullptr || pi->author == nullptr ||
		pi->authorEmail == nullptr || pi->copyright == nullptr || pi->homepage == nullptr)
		return FALSE;

	return TRUE;
}

int checkAPI(wchar_t* plugin, BASIC_PLUGIN_INFO* bpi, DWORD dwMirVer, int checkTypeAPI)
{
	HINSTANCE h = LoadLibrary(plugin);
	if (h == nullptr)
		return 0;

	// loaded, check for exports
	bpi->Load = (Miranda_Plugin_Load)GetProcAddress(h, "Load");
	bpi->Unload = (Miranda_Plugin_Unload)GetProcAddress(h, "Unload");
	bpi->InfoEx = (Miranda_Plugin_InfoEx)GetProcAddress(h, "MirandaPluginInfoEx");

	// if they were present
	if (!bpi->Load || !bpi->Unload || !bpi->InfoEx) {
LBL_Error:
		FreeLibrary(h);
		return 0;
	}

	bpi->Interfaces = (MUUID*)GetProcAddress(h, "MirandaInterfaces");
	if (bpi->Interfaces == nullptr) {
		typedef MUUID * (__cdecl * Miranda_Plugin_Interfaces) (void);
		Miranda_Plugin_Interfaces pFunc = (Miranda_Plugin_Interfaces)GetProcAddress(h, "MirandaPluginInterfaces");
		if (pFunc)
			bpi->Interfaces = pFunc();
	}

	PLUGININFOEX* pi = bpi->InfoEx(dwMirVer);
	if (!checkPI(bpi, pi))
		goto LBL_Error;

	bpi->pluginInfo = pi;
	// basic API is present
	if (checkTypeAPI == CHECKAPI_NONE) {
LBL_Ok:
		bpi->hInst = h;
		return 1;
	}
	// check clist ?
	if (checkTypeAPI == CHECKAPI_CLIST) {
		bpi->clistlink = (CList_Initialise)GetProcAddress(h, "CListInitialise");
		if ((pi->flags & UNICODE_AWARE) && bpi->clistlink)
			goto LBL_Ok;
	}
	goto LBL_Error;
}

// perform any API related tasks to freeing
void Plugin_Uninit(pluginEntry *p)
{
	// if the basic API check had passed, call Unload if Load(void) was ever called
	if (p->pclass & PCLASS_LOADED) {
		p->bpi.Unload();
		p->pclass &= ~PCLASS_LOADED;
	}

	// release the library
	HINSTANCE hInst = p->bpi.hInst;
	if (hInst != nullptr) {
		// we need to kill all resources which belong to that DLL before calling FreeLibrary
		KillModuleEventHooks(hInst);
		KillModuleServices(hInst);
		UnregisterModule(hInst);

		FreeLibrary(hInst);
		memset(&p->bpi, 0, sizeof(p->bpi));
	}
	
	if (p == plugin_crshdmp)
		plugin_crshdmp = nullptr;
	pluginList.remove(p);
}

int Plugin_UnloadDyn(pluginEntry *p)
{
	if (p->bpi.hInst) {
		if (CallPluginEventHook(p->bpi.hInst, hOkToExitEvent, 0, 0) != 0)
			return FALSE;

		KillModuleSubclassing(p->bpi.hInst);

		CallPluginEventHook(p->bpi.hInst, hPreShutdownEvent, 0, 0);
		CallPluginEventHook(p->bpi.hInst, hShutdownEvent, 0, 0);

		KillModuleEventHooks(p->bpi.hInst);
		KillModuleServices(p->bpi.hInst);
	}

	int _hLang = p->hLangpack;
	if (_hLang != 0) {
		KillModuleMenus(_hLang);
		KillModuleFonts(_hLang);
		KillModuleColours(_hLang);
		KillModuleEffects(_hLang);
		KillModuleIcons(_hLang);
		KillModuleHotkeys(_hLang);
		KillModuleSounds(_hLang);
		KillModuleExtraIcons(_hLang);
		KillModuleSrmmIcons(_hLang);
		KillModuleToolbarIcons(_hLang);
	}

	NotifyFastHook(hevUnloadModule, (WPARAM)p->bpi.pluginInfo, (LPARAM)p->bpi.hInst);

	Plugin_Uninit(p);

	// mark default plugins to be loaded
	if (!(p->pclass & PCLASS_CORE))
		for (int i = 0; i < _countof(pluginDefault); i++)
			if (pluginDefault[i].pImpl == p)
				pluginDefault[i].pImpl = nullptr;

	return TRUE;
}

// returns true if the given file is <anything>.dll exactly
static int valid_library_name(wchar_t *name)
{
	wchar_t *dot = wcsrchr(name, '.');
	if (dot != nullptr && mir_wstrcmpi(dot + 1, L"dll") == 0)
		if (dot[4] == 0)
			return 1;

	return 0;
}

void enumPlugins(SCAN_PLUGINS_CALLBACK cb, WPARAM wParam, LPARAM lParam)
{
	// get miranda's exe path
	wchar_t exe[MAX_PATH];
	GetModuleFileName(nullptr, exe, _countof(exe));
	wchar_t *p = wcsrchr(exe, '\\'); if (p) *p = 0;

	// create the search filter
	wchar_t search[MAX_PATH];
	mir_snwprintf(search, L"%s\\Plugins\\*.dll", exe);

	// FFFN will return filenames for things like dot dll+ or dot dllx
	WIN32_FIND_DATA ffd;
	HANDLE hFind = FindFirstFile(search, &ffd);
	if (hFind == INVALID_HANDLE_VALUE)
		return;

	do {
		if (!(ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && valid_library_name(ffd.cFileName))
			cb(&ffd, exe, wParam, lParam);
	} while (FindNextFile(hFind, &ffd));
	FindClose(hFind);
}

pluginEntry* OpenPlugin(wchar_t *tszFileName, wchar_t *dir, wchar_t *path)
{
	pluginEntry *p = (pluginEntry*)HeapAlloc(hPluginListHeap, HEAP_NO_SERIALIZE | HEAP_ZERO_MEMORY, sizeof(pluginEntry));
	wcsncpy_s(p->pluginname, tszFileName, _TRUNCATE);

	// add it to the list anyway
	pluginList.insert(p);

	wchar_t tszFullPath[MAX_PATH];
	mir_snwprintf(tszFullPath, L"%s\\%s\\%s", path, dir, tszFileName);

	// map dll into the memory and check its exports
	bool bIsPlugin = false;
	mir_ptr<MUUID> pIds(GetPluginInterfaces(tszFullPath, bIsPlugin));
	if (!bIsPlugin) {
		p->pclass |= PCLASS_FAILED;  // piece of shit
		return p;
	}

	// plugin declared that it's a database or a cryptor. load it asap!
	bool bIsDb = hasMuuid(pIds, miid_database);
	if (bIsDb || hasMuuid(pIds, miid_crypto)) {
		BASIC_PLUGIN_INFO bpi;
		if (checkAPI(tszFullPath, &bpi, mirandaVersion, CHECKAPI_NONE)) {
			// plugin is valid
			p->pclass |= ((bIsDb ? PCLASS_DB : PCLASS_CRYPT) | PCLASS_BASICAPI);
			// copy the dblink stuff
			p->bpi = bpi;

			RegisterModule(p->bpi.hInst);
			if (bpi.Load() != 0)
				p->pclass |= PCLASS_FAILED;
			else
				p->pclass |= PCLASS_LOADED;
		}
		else p->pclass |= PCLASS_FAILED;
	}
	// plugin declared that it's a contact list. mark it for the future load
	else if (hasMuuid(pIds, miid_clist)) {
		// keep a note of this plugin for later
		clistPlugins.insert(p);
		p->pclass |= PCLASS_CLIST;
	}
	// plugin declared that it's a ssl provider. mark it for the future load
	else if (hasMuuid(pIds, miid_ssl)) {
		plugin_ssl = p;
		p->pclass |= PCLASS_LAST;
	}
	// plugin declared that it's a service mode plugin.
	// load it for a profile manager's window
	else if (hasMuuid(pIds, miid_servicemode)) {
		BASIC_PLUGIN_INFO bpi;
		if (checkAPI(tszFullPath, &bpi, mirandaVersion, CHECKAPI_NONE)) {
			p->pclass |= (PCLASS_OK | PCLASS_BASICAPI);
			p->bpi = bpi;
			if (hasMuuid(bpi, miid_servicemode)) {
				p->pclass |= (PCLASS_SERVICE);
				servicePlugins.insert(p);
			}
		}
		else
			// didn't have basic APIs or DB exports - failed.
			p->pclass |= PCLASS_FAILED;
	}
	return p;
}

void SetPluginOnWhiteList(const wchar_t* pluginname, int allow)
{
	db_set_b(0, PLUGINDISABLELIST, _strlwr(_T2A(pluginname)), allow == 0);
}

// returns 1 if the plugin should be enabled within this profile, filename is always lower case
int isPluginOnWhiteList(const wchar_t* pluginname)
{
	int rc = db_get_b(0, PLUGINDISABLELIST, _strlwr(_T2A(pluginname)), 0);
	if (rc != 0 && askAboutIgnoredPlugins) {
		wchar_t buf[256];
		mir_snwprintf(buf, TranslateT("'%s' is disabled, re-enable?"), pluginname);
		if (MessageBox(nullptr, buf, TranslateT("Re-enable Miranda plugin?"), MB_YESNO | MB_ICONQUESTION) == IDYES) {
			SetPluginOnWhiteList(pluginname, 1);
			rc = 0;
		}
	}

	return rc == 0;
}

bool TryLoadPlugin(pluginEntry *p, bool bDynamic)
{
	wchar_t exe[MAX_PATH], tszFullPath[MAX_PATH];
	GetModuleFileName(nullptr, exe, _countof(exe));
	wchar_t* slice = wcsrchr(exe, '\\');
	if (slice)
		*slice = 0;

	if (!(p->pclass & (PCLASS_LOADED | PCLASS_DB | PCLASS_CLIST))) {
		if (!bDynamic && !isPluginOnWhiteList(p->pluginname))
			return false;

		if (!(p->pclass & PCLASS_BASICAPI)) {
			BASIC_PLUGIN_INFO bpi;
			mir_snwprintf(tszFullPath, L"%s\\%s\\%s", exe, (p->pclass & PCLASS_CORE) ? L"Core" : L"Plugins", p->pluginname);
			if (!checkAPI(tszFullPath, &bpi, mirandaVersion, CHECKAPI_NONE)) {
				p->pclass |= PCLASS_FAILED;
				return false;
			}

			p->bpi = bpi;
			p->pclass |= PCLASS_OK | PCLASS_BASICAPI;
		}

		if (p->bpi.Interfaces) {
			MUUID *piface = p->bpi.Interfaces;
			for (int i = 0; piface[i] != miid_last; i++) {
				int idx = getDefaultPluginIdx(piface[i]);
				if (idx != -1 && pluginDefault[idx].pImpl) {
					if (!bDynamic) { // this place is already occupied, skip & disable
						SetPluginOnWhiteList(p->pluginname, 0);
						return false;
					}

					// we're loading new implementation dynamically, let the old one die
					if (!(p->pclass & PCLASS_CORE))
						Plugin_UnloadDyn(pluginDefault[idx].pImpl);
				}
			}
		}

		RegisterModule(p->bpi.hInst);
		if (p->bpi.Load() != 0)
			return false;

		p->pclass |= PCLASS_LOADED;
		if (p->bpi.Interfaces) {
			MUUID *piface = p->bpi.Interfaces;
			for (int i = 0; piface[i] != miid_last; i++) {
				int idx = getDefaultPluginIdx(piface[i]);
				if (idx != -1)
					pluginDefault[idx].pImpl = p;
			}
		}
	}
	else if (p->bpi.hInst != nullptr) {
		RegisterModule(p->bpi.hInst);
		p->pclass |= PCLASS_LOADED;
	}
	return true;
}

/////////////////////////////////////////////////////////////////////////////////////////
// Core plugins support

static wchar_t tszCoreErr[] = LPGENW("Core plugin '%s' cannot be loaded or missing. Miranda will exit now");

bool LoadCorePlugin(MuuidReplacement &mr)
{
	wchar_t exe[MAX_PATH], tszPlugName[MAX_PATH];
	GetModuleFileName(nullptr, exe, _countof(exe));
	wchar_t *p = wcsrchr(exe, '\\'); if (p) *p = 0;

	mir_snwprintf(tszPlugName, L"%s.dll", mr.stdplugname);
	pluginEntry* pPlug = OpenPlugin(tszPlugName, L"Core", exe);
	if (pPlug->pclass & PCLASS_FAILED) {
LBL_Error:
		MessageBox(nullptr, CMStringW(FORMAT, TranslateW(tszCoreErr), mr.stdplugname), TranslateT("Fatal error"), MB_OK | MB_ICONSTOP);

		Plugin_UnloadDyn(pPlug);
		mr.pImpl = nullptr;
		return false;
	}

	pPlug->pclass |= PCLASS_CORE;

	if (!TryLoadPlugin(pPlug, true))
		goto LBL_Error;

	if (bModulesLoadedFired) {
		if (CallPluginEventHook(pPlug->bpi.hInst, hModulesLoadedEvent, 0, 0) != 0)
			goto LBL_Error;

		NotifyEventHooks(hevLoadModule, (WPARAM)pPlug->bpi.pluginInfo, (LPARAM)pPlug->bpi.hInst);
	}
	mr.pImpl = pPlug;
	return true;
}

/////////////////////////////////////////////////////////////////////////////////////////
// Contact list plugins support

static bool loadClistModule(wchar_t* exe, pluginEntry *p)
{
	BASIC_PLUGIN_INFO bpi;
	if (checkAPI(exe, &bpi, mirandaVersion, CHECKAPI_CLIST)) {
		p->bpi = bpi;
		p->pclass |= PCLASS_LAST | PCLASS_OK | PCLASS_BASICAPI;
		RegisterModule(p->bpi.hInst);
		if (bpi.clistlink() == 0) {
			p->bpi = bpi;
			p->pclass |= PCLASS_LOADED;
			pluginDefault[10].pImpl = p;

			LoadExtraIconsModule();
			return true;
		}
		Plugin_Uninit(p);
	}
	return false;
}

static pluginEntry* getCListModule(wchar_t *exe)
{
	wchar_t tszFullPath[MAX_PATH];

	for (int i = 0; i < clistPlugins.getCount(); i++) {
		pluginEntry *p = clistPlugins[i];
		if (!isPluginOnWhiteList(p->pluginname))
			continue;

		mir_snwprintf(tszFullPath, L"%s\\Plugins\\%s", exe, p->pluginname);
		if (loadClistModule(tszFullPath, p))
			return p;
	}

	MuuidReplacement& stdClist = pluginDefault[10];
	if (LoadCorePlugin(stdClist)) {
		mir_snwprintf(tszFullPath, L"%s\\Core\\%s.dll", exe, stdClist.stdplugname);
		if (loadClistModule(tszFullPath, stdClist.pImpl))
			return stdClist.pImpl;
	}

	return nullptr;
}

int UnloadPlugin(wchar_t* buf, int bufLen)
{
	for (int i = pluginList.getCount() - 1; i >= 0; i--) {
		pluginEntry *p = pluginList[i];
		if (!mir_wstrcmpi(p->pluginname, buf)) {
			GetModuleFileName(p->bpi.hInst, buf, bufLen);
			Plugin_Uninit(p);
			return TRUE;
		}
	}

	return FALSE;
}

/////////////////////////////////////////////////////////////////////////////////////////
// Service plugins functions

void SetServiceModePlugin(pluginEntry *p)
{
	serviceModePlugin = p;
}

static int LaunchServicePlugin(pluginEntry *p)
{
	// plugin load failed - terminating Miranda
	if (!(p->pclass & PCLASS_LOADED)) {
		if (p->bpi.Load() != ERROR_SUCCESS) {
			Plugin_Uninit(p);
			return SERVICE_FAILED;
		}
		p->pclass |= PCLASS_LOADED;
	}

	INT_PTR res = CallService(MS_SERVICEMODE_LAUNCH, 0, 0);
	if (res != CALLSERVICE_NOTFOUND)
		return res;

	MessageBox(nullptr, TranslateT("Unable to load plugin in service mode!"), p->pluginname, MB_ICONSTOP);
	Plugin_Uninit(p);
	return SERVICE_FAILED;
}

int LoadDefaultServiceModePlugin()
{
	LPCTSTR param = CmdLine_GetOption(L"svc");
	if (param == nullptr || *param == 0)
		return SERVICE_CONTINUE;

	size_t cbLen = mir_wstrlen(param);
	for (int i = 0; i < servicePlugins.getCount(); i++) {
		pluginEntry *p = servicePlugins[i];
		if (!wcsnicmp(p->pluginname, param, cbLen)) {
			int res = LaunchServicePlugin(p);
			if (res == SERVICE_ONLYDB) // load it later
				serviceModePlugin = p;
			return res;
		}
	}

	return SERVICE_CONTINUE;
}

int LoadServiceModePlugin()
{
	return (serviceModePlugin == nullptr) ? SERVICE_CONTINUE : LaunchServicePlugin(serviceModePlugin);
}

void EnsureCheckerLoaded(bool bEnable)
{
	for (int i = 0; i < pluginList.getCount(); i++) {
		pluginEntry *p = pluginList[i];
		if (mir_wstrcmpi(p->pluginname, L"dbchecker.dll"))
			continue;

		if (bEnable) {
			if (!(p->pclass & PCLASS_LOADED)) {
				if (p->bpi.Load() != ERROR_SUCCESS)
					Plugin_Uninit(p);
				else {
					p->pclass |= PCLASS_LOADED;
					servicePlugins.remove(p);
				}
			}
		}
		else Plugin_Uninit(p);
		break;
	}
}

/////////////////////////////////////////////////////////////////////////////////////////

int LoadSslModule(void)
{
	bool bExtSSLLoaded = false;
	
	if (plugin_ssl != nullptr) 
	{
		if (!TryLoadPlugin(plugin_ssl, false)) 
		{
			Plugin_Uninit(plugin_ssl);
		}
		else
			bExtSSLLoaded = true;
	}
	if (!bExtSSLLoaded)
	{
		MuuidReplacement stdSsl = { MIID_SSL, L"stdssl", nullptr };
		if (!LoadCorePlugin(stdSsl))
			return 1;
	}
	mir_getSI(&sslApi);
	return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////
// Event hook to unload all non-core plugins
// hooked very late, after all the internal plugins, blah

void UnloadNewPlugins(void)
{
	// unload everything but the special db/clist plugins
	for (int i = pluginList.getCount() - 1; i >= 0; i--) {
		pluginEntry *p = pluginList[i];
		if (!(p->pclass & PCLASS_LAST) && (p->pclass & PCLASS_OK))
			Plugin_Uninit(p);
	}
}

/////////////////////////////////////////////////////////////////////////////////////////
// Loads all plugins

int LoadNewPluginsModule(void)
{
	// make full path to the plugin
	wchar_t exe[MAX_PATH], fullPath[MAX_PATH];
	GetModuleFileName(nullptr, exe, _countof(exe));
	wchar_t *slice = wcsrchr(exe, '\\');
	if (slice)
		*slice = 0;

	// remember some useful options
	askAboutIgnoredPlugins = (UINT)GetPrivateProfileInt(L"PluginLoader", L"AskAboutIgnoredPlugins", 0, mirandabootini);

	// if Crash Dumper is present, load it to provide Crash Reports
	if (plugin_crshdmp != nullptr && isPluginOnWhiteList(plugin_crshdmp->pluginname))
		if (!TryLoadPlugin(plugin_crshdmp, false))
			Plugin_Uninit(plugin_crshdmp);

	// if freeimage is present, load it to provide the basic core functions
	if (plugin_freeimg != nullptr) {
		BASIC_PLUGIN_INFO bpi;
		mir_snwprintf(fullPath, L"%s\\Plugins\\%s", exe, plugin_freeimg->pluginname);
		if (checkAPI(fullPath, &bpi, mirandaVersion, CHECKAPI_NONE)) {
			plugin_freeimg->bpi = bpi;
			plugin_freeimg->pclass |= PCLASS_OK | PCLASS_BASICAPI;
			if (bpi.Load() == 0)
				plugin_freeimg->pclass |= PCLASS_LOADED;
			else
				Plugin_Uninit(plugin_freeimg);
		}
	}

	// first load the clist cos alot of plugins need that to be present at Load(void)
	pluginEntry* clist = getCListModule(exe);

	/* the loop above will try and get one clist DLL to work, if all fail then just bail now */
	if (clist == nullptr) {
		// result = 0, no clist_* can be found
		if (clistPlugins.getCount())
			MessageBox(nullptr, TranslateT("Unable to start any of the installed contact list plugins, I even ignored your preferences for which contact list couldn't load any."), L"Miranda NG", MB_OK | MB_ICONERROR);
		else
			MessageBox(nullptr, TranslateT("Can't find a contact list plugin! You need StdClist or any other contact list plugin."), L"Miranda NG", MB_OK | MB_ICONERROR);
		return 1;
	}

	/* enable and disable as needed  */
	for (int i = 0; i < clistPlugins.getCount(); i++)
		SetPluginOnWhiteList(clistPlugins[i]->pluginname, clist != clistPlugins[i] ? 0 : 1);

	/* now loop thru and load all the other plugins, do this in one pass */
	for (int i = 0; i < pluginList.getCount(); i++) {
		pluginEntry *p = pluginList[i];
		if (!TryLoadPlugin(p, false)) {
			Plugin_Uninit(p);
			i--;
		}
	}

	HookEvent(ME_OPT_INITIALISE, PluginOptionsInit);
	return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////
// Plugins module initialization
// called before anything real is loaded, incl. database

static BOOL scanPluginsDir(WIN32_FIND_DATA *fd, wchar_t *path, WPARAM, LPARAM)
{
	pluginEntry *p = OpenPlugin(fd->cFileName, L"Plugins", path);
	if (!(p->pclass & PCLASS_FAILED)) {
		if (plugin_freeimg == nullptr && mir_wstrcmpi(fd->cFileName, L"advaimg.dll") == 0) {
			plugin_freeimg = p;
			p->pclass |= PCLASS_LAST;
		}

		if (plugin_crshdmp == nullptr && mir_wstrcmpi(fd->cFileName, L"crashdumper.dll") == 0) {
			plugin_crshdmp = p;
			p->pclass |= PCLASS_LAST;
		}
	}

	return TRUE;
}

int LoadNewPluginsModuleInfos(void)
{
	bModuleInitialized = TRUE;

	LoadPluginOptions();

	hPluginListHeap = HeapCreate(HEAP_NO_SERIALIZE, 0, 0);
	mirandaVersion = Miranda_GetVersion();

	// remember where the mirandaboot.ini goes
	PathToAbsoluteW(L"mirandaboot.ini", mirandabootini);

	// look for all *.dll's
	enumPlugins(scanPluginsDir, 0, 0);

	MuuidReplacement stdCrypt = { MIID_CRYPTO, L"stdcrypt", nullptr };
	return !LoadCorePlugin(stdCrypt);
}

/////////////////////////////////////////////////////////////////////////////////////////
// Plugins module unloading
// called at the end of module chain unloading, just modular engine left at this point

void UnloadDatabase(void)
{
	if (currDb != nullptr) {
		db_setCurrent(nullptr);
		currDblink->Unload(currDb);
		currDb = nullptr;
		currDblink = nullptr;
	}

	UninitIni();
}

void UnloadNewPluginsModule(void)
{
	if (!bModuleInitialized)
		return;

	UnloadPluginOptions();

	// unload everything but the DB
	for (int i = pluginList.getCount() - 1; i >= 0; i--) {
		pluginEntry *p = pluginList[i];
		if (!(p->pclass & (PCLASS_DB | PCLASS_CRYPT)) && p != plugin_crshdmp)
			Plugin_Uninit(p);
	}

	if (plugin_crshdmp)
		Plugin_Uninit(plugin_crshdmp);

	UnloadDatabase();

	for (int k = pluginList.getCount() - 1; k >= 0; k--) {
		pluginEntry *p = pluginList[k];
		Plugin_Uninit(p);
	}

	if (hPluginListHeap)
		HeapDestroy(hPluginListHeap);
	hPluginListHeap = 0;
}
