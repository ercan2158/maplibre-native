#pragma once

#include <memory>
#include <array>

namespace mln {

class PaintParameters;

namespace style {

/**
 * Parameters that define the current camera position for a
 * `CustomLayerHost::render()` function.
 */
struct CustomLayerRenderParameters {
    double width;
    double height;
    double latitude;
    double longitude;
    double zoom;
    double bearing;
    double pitch;
    double fieldOfView;

    /// Standard projection matrix (nearZ = 1 tile unit).
    /// Use this for 2D/flat custom geometry.
    std::array<double, 16> projectionMatrix;

    /// A 4×4 matrix representing the map view’s current near clip projection state.
    std::array<double, 16> nearClippedProjectionMatrix;

    /// The projection the map's own 3D geometry (terrain, fill-extrusions) is drawn with, in
    /// the renderer's clip convention (on Metal, Vulkan and WebGPU, z in [0, 1]). Geometry drawn
    /// with it depth-tests correctly against the terrain and the 3D buildings.
    std::array<double, 16> projectionMatrix3D;

    CustomLayerRenderParameters(const PaintParameters&);
};

} // namespace style
} // namespace mln
