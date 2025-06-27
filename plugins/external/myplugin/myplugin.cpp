#include "PluginManager.h"
#include "VTableInterpose.h"
#include "TileTypes.h"
#include "Core.h"
#include "DataDefs.h"
#include "LRUCache.h"

#include <iostream>
#include <unordered_map>

#include "df/block_square_event.h"
#include "df/block_square_event_mineralst.h"
#include "df/block_square_event_type.h"
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
#include "df/engraving.h"
#include "df/event_handlerst.h"
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
#include "df/tile_bitmask.h"
#include "df/tile_designation.h"
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
#include "modules/Gui.h"
#include "modules/MapCache.h"
#include "modules/Maps.h"
#include "modules/Materials.h"
#include "modules/Textures.h"
#include "modules/DFSDL.h"

#include <SDL_surface.h>
#include <SDL_pixels.h>

#ifndef BITS

#define BITS

#define BIT1 1
#define BIT2 2
#define BIT3 4
#define BIT4 8
#define BIT5 16
#define BIT6 32
#define BIT7 64
#define BIT8 128
#define BIT9 256
#define BIT10 512
#define BIT11 1024
#define BIT12 2048
#define BIT13 4096
#define BIT14 8192
#define BIT15 16384
#define BIT16 32768
#define BIT17 65536UL
#define BIT18 131072UL
#define BIT19 262144UL
#define BIT20 524288UL
#define BIT21 1048576UL
#define BIT22 2097152UL
#define BIT23 4194304UL
#define BIT24 8388608UL
#define BIT25 16777216UL
#define BIT26 33554432UL
#define BIT27 67108864UL
#define BIT28 134217728UL
#define BIT29 268435456UL
#define BIT30 536870912UL
#define BIT31 1073741824UL
#define BIT32 2147483648UL
#define BIT33 4294967296ULL
#define BIT34 8589934592ULL
#define BIT35 17179869184ULL
#define BIT36 34359738368ULL
#define BIT37 68719476736ULL
#define BIT38 137438953472ULL
#define BIT39 274877906944ULL
#define BIT40 549755813888ULL
#define BIT41 1099511627776ULL
#define BIT42 2199023255552ULL
#define BIT43 4398046511104ULL
#define BIT44 8796093022208ULL
#define BIT45 17592186044416ULL
#define BIT46 35184372088832ULL
#define BIT47 70368744177664ULL
#define BIT48 140737488355328ULL
#define BIT49 281474976710656ULL
#define BIT50 562949953421312ULL
#define BIT51 1125899906842624ULL
#define BIT52 2251799813685248ULL
#define BIT53 4503599627370496ULL
#define BIT54 9007199254740992ULL
#define BIT55 18014398509481984ULL
#define BIT56 36028797018963968ULL
#define BIT57 72057594037927936ULL
#define BIT58 144115188075855872ULL
#define BIT59 288230376151711744ULL
#define BIT60 576460752303423488ULL
#define BIT61 1152921504606846976ULL
#define BIT62 2305843009213693952ULL
#define BIT63 4611686018427387904ULL
#define BIT64 9223372036854775808ULL
#endif

using namespace DFHack::Textures;

static int BLOCK_X = df::global::world->map.x_count_block;
static int BLOCK_Y = df::global::world->map.y_count_block;

enum ViewChangeType
{
    NONE,
    NO_CHANGE,
    SHIFT,
    ZOOM
};
typedef uint8_t ViewChange;

enum EdgingType
{
	EDGING_NONE,//0, must be here because of bit shift stuff
	EDGING_GRASS,
	EDGING_SOIL,
	EDGING_SOIL_SAND,
	EDGING_SOIL_SAND_YELLOW,
	EDGING_SOIL_SAND_WHITE,
	EDGING_SOIL_SAND_BLACK,
	EDGING_SOIL_SAND_RED,
	EDGING_STONE,
	EDGING_CUSTOM_GRASS_1,
	EDGING_CUSTOM_GRASS_2,
	EDGING_CUSTOM_GRASS_3,
	EDGING_CUSTOM_GRASS_4,
	EDGING_CUSTOM_GRASS_5,
	EDGING_CUSTOM_GRASS_6,
	EDGING_CUSTOM_GRASS_7,
	EDGING_CUSTOM_GRASS_8,
	EDGING_CUSTOM_GRASS_9,
	EDGING_CUSTOM_GRASS_10,
	EDGING_CUSTOM_GRASS_11,
	EDGING_CUSTOM_GRASS_12,
	EDGING_CUSTOM_GRASS_13,
	EDGING_CUSTOM_GRASS_14,
	EDGING_CUSTOM_GRASS_15,
	EDGING_CUSTOM_GRASS_16,
	EDGING_CUSTOM_GRASS_17,
	EDGING_CUSTOM_GRASS_18,
	EDGING_CUSTOM_GRASS_19,
	EDGING_CUSTOM_GRASS_20,
	EDGING_CUSTOM_GRASS_21,
	EDGING_CUSTOM_GRASS_22,
	EDGING_CUSTOM_GRASS_23,
	EDGING_CUSTOM_GRASS_24,
	EDGING_CUSTOM_GRASS_25,
	EDGING_CUSTOM_GRASS_26,
	EDGING_CUSTOM_GRASS_27,
	EDGING_CUSTOM_GRASS_28,
	EDGING_CUSTOM_GRASS_29,
	EDGING_CUSTOM_GRASS_30,
	EDGING_CUSTOM_GRASS_31,
	EDGING_CUSTOM_GRASS_32,
	EDGING_UNUSED_36//max=255
};
typedef uint32_t Edging;

#define VIEWPORT_FLOOR_FLAG_S_EDGING (BIT1|BIT2|BIT3|BIT4|BIT5|BIT6|BIT7|BIT8)
	#define VIEWPORT_FLOOR_FLAG_S_EDGING_SHIFT 0
#define VIEWPORT_FLOOR_FLAG_W_EDGING (BIT9|BIT10|BIT11|BIT12|BIT13|BIT14|BIT15|BIT16)
	#define VIEWPORT_FLOOR_FLAG_W_EDGING_SHIFT 8
#define VIEWPORT_FLOOR_FLAG_E_EDGING (BIT17|BIT18|BIT19|BIT20|BIT21|BIT22|BIT23|BIT24)
	#define VIEWPORT_FLOOR_FLAG_E_EDGING_SHIFT 16
#define VIEWPORT_FLOOR_FLAG_N_EDGING (BIT25|BIT26|BIT27|BIT28|BIT29|BIT30|BIT31|BIT32)
	#define VIEWPORT_FLOOR_FLAG_N_EDGING_SHIFT 24
#define VIEWPORT_FLOOR_FLAG_SPECIAL_TEXTURE (BIT33|BIT34|BIT35)
	#define VIEWPORT_FLOOR_FLAG_SOIL_BACKGROUND_1 BIT33
	#define VIEWPORT_FLOOR_FLAG_SOIL_BACKGROUND_2 BIT33|BIT34
	#define VIEWPORT_FLOOR_FLAG_SOIL_BACKGROUND_3 BIT33|BIT35
	#define VIEWPORT_FLOOR_FLAG_SOIL_BACKGROUND_4 BIT33|BIT34|BIT35

enum GraphicsRampType
{
	GRAPHICS_RAMP_NOTHING,//must put 0 here so ramp_flag==0 does cause trouble in derive
	GRAPHICS_RAMP_GRASS,
	GRAPHICS_RAMP_SOIL,
	GRAPHICS_RAMP_STONE,
	GRAPHICS_RAMP_FROZEN,
	GRAPHICS_RAMP_CONSTRUCTED_WOODEN,
	GRAPHICS_RAMP_CONSTRUCTED_STONE,
	GRAPHICS_RAMP_CONSTRUCTED_METAL,
	GRAPHICS_RAMP_CUSTOM_GRASS_1,
	GRAPHICS_RAMP_CUSTOM_GRASS_2,
	GRAPHICS_RAMP_CUSTOM_GRASS_3,
	GRAPHICS_RAMP_CUSTOM_GRASS_4,
	GRAPHICS_RAMP_CUSTOM_GRASS_5,
	GRAPHICS_RAMP_CUSTOM_GRASS_6,
	GRAPHICS_RAMP_CUSTOM_GRASS_7,
	GRAPHICS_RAMP_CUSTOM_GRASS_8,
	GRAPHICS_RAMP_CUSTOM_GRASS_9,
	GRAPHICS_RAMP_CUSTOM_GRASS_10,
	GRAPHICS_RAMP_CUSTOM_GRASS_11,
	GRAPHICS_RAMP_CUSTOM_GRASS_12,
	GRAPHICS_RAMP_CUSTOM_GRASS_13,
	GRAPHICS_RAMP_CUSTOM_GRASS_14,
	GRAPHICS_RAMP_CUSTOM_GRASS_15,
	GRAPHICS_RAMP_CUSTOM_GRASS_16,
	GRAPHICS_RAMP_CUSTOM_GRASS_17,
	GRAPHICS_RAMP_CUSTOM_GRASS_18,
	GRAPHICS_RAMP_CUSTOM_GRASS_19,
	GRAPHICS_RAMP_CUSTOM_GRASS_20,
	GRAPHICS_RAMP_CUSTOM_GRASS_21,
	GRAPHICS_RAMP_CUSTOM_GRASS_22,
	GRAPHICS_RAMP_CUSTOM_GRASS_23,
	GRAPHICS_RAMP_CUSTOM_GRASS_24,
	GRAPHICS_RAMP_CUSTOM_GRASS_25,
	GRAPHICS_RAMP_CUSTOM_GRASS_26,
	GRAPHICS_RAMP_CUSTOM_GRASS_27,
	GRAPHICS_RAMP_CUSTOM_GRASS_28,
	GRAPHICS_RAMP_CUSTOM_GRASS_29,
	GRAPHICS_RAMP_CUSTOM_GRASS_30,
	GRAPHICS_RAMP_CUSTOM_GRASS_31,
	GRAPHICS_RAMP_CUSTOM_GRASS_32,
	GRAPHICS_UNUSED_40	//max 255
};

#define VIEWPORT_RAMP_FLAG_TYPE (BIT1|BIT2|BIT3|BIT4|BIT5|BIT6|BIT7|BIT8)
#define VIEWPORT_RAMP_FLAG_WALL_N BIT9
#define VIEWPORT_RAMP_FLAG_WALL_W BIT10
#define VIEWPORT_RAMP_FLAG_WALL_E BIT11
#define VIEWPORT_RAMP_FLAG_WALL_S BIT12
#define VIEWPORT_RAMP_FLAG_WALL_NW BIT13
#define VIEWPORT_RAMP_FLAG_WALL_NE BIT14
#define VIEWPORT_RAMP_FLAG_WALL_SW BIT15
#define VIEWPORT_RAMP_FLAG_WALL_SE BIT16
#define VIEWPORT_RAMP_FLAG_N_IS_DARK_CORNER BIT17
#define VIEWPORT_RAMP_FLAG_S_IS_DARK_CORNER BIT18
#define VIEWPORT_RAMP_FLAG_W_IS_DARK_CORNER BIT19
#define VIEWPORT_RAMP_FLAG_E_IS_DARK_CORNER BIT20
#define VIEWPORT_RAMP_FLAG_N_IS_OPEN_AIR BIT21
#define VIEWPORT_RAMP_FLAG_S_IS_OPEN_AIR BIT22
#define VIEWPORT_RAMP_FLAG_W_IS_OPEN_AIR BIT23
#define VIEWPORT_RAMP_FLAG_E_IS_OPEN_AIR BIT24
#define VIEWPORT_RAMP_FLAG_SHOW_UP_ARROW BIT25
#define VIEWPORT_RAMP_FLAG_SHOW_DOWN_ARROW BIT26
#define VIEWPORT_RAMP_FLAG_COLOR_INDEX_BITS (BIT27|BIT28|BIT29|BIT30|BIT31|BIT32|BIT33|BIT34)
	#define VIEWPORT_RAMP_FLAG_COLOR_INDEX_SHIFT 26

DFHACK_PLUGIN("myplugin");

using namespace DFHack;
using namespace DFHack::DFSDL;
using namespace df;
using namespace df::enums;
using namespace Textures;

static int WORLD_TILE_X;
static int WORLD_TILE_Y;
static int WORLD_TILE_Z;

static df::palettest base_organic_palette;
static df::palettest base_inorganic_palette;

int64_t hash_coord(int x, int y, int z) {
    // Use a simple bit-packing hash (similar to DFHack's coord_key)
    return ((int64_t)z << 40) | ((int64_t)y << 20) | x;
}

int pseudo_rand(int64_t seed, int salt) {
    seed ^= salt * 0x5bd1e995;
    seed ^= (seed >> 24);
    seed = seed * 0x27d4eb2d;
    return (int)(seed % 7) - 3;  // Yields a value in [-3, 3]
}

void apply_tile_variation(df::coord pos, uint8_t &r, uint8_t &g, uint8_t &b) {
    int64_t key = hash_coord(pos.x, pos.y, pos.z);
    r = std::clamp((int)r + pseudo_rand(key, 1), 0, 255);
    g = std::clamp((int)g + pseudo_rand(key, 2), 0, 255);
    b = std::clamp((int)b + pseudo_rand(key, 3), 0, 255);
}

struct TilePaletteKey {
    uint16_t type;
    df::palettest palette; // Assuming df_palette is defined elsewhere
    uint64_t flag;

    bool operator==(const TilePaletteKey &other) const {
        return type == other.type &&
               flag == other.flag &&
               std::memcmp(palette.color, other.palette.color, sizeof(palette.color)) == 0;
    }
};

