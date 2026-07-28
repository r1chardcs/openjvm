#ifndef JVM_INSPECTOR
#define JVM_INSPECTOR

#include "overlay.h"

Renderable GetRenderableJvmInspector();
Renderable GetRenderableUnhookMenu();

void RenderUnhookMenu(RenderContext context);
void RenderJvmInspector(RenderContext context);

#endif
