
#include <algorithm>
#include <atomic>
#include <mutex>
#include <numeric>
#include <unordered_map>

#include "Console.h"
#include "Internal.h"

#include "modules/RenderTools.h"
#include "modules/Textures.h"

#include "Debug.h"
#include "PluginManager.h"
#include "VTableInterpose.h"

#include "df/coord.h"
#include "df/enabler.h"
#include "df/graphic.h"
#include "df/graphic_viewportst.h"
#include "df/renderer_2d.h"

#include "modules/Gui.h"

static const size_t NUM_VIEWPORTS = 9;

static df::coord framesafe_viewport_pos = df::coord(-1000, -1000, -1000);

namespace DFHack {
DBG_DECLARE(core, rendertools, DebugCategory::LINFO);
}

namespace RenderTools {

int getFramesafeScreentexposDepthAt(int32_t x, int32_t y, int32_t* (*accessor)(df::graphic_viewportst* vp))
{
    int vx = framesafe_viewport_pos.x;
    int vy = framesafe_viewport_pos.y;

    int local_x = x - vx;
    int local_y = y - vy;

    if (x < vx || x >= df::global::gps->viewport[0]->dim_x + vx ||
        y < vy || y >= df::global::gps->viewport[0]->dim_y + vy)
        return 0;

    for (int depth = 0; depth < NUM_VIEWPORTS; ++depth) {
        auto* vp = df::global::gps->viewport[depth];
        int32_t* buffer = accessor(vp);

        if (buffer && buffer[local_x * vp->dim_y + local_y])
            return depth;
    }
    return 0;
}

df::coord getFramesafeViewportPos(){
    return framesafe_viewport_pos;
}

int getDepthAt(int32_t x, int32_t y)
{
    int vx = framesafe_viewport_pos.x;
    int vy = framesafe_viewport_pos.y;

    int local_x = x - vx;
    int local_y = y - vy;

    if (x < vx || x >= df::global::gps->viewport[0]->dim_x + vx || y < vy || y >= df::global::gps->viewport[0]->dim_y + vy)
        return 0;

    const size_t num_viewports = 9;

    for (int depth = 0; depth < num_viewports; ++depth) {
        if (df::global::gps->viewport[depth]->screentexpos_background[local_x * df::global::gps->viewport[depth]->dim_y + local_y]) {
            return depth;
        }
    }
    return 0;
}

int getEdgeDepthAt(int32_t x, int32_t y)
{
    int vx = framesafe_viewport_pos.x;
    int vy = framesafe_viewport_pos.y;

    int local_x = x - vx;
    int local_y = y - vy;

    if (x < vx || x >= df::global::gps->viewport[0]->dim_x + vx || y < vy || y >= df::global::gps->viewport[0]->dim_y + vy)
        return 0;

    const size_t num_viewports = 9;

    for (int depth = 0; depth < num_viewports; ++depth) {
        if (df::global::gps->viewport[depth]->screentexpos_floor_flag[local_x * df::global::gps->viewport[depth]->dim_y + local_y]) {
            return depth;
        }
    }
    return 0;
}

df::graphic_viewportst* getDeepestGraphicsViewport(int z) {
    std::vector<df::graphic_viewportst*> vps = df::global::gps->viewport;
    int current_z = (int)*df::global::window_z;
    for (int depth = 0; depth < 9; ++depth) {
        if (current_z - depth == z) {
            return vps[depth];
        }
    }
    return nullptr;
}

struct RendererInterpose : df::renderer_2d {
    typedef df::renderer_2d interpose_base;

    DEFINE_VMETHOD_INTERPOSE(void, render, ()) {
        framesafe_viewport_pos = df::coord(*df::global::window_x, *df::global::window_y, *df::global::window_z);
        INTERPOSE_NEXT(render)();
    }
};

IMPLEMENT_VMETHOD_INTERPOSE(RendererInterpose, render);

static void install_render_point() {
    INTERPOSE_HOOK(RendererInterpose, render).apply();
}

static void uninstall_render_point() {
    INTERPOSE_HOOK(RendererInterpose, render).remove();
}

void init(DFHack::color_ostream& out) {
    if (!df::global::enabler) {
        return;
    }

    install_render_point();
    DEBUG(rendertools, out).print("render safe variables  ready, reserved range %d-%d\n");
}

void cleanup() {
    if (!df::global::enabler)
        return;
    
    uninstall_render_point();
}
}