namespace std {
    template <>
    struct hash<TilePaletteKey> {
        size_t operator()(const TilePaletteKey &key) const {
            size_t h1 = std::hash<uint16_t>{}(key.type);
            size_t h2 = std::hash<uint64_t>{}(key.flag);

            // Hash the palette color bytes
            size_t h3 = 0;
            for (size_t i = 0; i < sizeof(key.palette.color); ++i) {
                h3 ^= std::hash<uint8_t>{}(key.palette.color[i]) + 0x9e3779b9 + (h3 << 6) + (h3 >> 2);
            }

            // Combine hashes
            size_t result = h1;
            result ^= h2 + 0x9e3779b9 + (result << 6) + (result >> 2);
            result ^= h3 + 0x9e3779b9 + (result << 6) + (result >> 2);
            return result;
        }
    };
}

// TileKey
//   type -> current square TileType
//   color -> current square color
//   flag -> current square flag (edges and their tiletypes)

// EdgeKey
//   color -> current square color (edges and their colors)
//   flag -> current square flag (edges and their tiletypes)

// NewTileKey
//   uint16_t type -> current square TileType
//   uint16_t color -> current square color
//   uint64_t edge_color -> current square color (edges and their colors)
//   uint64_t flag -> current square flag (edges and their tiletypes) -> at 64th bit, store whether value is for diagonal edge

// First hit:
//   Calculate:
//     TileType of square;
//     Color of square;
//   Result:
//     Match (hit cache at <tile, color, 0, 0>):
//       If true, Check if square's flag is 0; if true, just caculate custom color edges; if false, do both.
//       Calculate:
//         Tiletype of surrounding squares;
//         Color of surrounding squares;
//           Match (hit cache at <tile, color, edge_color, flag>):
//             If true, return cache at <tile, color, edge_color, flag>.
//             If false, get cached square value and calculate new edges, store edges and combined value.
//       If false, Check if square's flag is 0; if true, just caculate square; if false, do both.
//       Calculate:
//         Tiletype of surrounding squares;
//         Color of surrounding squares;
//           Match (hit cache at <0, 0, edge_color, flag>):
//             If true, return cache at <0, 0, edge_color, flag>, calcuate square and store square and combined value.
//             If false, calculate everything, cache square, edges and combined values.
    
//    tile, color, 0, 0 ----> stores base color; no edges, check if color and flag info.
//  First extended hit: tile, color, 0, 0 ----> stores base color; no edges, check if color and flag info.

struct CompTileKey {
    uint16_t type;
    uint16_t color;
    uint64_t edge_color;
    uint64_t flag;

    bool operator==(const CompTileKey &other) const {
        return type == other.type &&
               color == other.color &&
               edge_color == other.edge_color &&
               flag == other.flag;
    }
};

namespace std {
    template<>
    struct hash<CompTileKey> {
        std::size_t operator()(const CompTileKey &key) const {
            std::size_t h1 = std::hash<uint16_t>()(key.type);
            std::size_t h2 = std::hash<uint16_t>()(key.color);
            std::size_t h3 = std::hash<uint64_t>()(key.edge_color);
            std::size_t h4 = std::hash<uint64_t>()(key.flag);
            return ((h1 ^ (h2 << 1)) >> 1) ^ (h3 << 1) ^ (h4 << 2);
        }
    };
}

struct TileKey {
    uint16_t type;
    uint16_t color;
    uint64_t flag;

    bool operator==(const TileKey& other) const {
        return type == other.type && color == other.color && flag == other.flag;
    }
};

namespace std {
    template<>
    struct hash<TileKey> {
        std::size_t operator()(const TileKey& k) const {
            size_t h1 = std::hash<int>()(static_cast<int>(k.type));
            size_t h2 = std::hash<int>()(k.color);
            size_t h3 = std::hash<uint64_t>()(k.flag);

            return h1 ^ (h2 << 1) ^ (h3 << 2);
        }
    };
}

struct EdgeKey {
    uint64_t color;
    uint64_t flag;

    bool operator==(const EdgeKey& other) const {
        return color == other.color && flag == other.flag;
    }
};

namespace std {
    template <>
    struct hash<EdgeKey> {
        std::size_t operator()(const EdgeKey& k) const {
            return std::hash<uint64_t>{}(k.color) ^ (std::hash<uint64_t>{}(k.flag) << 1);
        }
    };
}

struct BackgroundTwoKey {
    uint64_t color;
    uint64_t flag;
    uint64_t texposhandle;

    bool operator==(const BackgroundTwoKey& other) const {
        return color == other.color &&
               flag == other.flag &&
               texposhandle == other.texposhandle;
    }
};

// Custom hash function
namespace std {
    template <>
    struct hash<BackgroundTwoKey> {
        std::size_t operator()(const BackgroundTwoKey& key) const {
            // Combine the three fields using a standard hash combine method
            std::size_t h1 = std::hash<uint64_t>{}(key.color);
            std::size_t h2 = std::hash<uint64_t>{}(key.flag);
            std::size_t h3 = std::hash<uint64_t>{}(key.texposhandle);

            // A simple hash combine method
            return h1 ^ (h2 << 1) ^ (h3 << 2);
        }
    };
}

std::unordered_map<TileKey, TexposHandle> tile_floor_texpos_cache;
std::unordered_map<EdgeKey, TexposHandle> tile_floor_edge_cache;
std::unordered_map<CompTileKey, TexposHandle> tile_floor_comptile_cache;
std::unordered_map<TilePaletteKey, TexposHandle> tile_floor_palette_cache;
std::unordered_map<uint32_t, TexposHandle> background_two_cache;
std::unordered_map<BackgroundTwoKey, TexposHandle> background_two_full_cache;
LRUCache<df::coord, LRUCacheBlock*> lrucache(2000);



bool operator==(const df::coord& lhs, const df::coord& rhs) {
    return lhs.x == rhs.x && lhs.y == rhs.y && lhs.z == rhs.z;
}

static const int WORLD_X = df::global::world->map.x_count;
static const int WORLD_Y = df::global::world->map.y_count;
static const int WORLD_Z = df::global::world->map.z_count;
std::unordered_map<df::coord, int32_t> coord_to_tile_texpos;
std::unordered_map<df::coord, int> coord_to_tile_color;

static uint8_t depthMap[256];
static uint16_t zeros512[512] = {0};
static uint16_t depths[16][16] = {0};


static int dry_grass_color = 14;
static int dead_grass_color = 31;

std::vector<TexposHandle> set_pfloor;
std::vector<TexposHandle> set_pstone_engraved;
std::vector<TexposHandle> set_palt_floor;
std::vector<TexposHandle> set_ppebbles;
std::vector<TexposHandle> set_pfurrowed_soil;
std::vector<TexposHandle> set_pstairs;
std::vector<TexposHandle> set_pramp_stone;
std::vector<TexposHandle> set_pramp_soil;
std::vector<TexposHandle> set_pramp_grass;

// offset in tilepages between elements
int grass_off = 10;
int stone_off = 37;
int smooth_off = 33;
int soil_off = 64;
int pebbles_off = 3;
int farmplot_off = 15;
int stairs_stone_off = 3;
int stairs_soil_off = 6;
int stairs_UD_off = 0;
int stairs_U_off = 1;
int stairs_D_off = 2;

int32_t ORAMP_WITH_WALL_N_S_E_W = 26;
int32_t ORAMP_WITH_WALL_N_S_E = 54;
int32_t ORAMP_WITH_WALL_N_S_W = 84;
int32_t ORAMP_WITH_WALL_N_W_E = 82;
int32_t ORAMP_WITH_WALL_S_W_E = 52;
int32_t ORAMP_WITH_WALL_N_S = 22;
int32_t ORAMP_WITH_WALL_N_W = 64;
int32_t ORAMP_WITH_WALL_N_E = 62;
int32_t ORAMP_WITH_WALL_S_W = 34;
int32_t ORAMP_WITH_WALL_S_E = 32;
int32_t ORAMP_WITH_WALL_W_E = 24;
int32_t ORAMP_WITH_WALL_N = 78;
int32_t ORAMP_WITH_WALL_S = 18;
int32_t ORAMP_WITH_WALL_W = 50;
int32_t ORAMP_WITH_WALL_E = 46;
int32_t ORAMP_WITH_WALL_NW = 79;
int32_t ORAMP_WITH_WALL_SW = 19;
int32_t ORAMP_WITH_WALL_SE = 17;
int32_t ORAMP_WITH_WALL_NE = 77;
int32_t ORAMP_WITH_WALL_NW_NE = 56;
int32_t ORAMP_WITH_WALL_NE_SE = 58;
int32_t ORAMP_WITH_WALL_SW_SE = 86;
int32_t ORAMP_WITH_WALL_NW_SW = 88;
int32_t ORAMP_OTHER = 28;
int32_t ORAMP_WITH_WALL_S_NE = 106;
int32_t ORAMP_WITH_WALL_W_SE = 108;
int32_t ORAMP_WITH_WALL_N_SW = 136;
int32_t ORAMP_WITH_WALL_E_NW = 138;
int32_t ORAMP_WITH_WALL_E_SW = 112;
int32_t ORAMP_WITH_WALL_S_NW = 114;
int32_t ORAMP_WITH_WALL_W_NE = 116;
int32_t ORAMP_WITH_WALL_N_SE = 118;
int32_t ORAMP_WITH_WALL_S_E_NW = 142;
int32_t ORAMP_WITH_WALL_S_W_NE = 144;
int32_t ORAMP_WITH_WALL_N_W_SE = 146;
int32_t ORAMP_WITH_WALL_N_E_SW = 148;
int32_t ORAMP_WITH_WALL_N_SW_SE = 172;
int32_t ORAMP_WITH_WALL_E_NW_SW = 174;
int32_t ORAMP_WITH_WALL_S_NW_NE = 176;
int32_t ORAMP_WITH_WALL_W_NE_SE = 178;
int32_t ORAMP_WITH_WALL_NW_SE = 166;
int32_t ORAMP_WITH_WALL_NE_SW = 168;
int32_t ORAMP_WITH_WALL_NW_NE_SW_SE = 170;
int32_t ORAMP_WITH_WALL_NW_NE_SW = 142;
int32_t ORAMP_WITH_WALL_NW_NE_SE = 144;
int32_t ORAMP_WITH_WALL_NE_SW_SE = 146;
int32_t ORAMP_WITH_WALL_NW_SW_SE = 148;

df::graphic_viewportst* getDeepestGraphicsViewport(int z) {
    std::vector<graphic_viewportst*> vps = df::global::gps->viewport;
    int current_z = (int)*df::global::window_z;
    for (int depth = 0; depth < 9; ++depth) {
        if (current_z - depth == z) {
            return vps[depth];
        }
    }
    return nullptr;
}

df::tiletype *get_tiletype_wrapper(int32_t x, int32_t y, int32_t z) {
    df::tiletype *tt = Maps::getTileType(x, y, z);
    if (!tt) {
        std::cerr << "TT IS NULL!!! ABANDONING RENDER ATTEMPT THIS FRAME!!! wx: " << x << " wy: " << y << " z: " << z << "\n";
        return NULL;
    }
    return tt;
}

