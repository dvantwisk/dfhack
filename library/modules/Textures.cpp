#include <algorithm>
#include <atomic>
#include <mutex>
#include <numeric>
#include <unordered_map>

#include "Internal.h"

#include "modules/DFSDL.h"
#include "modules/Textures.h"

#include "Debug.h"
#include "PluginManager.h"
#include "VTableInterpose.h"

#include "df/enabler.h"
#include "df/graphic.h"
#include "df/graphic_viewportst.h"
#include "df/renderer.h"
#include "df/renderer_2d.h"
#include "df/tile_pagest.h"
#include "df/texture_handlerst.h"
#include "df/viewscreen_adopt_regionst.h"
#include "df/viewscreen_loadgamest.h"
#include "df/viewscreen_new_arenast.h"
#include "df/viewscreen_new_regionst.h"

#include <SDL_surface.h>
#include <SDL_pixels.h>

using df::global::enabler;
using namespace DFHack;
using namespace DFHack::DFSDL;

namespace DFHack {
DBG_DECLARE(core, textures, DebugCategory::LINFO);
}

struct ReservedRange {
    void init(int32_t start) {
        this->start = start;
        this->end = start + ReservedRange::size;
        this->current = start;
        this->is_installed = true;
    }
    long get_new_texpos() {
        if (this->current == this->end)
            return -1;
        return this->current++;
    }

    static const int32_t size = 10000; // size of reserved texpos buffer
    int32_t start = -1;
    int32_t end = -1;
    long current = -1;
    bool is_installed = false;
};

static const size_t NUM_VIEWPORTS = 9;

static df::coord framesafe_viewport_pos_old = df::coord(-1000, -1000, -1000);
static df::coord framesafe_viewport_pos = df::coord(-1000, -1000, -1000);


const uint8_t basic_organic_color_array[27] = {
    // Organic colors
    47, 48, 56,    // Color 0
    70, 62, 61,    // Color 1
    78, 71, 67,   // Color 2
    86, 79, 73,  // Color 3
    96, 88, 82, // Color 4
    116, 106, 99, // Color 5
    148, 135, 126, // Color 6
    179, 165, 158, // Color 7
    215, 211, 210, // Color 8
};
static df::palettest basic_organic_palette;

const uint8_t basic_inorganic_color_array[27] = {
    // Inorganic colors
    47, 48, 56,    // Color 0
    60, 62, 70,    // Color 1
    73, 76, 84, // Color 2
    90, 94, 101, // Color 3
    107, 112, 118, // Color 4
    138, 144, 145, // Color 5
    165, 166, 165, // Color 6
    199, 196, 180, // Color 7
    255, 255, 255  // Color 8
};
static df::palettest basic_inorganic_palette;

const std::vector<std::string> color_lookup = {
    "AMBER", "AMETHYST", "AQUA", "AQUAMARINE", "ASH_GRAY", "AUBURN", "AZURE",
    "BEIGE", "BLACK", "BLUE", "BRASS", "BRONZE", "BROWN", "BUFF",
    "BURNT_SIENNA", "BURNT_UMBER", "CARDINAL", "CARMINE", "CERULEAN",
    "CHARCOAL", "CHARTREUSE", "CHESTNUT", "CHOCOLATE", "CINNAMON", "CLEAR",
    "COBALT", "COPPER", "CREAM", "CRIMSON", "DARK_BLUE", "DARK_BROWN",
    "DARK_CHESTNUT", "DARK_GREEN", "DARK_INDIGO", "DARK_OLIVE", "DARK_PEACH",
    "DARK_PINK", "DARK_SCARLET", "DARK_TAN", "DARK_VIOLET", "ECRU",
    "EGGPLANT", "EMERALD", "FERN_GREEN", "FLAX", "FUCHSIA", "GOLD",
    "GOLDEN_YELLOW", "GOLDENROD", "GRAY", "GREEN", "GREEN-YELLOW",
    "HELIOTROPE", "INDIGO", "IVORY", "JADE", "LAVENDER", "LAVENDER_BLUSH",
    "LEMON", "LIGHT_BLUE", "LIGHT_BROWN", "LILAC", "LIME", "MAHOGANY",
    "MAROON", "MAUVE", "MAUVE_TAUPE", "MIDNIGHT_BLUE", "MINT_GREEN",
    "MOSS_GREEN", "OCHRE", "OLIVE", "ORANGE", "PALE_BLUE", "PALE_BROWN",
    "PALE_CHESTNUT", "PALE_PINK", "PEACH", "PEARL", "PERIWINKLE",
    "PINE_GREEN", "PINK", "PLUM", "PUCE", "PUMPKIN", "PURPLE", "RAW_UMBER",
    "RED", "RED_PURPLE", "ROSE", "RUSSET", "RUST", "SAFFRON", "SCARLET",
    "SEA_GREEN", "SEPIA", "SILVER", "SKY_BLUE", "SLATE_GRAY", "SPRING_GREEN",
    "TAN", "TAUPE_DARK", "TAUPE_GRAY", "TAUPE_MEDIUM", "TAUPE_PURPLE",
    "TAUPE_PALE", "TAUPE_ROSE", "TAUPE_SANDY", "TEAL", "TURQUOISE",
    "VERMILION", "VIOLET", "WHITE", "YELLOW", "YELLOW_GREEN", "BLUE-GRAY"
};
static std::unordered_map<std::string, size_t> color_index_map;

