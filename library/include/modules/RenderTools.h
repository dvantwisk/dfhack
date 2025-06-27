#pragma once

#include <cstdint>
#include "df/graphic_viewportst.h"
#include <string>
#include <vector>

#include "ColorText.h"
#include "Export.h"

#include "df/coord.h"

namespace DFHack {
namespace RenderTools {

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

DFHACK_EXPORT int getDepthAt(int32_t x, int32_t y);

DFHACK_EXPORT int getEdgeDepthAt(int32_t x, int32_t y);

/**
 * Initialize RenderTools — call during plugin initialization.
 */
void init(DFHack::color_ostream &out);

/**
 * Clean up RenderTools — call during plugin shutdown.
 */
void cleanup();

}  // namespace RenderTools
}  // namespace DFHack