static TexposHandle getCustomRampTexture(std::vector<TexposHandle> handles, uint64_t rf, df::palettest base_palette, uint16_t tt, uint16_t color) {
    int32_t ramp_texpos = 0;
    	                                if((rf & VIEWPORT_RAMP_FLAG_WALL_N)&&
						(rf & VIEWPORT_RAMP_FLAG_WALL_S)&&
						(rf & VIEWPORT_RAMP_FLAG_WALL_E)&&
						(rf & VIEWPORT_RAMP_FLAG_WALL_W)) ramp_texpos = ORAMP_WITH_WALL_N_S_E_W;
					else if((rf & VIEWPORT_RAMP_FLAG_WALL_N)&&
						(rf & VIEWPORT_RAMP_FLAG_WALL_S)&&
						(rf & VIEWPORT_RAMP_FLAG_WALL_E)) ramp_texpos = ORAMP_WITH_WALL_N_S_E;
					else if((rf & VIEWPORT_RAMP_FLAG_WALL_N)&&
						(rf & VIEWPORT_RAMP_FLAG_WALL_S)&&
						(rf & VIEWPORT_RAMP_FLAG_WALL_W)) ramp_texpos = ORAMP_WITH_WALL_N_S_W;
					else if((rf & VIEWPORT_RAMP_FLAG_WALL_N)&&
						(rf & VIEWPORT_RAMP_FLAG_WALL_W)&&
						(rf & VIEWPORT_RAMP_FLAG_WALL_E)) ramp_texpos = ORAMP_WITH_WALL_N_W_E;
					else if((rf & VIEWPORT_RAMP_FLAG_WALL_S)&&
						(rf & VIEWPORT_RAMP_FLAG_WALL_W)&&
						(rf & VIEWPORT_RAMP_FLAG_WALL_E)) ramp_texpos = ORAMP_WITH_WALL_S_W_E;
					else if((rf & VIEWPORT_RAMP_FLAG_WALL_S)&&
						(rf & VIEWPORT_RAMP_FLAG_WALL_E)&&
						(rf & VIEWPORT_RAMP_FLAG_WALL_NW)) ramp_texpos = ORAMP_WITH_WALL_S_E_NW;
					else if((rf & VIEWPORT_RAMP_FLAG_WALL_S)&&
						(rf & VIEWPORT_RAMP_FLAG_WALL_W)&&
						(rf & VIEWPORT_RAMP_FLAG_WALL_NE)) ramp_texpos = ORAMP_WITH_WALL_S_W_NE;
					else if((rf & VIEWPORT_RAMP_FLAG_WALL_N)&&
						(rf & VIEWPORT_RAMP_FLAG_WALL_W)&&
						(rf & VIEWPORT_RAMP_FLAG_WALL_SE)) ramp_texpos = ORAMP_WITH_WALL_N_W_SE;
					else if((rf & VIEWPORT_RAMP_FLAG_WALL_N)&&
						(rf & VIEWPORT_RAMP_FLAG_WALL_E)&&
						(rf & VIEWPORT_RAMP_FLAG_WALL_SW)) ramp_texpos = ORAMP_WITH_WALL_N_E_SW;
					else if((rf & VIEWPORT_RAMP_FLAG_WALL_N)&&
						(rf & VIEWPORT_RAMP_FLAG_WALL_S)) ramp_texpos = ORAMP_WITH_WALL_N_S;
					else if((rf & VIEWPORT_RAMP_FLAG_WALL_N)&&
						(rf & VIEWPORT_RAMP_FLAG_WALL_W)) ramp_texpos = ORAMP_WITH_WALL_N_W;
					else if((rf & VIEWPORT_RAMP_FLAG_WALL_N)&&
						(rf & VIEWPORT_RAMP_FLAG_WALL_E)) ramp_texpos = ORAMP_WITH_WALL_N_E;
					else if((rf & VIEWPORT_RAMP_FLAG_WALL_S)&&
						(rf & VIEWPORT_RAMP_FLAG_WALL_W)) ramp_texpos = ORAMP_WITH_WALL_S_W;
					else if((rf & VIEWPORT_RAMP_FLAG_WALL_S)&&
						(rf & VIEWPORT_RAMP_FLAG_WALL_E)) ramp_texpos = ORAMP_WITH_WALL_S_E;
	                                else if((rf & VIEWPORT_RAMP_FLAG_WALL_W)&&
						(rf & VIEWPORT_RAMP_FLAG_WALL_E)) ramp_texpos = ORAMP_WITH_WALL_W_E;
					else if((rf & VIEWPORT_RAMP_FLAG_WALL_N)&&
						(rf & VIEWPORT_RAMP_FLAG_WALL_SW)&&
						(rf & VIEWPORT_RAMP_FLAG_WALL_SE)) ramp_texpos = ORAMP_WITH_WALL_N_SW_SE;
					else if((rf & VIEWPORT_RAMP_FLAG_WALL_E)&&
						(rf & VIEWPORT_RAMP_FLAG_WALL_NW)&&
						(rf & VIEWPORT_RAMP_FLAG_WALL_SW)) ramp_texpos = ORAMP_WITH_WALL_E_NW_SW;
					else if((rf & VIEWPORT_RAMP_FLAG_WALL_S)&&
						(rf & VIEWPORT_RAMP_FLAG_WALL_NW)&&
						(rf & VIEWPORT_RAMP_FLAG_WALL_NE)) ramp_texpos = ORAMP_WITH_WALL_S_NW_NE;
					else if((rf & VIEWPORT_RAMP_FLAG_WALL_W)&&
						(rf & VIEWPORT_RAMP_FLAG_WALL_NE)&&
						(rf & VIEWPORT_RAMP_FLAG_WALL_SE)) ramp_texpos = ORAMP_WITH_WALL_W_NE_SE;
					else if((rf & VIEWPORT_RAMP_FLAG_WALL_S)&&
						(rf & VIEWPORT_RAMP_FLAG_WALL_NE)) ramp_texpos = ORAMP_WITH_WALL_S_NE;
					else if((rf & VIEWPORT_RAMP_FLAG_WALL_W)&&
						(rf & VIEWPORT_RAMP_FLAG_WALL_SE)) ramp_texpos = ORAMP_WITH_WALL_W_SE;
					else if((rf & VIEWPORT_RAMP_FLAG_WALL_N)&&
						(rf & VIEWPORT_RAMP_FLAG_WALL_SW)) ramp_texpos = ORAMP_WITH_WALL_N_SW;
					else if((rf & VIEWPORT_RAMP_FLAG_WALL_E)&&
						(rf & VIEWPORT_RAMP_FLAG_WALL_NW)) ramp_texpos = ORAMP_WITH_WALL_E_NW;
					else if((rf & VIEWPORT_RAMP_FLAG_WALL_E)&&
						(rf & VIEWPORT_RAMP_FLAG_WALL_SW)) ramp_texpos = ORAMP_WITH_WALL_E_SW;
					else if((rf & VIEWPORT_RAMP_FLAG_WALL_S)&&
						(rf & VIEWPORT_RAMP_FLAG_WALL_NW)) ramp_texpos = ORAMP_WITH_WALL_S_NW;
					else if((rf & VIEWPORT_RAMP_FLAG_WALL_W)&&
						(rf & VIEWPORT_RAMP_FLAG_WALL_NE)) ramp_texpos = ORAMP_WITH_WALL_W_NE;
					else if((rf & VIEWPORT_RAMP_FLAG_WALL_N)&&
						(rf & VIEWPORT_RAMP_FLAG_WALL_SE)) ramp_texpos = ORAMP_WITH_WALL_N_SE;
					else if(rf & VIEWPORT_RAMP_FLAG_WALL_N) ramp_texpos = ORAMP_WITH_WALL_N;
					else if(rf & VIEWPORT_RAMP_FLAG_WALL_S) ramp_texpos = ORAMP_WITH_WALL_S;
					else if(rf & VIEWPORT_RAMP_FLAG_WALL_E) ramp_texpos = ORAMP_WITH_WALL_E;
					else if(rf & VIEWPORT_RAMP_FLAG_WALL_W) ramp_texpos = ORAMP_WITH_WALL_W;
					else if(rf & VIEWPORT_RAMP_FLAG_WALL_NE) ramp_texpos = ORAMP_WITH_WALL_NE;
					else if(rf & VIEWPORT_RAMP_FLAG_WALL_SE) ramp_texpos = ORAMP_WITH_WALL_SE;
					else if(rf & VIEWPORT_RAMP_FLAG_WALL_NW) ramp_texpos = ORAMP_WITH_WALL_NW;
					else if(rf & VIEWPORT_RAMP_FLAG_WALL_SW) ramp_texpos = ORAMP_WITH_WALL_SW;
					else if((rf & VIEWPORT_RAMP_FLAG_WALL_NW)&&
						(rf & VIEWPORT_RAMP_FLAG_WALL_NE)&&
						(rf & VIEWPORT_RAMP_FLAG_WALL_SW)&&
						(rf & VIEWPORT_RAMP_FLAG_WALL_SE)) ramp_texpos = ORAMP_WITH_WALL_NW_NE_SW_SE;
					else if((rf & VIEWPORT_RAMP_FLAG_WALL_NW)&&
						(rf & VIEWPORT_RAMP_FLAG_WALL_NE)&&
						(rf & VIEWPORT_RAMP_FLAG_WALL_SW)) ramp_texpos = ORAMP_WITH_WALL_NW_NE_SW;
					else if((rf & VIEWPORT_RAMP_FLAG_WALL_NW)&&
						(rf & VIEWPORT_RAMP_FLAG_WALL_NE)&&
						(rf & VIEWPORT_RAMP_FLAG_WALL_SE)) ramp_texpos = ORAMP_WITH_WALL_NW_NE_SE;
					else if((rf & VIEWPORT_RAMP_FLAG_WALL_NW)&&
						(rf & VIEWPORT_RAMP_FLAG_WALL_SW)&&
						(rf & VIEWPORT_RAMP_FLAG_WALL_SE)) ramp_texpos = ORAMP_WITH_WALL_NE_SW_SE;
					else if((rf & VIEWPORT_RAMP_FLAG_WALL_NE)&&
						(rf & VIEWPORT_RAMP_FLAG_WALL_SW)&&
						(rf & VIEWPORT_RAMP_FLAG_WALL_SE)) ramp_texpos = ORAMP_WITH_WALL_NW_SW_SE;
					else if((rf & VIEWPORT_RAMP_FLAG_WALL_NW)&&
						(rf & VIEWPORT_RAMP_FLAG_WALL_NE)) ramp_texpos = ORAMP_WITH_WALL_NW_NE;
					else if((rf & VIEWPORT_RAMP_FLAG_WALL_SW)&&
						(rf & VIEWPORT_RAMP_FLAG_WALL_SE)) ramp_texpos = ORAMP_WITH_WALL_SW_SE;
					else if((rf & VIEWPORT_RAMP_FLAG_WALL_NW)&&
						(rf & VIEWPORT_RAMP_FLAG_WALL_SW)) ramp_texpos = ORAMP_WITH_WALL_NW_SW;
					else if((rf & VIEWPORT_RAMP_FLAG_WALL_NE)&&
						(rf & VIEWPORT_RAMP_FLAG_WALL_SE)) ramp_texpos = ORAMP_WITH_WALL_NE_SE;
					else if((rf & VIEWPORT_RAMP_FLAG_WALL_NW)&&
						(rf & VIEWPORT_RAMP_FLAG_WALL_SE)) ramp_texpos = ORAMP_WITH_WALL_NW_SE;
					else if((rf & VIEWPORT_RAMP_FLAG_WALL_NE)&&
						(rf & VIEWPORT_RAMP_FLAG_WALL_SW)) ramp_texpos = ORAMP_WITH_WALL_NE_SW;

    TexposHandle ramp_texpos_handle = handles[ramp_texpos];

    //CompTileKey ramp_comptile_key = CompTileKey(tt, color, 0, rf);
    //std::unordered_map<CompTileKey, TexposHandle>::iterator it = tile_floor_comptile_cache.find(ramp_comptile_key);
    //if (it != tile_floor_comptile_cache.end()) {
    //    ramp_texpos_handle = it->second;
    //} else {
        if (ramp_texpos == 0) {
            return 0;
        }
        df::palettest material_palette = df::global::world->raws.descriptors.colors[color]->palette;
        ramp_texpos_handle = swapPalettes(ramp_texpos_handle, base_palette, material_palette);
        //tile_floor_comptile_cache[ramp_comptile_key] = ramp_texpos_handle;
    //}
    return ramp_texpos_handle;
}

uint16_t getGrassColorBiome(df::coord pos) {
    uint16_t new_color = 0;
    df::tile_designation desi = *Maps::getTileDesignation(pos.x, pos.y, pos.z);
    switch(desi.bits.biome) {
        case 0: // Mountains
            new_color = 34; //DARK_OLIVE
            break;
        case 1: // GLACIER
            new_color = 34; //DARK_OLIVE
            break;
        case 2: // TUNDRA
            new_color = 34; //DARK_OLIVE
            break;
        case 3: // SWAMP_TEMPERATE_FRESHWATER
            new_color = 94; //SEA_GREEN
            break;
        case 4: // SWAMP_TEMPERATE_SALTWATER
            new_color = 94; //SEA_GREEN
            break;
        case 5: // MARSH_TEMPERATE_FRESHWATER
            new_color = 94; //SEA_GREEN
            break;
        case 6: // MARSH_TEMPERATE_SALTWATER
            new_color = 94; //SEA_GREEN
            break;
        case 7: // SWAMP_TROPICAL_FRESHWATER
            new_color = 114; //yellow green
            break;
        case 8: // SWAMP_TROPICAL_SALTWATER
            new_color = 114; //yellow green
            break;
        case 9: // SWAMP_MANGROVE
            new_color = 71; //OLIVE
            break;
        case 10: // MARSH_TROPICAL_FRESHWATER
            new_color = 114; //yellow green
            break;
        case 11: // MARSH_TROPICAL_SALTWATER
            new_color = 114; //yellow green
            break;
        case 12: // FORREST_TIAGA
            new_color = 51; //GREEN_YELLOW
            break;
        case 13: // FORREST_TEMPERATE_CONIFER
            new_color = 43; //FERN_GREEN
            break;
        case 14: // FORREST_TEMPERATE_BROADLEAF
            new_color = 69; //MOSS_GREEN
            break;
        case 15: // FORREST_TROPICAL_CONIFER
            new_color = 80; //PINE_GREEN
            break;
        case 16: // FORREST_TROPICAL_DRY_BROADLEAF
            new_color = 114; //yellow green
            break;
        case 17: // FORREST_TROPICAL_WET_BROADLEAF
            new_color = 69; //MOSS_GREEN
            break;
        case 18: // GRASSLAND_TEMPERATE
            new_color = 68; //MINT_GREEN
            break;
        case 19: // SAVANNAH_TEMPERATE
            new_color = 94; //SEA_GREEN
            break;
        case 20: // SHRUBLAND_TEMPERAT
            new_color = 114; //YELLOW_GOLD
            break;
        case 21: // GRASSLAND_TROPICAL
            new_color = 47; //GOLDEN_YELLOW
            break;
        case 22: // SAVANNAH_TROPICAL
            new_color = 48; //GOLDENROD
            break;
        case 23: // SHRUBLAND_TROPICAL
            new_color = 62; //LIME
            break;
        case 24: // DESERT_BADLAND
            new_color = 92; //SAFFRON
            break;
        case 25: // DESERT_ROCK
            new_color = 38; //DARK_TAN
            break;
        case 26: // DESERT_SAND
            new_color = 38; //DARK_TAN
            break;
    }
    return new_color;
}

