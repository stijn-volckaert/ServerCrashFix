/*=============================================================================
	ServerCrashFix_v11Private.h

	Revision history:
		* Created by AnthraX
=============================================================================*/
#ifndef _INC_SERVERCRASHFIX
#define _INC_SERVERCRASHFIX

/*-----------------------------------------------------------------------------
	Platform includes
-----------------------------------------------------------------------------*/
#ifndef __LINUX_X86__
	#include <windows.h>
	#include <Mmsystem.h>
	#include "detours.h"
	#pragma comment(lib, "winmm.lib")
	#pragma comment(lib, "detours.lib")
#else
	#define _GNU_SOURCE
	#define TRUE 1
	#define FALSE 0
	#include <stdlib.h>
	#include <sys/types.h>
	#include <errno.h>
	#include <unistd.h>
	#include <sched.h>	
	#include <pthread.h>
	#include <fcntl.h>
	#include <execinfo.h>
	#include <dlfcn.h>
#endif

/*-----------------------------------------------------------------------------
	Definitions - Modify for all new releases!
-----------------------------------------------------------------------------*/
#define ENGINE_API				DLL_IMPORT
#define APPSECONDSSLOW			"?appSecondsSlow@@YA?AVFTime@@XZ"
#define PKG_OUTPUT_PACKAGE		"ServerCrashFix_v11"
#define PKG_CLASSES_HEADER		"ServerCrashFix_v11Classes.h"
#define PKG_API					SERVERCRASHFIX_V11_API
#define PKG_NAME(name)			SERVERCRASHFIX_V11_##name
#define IMPLEMENT_PKG_PACKAGE	IMPLEMENT_PACKAGE(ServerCrashFix_v11)

#ifndef __LINUX_X86__
	#define SERVERCRASHFIX_V11_API	DLL_EXPORT
#endif

/*-----------------------------------------------------------------------------
	Includes
-----------------------------------------------------------------------------*/
#include "Core.h"
#include "Engine.h"
#include "UnNet.h"

/*-----------------------------------------------------------------------------
	Platform specific definitions/includes
-----------------------------------------------------------------------------*/
#ifdef __LINUX_X86__
	#include "ServerCrashFix_Linux.h"
	#include "ServerCrashFix_Generic.h"	
	#include "ServerCrashFix_v11Classes.h"
#else
	#include "ServerCrashFix_Windows.h"
	#include "ServerCrashFix_Generic.h"	
	#include "ServerCrashFix_v11Classes.h"
#endif

/*-----------------------------------------------------------------------------
	Global variables	
-----------------------------------------------------------------------------*/
FNetworkNotifyHook*		netHook				= NULL;
SCFThreadSafeMalloc*	mallocHook			= NULL;
FString					banList				= TEXT(" ");
bool					useBanList			= false;
UBOOL					HookedExec			= FALSE;
DLOHook*				dloHook				= NULL;
UFunctionHookHelper*	dloHelper			= NULL;
ExecHook*				execHook			= NULL;
UFunctionHookHelper*	execHelper			= NULL;

#endif
