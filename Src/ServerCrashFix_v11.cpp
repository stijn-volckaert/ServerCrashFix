/*=============================================================================
	ServerCrashFix_v11.cpp

	Revision history:
		* Created by AnthraX
=============================================================================*/

/*-----------------------------------------------------------------------------
	Includes
-----------------------------------------------------------------------------*/
#include "ServerCrashFix_Private.h"
#include "FOutputDeviceFile.h"
#include <stdio.h>

/*-----------------------------------------------------------------------------
	Debugging Stuff
-----------------------------------------------------------------------------*/
void SCFInit();
void SCFFini();

/*-----------------------------------------------------------------------------
	Implementation definitions - Does NOT need to be modified between versions.
	Edit ServerCrashFix_Private instead!!!

	Name registration works on both platforms.
-----------------------------------------------------------------------------*/
#undef IMPLEMENT_PACKAGE_PLATFORM
#define IMPLEMENT_PACKAGE_PLATFORM(pkgname) \
	extern "C" {HINSTANCE hInstance;} \
	INT DLL_EXPORT STDCALL DllMain( HINSTANCE hInInstance, DWORD Reason, void* Reserved ) \
	{\
		hInstance = hInInstance;\
		if (Reason == DLL_PROCESS_ATTACH)\
		{\
			SCFInit();\
		}\
		else if (Reason == DLL_PROCESS_DETACH)\
		{\
			SCFFini();\
		}\
		return 1;\
	}

IMPLEMENT_PKG_PACKAGE;
IMPLEMENT_CLASS(ASCFNative);

#define NAMES_ONLY
#ifndef __LINUX_X86__
	#define AUTOGENERATE_NAME(name) PKG_API FName PKG_NAME(name)=FName(TEXT(#name));
#else
	#define AUTOGENERATE_NAME(name) extern PKG_API FName PKG_NAME(name)=FName(TEXT(#name),FNAME_Intrinsic);
#endif
#define AUTOGENERATE_FUNCTION(cls,idx,name) IMPLEMENT_FUNCTION(cls,idx,name)
#include PKG_CLASSES_HEADER
#undef AUTOGENERATE_FUNCTION
#undef AUTOGENERATE_NAME
#undef NAMES_ONLY

/*-----------------------------------------------------------------------------
	Secure DynamicLoadObject Implementation - Initial Hooking is done elsewhere!
	Intercepts DLO call, checks arguments, unhooks, calls original function, rehooks
-----------------------------------------------------------------------------*/
void DLOHook::NewFunc(FFrame& Stack, RESULT_DECL)
{
	guard(DLOHook::NewFunc);
	BYTE *oldCode = Stack.Code; // Save stack position! Needs to be restored later!
	P_GET_STR(TheClass);
	P_GET_UBOOL(bMayFail);

	//GLog->Logf(TEXT("[SCF] DLO: %s"), *TheClass);

	// Malformed string!
	if (TheClass.InStr(TEXT("%")) != -1)
	{
		GLog->Logf(TEXT("[SCF] Prevented the following class from loading -> %s (possible malformed string exploit)"),*TheClass);
		P_FINISH;
		return;
	}

	// Restore stack
	Stack.Code = oldCode;
	dloHelper->CallFunction(Stack, Result);

	unguard;
}

/*-----------------------------------------------------------------------------
	Meh
-----------------------------------------------------------------------------*/
Native DLOHook::GetNewFunc()
{
	return static_cast<Native>(&DLOHook::NewFunc);
}

