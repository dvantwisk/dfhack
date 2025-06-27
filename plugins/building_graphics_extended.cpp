#include "PluginManager.h"
#include "VTableInterpose.h"
#include "TileTypes.h"
#include "Core.h"
#include "DataDefs.h"

#include <iostream>
#include <unordered_map>

#include "df/building.h"
#include "df/building_actual.h"
#include "df/building_drawbuffer.h"
#include "df/building_display_furniturest.h"
#include "df/building_farmplotst.h"
#include "df/building_offering_placest.h"
#include "df/building_roadst.h"
#include "df/building_road_pavedst.h"
#include "df/building_traction_benchst.h"
#include "df/building_wellst.h"
#include "df/buildingitemst.h"
#include "df/descriptor_color.h"
#include "df/enabler.h"
#include "df/enabler_textures.h"
#include "df/graphic.h"
#include "df/graphic_viewportst.h"
#include "df/item.h"
#include "df/item_barst.h"
#include "df/item_blocksst.h"
#include "df/item_boulderst.h"
#include "df/item_crafted.h"
#include "df/item_toolst.h"
#include "df/item_tool_graphics_flag.h"
#include "df/item_tool_graphics_infost.h"
#include "df/item_traction_bench_graphics_flag.h"
#include "df/item_traction_bench_graphics_infost.h"
#include "df/item_woodst.h"
#include "df/itemdef_handlerst.h"
#include "df/itemdef_toolst.h"
#include "df/material.h"
#include "df/material_flags.h"
#include "df/palettest.h"
#include "df/renderer.h"
#include "df/renderer_2d.h"
#include "df/tile_pagest.h"
#include "df/texture_handlerst.h"
#include "df/world.h"
#include "df/world_geo_biome.h"

#include "df/map_block.h"
#include "df/world_data.h"
#include "df/world_region.h"
#include "df/world_geo_layer.h"
#include "df/region_map_entry.h"
#include "df/inorganic_raw.h"
#include "df/tiletype_material.h"
#include "df/tile_designation.h"
#include "df/geo_biome_type.h"

#include "modules/Buildings.h"
#include "modules/RenderTools.h"
#include "modules/Gui.h"
#include "modules/Maps.h"
#include "modules/Materials.h"
#include "modules/Textures.h"
#include "modules/DFSDL.h"

#include <SDL_surface.h>
#include <SDL_pixels.h>

DFHACK_PLUGIN("building_graphics_extended");

using namespace DFHack;
using namespace DFHack::DFSDL;
using namespace df;
using namespace df::enums;
using namespace Textures;

static std::vector<TexposHandle> set_pfloor;
static std::vector<TexposHandle> set_palt_floor;
static std::vector<TexposHandle> set_pfarmplot_planted;
static std::vector<TexposHandle> set_pfurrowed_soil;

struct FarmplotKey {
    int color;
    int build_stage;
    bool planted;

    bool operator==(const FarmplotKey& other) const {
        return color == other.color &&
               build_stage == other.build_stage &&
               planted == other.planted;
    }
};
namespace std {
    template <>
    struct hash<FarmplotKey> {
        size_t operator()(const FarmplotKey& key) const {
            size_t h1 = hash<int>()(key.color);
            size_t h2 = hash<int>()(key.build_stage);
            size_t h3 = hash<bool>()(key.planted);

            return h1 ^ (h2 << 1) ^ (h3 << 2);
        }
    };
}

struct BuildingKey {
    int color;
    int build_stage;

    bool operator==(const BuildingKey& other) const {
        return color == other.color && build_stage == other.build_stage;
    }
};

namespace std {
    template <>
    struct hash<BuildingKey> {
        std::size_t operator()(const BuildingKey& key) const {
            return std::hash<int>()(key.color) ^ (std::hash<int>()(key.build_stage) << 1);
        }
    };
}

static std::unordered_map<BuildingKey, TexposHandle> swapped_texpos_cache_paved_road;
static std::unordered_map<int, TexposHandle> swapped_texpos_cache_well;
static std::unordered_map<FarmplotKey, TexposHandle> swapped_texpos_cache_farmplot;
static std::unordered_map<int, TexposHandle> swapped_texpos_cache_furrow_soil;

static df::palettest base_organic_palette;
static df::palettest base_inorganic_palette;

static int32_t farmplot_planted_texpos_ref;
static int32_t farmplot_not_planted_texpos_ref;

struct WellRecolorBufferInterpose : df::building_wellst {
    typedef df::building_wellst interpose_base;

