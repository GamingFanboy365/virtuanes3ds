#include <stdlib.h>
#include <3ds.h>

#include "3dsexit.h"
#include "3dsemu.h"
#include "3dsmain.h"
#include "3dssound.h"
#include "3dstrace.h"

aptHookCookie hookCookie;
int appExiting = 0;

void setExitFlag(APT_HookType hook, void* param)
{
    TRACE3DS_LOG("APT hook %d", (int)hook);
    switch (hook)
    {
        case APTHOOK_ONEXIT:
            appExiting = 1;
            break;

        // The 3DS keeps playing the sound buffer on a loop until it's told
        // to stop, so silence it while the HOME menu is up or the lid is
        // closed, and restart it when the game comes back.
        case APTHOOK_ONSUSPEND:
        case APTHOOK_ONSLEEP:
            snd3dsStopPlaying();
            break;

        // The system also wakes the app this way to close it: no sound then.
        case APTHOOK_ONRESTORE:
        case APTHOOK_ONWAKEUP:
            if (emulator.emulatorState == EMUSTATE_EMULATE && !appExiting && !aptShouldClose())
                snd3dsStartPlaying();
            break;

        default:
            break;
    }
}

bool app3dsMainLoop()
{
    if (!aptMainLoop() && !appExiting)
    {
        TRACE3DS_LOG("system asked to close");
        appExiting = 1;
    }
    return !appExiting;
}

void enableExitHook() {
    aptHook(&hookCookie, setExitFlag, NULL);
}
