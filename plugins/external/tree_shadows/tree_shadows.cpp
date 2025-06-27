#include "PluginManager.h"
#include "VTableInterpose.h"
#include "TileTypes.h"
#include "TreeRecord.h"

#include "modules/Gui.h"
#include "modules/Maps.h"
#include "modules/Textures.h"
#include "modules/World.h"

#include "df/enabler.h"
#include "df/global_objects.h"
#include "df/graphic.h"
#include "df/graphic_viewportst.h"
#include "df/map_block_column.h"
#include "df/plant.h"
#include "df/renderer_2d.h"
#include "df/world.h"

#include <SDL_surface.h>
#include <SDL_pixels.h>

#include <cmath> 
#include <unordered_map>
#include <iostream>
#include <cstdlib>
#include <ctime>
#include <random>
#include <limits>

DFHACK_PLUGIN_IS_ENABLED(disabled);
DFHACK_PLUGIN("tree_shadows");

using namespace DFHack;
using namespace df;
using namespace DFHack::Textures;
using df::global::gps;

static std::unordered_map<df::coord, TreeRecord*> treerecord_index;
static std::unordered_map<int64_t, plant*> tree_index;

static int tile_x;
static int tile_y;
static int tile_z;
static int block_x;
static int block_y;

static std::unordered_map<df::coord, std::vector<TexposHandle>> block_cache;

static std::vector<std::vector<df::plant*>> tree_blocks;
static std::vector<TexposHandle> tree_shadow_texposhandles;

static std::vector<int32_t> NULL_leaves = {168, 51};
static std::vector<int32_t> N_leaves = {159};
static std::vector<int32_t> NE_leaves = {194, 207};
static std::vector<int32_t> NW_leaves = {195, 206};
static std::vector<int32_t> NS_leaves = {170, 196};
static std::vector<int32_t> NEW_leaves = {180, 218, 160};
static std::vector<int32_t> NES_leaves = {182, 219, 163};
static std::vector<int32_t> NWS_leaves = {183, 217, 162};
static std::vector<int32_t> NEWS_leaves = {171, 15, 3};
static std::vector<int32_t> E_leaves = {156};
static std::vector<int32_t> EW_leaves = {169, 197};
static std::vector<int32_t> ES_leaves = {192, 205};
static std::vector<int32_t> EWS_leaves = {181, 216, 161};
static std::vector<int32_t> W_leaves = {158};
static std::vector<int32_t> WS_leaves = {193, 204};
static std::vector<int32_t> S_leaves = {157};

// Create a random number generator
int getRandomInt() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_int_distribution<> dist(0, std::numeric_limits<int>::max());
    return dist(gen);
}

// 93 : tree trunk base
// 94 : trunk up from base (uni-direction)
// 115 : brances
// 116 : twigs
// 108 branch EW

static bool isTreePart(int32_t tt){
    return tt > 70 && tt < 227;
}

static int32_t getTileTypeWrapper(df::coord pos) {
    df::tiletype* tt = Maps::getTileType(pos);
    if (!tt) {
        std::cerr << "HIT BAD TTI IN DERIVE TEXTURES at x=" << pos.x << " y=" << pos.y << " z=" << pos.z << "\n";
        return 0;
    }
    return (int32_t)* tt;
}

// 8: N
// 7: E
// 6: W
// 5: S
// 4: NE
// 3: NW
// 2: SE
// 1: SW
static uint8_t checkTreePartDirs(int x, int y, int z) {
    uint8_t res = 0;
    //std::cerr << "Enter CheckTreeDir x=" << x << " y=" << y << " z=" << z << "\n";
    if (y > 0) {
        if (isTreePart(getTileTypeWrapper(df::coord(x, y-1, z)))) { //N
            res |= (1 << 3);
        }
    }
    if (x < df::global::world->map.x_count - 1) {
        if (isTreePart(getTileTypeWrapper(df::coord(x+1, y, z)))) { //E
            res |= (1 << 2);
        }
    }
    if (x > 0) {
        if (isTreePart(getTileTypeWrapper(df::coord(x-1, y, z)))) { //W
            res |= (1 << 1);
        }
    }
    if (y < df::global::world->map.y_count - 1) {
        if (isTreePart(getTileTypeWrapper(df::coord(x, y+1, z)))) { //S
            res |= 1;
        }
    }

    //std::cerr << "    Exit CheckTreeDir res=" << (int32_t)res << "\n";

    return res;
}

static TexposHandle getRandomLeafTexture(std::vector<int32_t> handles) {
    int32_t idx = handles[getRandomInt() % handles.size()];
    return tree_shadow_texposhandles[idx];
}