/*-----------------------------------------------------------------------------
	Secure execConsoleCommand
-----------------------------------------------------------------------------*/
void ExecHook::NewFunc(FFrame& Stack, RESULT_DECL)
{
	guard(ExecHook::NewFunc);
	P_GET_STR(ConsoleCommand);
	P_FINISH;

	const TCHAR* Str	= *ConsoleCommand;
	FStringOutputDevice OutString;

	GLog->Logf(TEXT("Intercepted ConsoleCommand: %s from func: %s"), *ConsoleCommand, Stack.Node->GetFullName());

	if (ParseCommand(&Str, TEXT("GET")))
	{
		// Get a class default variable.
		TCHAR ClassName[256], PropertyName[256];
		UClass* Class;
		UProperty* Property;
		if
		(	ParseToken( Str, ClassName, ARRAY_COUNT(ClassName), 1 )
		&&	(Class=FindObject<UClass>( ANY_PACKAGE, ClassName))!=NULL )
		{
			if
			(	ParseToken( Str, PropertyName, ARRAY_COUNT(PropertyName), 1 )
			&&	(Property=FindField<UProperty>( Class, PropertyName))!=NULL )
			{
				TCHAR* Temp = new TCHAR[65536];
				if( Class->Defaults.Num() )
					Property->ExportText( 0, Temp, (BYTE*)&Class->Defaults(0), (BYTE*)&Class->Defaults(0), PPF_Localized );
				OutString.Log( Temp );
				delete[] Temp;
			}
			else OutString.Logf( NAME_ExecWarning, TEXT("Unrecognized property %s"), PropertyName );
		}
		else OutString.Logf( NAME_ExecWarning, TEXT("Unrecognized class %s"), ClassName );
		*(FString*)Result = *OutString;
	}
	else
	{
		for (TObjectIterator<UGameEngine> It; It; ++It)
		{
			It->Exec(Str, OutString);
			*(FString*)Result = *OutString;
			break;
		}
	}
	unguard;
}

/*-----------------------------------------------------------------------------
	Meh
-----------------------------------------------------------------------------*/
Native ExecHook::GetNewFunc()
{
	return static_cast<Native>(&ExecHook::NewFunc);
}