static ReservedRange reserved_range{};
static std::unordered_map<TexposHandle, long> g_handle_to_texpos;
static std::unordered_map<TexposHandle, long> g_handle_to_reserved_texpos;
static std::unordered_map<TexposHandle, SDL_Surface*> g_handle_to_surface;
static std::unordered_map<std::string, std::vector<TexposHandle>> g_tileset_to_handles;
static std::vector<TexposHandle> g_delayed_regs;
static std::mutex g_adding_mutex;
static std::atomic<bool> loading_state = false;
static SDL_Surface* dummy_surface = NULL;

// Converts an arbitrary Surface to something like the display format
// (32-bit RGBA), and converts magenta to transparency if convert_magenta is set
// and the source surface didn't already have an alpha channel.
// It also deletes the source surface.
//
// It uses the same pixel format (RGBA, R at lowest address) regardless of
// hardware.
SDL_Surface* canonicalize_format(SDL_Surface* src) {
    // even though we have null check after DFIMG_Load
    // in loadTileset() (the only consumer of this method)
    // it's better put nullcheck here as well
    if (!src)
        return src;

    auto fmt = DFSDL_AllocFormat(SDL_PixelFormatEnum::SDL_PIXELFORMAT_RGBA32);
    SDL_Surface* tgt = DFSDL_ConvertSurface(src, fmt, SDL_SWSURFACE);
    DFSDL_FreeSurface(src);
    for (int x = 0; x < tgt->w; ++x) {
        for (int y = 0; y < tgt->h; ++y) {
            Uint8* p = (Uint8*)tgt->pixels + y * tgt->pitch + x * 4;
            if (p[3] == 0) {
                for (int c = 0; c < 3; c++) {
                    p[c] = 0;
                }
            }
        }
    }

    return tgt;
}

df::tile_pagest* Textures::getTilePage(std::string token) {
    for (int i = 0; i < df::global::texture->page.size(); ++i) {
        if(token == df::global::texture->page[i]->token) {
            return df::global::texture->page[i];
        }
    }
    return nullptr;
}

// get the distance between two colors
static inline int colorDistanceSquared(const SDL_Color& c1, const SDL_Color& c2) {
    int dr = int(c1.r) - int(c2.r);
    int dg = int(c1.g) - int(c2.g);
    int db = int(c1.b) - int(c2.b);
    return dr*dr + dg*dg + db*db;
}

// register surface in texture raws, get a texpos
static long add_texture(SDL_Surface* surface) {
    std::lock_guard<std::mutex> lg_add_texture(g_adding_mutex);
    auto texpos = enabler->textures.raws.size();
    enabler->textures.raws.push_back(surface);
    return texpos;
}

// register surface in texture raws to specific texpos
static void insert_texture(SDL_Surface* surface, long texpos) {
    std::lock_guard<std::mutex> lg_add_texture(g_adding_mutex);
    enabler->textures.raws[texpos] = surface;
}

// delete surface from texture raws
static void delete_texture(long texpos) {
    std::lock_guard<std::mutex> lg_add_texture(g_adding_mutex);
    auto pos = static_cast<size_t>(texpos);
    if (pos >= enabler->textures.raws.size())
        return;
    enabler->textures.raws[texpos] = NULL;
}

