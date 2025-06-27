#pragma once

#include <string>
#include <vector>

#include "ColorText.h"
#include "Export.h"

#include "df/graphic_viewportst.h"
#include "df/palettest.h"
#include "df/tile_pagest.h"


struct SDL_Surface;
struct SDL_Color;

typedef uintptr_t TexposHandle;

namespace DFHack {

/**
 * The Textures module - loads and provides access to DFHack textures
 * \ingroup grp_modules
 * \ingroup grp_textures
 */
namespace Textures {

const uint32_t TILE_WIDTH_PX = 8;
const uint32_t TILE_HEIGHT_PX = 12;

/**
 * Load texture and get handle.
 * Keep it to obtain valid texpos.
 */
DFHACK_EXPORT TexposHandle loadTexture(SDL_Surface* surface, bool reserved = false);

/**
 * Load tileset from image file.
 * Return vector of handles to obtain valid texposes.
 */
DFHACK_EXPORT std::vector<TexposHandle> loadTileset(const std::string& file,
                                                    int tile_px_w = TILE_WIDTH_PX,
                                                    int tile_px_h = TILE_HEIGHT_PX,
                                                    bool reserved = false);


/**
 * Get SDL_Surface* by handle.
 * Always use this function, if you need to get valid texpos for your texture.
 * Texpos can change on game textures reset, but handle will be the same.
 */
DFHACK_EXPORT SDL_Surface* getSurfaceByHandle(const TexposHandle& handle);

/**
 * Get texpos by handle.
 * Always use this function, if you need to get valid texpos for your texture.
 * Texpos can change on game textures reset, but handle will be the same.
 */
DFHACK_EXPORT long getTexposByHandle(TexposHandle handle);

/**
 * Get SDL_Surface* using texpos.
 */

DFHACK_EXPORT SDL_Surface* getSurfaceByTexpos(long texpos);

/**
 * Copy SDL_Surface, returns SDL_Surface* to new SDL_Surface.
 */
DFHACK_EXPORT SDL_Surface* copySurface(SDL_Surface* original_surface);

/**
 * Copy SDL_Surface associated with the handle, returns new TexposHandle to new SDL_Surface.
 */
DFHACK_EXPORT TexposHandle copyHandle(TexposHandle handle);

/**
 * Swaps the color palette of a SDL_Surface associated with the TexposHandle with the target_palette.
 */
DFHACK_EXPORT int paletteSwapHandle(TexposHandle handle,
                                    SDL_Color* source_palette,
                                    SDL_Color* target_palette);

/**
 * Swaps the df::palettest color palette of a SDL_Surface associated with the TexposHandle with the df::palettest target_palette.
 */
DFHACK_EXPORT TexposHandle swapPalettes(TexposHandle handle, df::palettest& base_palette, df::palettest& swap_palette);

/**
 * returns a "shadow" of the SDL_Surface associated with the TexposHandle that is colored with SDL_Color and has an opacity of alpha_scale.
 */
DFHACK_EXPORT int tintSurfaceWithColor(TexposHandle handle, SDL_Color* tint_color, float alpha_scale);

/**
 * Remap colors to of an SDL_Surface to the target_palette based on lowest distance between colors.
 */
DFHACK_EXPORT int remapSurfaceToPalette(TexposHandle handle, SDL_Color* target_palette, bool bias_lighter);

/**
 * Create "shadow" images to of an SDL_Surface to the SDL_color with an opacity of .
 */

DFHACK_EXPORT std::vector<TexposHandle> tintTilePage(std::string token, SDL_Color* tint_color, float alpha_scale);

/**
 * Remap colors to of an SDL_Surface to the target_palette based on lowest distance between colors.
 */

DFHACK_EXPORT std::vector<TexposHandle> palettizeTilePage(std::string token, df::palettest& base_palette, bool bias_lighter);

/**
 * Combine a std::vector of SDL_Surface objects. The first SDL_Suface is on the bottom, and the last is on top.
 */
DFHACK_EXPORT SDL_Surface* combineSurfaces(const std::vector<SDL_Surface*>& layers);

/**
 * Combine a std::vector of TexposHandle objects. The first SDL_Suface is on the bottom, and the last is on top.
 */
DFHACK_EXPORT TexposHandle combineHandles(const std::vector<TexposHandle>& layers);

/**
 * Get index of description color by the color's name as an std::string.
 */
DFHACK_EXPORT int getColorIndex(std::string token);

/**
 * Populate a SDL_Color array with values from a df::palettest.
 */
DFHACK_EXPORT void convertPalettestToSDLColor(const df::palettest& palette, SDL_Color out_colors[9]);

/**
 * Delete all info about texture by TexposHandle
 */
DFHACK_EXPORT void deleteHandle(TexposHandle handle);

DFHACK_EXPORT df::tile_pagest* getTilePage(std::string token);

/**
 * Create new texture with RGBA32 format and pixels as data in row major order.
 * Register this texture and return TexposHandle.
 */
DFHACK_EXPORT TexposHandle createTile(std::vector<uint32_t>& pixels, int tile_px_w = TILE_WIDTH_PX,
                                      int tile_px_h = TILE_HEIGHT_PX, bool reserved = false);

/**
 * Create new textures as tileset with RGBA32 format and pixels as data in row major order.
 * Register this textures and return vector of TexposHandle.
 */
DFHACK_EXPORT std::vector<TexposHandle> createTileset(std::vector<uint32_t>& pixels,
                                                      int texture_px_w, int texture_px_h,
                                                      int tile_px_w = TILE_WIDTH_PX,
                                                      int tile_px_h = TILE_HEIGHT_PX,
                                                      bool reserved = false);

DFHACK_EXPORT df::palettest getBasicOrganicPalette();

DFHACK_EXPORT df::palettest getBasicInorganicPalette();

/**
 * Return z-depth of the first non-zero screentexpos value for a given (x, y) position.
 * The accessor function defines which screentexpos layer to check.
 *
 * @param x Screen-space x coordinate (not world-space).
 * @param y Screen-space y coordinate.
 * @param accessor Function that receives a pointer to a graphic_viewportst and returns a pointer to its desired screentexpos layer.
 * @return Depth (z-index) or -1 if not found.
 */
DFHACK_EXPORT int getFramesafeScreentexposDepthAt(
    int32_t x, int32_t y,
    int32_t* (*accessor)(df::graphic_viewportst* vp)
);

/**
 * The current viewport coordinates used for rendering.
 * This variable is updated every frame after the render() function to ensure the current viewport coordinates are used.
 * This argument is "framesafe" so it is safe to use while inserting new graphics to render.
 */
DFHACK_EXPORT df::coord getFramesafeViewportPos();
DFHACK_EXPORT df::coord getFramesafeViewportPosOld();

DFHACK_EXPORT int getDepthAt(int32_t x, int32_t y);

DFHACK_EXPORT int getEdgeDepthAt(int32_t x, int32_t y);

/**
 * Call this on DFHack init just once to setup interposed handlers and
 * init static assets.
 */
void init(DFHack::color_ostream& out);

/**
 * Call this when DFHack is being unloaded.
 *
 */
void cleanup();

} // namespace Textures
} // namespace DFHack