// might be able to do this easier based on tiletype interval instead of switch.
bool tileIsGrass(uint16_t tt) {
    bool ret = false;
    switch(tt + 1) {
            case (df::tiletype::GrassDarkFloor1 + 1):
            case (df::tiletype::GrassDarkFloor2 + 1):
            case (df::tiletype::GrassDarkFloor3 + 1):
            case (df::tiletype::GrassDarkFloor4 + 1):
            case (df::tiletype::GrassLightFloor1 + 1):
            case (df::tiletype::GrassLightFloor2 + 1):
            case (df::tiletype::GrassLightFloor3 + 1):
            case (df::tiletype::GrassLightFloor4 + 1):
            case df::tiletype::GrassDryFloor1:
            case df::tiletype::GrassDryFloor2:
            case df::tiletype::GrassDryFloor3:
            case df::tiletype::GrassDryFloor4:
            case df::tiletype::GrassDeadFloor1 + 1:
            case df::tiletype::GrassDeadFloor2 + 1:
            case df::tiletype::GrassDeadFloor3 + 1:
            case df::tiletype::GrassDeadFloor4 + 1:
            case df::tiletype::TreeTrunkThickN + 1:
            case df::tiletype::TreeTrunkThickE + 1:
            case df::tiletype::TreeTrunkThickW + 1:
            case df::tiletype::TreeTrunkThickS + 1:
            case df::tiletype::TreeTrunkThickNW + 1:
            case df::tiletype::TreeTrunkThickNE + 1:
            case df::tiletype::TreeTrunkThickSW + 1:
            case df::tiletype::TreeTrunkThickSE + 1:
            case df::tiletype::TreeTrunkPillar + 1:
            case df::tiletype::Shrub + 1:
            case df::tiletype::Sapling + 1:
            case df::tiletype::ShrubDead + 1:
            case df::tiletype::SaplingDead + 1:
                ret = true;
                break;
    }
    return ret;
}

static std::pair<int, int> getMaterialIdxAndColorByPos(df::coord pos, uint16_t tt) {
    if (tileIsGrass(tt)) {
        int color = (int)getGrassColorBiome(pos);
        return std::make_pair(0, color);
    }
    if(auto *block = Maps::getTileBlock(pos.x, pos.y, pos.z)) {
        for (int i = 0; i < block->block_events.size(); ++i) {
            if (block->block_events[i]->getType() == df::block_square_event_type::mineral) {
                df::block_square_event_mineralst *mineral_event = reinterpret_cast<df::block_square_event_mineralst *>(block->block_events[i]);
            //std::cerr << "LOOKING AT MINERAL EVENT x = " << pos.x << "  y = " << pos.y << "  z = " << pos.z << "\n";
                if (mineral_event->tile_bitmask.getassignment(pos.x, pos.y)) {
                    int mat_idx = mineral_event->inorganic_mat;
                    int color = df::global::world->raws.inorganics.all[mat_idx]->material.state_color[0];
                    //std::cerr << "GOT MINERAL EVENT x = " << pos.x << "  y = " << pos.y << "  z = " << pos.z << " color=" << color << " mat=" << mat_idx << "\n";
                    return std::make_pair(mat_idx, color);
                }
            }
        }
    }
    df::coord2d biome_region = Maps::getTileBiomeRgn(pos);
    df::region_map_entry *rb = Maps::getRegionBiome(biome_region);
    df::tile_designation *des = Maps::getTileDesignation(pos);

    int layer = des->bits.geolayer_index;
    int mat_idx = df::global::world->world_data->geo_biomes[rb->geo_index]->layers[layer]->mat_index;
    int color = df::global::world->raws.inorganics.all[mat_idx]->material.state_color[0];

    return std::make_pair(mat_idx, color);
}

static int32_t floor_up = 9;
static int32_t floor_down = -9;
static int32_t floor_left = 1;
static int32_t floor_right = -1;

static int32_t floor_up_left =  10;
static int32_t floor_up_right =  8;
static int32_t floor_down_left =  -8;
static int32_t floor_down_right =  -10;

static int32_t pebbles_floor_up = 7;
static int32_t pebbles_floor_down = 6;
static int32_t pebbles_floor_left = 4;
static int32_t pebbles_floor_right = 5;

static int32_t pebbles_floor_up_left = 8;
static int32_t pebbles_floor_up_right = 9;
static int32_t pebbles_floor_down_left = 10;
static int32_t pebbles_floor_down_right = 11;

static std::pair<int,int> pair_up = std::pair<int,int>(floor_up, pebbles_floor_up);
static std::pair<int,int> pair_down = std::pair<int,int>(floor_down, pebbles_floor_down);
static std::pair<int,int> pair_left = std::pair<int,int>(floor_left, pebbles_floor_left);
static std::pair<int,int> pair_right = std::pair<int,int>(floor_right, pebbles_floor_right);

static std::pair<int,int> pair_up_left = std::pair<int,int>(floor_up_left, pebbles_floor_up_left);
static std::pair<int,int> pair_down_left = std::pair<int,int>(floor_down_left, pebbles_floor_down_left);
static std::pair<int,int> pair_up_right = std::pair<int,int>(floor_up_right, pebbles_floor_up_right);
static std::pair<int,int> pair_down_right = std::pair<int,int>(floor_down_right, pebbles_floor_down_right);

bool isSand(std::string token) {
    if (token == "SAND_YELLOW" || token == "SAND_RED" || token == "SAND_BLACK" || token == "SAND_WHITE" || token == "SAND_TAN") {
        return true;
    }
    return false;
}

uint32_t getEdgingCode(uint16_t tt) {
    uint32_t code = 0;
        switch(tt) {
            case df::tiletype::GrassDarkFloor1:
            case df::tiletype::GrassDarkFloor2:
            case df::tiletype::GrassDarkFloor3:
            case df::tiletype::GrassDarkFloor4:
            case df::tiletype::GrassLightFloor1:
            case df::tiletype::GrassLightFloor2:
            case df::tiletype::GrassLightFloor3:
            case df::tiletype::GrassLightFloor4:
            case df::tiletype::TreeTrunkThickN:
            case df::tiletype::TreeTrunkThickE:
            case df::tiletype::TreeTrunkThickW:
            case df::tiletype::TreeTrunkThickS:
            case df::tiletype::TreeTrunkThickNW:
            case df::tiletype::TreeTrunkThickNE:
            case df::tiletype::TreeTrunkThickSW:
            case df::tiletype::TreeTrunkThickSE:
            case df::tiletype::TreeTrunkPillar:
            case df::tiletype::Shrub:
            case df::tiletype::Sapling:
            case df::tiletype::ShrubDead:
            case df::tiletype::SaplingDead:
                code = 1;
                break;
            case df::tiletype::StoneBoulder:
            case df::tiletype::LavaBoulder:
            case df::tiletype::FeatureBoulder:
                code = 2;
                break;            
            case df::tiletype::StonePebbles1:
            case df::tiletype::StonePebbles2:
            case df::tiletype::StonePebbles3:
            case df::tiletype::StonePebbles4:
            case df::tiletype::LavaPebbles1:
            case df::tiletype::LavaPebbles2:
            case df::tiletype::LavaPebbles3:
            case df::tiletype::LavaPebbles4:
            case df::tiletype::FeaturePebbles1:
            case df::tiletype::FeaturePebbles2:
            case df::tiletype::FeaturePebbles3:
            case df::tiletype::FeaturePebbles4:
                code = 9;
                break;
            case df::tiletype::StoneFloor4:
            case df::tiletype::StoneFloor3:
            case df::tiletype::StoneFloor2:
            case df::tiletype::StoneFloor1:
            case df::tiletype::LavaFloor4:
            case df::tiletype::LavaFloor3:
            case df::tiletype::LavaFloor2:
            case df::tiletype::LavaFloor1 :
            case df::tiletype::MineralFloor4:
            case df::tiletype::MineralFloor3:
            case df::tiletype::MineralFloor2:
            case df::tiletype::MineralFloor1:
            case df::tiletype::FeatureFloor4:
            case df::tiletype::FeatureFloor3:
            case df::tiletype::FeatureFloor2:
            case df::tiletype::FeatureFloor1:
            case df::tiletype::StoneStairUD:
            case df::tiletype::LavaStairUD:
            case df::tiletype::MineralStairUD:
            case df::tiletype::FeatureStairUD:
            case df::tiletype::StoneStairU:
            case df::tiletype::LavaStairU:
            case df::tiletype::MineralStairU:
            case df::tiletype::FeatureStairU:
            case df::tiletype::StoneStairD:
            case df::tiletype::LavaStairD:
            case df::tiletype::MineralStairD:
            case df::tiletype::FeatureStairD:
                code = 8;
                break;
            case df::tiletype::SoilFloor4:
            case df::tiletype::SoilFloor3:
            case df::tiletype::SoilFloor2:
            case df::tiletype::SoilFloor1:
            case df::tiletype::SoilStairUD:
            case df::tiletype::SoilStairU:
            case df::tiletype::SoilStairD:
                code = 2;
                break;
    }
    return code;
}

TexposHandle getCustomEdgingTexture(uint16_t tt, std::pair<int, int> direction_offset, uint16_t col, uint8_t bit_shift, uint16_t diagonal) {
    uint32_t edge_texpos = -1;
    std::vector<TexposHandle> handles;
    uint32_t code = getEdgingCode(tt);

    switch(code) {
        case 1:
            edge_texpos = grass_off + direction_offset.first;
            handles = set_palt_floor;
            break;
        case 2:
            edge_texpos = soil_off + direction_offset.first;
            handles = set_palt_floor;
            break;
        case 8:
            edge_texpos = stone_off + direction_offset.first;
            handles = set_palt_floor;
            break;
        case 9:
            edge_texpos = direction_offset.second;
            handles = set_ppebbles;
            break;
    }
    if (edge_texpos < 0 || handles.size() == 0) return 0;

    TexposHandle swapped_texpos;

    uint64_t tt_key = ((uint64_t)tt) << bit_shift;
    uint64_t color_key = ((uint64_t)col) << (bit_shift * 2);

    CompTileKey edging_comptilekey = CompTileKey(0, diagonal, color_key, tt_key);
    
    std::unordered_map<CompTileKey, TexposHandle>::iterator it = tile_floor_comptile_cache.find(edging_comptilekey);
    if (it != tile_floor_comptile_cache.end()) {
        swapped_texpos = it->second;
    } else {
        df::palettest material_palette = df::global::world->raws.descriptors.colors[col]->palette;
        swapped_texpos = swapPalettes(handles[edge_texpos], base_inorganic_palette, material_palette);
        if (swapped_texpos) {
            tile_floor_comptile_cache[edging_comptilekey] = swapped_texpos;
        }
    }
    return swapped_texpos;
}

static bool checkTileTypePebbles(uint16_t tt) {
    bool ret = false;
        if (tt == df::tiletype::StonePebbles1 ||
            tt == df::tiletype::StonePebbles2 ||
            tt == df::tiletype::StonePebbles3 ||
            tt == df::tiletype::StonePebbles4 ||
            tt == df::tiletype::LavaPebbles1 ||
            tt == df::tiletype::LavaPebbles2 ||
            tt == df::tiletype::LavaPebbles3 ||
            tt == df::tiletype::LavaPebbles4 ||
            tt == df::tiletype::FeaturePebbles1 ||
            tt == df::tiletype::FeaturePebbles2 ||
            tt == df::tiletype::FeaturePebbles3 ||
            tt == df::tiletype::FeaturePebbles4) ret = true;
    return ret;
}

