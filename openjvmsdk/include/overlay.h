#ifndef OVERLAY
#define OVERLAY

#include <common_typedef.h>

typedef struct {
    PV HGLRC;
    PV HDC;
    PV HWND;
}RenderContext;

typedef void (*RenderCallback)(RenderContext context);

typedef struct {
    RenderCallback callback = nullptr;
}Renderable;

void SetRenderable(Renderable renderable);
void InitializeOverlay();
void DestroyOverlay();

#endif
