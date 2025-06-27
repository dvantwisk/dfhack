#include "PluginManager.h"
#include "Core.h"
#include "VTableInterpose.h"

#include <vector>
#include <string>

#include "df/enabler.h"
#include "df/graphic.h"
#include "df/graphic_viewportst.h"
#include "df/renderer.h"
#include "df/renderer_2d.h"

using namespace DFHack;

DFHACK_PLUGIN("submerge_buildings");

// Interpose to swap buildings layer with splatter layer if water / magma is present.
struct BuildingWaterInterpose : public df::renderer_2d {
    typedef renderer_2d interpose_base;

    DEFINE_VMETHOD_INTERPOSE(void, update_viewport_tile,
        (df::graphic_viewportst* vp, int32_t x, int32_t y)) 
    {
        int32_t liquid_val = vp->screentexpos_liquid_flag[x * vp->dim_y + y];

        if (liquid_val) {
            vp->screentexpos_spatter[x * vp->dim_y + y] = vp->screentexpos_building_one[x * vp->dim_y + y];
            vp->screentexpos_building_one[x * vp->dim_y + y] = 0;
        }

        INTERPOSE_NEXT(update_viewport_tile)(vp, x, y);
    }
};
IMPLEMENT_VMETHOD_INTERPOSE(BuildingWaterInterpose, update_viewport_tile);

DFhackCExport command_result plugin_shutdown(color_ostream &out)
{
    return CR_OK;
}

command_result submerge_buildings_cmd(color_ostream &out, std::vector<std::string> &args)
{
    bool enable = true;

    if (!args.empty()) {
        if (args[0] == "0" || args[0] == "false" || args[0] == "off" || args[0] == "disable")
            enable = false;
    }

    if (enable) {
        if (!INTERPOSE_HOOK(BuildingWaterInterpose, update_viewport_tile).apply(true)) {
            out.printerr("Failed to interpose update_viewport_tile.\n");
            return CR_FAILURE;
        }
        out.print("Interpose on submerge_buildings enabled.\n");
    } else {
        INTERPOSE_HOOK(BuildingWaterInterpose, update_viewport_tile).apply(false);
        out.print("Interpose on submerge_buildings disabled.\n");
    }

    return CR_OK;
}

DFhackCExport command_result plugin_init(color_ostream &out, std::vector<PluginCommand> &commands)
{
    commands.push_back(PluginCommand(
        "submerge_buildings",
        "Enable or disable submerged building interpose.",
        submerge_buildings_cmd));

    return CR_OK;
}