int Textures::getColorIndex(std::string token) {
    return color_index_map[token];
}

// create new surface with RGBA32 format and pixels as data
SDL_Surface* create_texture(std::vector<uint32_t>& pixels, int texture_px_w, int texture_px_h) {
    auto surface = DFSDL_CreateRGBSurfaceWithFormat(0, texture_px_w, texture_px_h, 32,
                                                    SDL_PixelFormatEnum::SDL_PIXELFORMAT_RGBA32);
    auto canvas_length = static_cast<size_t>(texture_px_w * texture_px_h);
    for (size_t i = 0; i < pixels.size() && i < canvas_length; i++) {
        uint32_t* p = (uint32_t*)surface->pixels + i;
        *p = pixels[i];
    }
    return surface;
}

// convert single surface into tiles according w/h
// register tiles in texture raws and return handles
std::vector<TexposHandle> slice_tileset(SDL_Surface* surface, int tile_px_w, int tile_px_h,
                                        bool reserved) {
    std::vector<TexposHandle> handles{};
    if (!surface)
        return handles;

    int dimx = surface->w / tile_px_w;
    int dimy = surface->h / tile_px_h;

    if (reserved && (dimx * dimy > reserved_range.end - reserved_range.current)) {
        WARN(textures).print(
            "there is not enough space in reserved range for whole tileset, using dynamic range\n");
        reserved = false;
    }

    for (int y = 0; y < dimy; y++) {
        for (int x = 0; x < dimx; x++) {
            SDL_Surface* tile = DFSDL_CreateRGBSurface(
                0, tile_px_w, tile_px_h, 32, surface->format->Rmask, surface->format->Gmask,
                surface->format->Bmask, surface->format->Amask);
            SDL_Rect vp{tile_px_w * x, tile_px_h * y, tile_px_w, tile_px_h};
            DFSDL_UpperBlit(surface, &vp, tile, NULL);
            auto handle = Textures::loadTexture(tile, reserved);
            handles.push_back(handle);
        }
    }

    DFSDL_FreeSurface(surface);
    return handles;
}

TexposHandle Textures::loadTexture(SDL_Surface* surface, bool reserved) {
    if (!surface || !enabler)
        return 0; // should be some error, i guess
    if (loading_state)
        reserved = true; // use reserved range during loading for all textures

    auto handle = reinterpret_cast<uintptr_t>(surface);
    g_handle_to_surface.emplace(handle, surface);
    surface->refcount++; // prevent destruct on next FreeSurface by game

    if (reserved && reserved_range.is_installed) {
        auto texpos = reserved_range.get_new_texpos();
        if (texpos != -1) {
            insert_texture(surface, texpos);
            g_handle_to_reserved_texpos.emplace(handle, texpos);
            dummy_surface->refcount--;
            return handle;
        }

        if (loading_state) { // if we in loading state and reserved range is full -> error
            ERR(textures).printerr("reserved range limit has been reached, use dynamic range\n");
            return 0;
        }
    }

    // if we here in loading state = true, then it should be dynamic range -> delay reg
    if (loading_state) {
        g_delayed_regs.push_back(handle);
    } else {
        auto texpos = add_texture(surface);
        g_handle_to_texpos.emplace(handle, texpos);
    }

    return handle;
}

std::vector<TexposHandle> Textures::loadTileset(const std::string& file, int tile_px_w,
                                                int tile_px_h, bool reserved) {
    if (g_tileset_to_handles.contains(file))
        return g_tileset_to_handles[file];
    if (!enabler)
        return std::vector<TexposHandle>{};

    SDL_Surface* surface = DFIMG_Load(file.c_str());
    if (!surface) {
        ERR(textures).printerr("unable to load textures from '%s'\n", file.c_str());
        return std::vector<TexposHandle>{};
    }

    surface = canonicalize_format(surface);
    auto handles = slice_tileset(surface, tile_px_w, tile_px_h, reserved);

    DEBUG(textures).print("loaded %zd textures from '%s'\n", handles.size(), file.c_str());
    g_tileset_to_handles[file] = handles;

    return handles;
}

SDL_Surface* Textures::getSurfaceByHandle(const TexposHandle& handle) {
    auto it = g_handle_to_surface.find(handle);
    return (it != g_handle_to_surface.end()) ? it->second : nullptr;
}