static TexposHandle deriveShadowTexture(df::coord pos, int x, int y, int z) {
    for (int height = z; height < z + 10; ++height) {
        if (height > tile_z - 1) break;
        //std::cerr << "Deriving texture at x=" << x << " y=" << y << " z=" << height << "\n";
        int32_t tti = getTileTypeWrapper(df::coord(x, y, height));
        if (isTreePart(tti)) {
            if (height - z > 1) {
                return getRandomLeafTexture(NULL_leaves);   // full leaf texture
            } else {      // cardinal directions for tree parts
                uint8_t dirs = checkTreePartDirs(x, y, height);
                switch(dirs) {
                    case 0: // lone leaves
                        return getRandomLeafTexture(NULL_leaves);
                        break;
                    case 1: // S
                        return getRandomLeafTexture(S_leaves);
                        break;
                    case 2: // W
                        return getRandomLeafTexture(W_leaves);
                        break;
                    case 3: // WS
                        return getRandomLeafTexture(WS_leaves);
                        break;
                    case 4: // E
                        return getRandomLeafTexture(E_leaves);
                        break;
                    case 5: // ES
                        return getRandomLeafTexture(ES_leaves);
                        break;
                    case 6: // EW
                        return getRandomLeafTexture(EW_leaves);
                        break;
                    case 7: // EWS
                        return getRandomLeafTexture(EWS_leaves);
                        break;
                    case 8: // N
                        return getRandomLeafTexture(N_leaves);
                        break;
                    case 9: // NS
                        return getRandomLeafTexture(NS_leaves);
                        break;
                    case 10: // NW
                        return getRandomLeafTexture(NW_leaves);
                        break;
                    case 11: // NWS
                        return getRandomLeafTexture(NWS_leaves);
                        break;
                    case 12: // NE
                        return getRandomLeafTexture(NE_leaves);
                        break;
                    case 13: // NES
                        return getRandomLeafTexture(NES_leaves);
                        break;
                    case 14: // NEW
                        return getRandomLeafTexture(NEW_leaves);
                        break;
                    case 15: // NEWS
                        return getRandomLeafTexture(NEWS_leaves);
                        break;
                }
            }
        }
    }
    return 0;
}

static TreeRecord* recordTree(df::plant* tree) {
    //std::cerr << "Recording tree at x=" << tree->pos.x << " y=" << tree->pos.y << " z=" << tree->pos.z << "\n";

    TreeRecord* tr = new TreeRecord();
    tr->pos = tree->pos;

    for (int z = tr->pos.z + tr->height - 1; z > tr->pos.z - 1; --z) {
        //std::cerr << "At recordTree z=" << z << "\n";
        for (int x = tr->pos.x - 3; x < tr->pos.x + 4; ++x) {
            if (x >= 0 && x < tile_x) {
                //std::cerr << "At recordTree x=" << x << "\n";
                for (int y = tr->pos.y - 3; y < tr->pos.y + 4; ++y) {
                    if (y >= 0 && y < tile_y) {
                        //std::cerr << "At recordTree y=" << y << "\n";
                        TexposHandle temp_th = deriveShadowTexture(tree->pos, x, y, z);
                        //std::cerr << "At recordTree shadow_map z=" << (z - tr->pos.z) << " x=" << (x - (tr->pos.x - 3)) << " y=" << (y - (tr->pos.y - 3)) << "\n";
                        //std::cerr << "    idx=" << (z - tr->pos.z) * 7 * 7 + (x - (tr->pos.x - 3)) * 7 + (y - (tr->pos.y - 3)) <<"\n";
                        tr->shadow_map[(z - tr->pos.z) * 7 * 7 + (x - (tr->pos.x - 3)) * 7 + (y - (tr->pos.y - 3))] = temp_th;
                    }
                }
            }
        }
    }
    return tr;
}

