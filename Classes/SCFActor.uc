// =============================================================================
// ServerCrashFix v1.1 - (c) 2009-2010 AnthraX
// =============================================================================
// Fixes for:
// * Malformed String Exploit (http://aluigi.org/adv/unrfs-adv.txt)
// * Fake Player DoS (http://aluigi.altervista.org/fakep/unrealfp.zip)
// =============================================================================
// Several new fixes added in 1.1:
// * Exec fix (prevents crashes caused by "GET" consolecommands)
// * Malloc fix (prevents crashes caused by multithreaded access to FMalloc)
// * Signal handler fix (installs custom signal handlers for better crash logging in linux)
// * OS Timer fix (windows only, requests high resolution timer)
// * Affinity fix (can be used to force CPU Core affinity on SMP systems)
// * TickRate fix (replaces appSeconds() by a multicore safe method)
// =============================================================================
// Cross-platform (although some linux v436 servers might need a Core.so update)
// Not compatible with AnthChecker NetFix (SCF will disable this fix automatically)
// =============================================================================
class SCFActor extends Actor
    config(ServerCrashFix);

// =============================================================================
// Variables
// =============================================================================
var config bool bEnabled;       // Must be true or won't work at all
var config bool bFixNetDriver;  // Protect against attacks from the outside
var config bool bFixExec;       // Protect against crashes caused by "GET" consolecommands
var config bool bFixMalloc;     // Protect against crashes caused by multithreaded access to FMalloc (eg: resolving hostnames)
var config bool bFixHandlers;   // Install custom signal handlers for better crash logs (linux only)
var config bool bFixAffinity;   // Force CPU core affinity for this server?
var config int  CPUAffinity;    // Don't auto-detect the best CPU core to let the server run on but use this one instead
var config bool bFixTimer;      // Request high resolution timer? (windows only)
var config bool bFixTickRate;   // Override appSeconds with a multicore-safe timing method? (works on both platforms but not recommended!)
var config bool bForceUnhook;   // Force SCF to unhook everything during mapswitches? (might crash with some mods, eg: NexGenX)
var SCFNative fixNative;        // Native class that handles everything

// =============================================================================
// PreBeginPlay ~ Disable AnthChecker's fix if needed
// =============================================================================
function PreBeginPlay()
{
    // local string ServerActors;
    local string ActorName;
    local Actor A;

    // Log
    Log("+---------------------------------------------+");
    Log("| ServerCrashFix v1.1 - (c) 2009-2010 AnthraX |");
    Log("+---------------------------------------------+");

    if (bEnabled)
    {
        // Check AnthChecker settings
        // ServerActors = ConsoleCommand("get Engine.GameEngine ServerActors");

        if (bFixNetDriver)
        {
            // AnthChecker v1.38 present?
            foreach Level.AllActors(class'Actor',A)
            {
                ActorName = string(A.Class);

                if (InStr(CAPS(ActorName),"ANTHCHECKERS_V138") != -1)
                {
                    // Check if malformed string fix is present
                    if (ConsoleCommand("get AnthCheckerS_v138.ACActor bEnableNetFix") ~= "true")
                    {
                        myLog("ServerCrashFix has disabled the AnthChecker NetFix and will now reboot the server...");
                        ConsoleCommand("set AnthCheckerS_v138.ACActor bEnableNetFix false");
                        SaveConfig();
                        RestartMap();
                        return;
                    }

                    // Found AnthChecker
                    break;
                }
            }
        }

        // Check if map is restarting
        if (Level.NextURL != "")
        {
            myLog("ServerCrashFix has not been loaded because the server is restarting");
            return;
        }

        // SCF should be loaded
        fixNative = Level.Spawn(class'SCFNative');
        fixNative.fixActor = self;
        fixNative.InitNative();
    }
}

// =============================================================================
// myLog ~ Log with [SCF] tag
// =============================================================================
function myLog(string msg)
{
    Log("[SCF]"@msg);
}

// =============================================================================
// RestartMap ~ Restart the current map
// =============================================================================
function RestartMap()
{
    Level.ServerTravel(Left(Mid(Level.GetLocalURL(),InStr(Level.GetLocalURL(),"/")+1),InStr(Mid(Level.GetLocalURL(),InStr(Level.GetLocalURL(),"/")+1),"?")),false);
}

// =============================================================================
// defaultproperties
// =============================================================================
defaultproperties
{
    bHidden=true
    bEnabled=true
    bFixNetDriver=true
    bFixExec=true
    bFixMalloc=true
    bFixHandlers=true
    bFixAffinity=false
    CPUAffinity=0
    bFixTimer=false
    bFixTickRate=false
    bForceUnhook=false
}