long Textures::getTexposByHandle(TexposHandle handle) {
    if (!handle || !enabler)
        return -1;

    if (g_handle_to_reserved_texpos.contains(handle))
        return g_handle_to_reserved_texpos[handle];
    if (g_handle_to_texpos.contains(handle))
        return g_handle_to_texpos[handle];
    if (std::find(g_delayed_regs.begin(), g_delayed_regs.end(), handle) != g_delayed_regs.end())
        return 0;
    if (g_handle_to_surface.contains(handle)) {
        g_handle_to_surface[handle]->refcount++; // prevent destruct on next FreeSurface by game
        if (loading_state) { // reinit dor dynamic range during loading -> delayed
            g_delayed_regs.push_back(handle);
            return 0;
        }
        auto texpos = add_texture(g_handle_to_surface[handle]);
        g_handle_to_texpos.emplace(handle, texpos);
        return texpos;
    }

    return -1;
}

SDL_Surface* Textures::getSurfaceByTexpos(long texpos)
{
    if (texpos < 0 || texpos >= (int32_t)df::global::enabler->textures.raws.size())
        return nullptr;

    return reinterpret_cast<SDL_Surface*>(df::global::enabler->textures.raws[texpos]);
}

TexposHandle Textures::createTile(std::vector<uint32_t>& pixels, int tile_px_w, int tile_px_h,
                                  bool reserved) {
    if (!enabler)
        return 0;

    auto texture = create_texture(pixels, tile_px_w, tile_px_h);
    auto handle = Textures::loadTexture(texture, reserved);
    return handle;
}

std::vector<TexposHandle> Textures::createTileset(std::vector<uint32_t>& pixels, int texture_px_w,
                                                  int texture_px_h, int tile_px_w, int tile_px_h,
                                                  bool reserved) {
    if (!enabler)
        return std::vector<TexposHandle>{};

    auto texture = create_texture(pixels, texture_px_w, texture_px_h);
    auto handles = slice_tileset(texture, tile_px_w, tile_px_h, reserved);
    return handles;
}

void Textures::deleteHandle(TexposHandle handle) {
    if (!enabler)
        return;

    auto texpos = Textures::getTexposByHandle(handle);
    if (texpos > 0)
        delete_texture(texpos);
    if (g_handle_to_reserved_texpos.contains(handle))
        g_handle_to_reserved_texpos.erase(handle);
    if (g_handle_to_texpos.contains(handle))
        g_handle_to_texpos.erase(handle);
    if (auto it = std::find(g_delayed_regs.begin(), g_delayed_regs.end(), handle);
        it != g_delayed_regs.end())
        g_delayed_regs.erase(it);
    if (g_handle_to_surface.contains(handle)) {
        auto surface = g_handle_to_surface[handle];
        while (surface->refcount)
            DFSDL_FreeSurface(surface);
        g_handle_to_surface.erase(handle);
    }
}

SDL_Surface* Textures::copySurface(SDL_Surface* original_surface) {
    if (!original_surface){
        std::cerr << "copy_surface failed, SDL_Surface* is null.\n";
        return nullptr;
    }

    SDL_Surface* copy = DFSDL_CreateRGBSurface(
        original_surface->flags,
        original_surface->w,
        original_surface->h,
        original_surface->format->BitsPerPixel,
        original_surface->format->Rmask,
        original_surface->format->Gmask,
        original_surface->format->Bmask,
        original_surface->format->Amask
    );
    if (!copy) {
        std::cerr << "Failed to create surface copy.\n";
        return nullptr;
    }

    // Copy the pixels over
    if (DFSDL_UpperBlit(original_surface, NULL, copy, NULL) != 0) {
        std::cerr << "Failed to run blit on surface.\n";
        DFSDL_FreeSurface(copy);
        return nullptr;
    }
    
    return copy;
}

TexposHandle Textures::copyHandle(TexposHandle handle) {
    if (!handle) return 0;
    SDL_Surface* original_surface = getSurfaceByHandle(handle);
    SDL_Surface* copy_surface = copySurface(original_surface);
    TexposHandle new_handle = loadTexture(copy_surface);
    return new_handle;
}