static TexposHandle getCombinedCustomEdgingTexture(int32_t x, int32_t y, int32_t z, uint64_t ff, std::vector<uint16_t> cardinal_colors, std::vector<uint8_t> dir_types, std::vector<uint16_t> tts, uint64_t color) {
    Edging type_n = dir_types[0];
    Edging type_e = dir_types[1];
    Edging type_w = dir_types[2];
    Edging type_s = dir_types[3];

    uint16_t colorn = cardinal_colors[0];
    uint16_t colore = cardinal_colors[1];
    uint16_t colorw = cardinal_colors[2];
    uint16_t colors = cardinal_colors[3];

    uint16_t tt_n = tts[0];
    uint16_t tt_e = tts[1];
    uint16_t tt_w = tts[2];
    uint16_t tt_s = tts[3];

    std::vector<TexposHandle> edging_texposes;

        //std::cerr << "AT getCombined for x: " << x << " y: " << y << " z: " << z << " ff: " << ff << "\n";
  
    if(ff & VIEWPORT_FLOOR_FLAG_N_EDGING) {
        if (type_n == 1 || type_n == 2 || type_n == 8 || type_n == 9) {
            TexposHandle edge_texture = getCustomEdgingTexture(tt_n, pair_up, colorn, (uint8_t)VIEWPORT_FLOOR_FLAG_N_EDGING_SHIFT, 0);
            if (edge_texture) edging_texposes.push_back(edge_texture);
//std::cerr << "N\n";

        }
    }
    if(ff & VIEWPORT_FLOOR_FLAG_E_EDGING) {
        if (type_e == 1 || type_e == 2 || type_e == 8 || type_e == 9) {
            TexposHandle edge_texture = getCustomEdgingTexture(tt_e, pair_right, colore, (uint8_t)VIEWPORT_FLOOR_FLAG_E_EDGING_SHIFT, 0);
            if (edge_texture) edging_texposes.push_back(edge_texture);
//std::cerr << "E\n";

        }
    }
    if(ff & VIEWPORT_FLOOR_FLAG_W_EDGING) {
        if (type_w == 1 || type_w == 2 || type_w == 8 || type_w == 9) {
            TexposHandle edge_texture = getCustomEdgingTexture(tt_w, pair_left, colorw, (uint8_t)VIEWPORT_FLOOR_FLAG_W_EDGING_SHIFT, 0);
            if (edge_texture) edging_texposes.push_back(edge_texture);
//std::cerr << "W\n";

        }
    }
    if(ff & VIEWPORT_FLOOR_FLAG_S_EDGING) {
        if (type_s == 1 || type_s == 2 || type_s == 8 || type_s == 9) {
            TexposHandle edge_texture = getCustomEdgingTexture(tt_s, pair_down, colors, VIEWPORT_FLOOR_FLAG_S_EDGING_SHIFT, 0);
            if (edge_texture) edging_texposes.push_back(edge_texture);
//std::cerr << "S\n";

        }
    }
    if((ff & VIEWPORT_FLOOR_FLAG_N_EDGING) &&
	(ff & VIEWPORT_FLOOR_FLAG_W_EDGING)) {
        if ((type_n == 1 || type_n == 2 || type_n == 8 || type_n == 9) && (type_w == 1 || type_w == 2 || type_w == 8 || type_w == 9)) {
        //if ((type_n == 2 && type_w == 8) || (type_w == 2 && type_n == 8) || (type_n == 2 && type_w == 2) || (type_n == 8 && type_w == 8)) {
            df::coord edge_pos = df::coord(-1, -1, 0);
            uint16_t tt = 0;
            uint16_t col = 0;
            int bit_shift = 0;
            if (type_n == 1) {tt = tt_n; col=colorn; bit_shift=VIEWPORT_FLOOR_FLAG_N_EDGING_SHIFT;}
            else if (type_w == 1) {tt = tt_w; col=colorw; bit_shift=VIEWPORT_FLOOR_FLAG_W_EDGING_SHIFT;}
            else if (type_n == 8 && (type_w != 2 && type_w != 9)) {tt = tt_n; col=colorn; bit_shift=VIEWPORT_FLOOR_FLAG_N_EDGING_SHIFT;}
            else if (type_w == 8 && (type_n != 2 && type_n != 9)) {tt = tt_w; col=colorw; bit_shift=VIEWPORT_FLOOR_FLAG_W_EDGING_SHIFT;}
            else if (type_n == 2 && type_w != 9) {tt = tt_n; col=colorn; bit_shift=VIEWPORT_FLOOR_FLAG_N_EDGING_SHIFT;}
            else if (type_w == 2 && type_n != 9) {tt = tt_w; col=colorw; bit_shift=VIEWPORT_FLOOR_FLAG_W_EDGING_SHIFT;}
            else if (type_n == 9) {tt = tt_n; col=colorn; bit_shift=VIEWPORT_FLOOR_FLAG_N_EDGING_SHIFT;}
            else if (type_w == 9) {tt = tt_w; col=colorw; bit_shift=VIEWPORT_FLOOR_FLAG_W_EDGING_SHIFT;}
            if (tt) {
                TexposHandle edge_texture = getCustomEdgingTexture(tt, pair_up_left, col, bit_shift, 1);
                if (edge_texture) edging_texposes.push_back(edge_texture);
//std::cerr << "NW\n";

            }
        }
    }
    if((ff & VIEWPORT_FLOOR_FLAG_N_EDGING) &&
	(ff & VIEWPORT_FLOOR_FLAG_E_EDGING)) {
        if ((type_n == 1 || type_n == 2 || type_n == 8 || type_n == 9) && (type_e == 1 || type_e == 2 || type_e == 8 || type_e == 9)) {
            df::coord edge_pos = df::coord(-1, -1, 0);
            uint16_t tt = 0;
            uint16_t col = 0;
            int bit_shift = 0;
            if (type_n == 1) {tt = tt_n; col=colorn; bit_shift=VIEWPORT_FLOOR_FLAG_N_EDGING_SHIFT;}
            else if (type_e == 1) {tt = tt_e; col=colore; bit_shift=VIEWPORT_FLOOR_FLAG_E_EDGING_SHIFT;}
            else if (type_n == 8 && (type_e != 2 && type_e != 9)) {tt = tt_n; col=colorn; bit_shift=VIEWPORT_FLOOR_FLAG_N_EDGING_SHIFT;}
            else if (type_e == 8 && (type_n != 2 && type_n != 9)) {tt = tt_e; col=colore; bit_shift=VIEWPORT_FLOOR_FLAG_E_EDGING_SHIFT;}
            else if (type_n == 2 && type_e != 9) {tt = tt_n; col=colorn; bit_shift=VIEWPORT_FLOOR_FLAG_N_EDGING_SHIFT;}
            else if (type_e == 2 && type_n != 9) {tt = tt_e; col=colore; bit_shift=VIEWPORT_FLOOR_FLAG_E_EDGING_SHIFT;}
            else if (type_n == 9) {tt = tt_n; col=colorn; bit_shift=VIEWPORT_FLOOR_FLAG_N_EDGING_SHIFT;}
            else if (type_e == 9) {tt = tt_e; col=colore; bit_shift=VIEWPORT_FLOOR_FLAG_E_EDGING_SHIFT;}
            if (tt) {
                TexposHandle edge_texture = getCustomEdgingTexture(tt, pair_up_right, col, bit_shift, 2);
                if (edge_texture) edging_texposes.push_back(edge_texture);
//std::cerr << "NE\n";

            }
        }
    }
    if ((ff & VIEWPORT_FLOOR_FLAG_S_EDGING) &&
	(ff & VIEWPORT_FLOOR_FLAG_E_EDGING)) {
        if ((type_s == 1 || type_s == 2 || type_s == 8 || type_s == 9) && (type_e == 1 || type_e == 2 || type_e == 8 || type_e == 9)) {
            uint16_t tt = 0;
            uint16_t col = 0;
            int bit_shift = 0;
            if (type_s == 1) {tt = tt_s; col=colors; bit_shift=VIEWPORT_FLOOR_FLAG_S_EDGING_SHIFT;}
            else if (type_e == 1) {tt = tt_e; col=colore; bit_shift=VIEWPORT_FLOOR_FLAG_E_EDGING_SHIFT;}
            else if (type_s == 8 && (type_e != 2 && type_e != 9)) {tt = tt_s; col=colors; bit_shift=VIEWPORT_FLOOR_FLAG_S_EDGING_SHIFT;}
            else if (type_e == 8 && (type_s != 2 && type_s != 9)) {tt = tt_e; col=colore; bit_shift=VIEWPORT_FLOOR_FLAG_E_EDGING_SHIFT;}
            else if (type_s == 2 && type_e != 9) {tt = tt_s; col=colors; bit_shift=VIEWPORT_FLOOR_FLAG_S_EDGING_SHIFT;}
            else if (type_e == 2 && type_s != 9) {tt = tt_e; col=colore; bit_shift=VIEWPORT_FLOOR_FLAG_E_EDGING_SHIFT;}
            else if (type_s == 9) {tt = tt_s; col=colors; bit_shift=VIEWPORT_FLOOR_FLAG_S_EDGING_SHIFT;}
            else if (type_e == 9) {tt = tt_e; col=colore; bit_shift=VIEWPORT_FLOOR_FLAG_E_EDGING_SHIFT;}
            if (tt) {
                TexposHandle edge_texture = getCustomEdgingTexture(tt, pair_down_right, col, bit_shift, 3);
//std::cerr << "SE_EDGE type_s == " << (int32_t)type_s << " type_e == " << (int32_t)type_e << " x: " << x << " y: " << y << "\n";
                if (edge_texture) edging_texposes.push_back(edge_texture);
//std::cerr << "SE\n";
            }
        }
    }
    if ((ff & VIEWPORT_FLOOR_FLAG_S_EDGING) &&
        (ff & VIEWPORT_FLOOR_FLAG_W_EDGING)) {
        if ((type_s == 1 || type_s == 2 || type_s == 8 || type_s == 9) && (type_w == 1 || type_w == 2 || type_w == 8 || type_w == 9)) {
            uint16_t tt = 0;
            uint16_t col = 0;
            int bit_shift = 0;
            if (type_s == 1) {tt = tt_s; col=colors; bit_shift=VIEWPORT_FLOOR_FLAG_S_EDGING_SHIFT;}
            else if (type_w == 1) {tt = tt_w; col=colorw; bit_shift=VIEWPORT_FLOOR_FLAG_W_EDGING_SHIFT;}
            else if (type_s == 8 && (type_w != 2 && type_w != 9)) {tt = tt_s; col=colors; bit_shift=VIEWPORT_FLOOR_FLAG_S_EDGING_SHIFT;}
            else if (type_w == 8 && (type_s != 2 && type_s != 9)) {tt = tt_w; col=colorw; bit_shift=VIEWPORT_FLOOR_FLAG_W_EDGING_SHIFT;}
            else if (type_s == 2 && type_w != 9) {tt = tt_s; col=colors; bit_shift=VIEWPORT_FLOOR_FLAG_S_EDGING_SHIFT;}
            else if (type_w == 2 && type_s != 9) {tt = tt_w; col=colorw; bit_shift=VIEWPORT_FLOOR_FLAG_W_EDGING_SHIFT;}
            else if (type_s == 9) {tt = tt_s; col=colors; bit_shift=VIEWPORT_FLOOR_FLAG_S_EDGING_SHIFT;}
            else if (type_w == 9) {tt = tt_w; col=colorw; bit_shift=VIEWPORT_FLOOR_FLAG_W_EDGING_SHIFT;}
            if (tt) {
                TexposHandle edge_texture = getCustomEdgingTexture(tt, pair_down_left, col, bit_shift, 4);
                if (edge_texture) edging_texposes.push_back(edge_texture);
//std::cerr << "SW\n";
            }
        }
    }

    TexposHandle combined_texture = 0;

    //std::cerr << "Edging texpos size=" << edging_texposes.size() << "\n";
    if(!edging_texposes.empty()) {
        //std::cerr << "AT edging_texposes not empty\n";
//std::cerr << "B4 comb edge\n";
        combined_texture = combineHandles(edging_texposes);
//std::cerr << "After comb edge\n";

        //std::cerr << "AFTER edging_texposes not empty for x: " << x << " y: " << y << " z: " << z << " ff: " << ff << "\n";
    }
    if (ff && combined_texture == 0) {
        //std::cerr << "Relost CombinedEdgeTile for x: " << x << " y: " << y << " z: " << z << " ff: " << ff << " edge_size: " << edging_texposes.size() << "\n";
        //std::cerr << "  tt_n: " << tt_n << " colorn: " << colorn << " type_n: " << type_n << "\n"; 
    }
    return combined_texture;
}

TexposHandle getCustomFloorTexture(std::vector<TexposHandle> handles, int idx, int fixed_color, graphic_viewportst* vp, int32_t x, int32_t y, int z, int32_t wx, int32_t wy, uint16_t tt_val, bool soil, std::pair<int, int> mat_color) {
    using namespace df::enums::tiletype;

    int color;

    if (fixed_color > 0) {
        color = fixed_color;
    } else {
        color = mat_color.second;
    }

    df::palettest material_palette = df::global::world->raws.descriptors.colors[color]->palette;
    std::string token = df::global::world->raws.inorganics.all[mat_color.first]->id;

    if ((token == "SAND_YELLOW" || token == "SAND_RED" || token == "SAND_BLACK" || token == "SAND_WHITE" || token == "SAND_TAN")) {
        return 0;
    }

    uint64_t flag_val = vp->screentexpos_ramp_flag[x * vp->dim_y + y];

    TexposHandle swapped_texpos = handles[idx];
    if (flag_val) {
        swapped_texpos = getCustomRampTexture(handles, flag_val, base_inorganic_palette, tt_val, color);
        //std::cerr << "At getCustomFloorTexture FLAGGO: " << swapped_texpos << "\n";
    } else {
        swapped_texpos = swapPalettes(swapped_texpos, base_inorganic_palette, material_palette);
        //std::cerr << "At getCustomFloorTexture NORMALLO: " << swapped_texpos << "\n";
    }

    //std::cerr << "At getCustomFloorTexture: " << swapped_texpos << "\n";

    return swapped_texpos;
}