// Initialize the spatial tree table
int build_tree_index() {
    tile_x = df::global::world->map.x_count;
    tile_y = df::global::world->map.y_count;
    tile_z = df::global::world->map.z_count;
    block_x = df::global::world->map.x_count_block;
    block_y = df::global::world->map.y_count_block;

    int processed_blocks = 0;
    int processed_plants = 0;
    int processed_trees = 0;

    tree_blocks.assign(block_x * block_y, std::vector<df::plant*>());

    for (int x = 0; x < df::global::world->map.x_count_block; x += 3) {
        for (int y = 0; y < df::global::world->map.y_count_block; y += 3) {
            int block_trees_processed = 0;
            //processed_blocks++;
            for (auto plant : df::global::world->map.map_block_columns[x * block_y + y]->plants) {
                processed_plants++;
                
                if (plant->type == 0 || plant->type == 1) {
                    tree_blocks[(plant->pos.x / 16) * block_y + (plant->pos.y / 16)].push_back(plant);
                    if (plant->tree_info) treerecord_index[plant->pos] = recordTree(plant);
                    //processed_trees++;
                    //block_trees_processed++;
                }
            }
            //std::cerr << "Processed " << block_trees_processed << " trees in column=" << processed_blocks << " x=" << x << " y=" << y << "\n";
        }
    }


    if (false) {
    std::cerr << "Finished building tree index of size " << treerecord_index.size() << "\n";
    std::cerr << "    Processed " << processed_blocks << " mid blocks\n";
    for (int x = 0; x < tile_x; ++x) {
        for (int y = 0; y < tile_y; ++y) {
            int bsize = tree_blocks[x * block_y + y].size();
            std::cerr << "        In column at x=" << x << " y=" << y << " processed " << bsize << " trees\n";
        }
    }
    std::cerr << "    Processed " << processed_plants << " plants\n";
    std::cerr << "    Processed " << processed_trees << " trees\n";
    }

    return 1;
}


// Look for new trees at the end of the array
void update_trees(int n) {
    auto &tree_wet = df::global::world->plants.tree_wet;
    auto &tree_dry = df::global::world->plants.tree_dry;

    int ntrees = tree_wet.size() + tree_dry.size();

    // record newly added trees
    for (int i = tree_wet.size() - 1; i >= 0; --i) {
        if (treerecord_index.find(tree_wet[i]->pos) != treerecord_index.end()) {
            df::plant* plant = df::global::world->plants.tree_wet[i];
            df::coord ppos = plant->pos;
            tree_blocks[ppos.x/16 * df::global::world->map.y_count_block + ppos.y/16].push_back(plant);
            treerecord_index[ppos] = recordTree(plant);
        } else break;
    }
    for (int i = tree_dry.size() - 1; i >= 0; --i) {
        if (treerecord_index.find(tree_dry[i]->pos) != treerecord_index.end()) {
            df::plant* plant = df::global::world->plants.tree_dry[i];
            df::coord ppos = plant->pos;
            tree_blocks[ppos.x/16 * df::global::world->map.y_count_block + ppos.y/16].push_back(plant);
            treerecord_index[ppos] = recordTree(plant);
        } else break;
    }

    for(int i = 0; i < n; ++i) {
        int rand_idx = getRandomInt() % ntrees;
        if (rand_idx > tree_wet.size()) {
            rand_idx -= tree_wet.size();
            df::plant* plant = tree_dry[rand_idx];
            TreeRecord* tr = recordTree(plant);
            treerecord_index[plant->pos] = tr;
        }
        else {
            df::plant* plant = tree_wet[rand_idx];
            TreeRecord* tr = recordTree(plant);
            treerecord_index[plant->pos] = tr;
        }
    }
}

static void placeTreeShadows(TreeRecord* tr, df::graphic_viewportst* vp, cuboid vc) {
    df::coord vpos = getFramesafeViewportPos();

    //std::cerr << "At place tree x=" << tr->pos.x << " y=" << tr->pos.y << " z=" << tr->pos.z << "\n";

    int tz = vpos.z - tr->pos.z;
    if (tz < 0) {
        tz = 0;
    }
    if (tz > 9) return;
    for (int i = tr->pos.x - 3; i < tr->pos.x + 4; ++i) {
        if (i >= vpos.x && i < vpos.x + vp->dim_x) {
            for (int j = tr->pos.y - 3; j < tr->pos.y + 4; ++j) {
                if (j >= vpos.y && j < vpos.y + vp->dim_y) {
                    TexposHandle th = tr->shadow_map[tz * 7 * 7 + (i - (tr->pos.x - 3)) * 7 + (j - (tr->pos.y - 3))];
                    if (
                        !vp->screentexpos_designation[(i - vpos.x) * vp->dim_y + (j - vpos.y)]
                    ) {
                        vp->screentexpos_designation[(i - vpos.x) * vp->dim_y + (j - vpos.y)] = getTexposByHandle(th);
                    }
                } 
            }
        }
    }
}

// Interpose wrapper for df::renderer
struct TreeRendererInterpose : public renderer_2d {
    typedef renderer_2d interpose_base;

