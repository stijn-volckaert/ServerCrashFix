// =============================================================================
// ServerCrashFix v1.1 - (c) 2009-2010 AnthraX
// =============================================================================
// SCF's native class.
// * Hooks DynamicLoadObject's UFunc to prevent Malformed String Crash
// * Hooks ULevel's NetDriver to prevent both Malformed String Crashes and Fake
//   Player DoS
// * Unhooks during mapswitches
// =============================================================================
// v1.1 Additions:
// * Installs a proxy between the engine and FMalloc to make FMalloc thread-safe
// This prevents some crashes during server startup (hostname resolving is done
// in different threads)
// * Hooks AActor::execConsoleCommand to intercept "GET" consolecommands. SCF
// reimplements this consolecommand in a safe way. This will prevent crashes
// caused by mods that execute commands like "get Engine.GameEngine ServerPackages"
// on servers with a lot of serverpackages
// * Installs a custom signal handler on linux servers. This will provide proper
// crashlogs when certain signals (such as SIGSEGV) terminate the server process
// =============================================================================
class SCFNative extends Actor
    native;

// =============================================================================
// Variables
// =============================================================================
var SCFActor fixActor; // Reference to the main actor
var SCFSpec fixSpec;   // Reference to the spec that tracks mapswitches
var bool bDLOHooked;
var bool bNetHooked;
var bool bExecHooked;
var bool bMallocHooked;
var bool bHandlersInstalled;
var string AffinitySet;
var bool bTimerSet;
var bool bAppSecondsHooked;

// =============================================================================
// native functions
// =============================================================================
native function int    HookDLO();
native function        UnHookDLO();
native function int    HookNet(bool bUseList);
native function        UnHookNet();
native function int    HookExec();
native function        UnHookExec();
native function int    HookMalloc();
native function int    GetPlatform();           // 1 = windows, 2 = linux
native function int    InstallHandlers();       // linux only
native function string SetAffinity(int Core);   // force affinity
native function int    RequestTimer();
native function int    HookAppSeconds();

// =============================================================================
// InitNative ~ Spawn Spec & Hook!
// =============================================================================
function InitNative()
{
    // Spawn spec first
    fixSpec = Level.Spawn(class'SCFSpec');
    fixSpec.fixNative = self;

    // Prevent crashes by attacks from the outside
    if (fixActor.bFixNetDriver)
    {
        // Try to hook DLO
        if (HookDLO() == 1)
            bDLOHooked = true;

        // Try to hook UNetDriver
        if (HookNet(true) == 1)
            bNetHooked = true;

        fixActor.myLog("DynamicLoadObject hooked:"@bDLOHooked);
        fixActor.myLog("UNetDriver hooked:"@bNetHooked);
    }

    // AActor::execConsoleCommand hook
    if (fixActor.bFixExec)
    {
        if (HookExec() == 1)
            bExecHooked = true;

        fixActor.myLog("exec hooked:"@bExecHooked);
    }

    // FMalloc multithreaded proxy
    if (fixActor.bFixMalloc)
    {
        if (HookMalloc() == 1)
            bMallocHooked = true;

        fixActor.myLog("GMalloc hooked:"@bMallocHooked);
    }

    // Signal handlers
    if (fixActor.bFixHandlers)
    {
        // Only on linux
        if (GetPlatform() == 2 && InstallHandlers() == 1)
            bHandlersInstalled = true;

        fixActor.myLog("Signal handlers installed:"@bHandlersInstalled);
    }

    // Set cpu core affinity
    if (fixActor.bFixAffinity)
    {
        AffinitySet = SetAffinity(fixActor.CPUAffinity);
        fixActor.myLog("CPU Affinity set:"@AffinitySet);
    }

    // Set timer
    if (fixActor.bFixTimer)
    {
        // Only on windows
        if (GetPlatform() == 1 && RequestTimer() == 1)
            bTimerSet = true;

        fixActor.myLog("High resolution timer requested:"@bTimerSet);
    }

    // Hook appSeconds()
    if (fixActor.bFixTickRate)
    {
        if (HookAppSeconds() == 1)
            bAppSecondsHooked = true;

        fixActor.myLog("appSeconds() hooked:"@bAppSecondsHooked);
    }

    //DynamicLoadObject("%n.%n",class'Object'); // DLO Test Pass 31/01/2009
}

// =============================================================================
// MapSwitch ~ Called by SCFSpec
// =============================================================================
function MapSwitch()
{
    fixActor.myLog("Mapswitch detected");
    if (bDLOHooked && fixActor.bForceUnhook)
    {
        fixActor.myLog("Unhooking DynamicLoadObject...");
        UnHookDLO();
    }
    if (bNetHooked)
    {
        fixActor.myLog("Unhooking UNetDriver...");
        UnHookNet();
    }
    if (bExecHooked && fixActor.bForceUnhook)
    {
        fixActor.myLog("Unhooking Exec...");
        UnHookExec();
    }
}
