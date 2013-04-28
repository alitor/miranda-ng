#pragma once

#include <windows.h>
#include <shlwapi.h>
#include <ShellAPI.h>
#include <malloc.h>
#include <tlhelp32.h>
#include <string>
#include <io.h>
#include <fcntl.h>
#include <time.h>

#include <newpluginapi.h>
#include <m_avatars.h>
#include <m_clist.h>
#include <m_clistint.h>
#include <m_clui.h>
#include <m_database.h>
#include <m_langpack.h>
#include <m_message.h>
#include <m_netlib.h>
#include <m_options.h>
#include <m_popup.h>
#include <m_protocols.h>
#include <m_protomod.h>
#include <m_protosvc.h>
#include <m_protoint.h>
#include <m_skin.h>
#include <m_userinfo.h>
#include <m_icolib.h>
#include <m_utils.h>
#include <m_system_cpp.h>
#include <m_xml.h>
#include <win2k.h>
#include <m_timezones.h>

#include <m_folders.h>

#include "resource.h"
#include "version.h"

#define MODULE "Skype"
#define SKYPE_SID_LIMIT 128
#define SKYPE_PASSWORD_LIMIT 128
#define SKYPE_GROUP_NAME_LIMIT 100

#define SKYPE_SETTINGS_STATUS "Status"
#define SKYPE_SETTINGS_LOGIN "sid"
#define SKYPE_SETTINGS_PASSWORD "Password"
#define SKYPE_SETTINGS_DEF_GROUP "DefaultGroup"

#define SKYPE_SEARCH_BYSID 1001
#define SKYPE_SEARCH_BYEMAIL 1002
#define SKYPE_SEARCH_BYNAMES 1003

#define SKYPE_DB_EVENT_TYPE_CALL 9000

#define CMI_AUTH_REVOKE 0
#define CMI_AUTH_REQUEST 1
#define CMI_AUTH_GRANT 2

#define SMI_CHAT_INVOKE 0

#define CMITEMS_COUNT 3

class CSkype;

extern CSkype *g_skype;
extern HINSTANCE g_hInstance;