TexposHandle getBaseSquareTexposHandle(int32_t x, int32_t y, int32_t z, int32_t wx, int32_t wy, df::graphic_viewportst *vp, uint16_t tt, std::pair<int, int> mat_color) {
        using namespace df::enums::tiletype;

        int tileoffset = -1;
        std::vector<TexposHandle> handles;
        int fixed_color = -1;
        bool is_soil = false;

        switch(tt + 1) {
            case df::tiletype::StonePebbles1:
            case df::tiletype::LavaPebbles1:
            case df::tiletype::FeaturePebbles1:
                tileoffset = 0;
                handles = set_ppebbles;
                break;
            case df::tiletype::StonePebbles2:
            case df::tiletype::LavaPebbles2:
            case df::tiletype::FeaturePebbles2:
                tileoffset = 1;
                handles = set_ppebbles;
                break;
            case df::tiletype::StonePebbles3:
            case df::tiletype::LavaPebbles3:
            case df::tiletype::FeaturePebbles3:
                tileoffset = 2;
                handles = set_ppebbles;
                break;
            case df::tiletype::StonePebbles4:
            case df::tiletype::LavaPebbles4:
            case df::tiletype::FeaturePebbles4:
                tileoffset = 3;
                handles = set_ppebbles;
                break;
            case df::tiletype::StoneFloor2:
            case df::tiletype::LavaFloor2:
            case df::tiletype::MineralFloor2 + 1:
            case df::tiletype::FeatureFloor2:
                tileoffset = 37;
                handles = set_palt_floor;
                break;
            case df::tiletype::StoneFloor1:
            case df::tiletype::LavaFloor1:
            case df::tiletype::MineralFloor1 + 1:
            case df::tiletype::FeatureFloor1:
                tileoffset = 43;
                handles = set_palt_floor;
                break;
            case df::tiletype::StoneFloor3:
            case df::tiletype::LavaFloor3:
            case df::tiletype::MineralFloor3 + 1:
            case df::tiletype::FeatureFloor3:
                tileoffset = 52;
                handles = set_palt_floor;
                break;
            case df::tiletype::StoneFloor4:
            case df::tiletype::LavaFloor4:
            case df::tiletype::MineralFloor4 + 1:
            case df::tiletype::FeatureFloor4:
                tileoffset = 61;
                handles = set_palt_floor;
                break;
            case (df::tiletype::StoneBoulder + 1):
            case (df::tiletype::LavaBoulder + 1):
            //case (df::tiletype::FeatureBoulder + 1):
            case (df::tiletype::SoilFloor1 + 1):
//std::cerr << "Boulder hit " << (int32_t) tt << "\n";
                is_soil = true;
                tileoffset = 64;
                handles = set_palt_floor;
                break;
            case (df::tiletype::SoilFloor2 + 1):
                is_soil = true;
                tileoffset = 70;
                handles = set_palt_floor;
                break;
            case (df::tiletype::SoilFloor3 + 1):
                is_soil = true;
                tileoffset = 79;
                handles = set_palt_floor;
                break;
            case (df::tiletype::SoilFloor4 + 1):
                is_soil = true;
                tileoffset = 88;
                handles = set_palt_floor;
                break;
            case (df::tiletype::GrassDarkFloor1 + 1):
                fixed_color = getGrassColorBiome(df::coord(wx, wy, z));
                tileoffset = 10;
                handles = set_palt_floor;
                break;
            case (df::tiletype::GrassDarkFloor2 + 1):
                fixed_color = getGrassColorBiome(df::coord(wx, wy, z));
                tileoffset = 16;
                handles = set_palt_floor;
                break;
            case (df::tiletype::GrassDarkFloor3 + 1):
                fixed_color = getGrassColorBiome(df::coord(wx, wy, z));
                tileoffset = 25;
                handles = set_palt_floor;
                break;
            case (df::tiletype::GrassDarkFloor4 + 1):
                fixed_color = getGrassColorBiome(df::coord(wx, wy, z));
                tileoffset = 34;
                handles = set_palt_floor;
                break;
            case (df::tiletype::GrassLightFloor1 + 1):
                fixed_color = getGrassColorBiome(df::coord(wx, wy, z));
                tileoffset = 10;
                handles = set_palt_floor;
                break;
            case (df::tiletype::GrassLightFloor2 + 1):
                fixed_color = getGrassColorBiome(df::coord(wx, wy, z));
                tileoffset = 16;
                handles = set_palt_floor;
                break;
            case (df::tiletype::GrassLightFloor3 + 1):
                fixed_color = getGrassColorBiome(df::coord(wx, wy, z));
                tileoffset = 25;
                handles = set_palt_floor;
                break;
            case (df::tiletype::GrassLightFloor4 + 1):
                fixed_color = getGrassColorBiome(df::coord(wx, wy, z));
                tileoffset = 34;
                handles = set_palt_floor;
                break;
            case df::tiletype::GrassDryFloor1:
                fixed_color = dry_grass_color;
                tileoffset = 10;
                handles = set_palt_floor;
                break;
            case df::tiletype::GrassDryFloor2:
                fixed_color = dry_grass_color;
                tileoffset = 16;
                handles = set_palt_floor;
                break;
            case df::tiletype::GrassDryFloor3:
                fixed_color = dry_grass_color;
                tileoffset = 25;
                handles = set_palt_floor;
                break;
            case df::tiletype::GrassDryFloor4:
                fixed_color = dry_grass_color;
                tileoffset = 34;
                handles = set_palt_floor;
                break;
            case df::tiletype::GrassDeadFloor1 + 1:
                fixed_color = dead_grass_color;
                tileoffset = 10;
                handles = set_palt_floor;
                break;
            case df::tiletype::GrassDeadFloor2 + 1:
                fixed_color = dead_grass_color;
                tileoffset = 16;
                handles = set_palt_floor;
                break;
            case df::tiletype::GrassDeadFloor3 + 1:
                fixed_color = dead_grass_color;
                tileoffset = 25;
                handles = set_palt_floor;
                break;
            case df::tiletype::GrassDeadFloor4 + 1:
                fixed_color = dead_grass_color;
                tileoffset = 34;
                handles = set_palt_floor;
                break;
            case df::tiletype::TreeTrunkThickN + 1:
            case df::tiletype::TreeTrunkThickE + 1:
            case df::tiletype::TreeTrunkThickW + 1:
            case df::tiletype::TreeTrunkThickS + 1:
            case df::tiletype::TreeTrunkThickNW + 1:
            case df::tiletype::TreeTrunkThickNE + 1:
            case df::tiletype::TreeTrunkThickSW + 1:
            case df::tiletype::TreeTrunkThickSE + 1:
            case df::tiletype::TreeTrunkPillar + 1:
            case (df::tiletype::Shrub + 1):
                fixed_color = getGrassColorBiome(df::coord(wx, wy, z));
                tileoffset = 34;
                handles = set_palt_floor;
                break;
            case (df::tiletype::ShrubDead + 1):
                fixed_color = getGrassColorBiome(df::coord(wx, wy, z));
                tileoffset = 34;
                handles = set_palt_floor;
                break;
            case (df::tiletype::Sapling + 1):
                fixed_color = getGrassColorBiome(df::coord(wx, wy, z));
                tileoffset = 34;
                handles = set_palt_floor;
                break;
            case (df::tiletype::SaplingDead + 1):
                fixed_color = getGrassColorBiome(df::coord(wx, wy, z));
                tileoffset = 34;
                handles = set_palt_floor;
                break;
            case df::tiletype::StoneStairUD + 1:
            case df::tiletype::LavaStairUD + 1:
            case df::tiletype::MineralStairUD + 1:
            case df::tiletype::FeatureStairUD + 1:
                tileoffset = 3;
                handles = set_pstairs;
                break;
            case df::tiletype::StoneStairD + 1:
            case df::tiletype::LavaStairD + 1:
            case df::tiletype::MineralStairD + 1:
            case df::tiletype::FeatureStairD + 1:
                tileoffset = 5;
                handles = set_pstairs;
                break;
            case df::tiletype::StoneStairU + 1:
            case df::tiletype::LavaStairU + 1:
            case df::tiletype::MineralStairU + 1:
            case df::tiletype::FeatureStairU + 1:
                tileoffset = 4;
                handles = set_pstairs;
                break;
            case df::tiletype::SoilStairUD + 1:
                tileoffset = 6;
                handles = set_pstairs;
                break;
            case df::tiletype::SoilStairD + 1:
                tileoffset = 8;
                handles = set_pstairs;
                break;
            case df::tiletype::SoilStairU + 1:
                tileoffset = 7;
                handles = set_pstairs;
                break;
            case df::tiletype::StoneRamp + 1:
            case df::tiletype::LavaRamp + 1:
            case df::tiletype::FeatureRamp + 1:
            case df::tiletype::MineralRamp + 1:
                tileoffset = 0;
                handles = set_pramp_stone;
                break;
            case df::tiletype::SoilRamp + 1:
                tileoffset = 0;
                handles = set_pramp_soil;
                break;
            case df::tiletype::GrassDarkRamp + 1:
                fixed_color = getGrassColorBiome(df::coord(wx, wy, z));
                tileoffset = 0;
                handles = set_pramp_grass;
                break;
            case df::tiletype::GrassLightRamp + 1:
                fixed_color = getGrassColorBiome(df::coord(wx, wy, z));
                tileoffset = 0;
                handles = set_pramp_grass;
                break;
            case df::tiletype::GrassDryRamp + 1:
                fixed_color = dry_grass_color;
                tileoffset = 0;
                handles = set_pramp_grass;
                break;
            case df::tiletype::GrassDeadRamp + 1:
                fixed_color = dead_grass_color;
                tileoffset = 0;
                handles = set_pramp_grass;
                break;
            case df::tiletype::FurrowedSoil + 1:
                tileoffset = 0;
                handles = set_pfurrowed_soil;
                break;
            case df::tiletype::StoneFloorSmooth + 1:
            case df::tiletype::LavaFloorSmooth + 1:
            case df::tiletype::FeatureFloorSmooth + 1:
            case df::tiletype::MineralFloorSmooth + 1:
                tileoffset = 33;
                handles = set_palt_floor;                    
                break;
        }

        if (tileoffset > -1) {
            return getCustomFloorTexture(handles, tileoffset, fixed_color, vp, x, y, z, wx, wy, tt, is_soil, mat_color);
        }
    return 0;
}

TexposHandle updateEngravingsTexposHandle(int32_t x, int32_t y, int32_t z, int32_t wx, int32_t wy, df::graphic_viewportst *vp, uint16_t tt, std::pair<int, int> mat_color) {
    int tileoffset = 0;
    std::vector<TexposHandle> handles = set_pstone_engraved;
    int fixed_color = -1;
    bool is_soil = false;

    TexposHandle th = getCustomFloorTexture(handles, tileoffset, fixed_color, vp, x, y, z, wx, wy, tt, is_soil, mat_color);
    //std::cerr << "At updateEngravingsTexposHandle: " << th << "\n";
    return th;
}