    DEFINE_VMETHOD_INTERPOSE(void, drawBuilding, (uint32_t a, df::building_drawbuffer* buf, int16_t b))
    {
        INTERPOSE_NEXT(drawBuilding)(a, buf, b);

        int z = *df::global::window_z - Gui::getDepthAt_graphical_treeless(x1, y1);

        auto *well = virtual_cast<building_wellst>(Buildings::findAtTile(df::coord(x1, y1, z)));

        // building is on a lower z-level
        if (well == nullptr) {
            return;
        }

        if (well->contained_items.empty()) {
            return;
        }

        auto base_item = well->contained_items[3]->item;
        auto ci = virtual_cast<df::item_blocksst>(base_item);

        DFHack::MaterialInfo item_mat = DFHack::MaterialInfo(ci->mat_type, ci->mat_index);

        if (!item_mat.isValid() || !item_mat.material) return;

        int32_t color = item_mat.material->state_color[0];

        TexposHandle swapped_texpos;

        auto it = swapped_texpos_cache_well.find(color);
        if (it != swapped_texpos_cache_well.end()) {
            swapped_texpos = it->second;
        } else {
            int32_t base_texpos = buf->building_one_texpos[0][0];
            SDL_Surface* base_surface = getSurfaceByTexpos(base_texpos);
            TexposHandle base_texture = loadTexture(base_surface);
            df::palettest material_palette = df::global::world->raws.descriptors.colors[color]->palette;
            swapped_texpos = swapPalettes(base_texture, base_inorganic_palette, material_palette);
            swapped_texpos_cache_well[color] = swapped_texpos;
        }

        buf->building_one_texpos[0][0] = getTexposByHandle(swapped_texpos);
    }
};
IMPLEMENT_VMETHOD_INTERPOSE(WellRecolorBufferInterpose, drawBuilding);

struct FarmRecolorBufferInterpose : df::building_farmplotst {
    typedef df::building_farmplotst interpose_base;

    DEFINE_VMETHOD_INTERPOSE(void, drawBuilding, (uint32_t a, df::building_drawbuffer* buf, int16_t b))
    {

        INTERPOSE_NEXT(drawBuilding)(a, buf, b);

        int z = *df::global::window_z - Gui::getDepthAt_graphical_treeless(x1, y1);

        auto *farm = virtual_cast<building_farmplotst>(Buildings::findAtTile(df::coord(x1, y1, z)));

        // building is on a lower z-level
        if (farm == nullptr) {
            return;
        }

        df::coord2d biome_region = Maps::getTileBiomeRgn(df::coord(x1, y1, z));
        df::region_map_entry *rb = Maps::getRegionBiome(biome_region);
        df::tile_designation *des = Maps::getTileDesignation(df::coord(x1, y1, z));

        int layer = des->bits.geolayer_index;
        int mat_idx = df::global::world->world_data->geo_biomes[rb->geo_index]->layers[layer]->mat_index;
        int color = df::global::world->raws.inorganics.all[mat_idx]->material.state_color[0];

        int building_stage = farm->construction_stage;

        TexposHandle swapped_texpos_no_plant;

        FarmplotKey fpk_no_plant = FarmplotKey(color, building_stage, false);

        auto it = swapped_texpos_cache_farmplot.find(fpk_no_plant);
        if (it != swapped_texpos_cache_farmplot.end()) {
            swapped_texpos_no_plant = it->second;
        } else {
            TexposHandle base_texture = set_palt_floor[15];
            df::palettest material_palette = df::global::world->raws.descriptors.colors[color]->palette;
            //int32_t base_texpos = buf->building_one_texpos[0][0];
            swapped_texpos_no_plant = swapPalettes(base_texture, base_inorganic_palette, material_palette);
            swapped_texpos_cache_farmplot[fpk_no_plant] = swapped_texpos_no_plant;
        }

        TexposHandle swapped_texpos_plant;

        FarmplotKey fpk_plant = FarmplotKey(color, building_stage, true);

        auto it2 = swapped_texpos_cache_farmplot.find(fpk_plant);
        if (it2 != swapped_texpos_cache_farmplot.end()) {
            swapped_texpos_plant = it2->second;
        } else {
            TexposHandle base_texture = set_pfarmplot_planted[0];
            df::palettest material_palette = df::global::world->raws.descriptors.colors[color]->palette;
            //int32_t base_texpos = buf->building_one_texpos[0][0];
            swapped_texpos_plant = swapPalettes(base_texture, base_inorganic_palette, material_palette);
            swapped_texpos_cache_farmplot[fpk_plant] = swapped_texpos_plant;
        }

        for (int x = buf->x1; x <= buf->x2; ++x) {
            for (int y = buf->y1; y <= buf->y2; ++y) {
                int local_x = x - buf->x1;
                int local_y = y - buf->y1;
                //std::cerr << "buf->building_one_texpos[local_x][local_y]" << buf->building_one_texpos[local_x][local_y] << "\n";
                if (buf->building_one_texpos[local_x][local_y] == farmplot_not_planted_texpos_ref) {
                    std::cerr << "FOUND NOT PLANTED\n";
                    buf->building_one_texpos[local_x][local_y] = getTexposByHandle(swapped_texpos_no_plant);
                }
                else if (buf->building_one_texpos[local_x][local_y] == farmplot_planted_texpos_ref) {
                    buf->building_one_texpos[local_x][local_y] = getTexposByHandle(swapped_texpos_plant);
                }
            }
        }
    }
};
IMPLEMENT_VMETHOD_INTERPOSE(FarmRecolorBufferInterpose, drawBuilding);