/*-----------------------------------------------------------------------------
	FNetworkNotifyHook::NotifyReceivedText - Secure NotifyReceivedText Implementation
-----------------------------------------------------------------------------*/
void FNetworkNotifyHook::NotifyReceivedText(UNetConnection* Connection, const TCHAR* Text)
{
	guard(FNetworkNotifyHook::NotifyReceivedText);

	FString TempStr = FString(Text);
	FString NameStr = TEXT(" ");
	FString ReasonStr = TEXT(" ");
	int Offset = -1;

	// Illegal commands: (fake player exploit)
	// * PETE
	// * BADBOY
	// * REPEAT
	// * CRITOBJCNT
	// * AUTH
	// Illegal command options:
	// * LOGIN RESPONSE=<Number> URL=<Mapname>?Name=<playername>?Class=<playerpawnclass>?team=<teamnumber>?skin=<skinclass>?Face=<faceclass>?Voice=<voiceclass>?OverrideClass=<overrideclass>?password=<pass>
	// --> % characters forbidden in anything but the playername. Most of these options are classes that will be DLO/SLO'd if not found ==> DANGEROUS
	// --> URL MUST NOT BE EMPTY!!!
	// * % characters in other ULevel messages also forbidden!
	// Also check for:
	// * JOIN if RequestURL is empty

	//GLog->Logf(TEXT("[SCF] NotifyReceivedText (%s): %s"),*Connection->URL.Host,*TempStr);

	if (Connection != NULL && useBanList && banList.InStr(Connection->URL.Host) != -1)
	{
		//GLog->Logf(TEXT("[SCF] Request from temporarily banned IP ( %s ) : %s "),Connection->URL.Host,TempStr);
		//GLog->Logf(TEXT("[SCF] REQUEST IGNORED"));
		return;
	}
	// Check for illegal commands first
	else if (TempStr.Left(4).Caps() == FString(TEXT("PETE"))
		|| TempStr.Left(6).Caps() == FString(TEXT("BADBOY"))
		|| TempStr.Left(6).Caps() == FString(TEXT("REPEAT"))
		|| TempStr.Left(10).Caps() == FString(TEXT("CRITOBJCNT"))
		|| TempStr.Left(4).Caps() == FString(TEXT("AUTH")))
	{
		ReasonStr = FString::Printf(TEXT("Illegal Command: %s"),*TempStr);
		CloseConnection(Connection,ReasonStr);
		return;
	}
	// Illegal options
	else if (TempStr.InStr(TEXT("%")) != -1 && TempStr.Left(14).Caps() != FString(TEXT("LOGIN RESPONSE")))
	{
		ReasonStr = FString::Printf(TEXT("Illegal Command: %s (Possible Malformed String Exploit)"),*TempStr);
		CloseConnection(Connection,ReasonStr);
		return;
	}
	// Empty join request
	else if (TempStr.Left(4).Caps() == FString(TEXT("JOIN")) && Connection->RequestURL.Len() <= 3)
	{
		ReasonStr = FString::Printf(TEXT("Illegal Join Request: %s (Fake Player DoS)"),*TempStr);
		CloseConnection(Connection,ReasonStr);
		return;
	}
	// Double checks on LOGIN RESPONSE -> % in classes, empty url
	else if (TempStr.Left(14) == FString(TEXT("LOGIN RESPONSE")))
	{
		// Check URL field
		Offset = TempStr.InStr(TEXT("URL="));
		if (Offset != -1)
		{
			TempStr = TempStr.Mid(Offset+4);

			// (Almost empty) => illegal option
			if (TempStr.Len() <= 3)
			{
				ReasonStr = FString::Printf(TEXT("Illegal Login Response URL: %s (Incorrect Format)"),*TempStr);
				CloseConnection(Connection,ReasonStr);
				return;
			}
		}
		else
		{
			ReasonStr = FString::Printf(TEXT("Illegal Login Response URL: %s (Incorrect Format)"),*TempStr);
			CloseConnection(Connection,ReasonStr);
			return;
		}

		// Parse out the Name field
		// At this point the TempStr looks like ?prop1=<val1>?prop2=<val2>?...?Name=<PlayerName>?propN=<valN>
		Offset = TempStr.InStr(TEXT("?Name="));
		if (Offset != -1)
		{
			NameStr = TempStr.Mid(Offset+6);
			TempStr = TempStr.Left(Offset);
			Offset = NameStr.InStr(TEXT("?"));
			if (Offset != -1)
			{
				TempStr += NameStr.Mid(Offset);
				NameStr = NameStr.Left(Offset);
			}
		}

		// At this point the TempStr looks like ?prop1=<val1>?prop2=<val2>?...?propN=<valN>
		if (TempStr.InStr(TEXT("%")) != -1)
		{
			ReasonStr = FString::Printf(TEXT("Illegal Login Response URL: %s (Possible Malformed String Exploit)"),*TempStr);
			CloseConnection(Connection,ReasonStr);
			return;
		}

		// Login response looks clean => pass to original
		OriginalNotify->NotifyReceivedText(Connection, Text);
	}
	// no login response and nothing illegal => pass to original
	else
	{
		OriginalNotify->NotifyReceivedText(Connection, Text);
	}

	unguard;
}

/*-----------------------------------------------------------------------------
	FNetworkNotifyHook::NotifyAcceptedConnection - Secure NotifyAcceptedConnection Implementation
-----------------------------------------------------------------------------*/
void FNetworkNotifyHook::NotifyAcceptedConnection(UNetConnection* Connection)
{
	guard(FNetworkNotifyHook::NotifyAcceptedConnection);

	//GLog->Logf(TEXT("[SCF] NotifyAcceptedConnection: %s"), *Connection->URL.Host);

	// Ignore banned ip's ;)
	if (Connection != NULL && useBanList && banList.InStr(Connection->URL.Host) != -1)
	{
		return;
	}

	OriginalNotify->NotifyAcceptedConnection(Connection);

	unguard;
}

/*-----------------------------------------------------------------------------
	FNetworkNotifyHook::NotifyAcceptingChannel - Secure NotifyAcceptingChannel Implementation
-----------------------------------------------------------------------------*/
UBOOL FNetworkNotifyHook::NotifyAcceptingChannel(UChannel* Channel)
{
	guard(FNetworkNotifyHook::NotifyAcceptingChannel);

	// Ignore banned ip's ;)
	if (Channel->Connection != NULL && useBanList && banList.InStr(Channel->Connection->URL.Host) != -1)
	{
		// Reply to the server crash tool with a malformed string to shut down the tool ;)
		Channel->Connection->Logf(TEXT("UPGRADE LOL"));
		return false;
	}

	return OriginalNotify->NotifyAcceptingChannel(Channel);

	unguard;
}