TexposHandle updateScreentexposBackgroundTwoTexposHandle(graphic_viewportst* vp, int32_t x, int32_t y, int32_t z, int32_t vz) {
        df::coord framesafe_viewport_pos = getFramesafeViewportPos();
        int32_t wx = framesafe_viewport_pos.x + x;
        int32_t wy = framesafe_viewport_pos.y + y;

        if (wx < 0 || wy < 0) return 0;

        TexposHandle edging_texposhandle = 0;

        df::tiletype *tt = Maps::getTileType(df::coord(wx, wy, z));
        if (!tt) {
            std::cerr << "TT IS NULL!!! ABANDONING RENDER ATTEMPT THIS FRAME!!! wx: " << (wx+1) << " wy: " << wy << " z: " << z << "\n";
            return 0;
        }
        std::pair<int, int> mat_color = std::pair<int, int>(0, 0);

        uint64_t ff = vp->screentexpos_floor_flag[x * vp->dim_y + y];

        uint64_t color = 0;
        std::vector<uint16_t> cardinal_colors = std::vector<uint16_t>(4, 0);
        std::vector<uint8_t> dir_types = std::vector<uint8_t>(4, 0);
        std::vector<uint16_t> tts = std::vector<uint16_t>(4, 0);

        std::pair<int, int> mat_colorn = std::make_pair(-1, -1);
        std::pair<int, int> mat_colore = std::make_pair(-1, -1);
        std::pair<int, int> mat_colorw = std::make_pair(-1, -1);
        std::pair<int, int> mat_colors = std::make_pair(-1, -1);

        uint16_t ctt = *tt;
        int ecode = getEdgingCode(ctt);

        uint16_t cttn = 0;
        if (wy - 1 >= 0) {
            df::tiletype *temp_ctt = Maps::getTileType(df::coord(wx, wy - 1, z));
            if (!temp_ctt) {
                std::cerr << "CTTN IS NULL!!! ABANDONING RENDER ATTEMPT THIS FRAME!!! wx: " << wx << " wy: " << (wy-1) << " z: " << z << "\n";
            } else {
                cttn = *temp_ctt;
            }
        }
        uint16_t ctte = 0;
        if (wx + 1 < WORLD_TILE_X) {
            df::tiletype *temp_ctt = Maps::getTileType(df::coord(wx + 1, wy, z));
            if (!temp_ctt) {
                std::cerr << "CTTE IS NULL!!! ABANDONING RENDER ATTEMPT THIS FRAME!!! wx: " << (wx+1) << " wy: " << wy << " z: " << z << "\n";
            } else {
                ctte = *temp_ctt;
            }
        }
        uint16_t cttw = 0;
        if (wx - 1 >= 0) {
            df::tiletype *temp_ctt = Maps::getTileType(df::coord(wx - 1, wy, z));
            if (!temp_ctt) {
                std::cerr << "CTTW IS NULL!!! ABANDONING RENDER ATTEMPT THIS FRAME!!! wx: " << (wx-1) << " wy: " << wy << " z: " << z << "\n";
            } else {
                cttw = *temp_ctt;
            }
        }
        uint16_t ctts = 0;
        if (wy + 1 < WORLD_TILE_Y) {
            df::tiletype *temp_ctt = Maps::getTileType(df::coord(wx, wy + 1, z));
            if (!temp_ctt) {
                std::cerr << "CTTS IS NULL!!! ABANDONING RENDER ATTEMPT THIS FRAME!!! wx: " << wx << " wy: " << (wy+1) << " z: " << z << "\n";
            } else {
                ctts = *temp_ctt;
            }
        }

        int necode = getEdgingCode(cttn);
        int eecode = getEdgingCode(ctte);
        int wecode = getEdgingCode(cttw);
        int secode = getEdgingCode(ctts);

        bool compcode = necode || eecode || wecode || secode;

        if (ff) {
            int32_t mapx, mapy, mapz;
            Maps::getTileSize(mapx, mapy, mapz);

            if (mat_color.second == 0) mat_color = getMaterialIdxAndColorByPos(df::coord(wx, wy, z), ctt);

            if (ff & VIEWPORT_FLOOR_FLAG_N_EDGING) {
                if (wy > 0) {
                    mat_colorn = getMaterialIdxAndColorByPos(df::coord(wx, wy - 1, z), cttn);
                } else {
                    mat_colorn = mat_color;
                }
                color |= ((uint64_t)mat_colorn.second << (VIEWPORT_FLOOR_FLAG_N_EDGING_SHIFT * 2));
                cardinal_colors[0] = mat_colorn.second;
                dir_types[0] = (ff & VIEWPORT_FLOOR_FLAG_N_EDGING)>>VIEWPORT_FLOOR_FLAG_N_EDGING_SHIFT;
                tts[0] = cttn;
            }
            color = color << 16;
    
            if (ff & VIEWPORT_FLOOR_FLAG_E_EDGING) {
                if (wx < mapx - 1) {
                    mat_colore = getMaterialIdxAndColorByPos(df::coord(wx + 1, wy, z), ctte);
                } else {
                    mat_colore = mat_color;
                }
                color |= ((uint64_t)mat_colore.second << (VIEWPORT_FLOOR_FLAG_E_EDGING_SHIFT * 2));
                cardinal_colors[1] = mat_colore.second;
                dir_types[1] = (ff & VIEWPORT_FLOOR_FLAG_E_EDGING)>>VIEWPORT_FLOOR_FLAG_E_EDGING_SHIFT;
                tts[1] = ctte;
            }
            color = color << 16;

            if (ff & VIEWPORT_FLOOR_FLAG_W_EDGING) {
                if (wx > 0) {
                    mat_colorw = getMaterialIdxAndColorByPos(df::coord(wx - 1, wy, z), cttw);
                } else {
                    mat_colorw = mat_color;
                }
                color |= ((uint64_t)mat_colorw.second << (VIEWPORT_FLOOR_FLAG_W_EDGING_SHIFT * 2));
                cardinal_colors[2] = mat_colorw.second;
                dir_types[2] = (ff & VIEWPORT_FLOOR_FLAG_W_EDGING)>>VIEWPORT_FLOOR_FLAG_W_EDGING_SHIFT;
                tts[2] = cttw;
            }
            color = color << 16;

            if (ff & VIEWPORT_FLOOR_FLAG_S_EDGING) {
                if (wy < mapy - 1) {
                    mat_colors = getMaterialIdxAndColorByPos(df::coord(wx, wy + 1, z), ctts);
                } else {
                    mat_colors = mat_color;
                }
                color |= ((uint64_t)mat_colors.second << (VIEWPORT_FLOOR_FLAG_S_EDGING_SHIFT * 2));
                cardinal_colors[3] = mat_colors.second;
                dir_types[3] = (ff & VIEWPORT_FLOOR_FLAG_S_EDGING)>>VIEWPORT_FLOOR_FLAG_S_EDGING_SHIFT;
                tts[3] = ctts;
            }
        }
        else if (ecode) {    // check if square is tiletype bieng monitored
            int32_t mapx, mapy, mapz;
            Maps::getTileSize(mapx, mapy, mapz);

            bool ecoden = ecode == necode;
            bool ecodee = ecode == eecode;
            bool ecodew = ecode == wecode;
            bool ecodes = ecode == secode;

            if (mat_color.second == 0) mat_color = getMaterialIdxAndColorByPos(df::coord(wx, wy, z), ctt);
            if (ecoden) {
                if (wy > 0) {
                    mat_colorn = getMaterialIdxAndColorByPos(df::coord(wx, wy - 1, z), cttn);
                } else {
                    mat_colorn = mat_color;
                }
                if (mat_color.second < mat_colorn.second) {
                    color |= ((uint64_t)mat_colorn.second << (VIEWPORT_FLOOR_FLAG_N_EDGING_SHIFT * 2));
                    cardinal_colors[0] = mat_colorn.second;
                    dir_types[0] = ecode;
                    tts[0] = cttn;
                    ff |= (dir_types[0]<<VIEWPORT_FLOOR_FLAG_N_EDGING_SHIFT);
                }
            }
            if (ecodee) {
                if (wx < mapx - 1) {
                    mat_colore = getMaterialIdxAndColorByPos(df::coord(wx + 1, wy, z), ctte);
                } else {
                    mat_colore = mat_color;
                }
                if (mat_color.second < mat_colore.second) {
                    color |= ((uint64_t)mat_colore.second << (VIEWPORT_FLOOR_FLAG_E_EDGING_SHIFT * 2));
                    cardinal_colors[1] = mat_colore.second;
                    dir_types[1] = ecode;
                    tts[1] = ctte;
                    ff |= (dir_types[1]<<VIEWPORT_FLOOR_FLAG_E_EDGING_SHIFT);
                }
            }
            if (ecodew) {
                if (wx > 0) {
                    mat_colorw = getMaterialIdxAndColorByPos(df::coord(wx - 1, wy, z), cttw);
                } else {
                    mat_colorw = mat_color;
                }
                if (mat_color.second < mat_colorw.second) {
                    color |= ((uint64_t)mat_colorw.second << (VIEWPORT_FLOOR_FLAG_W_EDGING_SHIFT * 2));
                    cardinal_colors[2] = mat_colorw.second;
                    dir_types[2] = ecode;
                    tts[2] = cttw;
                    ff |= (dir_types[2]<<VIEWPORT_FLOOR_FLAG_W_EDGING_SHIFT);
                }
            }
            if (ecodes) {
                if (wy < mapy - 1) {
                    mat_colors = getMaterialIdxAndColorByPos(df::coord(wx, wy + 1, z), ctts);
                } else {
                    mat_colors = mat_color;
                }
                if (mat_color.second < mat_colors.second) {
                    color |= ((uint64_t)mat_colors.second << (VIEWPORT_FLOOR_FLAG_S_EDGING_SHIFT * 2));
                    cardinal_colors[3] = mat_colors.second;
                    dir_types[3] = ecode;
                    tts[3] = ctts;
                    ff |= (dir_types[3]<<VIEWPORT_FLOOR_FLAG_S_EDGING_SHIFT);
                }
            }
        }

        CompTileKey comptile_edge_key = CompTileKey(0, 0, color, ff);

        if (compcode || ff) {
            std::unordered_map<CompTileKey, TexposHandle>::iterator it = tile_floor_comptile_cache.find(comptile_edge_key);
            if (it != tile_floor_comptile_cache.end()) {
                edging_texposhandle = it->second;
            } else {
                edging_texposhandle = getCombinedCustomEdgingTexture(wx, wy, z, ff, cardinal_colors, dir_types, tts, color);
                if (edging_texposhandle) {
                    //std::cerr << "Caching EdgeTile for x: " << wx << " y: " << wy << " z: " << z << " ff: " << ff << "\n";
                    tile_floor_comptile_cache[comptile_edge_key] = edging_texposhandle;
                }
            }
        } else {
            return 0;
        }

        //if (ff && edging_texposhandle == 0) {
        //    std::cerr << "Missing EdgeTile for x: " << wx << " y: " << wy << " z: " << z << " ff: " << ff << "\n";
        //}

        // Add other background two elements.
        int32_t background_two_texpos = vp->screentexpos_background_two[x * vp->dim_y + y];
        if (background_two_texpos) {
            TexposHandle background_two_handle;
            std::unordered_map<uint32_t, TexposHandle>::iterator it = background_two_cache.find(background_two_texpos);
            if (it != background_two_cache.end()) {
                //std::cerr << "background_two_cache hit\n!";
                background_two_handle = it->second;
            } else {
                //std::cerr << "background_two_cache miss " << background_two_texpos << " x: " << wx << " y: " << wy << "\n";
                background_two_handle = loadTexture(getSurfaceByTexpos(background_two_texpos));
                background_two_cache[background_two_texpos] = background_two_handle;
            }
            BackgroundTwoKey bgk = BackgroundTwoKey(color, ff, (uint64_t)background_two_handle);
            std::unordered_map<BackgroundTwoKey, TexposHandle>::iterator it2 = background_two_full_cache.find(bgk);
            if (it2 != background_two_full_cache.end()) {
                //std::cerr << "background_two_full_cache hit\n!";
                edging_texposhandle = it2->second;
            } else {
                //std::cerr << "background_two_full_cache miss\n!";

                std::vector<TexposHandle> edging_texposes;
                edging_texposes.push_back(edging_texposhandle);
                edging_texposes.push_back(background_two_handle);
                edging_texposhandle = combineHandles(edging_texposes);
                background_two_full_cache[bgk] = background_two_handle;
            }
        }

        return edging_texposhandle;
}

// Should this be done on a block basis?
TexposHandle updateScreentexposBackgroundTexposHandle(graphic_viewportst* vp, int32_t x, int32_t y, int32_t z, int32_t vz, bool engraved) {
        df::coord framesafe_viewport_pos = getFramesafeViewportPos();
        int32_t wx = framesafe_viewport_pos.x + x;
        int32_t wy = framesafe_viewport_pos.y + y;

        if (wx < 0 || wy < 0) return 0;

        //std::cerr << "Inside updateScreentexposBackgroundTexposHandle x=" << x << " y=" << y << " z=" << z << "\n";
        //std::cerr << "  wx=" << wx << " wy=" << wy << " vz=" << vz << "\n";

        // TODO: make sure we are not in complete air. Handle see through tree leaves.
        //std::cerr << "  address: " << std::hex << vp << std::dec << std::endl;

        bool in_view = false;
        for (int depth = 0; depth <= vz; ++depth) {
            //std::cerr << "  looking through depth=" << depth << "\n";
            //std::cerr << "    address: " << std::hex << df::global::gps->viewport[depth] << std::dec << std::endl;
            if (df::global::gps->viewport[depth] == vp) {
                in_view = true;
                break;
            }
        }
        if (!in_view) {
            //std::cerr << "Background exit 1\n";
            return 0;
        }

        TexposHandle base_square_texposhandle = 0;

        df::graphic_viewportst* dvp = df::global::gps->viewport[vz];
        df::tiletype *tt = NULL;
        std::pair<int, int> mat_color = std::pair<int, int>(0, 0);

        if (dvp == vp) {
            df::tiletype *tt = Maps::getTileType(wx, wy, z);
            if (!tt) {
                std::cerr << "TT IS NULL!!! ABANDONING RENDER ATTEMPT THIS FRAME!!! wx: " << wx << " wy: " << wy << " z: " << z << "\n";
                return 0;
            }
            uint16_t tt_val = (uint16_t) *tt;
            if (engraved) tt_val = 1001;

            std::pair<int, int> mat_color = getMaterialIdxAndColorByPos(df::coord(wx, wy, z), tt_val);

            uint64_t ramp_flag = vp->screentexpos_ramp_flag[x * vp->dim_y + y];

            CompTileKey comptile_key = CompTileKey(tt_val, mat_color.second, 0, ramp_flag);

            std::unordered_map<CompTileKey, TexposHandle>::iterator it = tile_floor_comptile_cache.find(comptile_key);
            if (it != tile_floor_comptile_cache.end()) {
                //std::cerr << "Fetching Floor Tile for x: " << wx << " y: " << wy << " z: " << z << "\n";
                base_square_texposhandle = it->second;
            } else {
                //std::cerr << "Searching FloorTile for x: " << wx << " y: " << wy << " z: " << z << "\n";
                if (engraved) base_square_texposhandle = updateEngravingsTexposHandle(x, y, z, wx, wy, vp, tt_val, mat_color);
                else base_square_texposhandle = getBaseSquareTexposHandle(x, y, z, wx, wy, vp, tt_val, mat_color);
                if (base_square_texposhandle) {
                    //std::cerr << "Caching Floor Tile for x: " << wx << " y: " << wy << " z: " << z << "\n";
                    tile_floor_comptile_cache[comptile_key] = base_square_texposhandle;
                }
            }
        }
        //std::cerr << "Background exit 2: " << getTexposByHandle(base_square_texposhandle) << "\n";
        return base_square_texposhandle;
}

int getDepthAtNoTree(int32_t x, int32_t y)
{
    df::coord framesafe_viewport_pos = Textures::getFramesafeViewportPos();

    int vx = framesafe_viewport_pos.x;
    int vy = framesafe_viewport_pos.y;
    int vz = framesafe_viewport_pos.z;

    int local_x = x - vx;
    int local_y = y - vy;

    if (x < vx || x >= df::global::gps->viewport[0]->dim_x + vx || y < vy || y >= df::global::gps->viewport[0]->dim_y + vy)
        return 0;

    const size_t num_viewports = 9;

    for (int depth = 0; depth < num_viewports; ++depth) {
        if (df::global::gps->viewport[depth]->screentexpos_background[local_x * df::global::gps->viewport[depth]->dim_y + local_y]) {
            int32_t tti = (int32_t)*Maps::getTileType(df::coord(x, y, vz - depth));
            if (tti < 71 || tti > 226) {
            //&& tileMaterial(*Maps::getTileType(df::coord(x, y, vz - depth))) != df::enums::tiletype_material::tiletype_material::TREE) {
                return depth;
            }
        }
    }
    return 0;
}

void forceAdjacentUpdate(int32_t x, int32_t y, int32_t z, int32_t bx, int32_t by, LRUCache<df::coord, LRUCacheBlock*> &cache, LRUCacheBlock* block) {
    //std::cerr << "forceAdjacent\n";
    bool north = by - 1 >= 0;
    bool east = bx + 1 < 16;
    bool west = bx - 1 >= 0;
    bool south = by + 1 < 16;

    LRUCacheBlock *nblock;

    df::tiletype update = (df::tiletype)65535;

    if (north) { // north
        block->tiletype[bx][by+1] = update;
        if (west) { // northwest
            block->tiletype[bx-1][by+1] = update;
        }
        else {
            if(cache.get(df::coord(x-1, y, z), nblock)) {
               nblock->tiletype[15][by+1] = update;
            }
        }
        if (east) { // northeast
            block->tiletype[bx+1][by+1] = update;
        }
        else {
            if(cache.get(df::coord(x+1, y, z), nblock)) {
               nblock->tiletype[0][by+1] = update;
            }
        }
    }
    else {
        if(cache.get(df::coord(x, y-1, z), nblock)) {
            nblock->tiletype[bx][15] = update;
        }
        if (west) {
            block->tiletype[bx-1][15] = update;
        }
        else {
            if(cache.get(df::coord(x-1, y-1, z), nblock)) {
               nblock->tiletype[15][15] = update;
            }
        }
        if (east) {
            block->tiletype[bx+1][15] = update;
        }
        else {
            if(cache.get(df::coord(x+1, y-1, z), nblock)) {
               nblock->tiletype[0][15] = update;
            }
        }
    }
    if (south) {
        block->tiletype[bx][by-1] = update;
        if (west) {
            block->tiletype[bx-1][by-1] = update;
        }
        else {
            if(cache.get(df::coord(x-1, y, z), nblock)) {
               nblock->tiletype[15][by-1] = update;
            }
        }
        if (east) {
            block->tiletype[bx+1][by-1] = update;
        }
        else {
            if(cache.get(df::coord(x+1, y, z), nblock)) {
               nblock->tiletype[0][by-1] = update;
            }
        }
    }
    else {
        if(cache.get(df::coord(x, y+1, z), nblock)) {
            nblock->tiletype[bx][0] = update;
        }
        if (west) {
            block->tiletype[bx-1][0] = update;
        }
        else {
            if(cache.get(df::coord(x-1, y+1, z), nblock)) {
               nblock->tiletype[15][0] = update;
            }
        }
        if (east) { //southeast
            block->tiletype[bx+1][0] = update;
        }
        else {
            if(cache.get(df::coord(x+1, y+1, z), nblock)) {
               nblock->tiletype[0][0] = update;
            }
        }
    }
    if (east) block->tiletype[bx+1][by] = update;
    else {
        if(cache.get(df::coord(x+1, y, z), nblock)) {
            nblock->tiletype[0][by] = update;
        }
    }
    if (west) block->tiletype[bx-1][by] = update;
    else {
        if(cache.get(df::coord(x-1, y, z), nblock)) {
            nblock->tiletype[15][by] = update;
        }
    }
}