void Textures::convertPalettestToSDLColor(const df::palettest& palette, SDL_Color out_colors[9])
{
    for (int i = 0; i < 9; ++i) {
        int base = i * 3;
        out_colors[i].r = palette.color[base];
        out_colors[i].g = palette.color[base + 1];
        out_colors[i].b = palette.color[base + 2];
        out_colors[i].a = 255; // fully opaque
    }
}

int Textures::paletteSwapHandle(TexposHandle handle,
                         SDL_Color* source_palette,
                         SDL_Color* target_palette)
{
    SDL_Surface* surface = getSurfaceByHandle(handle);

    if (!surface || !surface->format || !surface->pixels) {
        std::cerr << "Surface is invalid!" << std::endl;
        return 0;
    }

    uint8_t* pixels = static_cast<uint8_t*>(surface->pixels);
    int pitch = surface->pitch;
    int width = surface->w;
    int height = surface->h;
    int bpp = surface->format->BytesPerPixel;

    for (int y = 0; y < height; ++y) {
        uint8_t* row = pixels + y * pitch;
        for (int x = 0; x < width; ++x) {
            uint8_t* pixel = row + x * bpp;
            uint8_t r = pixel[0];
            uint8_t g = pixel[1];
            uint8_t b = pixel[2];

            for (int j = 0; j < 9; ++j) {
                if (r == source_palette[j].r &&
                    g == source_palette[j].g &&
                    b == source_palette[j].b)
                {
                    pixel[0] = target_palette[j].r;
                    pixel[1] = target_palette[j].g;
                    pixel[2] = target_palette[j].b;
                    break;
                }
            }
        }
    }
    return 1;
}

TexposHandle Textures::swapPalettes(TexposHandle handle, df::palettest& base_palette, df::palettest& swap_palette) {

    SDL_Color base_sdl_palette[9];
    SDL_Color swap_sdl_palette[9];
    
    convertPalettestToSDLColor(base_palette, base_sdl_palette);
    convertPalettestToSDLColor(swap_palette, swap_sdl_palette);

    TexposHandle handle_copy = copyHandle(handle);

    if (!paletteSwapHandle(handle_copy, base_sdl_palette, swap_sdl_palette)) {
        std::cerr << "Failed to swap palette colors: " << std::endl;
        return handle;
    }

    return handle_copy;
}

int Textures::tintSurfaceWithColor(TexposHandle handle, SDL_Color* tint_color, float alpha_scale)
{
    SDL_Surface* surface = getSurfaceByHandle(handle);

    if (!surface || !surface->format || !surface->pixels) {
        std::cerr << "Surface is invalid!" << std::endl;
        return 0;
    }

    uint8_t* pixels = static_cast<uint8_t*>(surface->pixels);
    int pitch = surface->pitch;
    int width = surface->w;
    int height = surface->h;
    int bpp = surface->format->BytesPerPixel;

    if (bpp != 4) {
        std::cerr << "Unsupported surface to retint.\n";
        return 0;
    }

    for (int y = 0; y < height; ++y) {
        uint8_t* row = pixels + y * pitch;
        for (int x = 0; x < width; ++x) {
            uint8_t* pixel = row + x * bpp;

            uint8_t& r = pixel[0];
            uint8_t& g = pixel[1];
            uint8_t& b = pixel[2];
            uint8_t& a = pixel[3];

            if (a > 0) {
                r = tint_color->r;
                g = tint_color->g;
                b = tint_color->b;
                a = static_cast<uint8_t>(a * alpha_scale);
            }
        }
    }

    return 1;
}