/*-----------------------------------------------------------------------------
	FNetworkNotifyHook::CloseConnection - Close a connection and log why!
-----------------------------------------------------------------------------*/
void FNetworkNotifyHook::CloseConnection( UNetConnection* Connection, FString Reason )
{
	guard(FNetworkNotifyHook::CloseConnection);

	FString ConStr;
	if (Connection != NULL)
		ConStr = Connection->URL.Host;
	else
		ConStr = FString(TEXT("No IP Found"));

	if (Connection != NULL)
	{
		// Log!
		GLog->Logf(TEXT("================================================================================"));
		GLog->Logf(TEXT("[SCF] A player has been prevented access from the server."));
		GLog->Logf(TEXT("[SCF] Player IP -> %s"),*ConStr);
		GLog->Logf(TEXT("[SCF] Reason -> %s"),*Reason);
		GLog->Logf(TEXT("================================================================================"));
		// Flush
		Connection->FlushNet();
		// Close socket
		Connection->State = USOCK_Closed;
		// Add to banlist for this level
		if (Connection != NULL && useBanList)
		{
			banList = banList+ConStr;
			banList = banList+TEXT(":");
		}
	}

	unguard;
}

/*-----------------------------------------------------------------------------
	ASCFNative::execHookNet - Hook the UNetDriver
-----------------------------------------------------------------------------*/
void ASCFNative::execHookNet (FFrame& Stack, RESULT_DECL)
{
	guard(ASCFNative::execHookNet);
	P_GET_UBOOL(bUseList);
	P_FINISH;

	// Try to destroy if it's still there
	if (netHook)
	{
		try
		{
			delete netHook;
		}
		catch(...)
		{
		}
	}

	//GNull = GLog;

	// Create object
	netHook = new FNetworkNotifyHook;

	// Fail? Shouldn't happen.
	if (!netHook)
	{
		*(DWORD*)Result=0;
		return;
	}

	// Find NetDriver and hook
	for (TObjectIterator<UNetDriver> It; It; ++It)
	{
		if (It->Notify != netHook)
		{
			netHook->OriginalNotify = It->Notify;
			It->Notify = netHook;
		}
	}

	// Temp ban list
	if (bUseList)
		useBanList=true;
	else
		useBanList=false;

	banList = TEXT(" ");

	// Success!
	*(DWORD*)Result=1;

	unguard;
}

/*-----------------------------------------------------------------------------
	ASCFNative::execUnHookNet - Unhook the UNetDriver
-----------------------------------------------------------------------------*/
void ASCFNative::execUnHookNet (FFrame& Stack, RESULT_DECL)
{
	guard(ASCFNative::execUnHookNet);
	P_FINISH;

	// Not hooked?
	if (!netHook)
		return;

	// Find NetDriver and unhook
	for (TObjectIterator<UNetDriver> It; It; ++It)
	{
		if (It->Notify == netHook)
		{
			It->Notify = netHook->OriginalNotify;
		}
	}

	// Clean up
	delete netHook;

	// Fix pointer
	netHook = NULL;

	unguard;
}

/*-----------------------------------------------------------------------------
	ASCFNative::execHookDLO - Hook the DynamicLoadObject UFunction
-----------------------------------------------------------------------------*/
void ASCFNative::execHookDLO (FFrame& Stack, RESULT_DECL)
{
	guard(ASCFNative::execHookDLO);
	P_FINISH;

	if (!dloHook)
	{
		// Create objects
		dloHook		= new DLOHook;
		dloHelper	= new UFunctionHookHelper;
		dloHook->Hook(dloHelper, TEXT("Function Core.Object.DynamicLoadObject"));
	}

	*(DWORD*)Result = dloHook ? 1 : 0;

	unguard;
}

