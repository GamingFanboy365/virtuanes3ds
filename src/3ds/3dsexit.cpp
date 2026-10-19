#include <stdlib.h>
#include <stdio.h>
#include <string.h>
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

#ifdef HWTEST3DS
//--------------------------------------------------------
// make HWTEST=1: simulates the HOME menu events that Azahar
// can't produce, as sdmc:/hwtest.txt asks ("suspend N",
// "close-game N" or "close-menu N": after N main loop calls
// in a game or the in-game menu). See tools/azahar/hwpathtest.py.
//
// suspend:  the suspend hook, then the restore hook 2 seconds
//           later, as for pressing HOME and returning.
// close-*:  what libctru does when the HOME menu closes the
//           app: the suspend hook, the GPU released, and then
//           aptMainLoop() returning false (with no restore).
// Returns true once the app should see itself being closed.
//--------------------------------------------------------
static bool hwtestSimulatedClose()
{
    static int mode = -1;       // -1: not read yet, 0: nothing to do
    static int target = 0, calls = 0, restoreAt = 0;
    static bool closed = false;

    if (mode < 0)
    {
        mode = 0;
        char name[32];
        FILE *fp = fopen("sdmc:/hwtest.txt", "r");
        if (fp)
        {
            if (fscanf(fp, "%31s %d", name, &target) == 2)
                mode = !strcmp(name, "suspend") ? 1 : !strcmp(name, "close-game") ? 2 : !strcmp(name, "close-menu") ? 3 : 0;
            fclose(fp);
        }
        TRACE3DS_LOG("hwtest: mode %d after %d calls", mode, target);
    }
    if (mode == 0 || closed)
        return closed;

    int state = mode == 3 ? EMUSTATE_PAUSEMENU : EMUSTATE_EMULATE;
    if (emulator.emulatorState == state && ++calls == target)
    {
        TRACE3DS_LOG("hwtest: HOME pressed, mixed %d", snd3dsMixCount);
        setExitFlag(APTHOOK_ONSUSPEND, NULL);
        if (mode == 1)
            restoreAt = calls + 120;
        else
        {
            TRACE3DS_LOG("hwtest: closing from the HOME menu");
            GSPGPU_ReleaseRight();
            closed = true;
        }
    }
    else if (mode == 1 && calls == restoreAt && emulator.emulatorState == state)
    {
        TRACE3DS_LOG("hwtest: back to the game, mixed %d", snd3dsMixCount);
        setExitFlag(APTHOOK_ONRESTORE, NULL);
    }
    else if (mode == 1 && restoreAt && calls == restoreAt + 120)
        TRACE3DS_LOG("hwtest: 2 seconds after returning, mixed %d", snd3dsMixCount);
    return closed;
}

//--------------------------------------------------------
// make HWTEST=1 links every GPU wait here (-Wl,--wrap). On a
// 3DS, waiting for a GPU event without owning the GPU (after
// the HOME menu took it to close the app) never returns, but
// Azahar signals the event anyway. Hang like the 3DS, so
// hwpathtest.py catches a wait without the gspHasGpuRight()
// check.
//--------------------------------------------------------
extern "C" void __real_gspWaitForEvent(GSPGPU_Event id, bool nextEvent);
extern "C" void __wrap_gspWaitForEvent(GSPGPU_Event id, bool nextEvent)
{
    if (!gspHasGpuRight())
    {
        TRACE3DS_LOG("hwtest: waiting for GPU event %d without the GPU, which never returns on a 3DS", (int)id);
        while (true)
            svcSleepThread(1000000000LL);
    }
    __real_gspWaitForEvent(id, nextEvent);
}
#endif

bool app3dsMainLoop()
{
    bool running = aptMainLoop();
#ifdef HWTEST3DS
    if (hwtestSimulatedClose())
        running = false;
#endif
    if (!running && !appExiting)
    {
        TRACE3DS_LOG("system asked to close");
        appExiting = 1;
    }
    return !appExiting;
}

void enableExitHook() {
    aptHook(&hookCookie, setExitFlag, NULL);
}