int Textures::remapSurfaceToPalette(TexposHandle handle, SDL_Color* target_palette, bool bias_lighter)
{
    SDL_Surface* surface = getSurfaceByHandle(handle);

    if (!surface) return 0;
    if (!surface->format) return 0;

    uint8_t* pixels = static_cast<uint8_t*>(surface->pixels);
    int pitch = surface->pitch;
    int width = surface->w;
    int height = surface->h;
    int bpp = surface->format->BytesPerPixel;

    if (bpp < 3) {
        std::cerr << "Unsupported BytesPerPixel: " << bpp << "\n";
        return 0;
    }

    for (int y = 0; y < height; ++y) {
        uint8_t* row = pixels + y * pitch;
        for (int x = 0; x < width; ++x) {
            uint8_t* pixel = row + x * bpp;
            SDL_Color current = { pixel[0], pixel[1], pixel[2], 255 };

            int best_idx = 0;
            int best_dist = std::numeric_limits<int>::max();

            for (int i = 0; i < 9; ++i) {
                int dist = colorDistanceSquared(current, target_palette[i]);

                if (bias_lighter) {
                    // Penalize darker colors to favor lighter ones
                    int brightness = target_palette[i].r + target_palette[i].g + target_palette[i].b;
                    int penalty = (255 * 3 - brightness); // darker colors have higher penalty
                    dist += penalty / 2; // adjust divisor to tune the strength of the bias
                }
                
                if (dist < best_dist) {
                    best_dist = dist;
                    best_idx = i;
                    }
                }

            pixel[0] = target_palette[best_idx].r;
            pixel[1] = target_palette[best_idx].g;
            pixel[2] = target_palette[best_idx].b;
        }
    }

    return 1;
}

std::vector<TexposHandle> Textures::tintTilePage(std::string token, SDL_Color* tint_color, float alpha_scale) {
    uint32_t first_texpos = 0;
    df::tile_pagest* tile_page = getTilePage(token);

    std::vector<TexposHandle> handles = std::vector<TexposHandle>(tile_page->texpos.size(), 0);

    for (int i = 0; i < tile_page->texpos.size(); ++i) {
        SDL_Surface* temp_surface = getSurfaceByTexpos(tile_page->texpos[i]);
        SDL_Surface* new_surface = copySurface(temp_surface);
        TexposHandle new_handle = loadTexture(new_surface);
        if (!new_handle) {
            std::cerr << "palettizeTilePage: Failed to copy new handle: " << new_handle << "\n";
            return handles;
        }
        tintSurfaceWithColor(new_handle, tint_color, alpha_scale);
        handles[i] = new_handle;
    }
    return handles;
}

std::vector<TexposHandle> Textures::palettizeTilePage(std::string token, df::palettest& base_palette, bool bias_lighter) {
    SDL_Color base_sdl_palette[9];
    convertPalettestToSDLColor(base_palette, base_sdl_palette);

    uint32_t first_texpos = 0;
    df::tile_pagest* tile_page = getTilePage(token);

    std::vector<TexposHandle> handles = std::vector<TexposHandle>(tile_page->texpos.size(), 0);

    for (int i = 0; i < tile_page->texpos.size(); ++i) {
        SDL_Surface* temp_surface = getSurfaceByTexpos(tile_page->texpos[i]);
        SDL_Surface* new_surface = copySurface(temp_surface);
        TexposHandle new_handle = loadTexture(new_surface);
        if (!new_handle) {
            std::cerr << "palettizeTilePage: Failed to copy new handle: " << new_handle << "\n";
            return handles;
        }
        // organic argument is useless since we already defined the palette earlier
        remapSurfaceToPalette(new_handle, base_sdl_palette, bias_lighter);
        handles[i] = new_handle;
    }
    //std::cerr << "palettized " << token << " length: " << handles.size() << "\n";
    return handles;
}

SDL_Surface* Textures::combineSurfaces(const std::vector<SDL_Surface*>& layers) {
    if (layers.empty()) {
        std::cerr << "No surfaces provided for combination.\n";
        return 0;
    }

    SDL_Surface* base = copySurface(layers[0]);
    if (!base) {
        std::cerr << "Failed to copy base surface.\n";
        return 0;
    }

    // Compose each surface on top of the base
    for (size_t i = 1; i < layers.size(); ++i) {
        if (!layers[i]) continue;

        if (DFSDL::DFSDL_UpperBlit(layers[i], nullptr, base, nullptr) != 0) {
            std::cerr << "DFSDL_UpperBlit failed on layer " << i << "\n";
            DFSDL::DFSDL_FreeSurface(base);
            return 0;
        }
    }

    return base;
}

TexposHandle Textures::combineHandles(const std::vector<TexposHandle>& layers) {
    if (layers.empty()) {
        std::cerr << "No surfaces provided for combination.\n";
        return 0;
    }

    std::vector<SDL_Surface*> sdl_layers = std::vector<SDL_Surface*>(layers.size(), nullptr);
    for (int i = 0; i < sdl_layers.size(); ++i) {
        sdl_layers[i] = getSurfaceByHandle(layers[i]);
    }
    
    SDL_Surface* combined_surface = combineSurfaces(sdl_layers);
    TexposHandle handle = loadTexture(combined_surface);

    return handle;
}