    DEFINE_VMETHOD_INTERPOSE(void, update_full_viewport,
        (graphic_viewportst* vp)) {

        df::coord vpos = getFramesafeViewportPos();

        if (vp == df::global::gps->main_viewport) {
            //update_trees(1);

            int processed_trees = 0;

            cuboid vcuboid_depth = cuboid(
                std::max(vpos.x - 4, 0),
                std::max(vpos.y - 4, 0),
                vpos.z,
                std::max(vpos.x + 4 + vp->dim_x - 1, vp->dim_x - 1),
                std::max(vpos.y + 4 + vp->dim_y - 1, vp->dim_y - 1),
                vpos.z + 5
            );

//std::cerr << "Init Vcuboid_depth is xmin=" << vcuboid_depth.x_min << " xmax=" << vcuboid_depth.x_max << " ymin=" << vcuboid_depth.y_min << " ymax=" << vcuboid_depth.y_max << "\n";

            for (int x = vcuboid_depth.x_min / 16; x < (vcuboid_depth.x_max / 16) + 1; ++x) {
                if (x > block_x - 1) continue;
                for (int y = vcuboid_depth.y_min / 16; y < (vcuboid_depth.y_max / 16) + 1; ++y) {
                    if (y > block_y - 1) continue;
                    //std::cerr << "In loop looking at column x=" << x << " y=" << y << "\n";
                    for (int i = 0; i < tree_blocks[x * block_y + y].size(); ++i) {
                        df::plant* p = tree_blocks[x * block_y + y][i];
                        df::plant* p_check = Maps::getPlantAtTile(p->pos);
                        if (p_check && vcuboid_depth.containsPos(p->pos)) {
                            //int rand_idx = getRandomInt();
                            //if (rand_idx % 10000 == 0) {
                            //    TreeRecord* tr = recordTree(p);
                            //    treerecord_index[p->pos] = tr;
                            //}
                            if (p->tree_info) {
                                if (!treerecord_index[p->pos]) {
                                    TreeRecord* tr = recordTree(p);
                                    treerecord_index[p->pos] = tr;
                                }
                                placeTreeShadows(treerecord_index[p->pos], vp, vcuboid_depth);
                            }
                            //processed_trees += 1;
                        } else {
                            //std::cerr << "Did not include tree at pos x=" << p->pos.x << " y=" << p->pos.y << " z=" << p->pos.z << "\n";
                            if (!p_check) {
                                tree_blocks[x * df::global::world->map.y_count_block + y].erase(tree_blocks[x * df::global::world->map.y_count_block + y].begin() + i);
                                treerecord_index.erase(p->pos);
                                //std::cerr << "Erased tree record at pos x=" << p->pos.x << " y=" << p->pos.y << " z=" << p->pos.z << "\n";
                            }
                        }
                    }
                }
            }
        }

        INTERPOSE_NEXT(update_full_viewport)(vp);
    }
};
IMPLEMENT_VMETHOD_INTERPOSE(TreeRendererInterpose, update_full_viewport);

command_result plugin_enable(color_ostream &out, std::vector<std::string> & args)
{
    bool enable = true;

    if (!args.empty()) {
        if (args[0] == "0" || args[0] == "false" || args[0] == "off" || args[0] == "disable")
            enable = false;
    }

    if (enable) {
        SDL_Color dark_color = { 49, 44, 52, 255 };
        tree_shadow_texposhandles = tintTilePage("TREES", &dark_color, 0.15f);

        if (!build_tree_index()) {
            out.printerr("Failed to create initial tree registry\n");
            return CR_FAILURE;
        }
        if (!INTERPOSE_HOOK(TreeRendererInterpose, update_full_viewport).apply(true)) {
            out.printerr("Failed to interpose update_viewport_tile\n");
            return CR_FAILURE;
        }
        out.print("Interpose on update_viewport_tile enabled.\n");
    } else {
        INTERPOSE_HOOK(TreeRendererInterpose, update_full_viewport).apply(false);
        out.print("Interpose on update_viewport_tile disabled.\n");
        tree_blocks.clear();
        treerecord_index.clear();

    }
    return CR_OK;
}

DFhackCExport command_result plugin_onupdate(color_ostream &out)
{
    return CR_OK;
}

// Plugin load and unload
DFhackCExport command_result plugin_init(color_ostream &out, std::vector<PluginCommand> &commands)
{
    commands.push_back(PluginCommand(
       "tree_shadows",
        "Tree shadow rendering",
        plugin_enable
    ));

    return CR_OK;
}

DFhackCExport command_result plugin_shutdown(color_ostream &out)
{
    tree_index.clear();
    treerecord_index.clear();
    return CR_OK;
}

DFhackCExport command_result plugin_onstatechange(color_ostream &out, state_change_event event)
{
    if (event == SC_WORLD_UNLOADED) {
        tree_index.clear();
        treerecord_index.clear();
    }
    return CR_OK;
}

