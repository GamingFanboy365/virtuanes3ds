#ifndef _3DSEXIT_H_
#define _3DSEXIT_H_

#include <3ds.h>

extern aptHookCookie hookCookie;
extern int appExiting;

void setExitFlag(APT_HookType hook, void* param);
void enableExitHook();

// Use instead of aptMainLoop(): also sets appExiting when the system wants
// the app to close. libctru 2.x only signals that through aptMainLoop()'s
// return value; the APTHOOK_ONEXIT hook comes later, from aptExit().
bool app3dsMainLoop();

#endif