struct PavedRoadRecolorBufferInterpose : df::building_road_pavedst {
    typedef df::building_road_pavedst interpose_base;

    DEFINE_VMETHOD_INTERPOSE(void, drawBuilding, (uint32_t a, df::building_drawbuffer* buf, int16_t b))
    {
        INTERPOSE_NEXT(drawBuilding)(a, buf, b);

        int z = *df::global::window_z - Gui::getDepthAt_graphical_treeless(x1, y1);

        auto *road = virtual_cast<building_road_pavedst>(Buildings::findAtTile(df::coord(x1, y1, z)));

        // building is on a lower z-level
        if (road == nullptr) {
            return;
        }

        if (road->contained_items.empty()) {
            return;
        }

        auto base_item = road->contained_items[0]->item;

        DFHack::MaterialInfo item_mat;

        if (auto ci = virtual_cast<df::item_woodst>(base_item)) {
            item_mat = DFHack::MaterialInfo(ci->mat_type, ci->mat_index);
        } else if (auto ci = virtual_cast<df::item_barst>(base_item)) {
            item_mat = DFHack::MaterialInfo(ci->mat_type, ci->mat_index);
        } else if (auto ci = virtual_cast<df::item_boulderst>(base_item)) {
            item_mat = DFHack::MaterialInfo(ci->mat_type, ci->mat_index);
        } else if (auto ci = virtual_cast<df::item_blocksst>(base_item)) {
            item_mat = DFHack::MaterialInfo(ci->mat_type, ci->mat_index);
        }

        if (!item_mat.isValid() || !item_mat.material) return;

        int32_t color = item_mat.material->state_color[0];

        int build_stage = road->construction_stage;

        TexposHandle swapped_texpos;

        auto it = swapped_texpos_cache_paved_road.find(BuildingKey(color, build_stage));
        if (it != swapped_texpos_cache_paved_road.end()) {
            swapped_texpos = it->second;
        } else {
            df::palettest material_palette = df::global::world->raws.descriptors.colors[color]->palette;
            int32_t base_texpos = buf->building_one_texpos[0][0];
            SDL_Surface* base_surface = getSurfaceByTexpos(base_texpos);
            TexposHandle base_texture = loadTexture(base_surface);
            swapped_texpos = swapPalettes(base_texture, base_inorganic_palette, material_palette);
            swapped_texpos_cache_paved_road[BuildingKey(color, build_stage)] = swapped_texpos;
        }

        int32_t ref_texpos = buf->building_one_texpos[0][0];
        buf->building_one_texpos[0][0] = getTexposByHandle(swapped_texpos);

        for (int x = buf->x1; x <= buf->x2; ++x) {
            for (int y = buf->y1; y <= buf->y2; ++y) {
                int local_x = x - buf->x1;
                int local_y = y - buf->y1;
                if (buf->building_one_texpos[local_x][local_y] == ref_texpos) {
                    buf->building_one_texpos[local_x][local_y] = getTexposByHandle(swapped_texpos);
                }
            }
        }
    }
};
IMPLEMENT_VMETHOD_INTERPOSE(PavedRoadRecolorBufferInterpose, drawBuilding);

struct TractionBenchRecolorBufferInterpose : df::building_traction_benchst {
    typedef df::building_traction_benchst interpose_base;