df::palettest Textures::getBasicOrganicPalette() {
    return basic_organic_palette;
}

df::palettest Textures::getBasicInorganicPalette() {
    return basic_inorganic_palette;
}

int Textures::getFramesafeScreentexposDepthAt(int32_t x, int32_t y, int32_t* (*accessor)(df::graphic_viewportst* vp))
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

df::coord Textures::getFramesafeViewportPos(){
    return framesafe_viewport_pos;
}

df::coord Textures::getFramesafeViewportPosOld(){
    return framesafe_viewport_pos_old;
}

int Textures::getDepthAt(int32_t x, int32_t y)
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

int Textures::getEdgeDepthAt(int32_t x, int32_t y)
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

static void reset_texpos() {
    DEBUG(textures).print("resetting texture mappings\n");
    g_handle_to_texpos.clear();
}

static void reset_reserved_texpos() {
    DEBUG(textures).print("resetting reserved texture mappings\n");
    g_handle_to_reserved_texpos.clear();
}

static void reset_tilesets() {
    DEBUG(textures).print("resetting tileset to handle mappings\n");
    g_tileset_to_handles.clear();
}

static void reset_surface() {
    DEBUG(textures).print("deleting cached surfaces\n");
    for (auto& entry : g_handle_to_surface) {
        DFSDL_FreeSurface(entry.second);
    }
    g_handle_to_surface.clear();
}

static void populate_color_index(){
    for (size_t i = 0; i < color_lookup.size(); ++i) {
        color_index_map[color_lookup[i]] = i;
    }
}

static void create_basic_palettes(){
    for (int i = 0; i < 27; ++i) {
        basic_organic_palette.color[i] = basic_organic_color_array[i];
        basic_inorganic_palette.color[i] = basic_inorganic_color_array[i];
    }
}

static void depopulate_color_index(){
    color_index_map.clear();
}

static void register_delayed_handles() {
    DEBUG(textures).print("register delayed handles, size %zd\n", g_delayed_regs.size());
    for (auto& handle : g_delayed_regs) {
        auto texpos = add_texture(g_handle_to_surface[handle]);
        g_handle_to_texpos.emplace(handle, texpos);
    }
    g_delayed_regs.clear();
}

struct RendererInterpose : df::renderer_2d {
    typedef df::renderer_2d interpose_base;

