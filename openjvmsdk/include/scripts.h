#ifndef SCRIPTS
#define SCRIPTS

#include <scriptsdk.h>
#include "overlay.h"

PTargetScript GetScriptByID(int ID);
PTargetScript BaseScript();

Renderable GetRenderableScriptMenu();
void RenderScriptMenu(RenderContext context);
const char* CheckAvailableAddress(const char* source);

#endif