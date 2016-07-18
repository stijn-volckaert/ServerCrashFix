/*=============================================================================
	ServerCrashFix_Generic.h

	OS independent structures and functions

	Revision history:
		* Created by AnthraX
=============================================================================*/

/*-----------------------------------------------------------------------------
	FNetworkNotifyHook - Original FNetworkNotify in UnLevel.h
-----------------------------------------------------------------------------*/
class FNetworkNotifyHook : public FNetworkNotify
{
public:
	// Reference to the original FNetworkNotify -> Must be restored on mapswitch
	FNetworkNotify* OriginalNotify;

	// Redirect these functions to the original FNetworkNotify
	virtual EAcceptConnection NotifyAcceptingConnection() { return OriginalNotify->NotifyAcceptingConnection(); }	
	virtual ULevel* NotifyGetLevel() { return OriginalNotify->NotifyGetLevel(); }	
	virtual UBOOL NotifySendingFile( UNetConnection* Connection, FGuid GUID ) { return OriginalNotify->NotifySendingFile(Connection,GUID); }
	virtual void NotifyReceivedFile( UNetConnection* Connection, INT PackageIndex, const TCHAR* Error, UBOOL Skipped ) { OriginalNotify->NotifyReceivedFile(Connection,PackageIndex,Error,Skipped); }
	virtual void NotifyProgress( const TCHAR* Str1, const TCHAR* Str2, FLOAT Seconds ) { OriginalNotify->NotifyProgress(Str1,Str2,Seconds); }
	
	// Override this function 
	void NotifyReceivedText( UNetConnection* Connection, const TCHAR* Text );

	// And here's a shortcut to stop banned ip's early ;)
	void NotifyAcceptedConnection( class UNetConnection* Connection );

	// Along with this one
	UBOOL NotifyAcceptingChannel( class UChannel* Channel );

	// Super duper close connection function
	void CloseConnection( UNetConnection* Connection, FString Reason );	

	// Default Constructor
	FNetworkNotifyHook() {};	
};

/*-----------------------------------------------------------------------------
	Can't properly add fields to a UObject without mirroring them in uscript
-----------------------------------------------------------------------------*/
class UFunctionHookHelper
{
public:
	void InitHelper(UFunction* UFunc, Native OldFunc, Native NewFunc)
	{
		OriginalUFunction	= UFunc;
		Old					= OldFunc;
		Func				= NewFunc;
	}

	void Hook()
	{
		OriginalUFunction->Func	= Func;
	}

	void UnHook()
	{
		OriginalUFunction->Func = Old;
	}

	void CallFunction(FFrame& Stack, RESULT_DECL)
	{
		UnHook();
		Stack.Node->CallFunction(Stack, Result, OriginalUFunction);
		Hook();
	}

private:
	UFunction*	OriginalUFunction;
	Native		Old;
	Native		Func;
};

/*-----------------------------------------------------------------------------
	DLOHook - Used to hook the DLO UFunction. Portable across platforms.
	Thanks goes to shambler for this idea!

	TODO: Add GNatives hooking for registered natives?
-----------------------------------------------------------------------------*/
class UFunctionHook : public UObject
{
public:
	virtual Native GetNewFunc()=0;
	
	UFunctionHook() {};

	void Hook(UFunctionHookHelper* Helper, FString FuncName)
 	{
		UFunction* OriginalUFunction = NULL;

		for (TObjectIterator<UFunction> It; It; ++It)
		{
			if (appStrcmp(It->GetFullName(), *FuncName) == 0)
			{
				OriginalUFunction = *It;
				break;
			}
		}
		
		if (OriginalUFunction)
		{
			Helper->InitHelper(OriginalUFunction, OriginalUFunction->Func, GetNewFunc());
			Helper->Hook();
			//GLog->Logf(TEXT("[SCF] Hooked : %s (0x%08X)"), OriginalUFunction->GetFullName(), OriginalUFunction);
		}
	}

	void UnHook(UFunctionHookHelper* Helper)
	{
		Helper->UnHook();
	}	
};

class DLOHook : public UFunctionHook
{
	void NewFunc(FFrame& Stack, RESULT_DECL);
	Native GetNewFunc();
};
class ExecHook : public UFunctionHook
{
	void NewFunc(FFrame& Stack, RESULT_DECL);
	Native GetNewFunc();
};