struct ViewportRendererInterpose : public df::renderer_2d {
    typedef renderer_2d interpose_base;

    DEFINE_VMETHOD_INTERPOSE(void, update_full_viewport,
        (graphic_viewportst* vp)) {

        df::coord vpos = getFramesafeViewportPos();

        // get viewport z
        int32_t z = 0;
        int32_t zval = 0;
        for (int depth = 0; depth < 9; ++depth) {
            if (df::global::gps->viewport[depth] == vp) {
                z = vpos.z - depth;
                zval = depth;
                break;
            }
        }

        int viewport_size = df::global::gps->viewport.size();
        df::graphic_viewportst *mvp = df::global::gps->main_viewport;
        df::graphic_viewportst *lvp = df::global::gps->viewport[viewport_size - 1];

        cuboid vcuboid = cuboid(std::max(vpos.x, (int16_t)0), std::max(vpos.y, (int16_t)0), z, std::max(vpos.x + vp->dim_x - 1, (vp->dim_x - 1)), std::max(vpos.y + vp->dim_y - 1, vp->dim_y - 1), z);
        //std::cerr << "Init Vcuboid is xmin=" << vcuboid.x_min << " xmax=" << vcuboid.x_max << " ymin=" << vcuboid.y_min << " ymax=" << vcuboid.y_max << "\n";

        //vcuboid = vcuboid.clampMap(false);

        auto &cache = lrucache;

        //std::cerr << "Vcuboid is xmin=" << vcuboid.x_min << " xmax=" << vcuboid.x_max << " ymin=" << vcuboid.y_min << " ymax=" << vcuboid.y_max << "\n";

        vcuboid.forBlock([this, vp, &vcuboid, &cache, &z, &zval, &vpos, &lvp](df::map_block *block, cuboid block_region) {
            LRUCacheBlock *lrublock;
            cuboid bcuboid = cuboid(block);
            bcuboid.clamp(vcuboid);
            int32_t bx;
            int32_t by;
            int32_t vx;
            int32_t vy;

            LRUCacheBlock *mlrublock;
            //uint8_t (*depth)[16];
            uint8_t depth[16][16] = {0};
            uint8_t max_depth = 0;

            if (lvp == vp) {
                //std::cerr << "ITER lvp TRUE: zval=" << zval << "\n";
                if (!cache.get(df::coord(block->map_pos.x, block->map_pos.y, vpos.z), mlrublock)) {
                    mlrublock = new LRUCacheBlock();
                    for (int x = bcuboid.x_min; x <= bcuboid.x_max; ++x) {
                        for (int y = bcuboid.y_min; y <= bcuboid.y_max; ++y) {
                            bx = x & 15;
                            by = y & 15;
                            int d = getDepthAtNoTree(x, y);
                            mlrublock->depth[bx][by] = d;
                            depth[bx][by] = d;
                            if (d > mlrublock->max_depth) mlrublock->max_depth = d;
                        }
                    }
                    max_depth = mlrublock->max_depth;
                    cache.put(block->map_pos, mlrublock);
                } else {
                    for (int x = bcuboid.x_min; x <= bcuboid.x_max; ++x) {
                        for (int y = bcuboid.y_min; y <= bcuboid.y_max; ++y) {
                            bx = x & 15;
                            by = y & 15;
                            depth[bx][by] = mlrublock->depth[bx][by];
                        }
                    }
                    max_depth = mlrublock->max_depth;
                }
            } else {
                //std::cerr << "ITER lvp FALSE: zval=" << zval << "\n";
                if(!cache.get(df::coord(block->map_pos.x, block->map_pos.y, vpos.z), mlrublock)) {
                    mlrublock = new LRUCacheBlock();
                    //std::cerr << "Couldn't get block for depth calculations!\n";
                    for (int x = bcuboid.x_min; x <= bcuboid.x_max; ++x) {
                        for (int y = bcuboid.y_min; y <= bcuboid.y_max; ++y) {
                            bx = x & 15;
                            by = y & 15;
                            int d = getDepthAtNoTree(x, y);
                            //std::cerr << "INSIDE PROB LOOP d=" << d << " bx=" << bx << " by=" << by << "\n";
                            mlrublock->depth[bx][by] = d;
                            depth[bx][by] = d;
                            if (d > mlrublock->max_depth) mlrublock->max_depth = d;
                        }
                    }
                    max_depth = mlrublock->max_depth;
                }
            }

            //std::cerr << "max_depth =" << (int32_t)max_depth << "\n";

            if (zval <= max_depth) {
            if (!cache.get(block->map_pos, lrublock)) {
                lrublock = new LRUCacheBlock();
                for (int x = bcuboid.x_min; x <= bcuboid.x_max; ++x) {
                    for (int y = bcuboid.y_min; y <= bcuboid.y_max; ++y) {
                        bx = x & 15;
                        by = y & 15;
                        vx = x - vcuboid.x_min;
                        vy = y - vcuboid.y_min;
                        int d = getDepthAtNoTree(x, y); //NoTree(x, y, vpos.z);
                        lrublock->depth[bx][by] = d;
                        if (d > max_depth) lrublock->max_depth = d;
                        if (depth[bx][by] >= zval) {
                            lrublock->tiletype[x&15][y&15] = block->tiletype[x&15][y&15];
                            if (TexposHandle background = updateScreentexposBackgroundTexposHandle(vp, vx, vy, z, zval, false))
                                lrublock->screentexpos_background[bx][by] = background;
                            if (TexposHandle background_two = updateScreentexposBackgroundTwoTexposHandle(vp, vx, vy, z, zval)) {
                                lrublock->screentexpos_background_two[bx][by] = background_two;
                                //forceAdjacentUpdate(x, y, z, bx, by, cache, lrublock);
                            }
                        }
                        //std::cerr << "Writing Tile: x=" << x << " y=" << y << "\n";
                        //std::cerr << "  screentexpos_background[bx][by]=" << lrublock->screentexpos_background[bx][by] << "\n";
                        //std::cerr << "  screentexpos_background_two[bx][by]=" << lrublock->screentexpos_background_two[bx][by] << "\n";
                    }
                }
                cache.put(block->map_pos, lrublock);
                if(max_depth < zval) {
                    return true;
                }
            }
            else {
                if (memcmp(lrublock->tiletype, block->tiletype, 512)) {
                //std::cerr << "Updating Block at x=" << block->map_pos.x << " y=" << block->map_pos.y << " z=" << block->map_pos.z << "\n";
                    for (int x = bcuboid.x_min; x <= bcuboid.x_max ; ++x) {
                        for (int y = bcuboid.y_min; y <= bcuboid.y_max ; ++y) {
                            bx = x & 15;
                            by = y & 15;
                            vx = x - vcuboid.x_min;
                            vy = y - vcuboid.y_min;
                            if (lrublock->tiletype[bx][by] != block->tiletype[bx][by]) {
                                if (depth[bx][by] >= zval) {
                                    if (TexposHandle background = updateScreentexposBackgroundTexposHandle(vp, vx, vy, z, zval, false))
                                        lrublock->screentexpos_background[bx][by] = background;
                                    if (TexposHandle background_two = updateScreentexposBackgroundTwoTexposHandle(vp, vx, vy, z, zval))
                                        lrublock->screentexpos_background_two[bx][by] = background_two;
                                    forceAdjacentUpdate(x, y, z, bx, by, cache, lrublock);
                                }
                            }
                        }
                    }
                } else {
                    std::cerr << "memcmp ELSE hit x=" << block->map_pos.x << " y=" << block->map_pos.y << " z=" << block->map_pos.z << "\n";
                }
            }

            for (int x = bcuboid.x_min; x <= bcuboid.x_max ; ++x) {
                for (int y = bcuboid.y_min; y <= bcuboid.y_max ; ++y) {
                    bx = x & 15;
                    by = y & 15;
                    vx = x - vcuboid.x_min;
                    vy = y - vcuboid.y_min;
                    if (lrublock->screentexpos_background[bx][by])
                        vp->screentexpos_background[vx * vp->dim_y + vy] = getTexposByHandle(lrublock->screentexpos_background[bx][by]);
                    if (lrublock->screentexpos_background_two[bx][by])
                        vp->screentexpos_background_two[vx * vp->dim_y + vy] = getTexposByHandle(lrublock->screentexpos_background_two[bx][by]);
                }
            }
            }

            return true; // return false to stop iteration early
        });

        // Update ungraving graphics since they have same tiletype as smooth floors
        if (lrucache.engravings_count < df::global::world->event.engravings.size()) {
            for (int i = lrucache.engravings_count; i < df::global::world->event.engravings.size(); ++i) {
                lrucache.engravings_cuboid->addPos(df::global::world->event.engravings[i]->pos);
            }
            lrucache.engravings_count = df::global::world->event.engravings.size();
        }

        if (vcuboid.clampNew(*lrucache.engravings_cuboid).isValid()) {
            for (int i = 0; i < df::global::world->event.engravings.size(); ++i) {
                df::coord pos = df::global::world->event.engravings[i]->pos;
                if (vcuboid.containsPos(pos)) {
                    int vposx = pos.x - vcuboid.x_min;
                    int vposy = pos.y - vcuboid.y_min;
                    //std::cerr << "GOT ENGRAVING x=" << pos.x << " y=" << pos.y << " z=" << pos.z << "\n";
                    vp->screentexpos_background[vposx * vp->dim_y + vposy] =
                        getTexposByHandle(updateScreentexposBackgroundTexposHandle(vp, vposx, vposy, z, zval, true));
                    //if (TexposHandle background_two = updateScreentexposBackgroundTwoTexposHandle(vp, vposx, vposy, z, zval)) {
                    //    vp->screentexpos_background_two[vposx * vp->dim_y + vposy] = background_two;
                    //}
                }
            }
        }

        INTERPOSE_NEXT(update_full_viewport)(vp);
    }
};
IMPLEMENT_VMETHOD_INTERPOSE(ViewportRendererInterpose, update_full_viewport);

DFhackCExport command_result plugin_shutdown(color_ostream &out)
{
    return CR_OK;
}

DFhackCExport command_result plugin_onupdate(color_ostream &out)
{
    return CR_OK;
}

command_result myplugin_cmd(color_ostream &out, std::vector<std::string> & args)
{
    bool enable = true;

    if (!args.empty()) {
        if (args[0] == "0" || args[0] == "false" || args[0] == "off" || args[0] == "disable")
            enable = false;
    }

    if (enable) {
        Maps::getTileSize(WORLD_TILE_X, WORLD_TILE_Y, WORLD_TILE_Z);

        base_organic_palette = getBasicOrganicPalette();
        base_inorganic_palette = getBasicInorganicPalette();

        set_pfloor = palettizeTilePage("FLOORS", base_inorganic_palette, false);
        set_pstone_engraved = palettizeTilePage("FLOOR_STONE_ENGRAVED_NON_PALETTE", base_inorganic_palette, false);
        set_palt_floor = palettizeTilePage("ALT_FLOORS", base_inorganic_palette, false);
        set_pfurrowed_soil = palettizeTilePage("FLOOR_FURROWED_SOIL", base_inorganic_palette, false);
        set_pstairs = palettizeTilePage("STAIRS", base_inorganic_palette, false);
        set_ppebbles = palettizeTilePage("FLOOR_PEBBLES", base_inorganic_palette, false);
        set_pramp_stone = palettizeTilePage("STONE_RAMPS", base_inorganic_palette, false);
        set_pramp_soil = palettizeTilePage("SOIL_RAMPS", base_inorganic_palette, true);
        set_pramp_grass = palettizeTilePage("GRASS_RAMPS", base_inorganic_palette, true);

         if (!INTERPOSE_HOOK(ViewportRendererInterpose, update_full_viewport).apply(true)) {
            out.printerr("Failed to interpose ViewportRendererInterpose on update_full_viewport.\n");
            return CR_FAILURE;
        }
        out.print("Interpose on update_viewport_tile enabled.\n");
    } else {
        INTERPOSE_HOOK(ViewportRendererInterpose, update_full_viewport).apply(false);

        tile_floor_comptile_cache.clear();
        tile_floor_texpos_cache.clear();
        tile_floor_edge_cache.clear();
        background_two_cache.clear();
        background_two_full_cache.clear();
        set_pfloor.clear();
        set_pstone_engraved.clear();
        set_palt_floor.clear();
        set_pfurrowed_soil.clear();
        set_pstairs.clear();
        set_ppebbles.clear();
        set_pramp_stone.clear();
        set_pramp_soil.clear();
        set_pramp_grass.clear();
        lrucache.clear();
        out.print("Interpose on draw_building disabled.\n");
    }
    return CR_OK;
}

DFhackCExport command_result plugin_init(color_ostream &out, std::vector<PluginCommand> &commands)
{
commands.push_back(PluginCommand(
    "myplugin",
    "Intercept building rendering",
    myplugin_cmd
));

    return CR_OK;
}