/*-----------------------------------------------------------------------------
	ASCFNative::execUnHookDLO - Unhook the DynamicLoadObject UFunction
-----------------------------------------------------------------------------*/
void ASCFNative::execUnHookDLO (FFrame& Stack, RESULT_DECL)
{
	guard(ASCFNative::execUnHookDLO);
	P_FINISH;

	// Not hooked?
	if (!dloHook)
		return;

	// Restore
	dloHook->UnHook(dloHelper);

	// Clean up
	delete dloHook;
	delete dloHelper;

	// Fix pointer
	dloHook		= NULL;
	dloHelper	= NULL;

	unguard;
}

/*-----------------------------------------------------------------------------
	ASCFNative::execHookAppSeconds
-----------------------------------------------------------------------------*/
void ASCFNative::execHookAppSeconds(FFrame& Stack, RESULT_DECL)
{
	P_FINISH;

	// On linux this is limited to setting GTimestamp to false. This will switch
	// the time measurement method to sys_gettimeofday which, in conjunction
	// with the affinity setting, should provide more reliable time readings
	// than RDTSC.
	//
	// On windows appHookAppSeconds() will effectively detour appSecondsSlow
	// to switch the time measurement system to timeGetTime (WINMM). This method
	// is multicore safe but slightly less accurate than RDTSC in ideal circumstances
	GTimestamp = FALSE;
	appHookAppSeconds();

	*(DWORD*)Result = 1;
}

/*-----------------------------------------------------------------------------
	ASCFNative::execRequestTimer
-----------------------------------------------------------------------------*/
void ASCFNative::execRequestTimer(FFrame& Stack, RESULT_DECL)
{
	P_FINISH;

	*(DWORD*)Result = appRequestTimer();
}

/*-----------------------------------------------------------------------------
	ASCFNative::execSetAffinity
-----------------------------------------------------------------------------*/
void ASCFNative::execSetAffinity(FFrame& Stack, RESULT_DECL)
{
	P_GET_INT(CPUCore);
	P_FINISH;

	INT Tmp = appSetAffinity(CPUCore);
	if (Tmp < 0)
		*(FString*)Result = TEXT("ERROR");
	else
		*(FString*)Result = FString::Printf(TEXT("%d"), Tmp);
}

/*-----------------------------------------------------------------------------
	ASCFNative::execInstallHandlers
-----------------------------------------------------------------------------*/
void ASCFNative::execInstallHandlers(FFrame& Stack, RESULT_DECL)
{
	P_FINISH;

	appInstallHandlers();
	*(DWORD*)Result = 1;
}

/*-----------------------------------------------------------------------------
	ASCFNative::execGetPlatform
-----------------------------------------------------------------------------*/
void ASCFNative::execGetPlatform(FFrame& Stack, RESULT_DECL)
{
	P_FINISH;

	*(DWORD*)Result = appGetPlatform();
}

/*-----------------------------------------------------------------------------
	ASCFNative::execHookMalloc
-----------------------------------------------------------------------------*/
void ASCFNative::execHookMalloc(FFrame& Stack, RESULT_DECL)
{
	P_FINISH;

	if (!mallocHook)
	{
		mallocHook = new SCFThreadSafeMalloc;
		mallocHook->InitThreadSafeMalloc((FMalloc*)GMalloc);
	}

	*(DWORD*)Result = 1;
}

/*-----------------------------------------------------------------------------
	ASCFNative::execHookExec - Hook execConsoleCommand
-----------------------------------------------------------------------------*/
void ASCFNative::execHookExec (FFrame& Stack, RESULT_DECL)
{
	guard(ASCFNative::execHookExec);
	P_FINISH;

	if (!execHook)
	{
		execHook	= new ExecHook;
		execHelper	= new UFunctionHookHelper;
		execHook->Hook(execHelper, TEXT("Function Engine.Actor.ConsoleCommand"));
	}

	*(DWORD*)Result = execHook ? 1 : 0;

	unguard;
}

/*-----------------------------------------------------------------------------
	ASCFNative::execUnHookExec - Unhook the execConsoleCommand
-----------------------------------------------------------------------------*/
void ASCFNative::execUnHookExec (FFrame& Stack, RESULT_DECL)
{
	guard(ASCFNative::execUnHookExec);
	P_FINISH;

	// Not hooked?
	if (!execHook)
		return;

	// Restore
	execHook->UnHook(execHelper);

	// Clean up
	delete execHook;
	delete execHelper;

	// Fix pointer
	execHook	= NULL;
	execHelper	= NULL;

	unguard;
}