    DEFINE_VMETHOD_INTERPOSE(void, drawBuilding, (uint32_t a, df::building_drawbuffer* buf, int16_t b))
    {
        INTERPOSE_NEXT(drawBuilding)(a, buf, b);

        int z = *df::global::window_z - Gui::getDepthAt_graphical_treeless(x1, y1);

        auto *bench = virtual_cast<building_traction_benchst>(Buildings::findAtTile(df::coord(x1, y1, z)));

        // building is on a lower z-level
        if (bench == nullptr) {
            return;
        }

        if (bench->contained_items.empty()) {
            return;
        }

        auto ci = virtual_cast<df::item_crafted>(bench->contained_items[0]->item);
        DFHack::MaterialInfo item_mat = DFHack::MaterialInfo(ci->mat_type, ci->mat_index);
        if (!item_mat.isValid() || !item_mat.material) return;

        int32_t color = item_mat.material->state_color[0];

        for (size_t i = 0; i < df::global::world->raws.itemdefs.traction_bench_graphics_info.size(); ++i) {
            auto iflags = df::global::world->raws.itemdefs.traction_bench_graphics_info[i]->flags;
            int item_color = (iflags.whole & df::item_traction_bench_graphics_flag::mask_color_index) >>
            df::item_traction_bench_graphics_flag::shift_color_index;
            if (item_color == color + 1) {
                buf->building_one_texpos[0][0] = df::global::world->raws.itemdefs.traction_bench_graphics_info[i]->texpos;
                break;
            }
        }
    }
};
IMPLEMENT_VMETHOD_INTERPOSE(TractionBenchRecolorBufferInterpose, drawBuilding);

struct OfferingPlaceRecolorBufferInterpose : df::building_offering_placest {
    typedef df::building_offering_placest interpose_base;

    DEFINE_VMETHOD_INTERPOSE(void, drawBuilding, (uint32_t a, df::building_drawbuffer* buf, int16_t b))
    {
        INTERPOSE_NEXT(drawBuilding)(a, buf, b);

        int z = *df::global::window_z - Gui::getDepthAt_graphical_treeless(x1, y1);

        auto *altar = virtual_cast<building_offering_placest>(Buildings::findAtTile(df::coord(x1, y1, z)));

        // building is on a lower z-level
        if (altar == nullptr) {
            return;
        }

        if (altar->contained_items.empty()) {
            return;
        }

        auto ci = virtual_cast<df::item_toolst>(altar->contained_items[0]->item);
        DFHack::MaterialInfo item_mat = DFHack::MaterialInfo(ci->mat_type, ci->mat_index);
        if (!item_mat.isValid() || !item_mat.material) return;

        int32_t color = item_mat.material->state_color[0];

        for (size_t i = 0; i < ci->subtype->graphics_info.size(); ++i) {
            auto iflags = ci->subtype->graphics_info[i]->flags;
            int item_color = (iflags.whole & df::item_tool_graphics_flag::mask_color_index) >>
                df::item_tool_graphics_flag::shift_color_index;
            if (item_color == color + 1) {
                buf->building_one_texpos[0][0] = ci->subtype->graphics_info[i]->texpos;
                break;
            }
        }
    }
};
IMPLEMENT_VMETHOD_INTERPOSE(OfferingPlaceRecolorBufferInterpose, drawBuilding);

struct DisplayFurnitureRecolorBufferInterpose : df::building_display_furniturest {
    typedef df::building_display_furniturest interpose_base;

    DEFINE_VMETHOD_INTERPOSE(void, drawBuilding, (uint32_t a, df::building_drawbuffer* buf, int16_t b))
    {

        INTERPOSE_NEXT(drawBuilding)(a, buf, b);

        int z = *df::global::window_z - Gui::getDepthAt_graphical_treeless(x1, y1);

        auto *display = virtual_cast<building_display_furniturest>(Buildings::findAtTile(df::coord(x1, y1, z)));
        if (display == nullptr) { // building is on a lower z-level
            return;
        }

        if (display->contained_items.empty()) {
            return;
        }

        auto ci = virtual_cast<df::item_toolst>(display->contained_items[0]->item);

        DFHack::MaterialInfo item_mat = DFHack::MaterialInfo(ci->mat_type, ci->mat_index);

        if (!item_mat.isValid() || !item_mat.material) return;

        int32_t color = item_mat.material->state_color[0];

        for (size_t i = 0; i < ci->subtype->graphics_info.size(); ++i) {
            auto iflags = ci->subtype->graphics_info[i]->flags;
            int item_color = (iflags.whole & df::item_tool_graphics_flag::mask_color_index) >>
                df::item_tool_graphics_flag::shift_color_index;
            if (item_color == color + 1) {
                buf->building_one_texpos[0][0] = ci->subtype->graphics_info[i]->texpos;
                break;
            }
        }
    }
};
IMPLEMENT_VMETHOD_INTERPOSE(DisplayFurnitureRecolorBufferInterpose, drawBuilding);

