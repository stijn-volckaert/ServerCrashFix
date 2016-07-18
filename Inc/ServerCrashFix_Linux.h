/*=============================================================================
	ServerCrashFix_Linux.h

	Revision history:
		* Created by AnthraX
=============================================================================*/

/*-----------------------------------------------------------------------------
	Thread Locking
-----------------------------------------------------------------------------*/
#define LOCKTYPE	pthread_mutex_t

//
// Create a normal mutex
//
#define DEFINE_LOCK(LockName)\
	pthread_mutex_t LockName;

// Definition
#define INIT_LOCK(LockName)\
	pthread_mutex_init(&LockName, NULL);

//
// Locking and unlocking
// 
#define LOCK(pLockName)\
	pthread_mutex_lock(pLockName);
#define UNLOCK(pLockName)\
	pthread_mutex_unlock(pLockName);

/*-----------------------------------------------------------------------------
	Platform specific functions
-----------------------------------------------------------------------------*/
//
// Signal constant to string
//
const char* getTextualSig(DWORD dwSignal)
{
    const char* result = "UNKNOWN";

#define DEF_SIGNAL(a) case a:\
    result = #a;\
    break;\

    switch(dwSignal)
    {
        DEF_SIGNAL(SIGALRM)
        DEF_SIGNAL(SIGHUP)
        DEF_SIGNAL(SIGINT)
        DEF_SIGNAL(SIGKILL)
        DEF_SIGNAL(SIGPIPE)
        DEF_SIGNAL(SIGPOLL)
        DEF_SIGNAL(SIGPROF)
        DEF_SIGNAL(SIGTERM)
        DEF_SIGNAL(SIGUSR1)
        DEF_SIGNAL(SIGUSR2)
        DEF_SIGNAL(SIGVTALRM)
//        DEF_SIGNAL(STKFLT) - Undefined on linux
        DEF_SIGNAL(SIGPWR)
        DEF_SIGNAL(SIGWINCH)
        DEF_SIGNAL(SIGCHLD)
        DEF_SIGNAL(SIGURG)
        DEF_SIGNAL(SIGTSTP)
        DEF_SIGNAL(SIGTTIN)
        DEF_SIGNAL(SIGTTOU)
        DEF_SIGNAL(SIGSTOP)
        DEF_SIGNAL(SIGCONT)
        DEF_SIGNAL(SIGABRT)
        DEF_SIGNAL(SIGFPE)
        DEF_SIGNAL(SIGILL)
        DEF_SIGNAL(SIGQUIT)
        DEF_SIGNAL(SIGSEGV)
        DEF_SIGNAL(SIGTRAP)
        DEF_SIGNAL(SIGSYS)
//        DEF_SIGNAL(SIGEMT) - Undefined on linux
        DEF_SIGNAL(SIGBUS)
        DEF_SIGNAL(SIGXCPU)
        DEF_SIGNAL(SIGXFSZ)
    }

    return result;
}

/*-----------------------------------------------------------------------------
	SCF Exception handler
-----------------------------------------------------------------------------*/
static void SCFExceptionHandler(int sig, siginfo_t *siginfo, void *context)
{
    static INT IsError = 0;

    if (!IsError)
    {
        IsError = 1;
        int j, nptrs;
        void *buffer[1000];
        char **strings;

        nptrs = backtrace(buffer, 1000);

        GLog->Logf(TEXT("[SCF] Server Shutdown"));
        GLog->Logf(TEXT("[SCF] Signal Received: %s"), ANSI_TO_TCHAR(getTextualSig(sig)));

        strings = backtrace_symbols(buffer, nptrs);
        if (strings == NULL)
        {
            GLog->Logf(TEXT("[SCF] Backtrace failed"));
            exit(EXIT_FAILURE);
        }    
    
        GLog->Logf(TEXT("[SCF] Developer backtrace (%d):"), nptrs-2);
        for (j = 2; j < nptrs; j++)
            GLog->Logf(TEXT("[SCF] %03d: %s"), j-2, ANSI_TO_TCHAR(strings[j]));

        GLog->Logf(TEXT("[SCF] Memory Map:"));
    
        char procname[100];
        char buf[65536];
        int pos = 0;
        sprintf(procname, "cat /proc/%d/maps", getpid());
        FILE* fp = popen(procname, "r");

        while (true)
        {
            int read = fread(buf + pos, 1, 1024, fp);
     	    pos += read;

            if (read < 1024 || feof(fp) || pos+1024 > 65536)
                break;            
        }

        pclose(fp);

        buf[pos] = '\0';
        char* tok = NULL;
        tok = strtok(buf, "\n");
        while (tok != NULL)
        {
             GLog->Logf(TEXT("[SCF] %s"), ANSI_TO_TCHAR(tok));
             tok = strtok(NULL, "\n");
        }
      
        // Check if this build supports unwind...
        /*void* corehandle = dlopen("Core.so", RTLD_NOW);
        jmp_buf* pEnv    = (jmp_buf*)dlsym(corehandle, "_9__Context.Env");
        if (dlerror() == NULL)
        {
             GLog->Logf(TEXT("ACE: __Context::Env found. Trying to unwind..."));
             sync();
             longjmp(*pEnv, 1);
        }*/

        appPreExit();
        appExit();        
    }
    
    exit(0);
}

//
// Platform: 1 = windows, 2 = linux 
//
DWORD FORCEINLINE appGetPlatform()
{
	return 2;
}

// 
// Request highest possible resolution (usually 1ms)
// -> return 0 on linux or on error
//
DWORD appRequestTimer()
{
	return 0;
}

//
// Change appSecondsSlow from GetTickCount to timeGetTime
//
UBOOL appHookAppSeconds()
{
	GTimestamp = 0;
	return 1;
}

//
// Install custom signal handlers for crash reporting
//	-> Not on windows
//
UBOOL appInstallHandlers()
{
	static INT Initialized = 0;

	if (Initialized == 0)
	{
		Initialized = 1;
		
		struct sigaction termaction;
		termaction.sa_sigaction = &SCFExceptionHandler;
		termaction.sa_flags = SA_SIGINFO;

		sigaction(SIGILL , &termaction, NULL);
		sigaction(SIGSEGV, &termaction, NULL);
		sigaction(SIGIOT , &termaction, NULL);
		sigaction(SIGBUS , &termaction, NULL);
	}

	return 1;
}

// 
// Force CPU Core affinity
//
INT appSetAffinity(INT CPUCore)
{
	static INT CPUSet = 0;

	if (CPUSet != 0)
		return CPUSet;
	
	int numCPU = sysconf( _SC_NPROCESSORS_ONLN );
	if (CPUCore > numCPU-1)
	{
		GLog->Logf(TEXT("[SCF] appSetAffinity ERROR"));
		GLog->Logf(TEXT("[SCF] CPUCore (user-defined): %d"), CPUCore);
		GLog->Logf(TEXT("[SCF] numCPU (sysconf): %d"), numCPU);
		return 0;
	}

	unsigned long mask = 0x00000001 << CPUCore;
	unsigned int len = sizeof(mask);
	CPUSet = sched_setaffinity(0, len, (cpu_set_t*)&mask);

	return (CPUSet >= 0 ? CPUCore : -1);
}