/*-----------------------------------------------------------------------------
	Loghook globals
-----------------------------------------------------------------------------*/
int HasLogFile = 0;

/*-----------------------------------------------------------------------------
	Loghook defs
-----------------------------------------------------------------------------*/

class SCFLogHook : public FOutputDevice
{
public:
	SCFLogHook()
	: LogAr( NULL )	
	, Opened( 0 )
	, Dead( 0 )
	{
		Filename[0]=0;
	}
	~SCFLogHook()
	{
		if( LogAr )
		{
			Logf( NAME_Log, TEXT("Log file closed, %s"), appTimestamp() );
			delete LogAr;
			LogAr = NULL;
		}
	}
	void Serialize(const TCHAR* Data, enum EName Event)
	{
		//guard(SCFLogHook::Serialize);
		if (HasLogFile)
		{
			static UBOOL Entry=0;
			if( !GIsCriticalError || Entry )
			{
				if( !FName::SafeSuppressed(Event) )
				{
					if( !LogAr && !Dead )
					{
						// Make log filename.
						if( !Filename[0] )
						{
							appStrcpy( Filename, appBaseDir() );
							if
							(	!Parse(appCmdLine(), TEXT("LOG="), Filename+appStrlen(Filename), ARRAY_COUNT(Filename)-appStrlen(Filename) )
							&&	!Parse(appCmdLine(), TEXT("ABSLOG="), Filename, ARRAY_COUNT(Filename) ) )
							{
								appStrcat( Filename, appPackage() );
								appStrcat( Filename, TEXT(".log") );
							}
							appStrcat( Filename, TEXT(".scflog"));
						}

						// Open log file.
						//printf("opening log: %s\n", TCHAR_TO_ANSI(Filename));
						LogAr = GFileManager->CreateFileWriter( Filename, FILEWRITE_AllowRead|FILEWRITE_Unbuffered|(Opened?FILEWRITE_Append:0));
						if( LogAr )
						{
							Opened = 1;
							Logf( NAME_Log, TEXT("Log file open, %s"), appTimestamp() );
						}
						else Dead = 1;
					}
					if( LogAr && Event!=NAME_Title )
					{
						TCHAR Ch[1024];
						ANSICHAR ACh[1024];
						appSprintf( Ch, TEXT("%s: %s%s"), FName::SafeString(Event), Data, LINE_TERMINATOR );
						INT i;
						for( i=0; Ch[i]; i++ )
							ACh[i] = ToAnsi(Ch[i] );
						ACh[i] = 0;
						LogAr->Serialize( ACh, i );
						LogAr->Flush();
					}
				}
			}
			else
			{
				Entry=1;
				try
				{
					// Ignore errors to prevent infinite-recursive exception reporting.
					Serialize( Data, Event );
				}
				catch( ... )
				{}
				Entry=0;
			}
		}
		else
		{
			fflush(stdout);	
		}
		//printf("[SCF]: %s", TCHAR_TO_ANSI(Data));
		//unguard;
	}

	FArchive* LogAr;
	UBOOL Opened, Dead;
	TCHAR Filename[1024];
};

/*-----------------------------------------------------------------------------
	SCFInit
-----------------------------------------------------------------------------*/
void SCFInit()
{
	GLog->Logf(TEXT("[SCF]: Early Load Hook"));
	//GLog->Logf(TEXT("[SCF]: CmdLine: %s"), appCmdLine());
	FString File = TEXT("");
	if (Parse(appCmdLine(), TEXT("log"), File))
	{
		HasLogFile = 1;
		GLog->Logf(TEXT("[SCF]: This server is logging to a logfile..."));
	}
	else
	{
		GLog->Logf(TEXT("[SCF]: This server is logging to stdout..."));
	}
	GLogHook = new SCFLogHook();
}

/*-----------------------------------------------------------------------------
	SCFFini
-----------------------------------------------------------------------------*/
void SCFFini()
{

}