DFhackCExport command_result plugin_shutdown(color_ostream &out)
{
    return CR_OK;
}

command_result building_graphics_extended_cmd(color_ostream &out, std::vector<std::string> & args)
{
    bool enable = true;

    if (!args.empty()) {
        if (args[0] == "0" || args[0] == "false" || args[0] == "off" || args[0] == "disable")
            enable = false;
    }

    if (enable) {
        base_organic_palette = getBasicOrganicPalette();
        base_inorganic_palette = getBasicInorganicPalette();

        set_pfloor = palettizeTilePage("FLOORS", base_inorganic_palette, false);
        set_palt_floor = palettizeTilePage("ALT_FLOORS", base_inorganic_palette, false);
        set_pfarmplot_planted = palettizeTilePage("FARMPLOT_PLANTED", base_inorganic_palette, false);
        set_pfurrowed_soil = palettizeTilePage("FLOOR_FURROWED_SOIL", base_inorganic_palette, false);

        df::tile_pagest* farmplotplanted_tilepage = getTilePage("FARMPLOT_PLANTED");
        farmplot_planted_texpos_ref = farmplotplanted_tilepage->texpos[0];
        df::tile_pagest* farmplot_not_planted_tilepage = getTilePage("FLOORS");
        farmplot_not_planted_texpos_ref = farmplot_not_planted_tilepage->texpos[15];
        std::cerr << "farmplot_not_planted_texpos_ref: " << farmplot_not_planted_texpos_ref << "\n";

        if (!INTERPOSE_HOOK(TractionBenchRecolorBufferInterpose, drawBuilding).apply(true)) {
            out.printerr("Failed to interpose on TractionBenchRecolorBufferInterpose.\n");
            return CR_FAILURE;
        }
        if (!INTERPOSE_HOOK(OfferingPlaceRecolorBufferInterpose, drawBuilding).apply(true)) {
            out.printerr("Failed to interpose on OfferingPlaceRecolorBufferInterpose.\n");
            return CR_FAILURE;
        }
        if (!INTERPOSE_HOOK(DisplayFurnitureRecolorBufferInterpose, drawBuilding).apply(true)) {
            out.printerr("Failed to interpose on DisplayFurnitureRecolorBufferInterpose.\n");
            return CR_FAILURE;
        }
        if (!INTERPOSE_HOOK(PavedRoadRecolorBufferInterpose, drawBuilding).apply(true)) {
            out.printerr("Failed to interpose on PavedRoadRecolorBufferInterpose.\n");
            return CR_FAILURE;
        }
        if (!INTERPOSE_HOOK(WellRecolorBufferInterpose, drawBuilding).apply(true)) {
            out.printerr("Failed to interpose on WellRecolorBufferInterpose.\n");
            return CR_FAILURE;
        }
        if (!INTERPOSE_HOOK(FarmRecolorBufferInterpose, drawBuilding).apply(true)) {
            out.printerr("Failed to interpose on FarmRecolorBufferInterpose.\n");
            return CR_FAILURE;
        }
        out.print("Interposes for buldings_graphics_extra enabled.\n");
    } else {
        INTERPOSE_HOOK(TractionBenchRecolorBufferInterpose, drawBuilding).apply(false);
        INTERPOSE_HOOK(OfferingPlaceRecolorBufferInterpose, drawBuilding).apply(false);
        INTERPOSE_HOOK(DisplayFurnitureRecolorBufferInterpose, drawBuilding).apply(false);
        INTERPOSE_HOOK(PavedRoadRecolorBufferInterpose, drawBuilding).apply(false);
        INTERPOSE_HOOK(WellRecolorBufferInterpose, drawBuilding).apply(false);
        INTERPOSE_HOOK(FarmRecolorBufferInterpose, drawBuilding).apply(false);

        swapped_texpos_cache_paved_road.clear();
        swapped_texpos_cache_well.clear();
        swapped_texpos_cache_farmplot.clear();
        swapped_texpos_cache_furrow_soil.clear();
        set_pfloor.clear();
        set_palt_floor.clear();
        set_pfarmplot_planted.clear();
        set_pfurrowed_soil.clear();
        out.print("Interpose on draw_building disabled.\n");
    }
    return CR_OK;
}

DFhackCExport command_result plugin_init(color_ostream &out, std::vector<PluginCommand> &commands)
{
commands.push_back(PluginCommand(
    "building_graphics_extended",
    "Modifies and colors building graphics",
    building_graphics_extended_cmd
));

    return CR_OK;
}