/*-----------------------------------------------------------------------------
	SCFScopedLock - Scoped critical section
-----------------------------------------------------------------------------*/
class SCFScopedLock
{
private:
	LOCKTYPE* Lock;

public:
	SCFScopedLock(LOCKTYPE* lLock)
	{
		Lock = lLock;
		LOCK(lLock);
	}

	~SCFScopedLock()
	{
		UNLOCK(Lock);
	}
};

/*-----------------------------------------------------------------------------
	SCFThreadSafeMalloc - Original Malloc was not thread safe :(
-----------------------------------------------------------------------------*/
class SCFThreadSafeMalloc : public FMalloc
{		
public:
	void* Malloc( DWORD Count, const TCHAR* Tag );
	void* Realloc( void* Original, DWORD Count, const TCHAR* Tag );
	void Free( void* Original );
	void DumpAllocs();
	void HeapCheck();
	void Init();
	void Exit();
	void InitThreadSafeMalloc(FMalloc* OldMalloc);
	void ExitThreadSafeMalloc();

private:
	DEFINE_LOCK(SCFMallocLock);
	FMalloc* OldFMalloc;
};

/*-----------------------------------------------------------------------------
	InitThreadSafeMalloc - Initializes critical section object and redirects
	GMalloc. MUST BE CALLED IN MAIN UT THREAD!!!
-----------------------------------------------------------------------------*/
void SCFThreadSafeMalloc::InitThreadSafeMalloc(FMalloc *OldMalloc)
{
	INIT_LOCK(SCFMallocLock);
	OldFMalloc = OldMalloc;
	GMalloc = this;	
}

/*-----------------------------------------------------------------------------
	ExitThreadSafeMalloc - Uninitializes critical section object and restores
	GMalloc. MUST BE CALLED IN MAIN UT THREAD!!!
-----------------------------------------------------------------------------*/
void SCFThreadSafeMalloc::ExitThreadSafeMalloc()
{
	GMalloc = OldFMalloc;
}

/*-----------------------------------------------------------------------------
	Malloc
-----------------------------------------------------------------------------*/
void* SCFThreadSafeMalloc::Malloc(DWORD Count, const TCHAR *Tag)
{	
	SCFScopedLock((LOCKTYPE*)&SCFMallocLock);
	return OldFMalloc->Malloc(Count, Tag);
}

/*-----------------------------------------------------------------------------
	Realloc
-----------------------------------------------------------------------------*/
void* SCFThreadSafeMalloc::Realloc(void *Original, DWORD Count, const TCHAR *Tag)
{	
	SCFScopedLock((LOCKTYPE*)&SCFMallocLock);
	return OldFMalloc->Realloc(Original, Count, Tag);
}

/*-----------------------------------------------------------------------------
	Free
-----------------------------------------------------------------------------*/
void SCFThreadSafeMalloc::Free(void *Original)
{	
	SCFScopedLock((LOCKTYPE*)&SCFMallocLock);
	OldFMalloc->Free(Original);
}

/*-----------------------------------------------------------------------------
	DumpAllocs
-----------------------------------------------------------------------------*/
void SCFThreadSafeMalloc::DumpAllocs()
{
	SCFScopedLock((LOCKTYPE*)&SCFMallocLock);
	OldFMalloc->DumpAllocs();
}

/*-----------------------------------------------------------------------------
	HeapCheck
-----------------------------------------------------------------------------*/
void SCFThreadSafeMalloc::HeapCheck()
{
	SCFScopedLock((LOCKTYPE*)&SCFMallocLock);
	OldFMalloc->HeapCheck();
}

/*-----------------------------------------------------------------------------
	Init
-----------------------------------------------------------------------------*/
void SCFThreadSafeMalloc::Init()
{
	SCFScopedLock((LOCKTYPE*)&SCFMallocLock);
	OldFMalloc->Init();
}

/*-----------------------------------------------------------------------------
	Exit
-----------------------------------------------------------------------------*/
void SCFThreadSafeMalloc::Exit()
{
	SCFScopedLock((LOCKTYPE*)&SCFMallocLock);
	OldFMalloc->Exit();
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
