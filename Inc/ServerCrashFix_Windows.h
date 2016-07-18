/*=============================================================================
	ServerCrashFix_Windows.h

	Revision history:
		* Created by AnthraX
=============================================================================*/

/*-----------------------------------------------------------------------------
	Windows specific globals
-----------------------------------------------------------------------------*/
DOUBLE	Correction			= NULL;		// Timer continuity
PVOID	pOldAppSecondsSlow	= NULL;		// Hooked func

/*-----------------------------------------------------------------------------
	Thread Locking (low-contention locks)
-----------------------------------------------------------------------------*/
#define LOCKTYPE	volatile LONG

//
// Create a normal mutex
//
#define DEFINE_LOCK(LockName)\
	volatile LONG LockName;

// Definition
#define INIT_LOCK(LockName)\
	LockName = 0;

//
// Locking and unlocking
// 
#define LOCK(pLockName)\
	while(InterlockedIncrement(pLockName) != 1)\
		InterlockedDecrement(pLockName);

#define UNLOCK(pLockName)\
	InterlockedDecrement(pLockName);

/*-----------------------------------------------------------------------------
	TFAppSecondsSlow - 
-----------------------------------------------------------------------------*/
FTime TFAppSecondsSlow()
{
	static DWORD  OldTime   = timeGetTime();
	static DOUBLE TimeInSec = 0.0;

	DWORD NewTime  = timeGetTime();
	
	// Fix for buggy timers
	if (NewTime > OldTime)
	{
		TimeInSec	  += (NewTime - OldTime) / 1000.0;
		OldTime    	   = NewTime;
	}

	FTime Result = TimeInSec + Correction;

	return Result;
}

/*-----------------------------------------------------------------------------
	TFGetCorrection
-----------------------------------------------------------------------------*/
FLOAT TFGetCorrection()
{
	return appSeconds().GetFloat() - TFAppSecondsSlow().GetFloat();
}

/*-----------------------------------------------------------------------------
	Platform specific functions
-----------------------------------------------------------------------------*/
//
// Platform: 1 = windows, 2 = linux 
//
DWORD FORCEINLINE appGetPlatform()
{
	return 1;
}

// 
// Request highest possible resolution (usually 1ms)
// -> return 0 on linux or on error
//
DWORD appRequestTimer()
{
	TIMECAPS caps;	

	if (timeGetDevCaps(&caps, sizeof(caps)) == MMSYSERR_NOERROR)
	{
		timeBeginPeriod(caps.wPeriodMin);
		return caps.wPeriodMin;
	}

	return 0;
}

//
// Change appSecondsSlow from GetTickCount to timeGetTime
//
UBOOL appHookAppSeconds()
{
	HMODULE hCore			= GetModuleHandle(L"Core.dll");
	FARPROC pAppSecondsSlow	= NULL;
	if (hCore)
		pAppSecondsSlow	= GetProcAddress(hCore, "?appSecondsSlow@@YA?AVFTime@@XZ");

	if (pAppSecondsSlow)
	{
		pOldAppSecondsSlow	= DetourFunction((PBYTE)pAppSecondsSlow, (PBYTE)&TFAppSecondsSlow);
		GTimestamp			= 0;
		Correction			= TFGetCorrection();
		return TRUE;
	}

	return FALSE;
}

//
// Install custom signal handlers for crash reporting
//	-> Not on windows
//
UBOOL appInstallHandlers()
{
	return FALSE;
}

// 
// Force CPU Core affinity
//
INT appSetAffinity(INT CPUCore)
{
	static INT CPUSet = 0;

	if (CPUSet != 0)
		return CPUSet;
	
	SYSTEM_INFO SI;
	GetSystemInfo( &SI );
	if (CPUCore > (int)SI.dwNumberOfProcessors-1)
	{
		GLog->Logf(TEXT("[SCF] appSetAffinity ERROR"));
		GLog->Logf(TEXT("[SCF] CPUCore (user-defined): %d"), CPUCore);
		GLog->Logf(TEXT("[SCF] dwNumberOfProcessors (SYSTEM_INFO): %d"), SI.dwNumberOfProcessors);
		return 0;
	}	

	CPUSet = SetThreadAffinityMask(GetCurrentThread(), 0x00000001 << CPUCore);
	return (CPUSet ? CPUCore : -1);
}
