// =============================================================================
// ServerCrashFix v1.1 - (c) 2009-2010 AnthraX
// =============================================================================
// SCF Spec - tracks mapswitches
// =============================================================================
class SCFSpec extends MessagingSpectator;

// =============================================================================
// Variables
// =============================================================================
var SCFNative fixNative;

// =============================================================================
// PreClientTravel ~ Used for MapChange detection. Netdriver/DLO MUST be unhooked
// when the server reboots or it will crash
// =============================================================================
event PreClientTravel()
{
    if (fixNative != none)
        fixNative.MapSwitch();
}

// =============================================================================
// Disabled functions ~ Here to prevent Accessed Nones
// =============================================================================
event TeamMessage (PlayerReplicationInfo PRI, coerce string S, name Type, optional bool bBeep) {}
function ClientVoiceMessage(PlayerReplicationInfo Sender, PlayerReplicationInfo Recipient, name messagetype, byte messageID) {}
event ClientMessage( coerce string S, optional name Type, optional bool bBeep ) {}