    DEFINE_VMETHOD_INTERPOSE(void, render, ()) {
        framesafe_viewport_pos_old = framesafe_viewport_pos;
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

// reset point on New Game
struct tracking_stage_new_region : df::viewscreen_new_regionst {
    typedef df::viewscreen_new_regionst interpose_base;

    DEFINE_VMETHOD_INTERPOSE(void, logic, ()) {
        if (this->m_raw_load_stage != this->raw_load_stage) {
            TRACE(textures).print("raw_load_stage %d -> %d\n", this->m_raw_load_stage,
                                  this->raw_load_stage);
            bool tmp_state = loading_state;
            loading_state = this->raw_load_stage >= 0 && this->raw_load_stage < 3 ? true : false;
            if (tmp_state != loading_state && !loading_state)
                register_delayed_handles();
            this->m_raw_load_stage = this->raw_load_stage;
            if (this->m_raw_load_stage == 1)
                reset_texpos();
        }
        INTERPOSE_NEXT(logic)();
    }

  private:
    inline static int m_raw_load_stage = -2; // not valid state at the start
};
IMPLEMENT_VMETHOD_INTERPOSE(tracking_stage_new_region, logic);

// reset point on New Game in Existing World
struct tracking_stage_adopt_region : df::viewscreen_adopt_regionst {
    typedef df::viewscreen_adopt_regionst interpose_base;

    DEFINE_VMETHOD_INTERPOSE(void, logic, ()) {
        if (this->m_cur_step != this->cur_step) {
            TRACE(textures).print("step %d -> %d\n", this->m_cur_step, this->cur_step);
            bool tmp_state = loading_state;
            loading_state = this->cur_step >= 0 && this->cur_step < 3 ? true : false;
            if (tmp_state != loading_state && !loading_state)
                register_delayed_handles();
            this->m_cur_step = this->cur_step;
            if (this->m_cur_step == 1)
                reset_texpos();
        }
        INTERPOSE_NEXT(logic)();
    }

  private:
    inline static int m_cur_step = -2; // not valid state at the start
};
IMPLEMENT_VMETHOD_INTERPOSE(tracking_stage_adopt_region, logic);

// reset point on Load Game
struct tracking_stage_load_region : df::viewscreen_loadgamest {
    typedef df::viewscreen_loadgamest interpose_base;

    DEFINE_VMETHOD_INTERPOSE(void, logic, ()) {
        if (this->m_cur_step != this->cur_step) {
            TRACE(textures).print("step %d -> %d\n", this->m_cur_step, this->cur_step);
            bool tmp_state = loading_state;
            loading_state = this->cur_step >= 0 && this->cur_step < 3 ? true : false;
            if (tmp_state != loading_state && !loading_state)
                register_delayed_handles();
            this->m_cur_step = this->cur_step;
            if (this->m_cur_step == 1)
                reset_texpos();
        }
        INTERPOSE_NEXT(logic)();
    }

  private:
    inline static int m_cur_step = -2; // not valid state at the start
};
IMPLEMENT_VMETHOD_INTERPOSE(tracking_stage_load_region, logic);

// reset point on New Arena
struct tracking_stage_new_arena : df::viewscreen_new_arenast {
    typedef df::viewscreen_new_arenast interpose_base;

    DEFINE_VMETHOD_INTERPOSE(void, logic, ()) {
        if (this->m_cur_step != this->cur_step) {
            TRACE(textures).print("step %d -> %d\n", this->m_cur_step, this->cur_step);
            bool tmp_state = loading_state;
            loading_state = this->cur_step >= 0 && this->cur_step < 3 ? true : false;
            if (tmp_state != loading_state && !loading_state)
                register_delayed_handles();
            this->m_cur_step = this->cur_step;
            if (this->m_cur_step == 0)
                reset_texpos();
        }
        INTERPOSE_NEXT(logic)();
    }

  private:
    inline static int m_cur_step = -2; // not valid state at the start
};
IMPLEMENT_VMETHOD_INTERPOSE(tracking_stage_new_arena, logic);

static void install_reset_point() {
    INTERPOSE_HOOK(tracking_stage_new_region, logic).apply();
    INTERPOSE_HOOK(tracking_stage_adopt_region, logic).apply();
    INTERPOSE_HOOK(tracking_stage_load_region, logic).apply();
    INTERPOSE_HOOK(tracking_stage_new_arena, logic).apply();
}

static void uninstall_reset_point() {
    INTERPOSE_HOOK(tracking_stage_new_region, logic).remove();
    INTERPOSE_HOOK(tracking_stage_adopt_region, logic).remove();
    INTERPOSE_HOOK(tracking_stage_load_region, logic).remove();
    INTERPOSE_HOOK(tracking_stage_new_arena, logic).remove();
}

static void reserve_static_range() {
    if (static_cast<size_t>(enabler->textures.init_texture_size) != enabler->textures.raws.size()) {
        WARN(textures).print(
            "reserved range can't be installed! all textures will be loaded to dynamic range!");
        return;
    }
    reserved_range.init(enabler->textures.init_texture_size);
    dummy_surface =
        DFSDL_CreateRGBSurfaceWithFormat(0, 0, 0, 32, SDL_PixelFormatEnum::SDL_PIXELFORMAT_RGBA32);
    dummy_surface->refcount += ReservedRange::size;
    for (int32_t i = 0; i < ReservedRange::size; i++) {
        add_texture(dummy_surface);
    }
    enabler->textures.init_texture_size += ReservedRange::size;
}

void Textures::init(color_ostream& out) {
    if (!enabler)
        return;

    reserve_static_range();
    install_reset_point();
    install_render_point();
    populate_color_index();
    create_basic_palettes();
    DEBUG(textures, out)
        .print("dynamic texture loading ready, reserved range %d-%d\n", reserved_range.start,
               reserved_range.end);
}

void Textures::cleanup() {
    if (!enabler)
        return;

    reset_texpos();
    reset_reserved_texpos();
    reset_tilesets();
    reset_surface();
    uninstall_render_point();
    depopulate_color_index();
    uninstall_reset_point();
}

