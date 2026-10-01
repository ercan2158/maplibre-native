#include <mln/renderer/layers/terrain_layer_tweaker.hpp>

#include <mln/gfx/context.hpp>
#include <mln/gfx/drawable.hpp>
#include <mln/renderer/layer_group.hpp>
#include <mln/renderer/paint_parameters.hpp>
#include <mln/renderer/render_terrain.hpp>
#include <mln/shaders/terrain_layer_ubo.hpp>
#include <mln/shaders/shader_defines.hpp>
#include <mln/util/constants.hpp>
#include <mln/util/convert.hpp>
#include <mln/util/mat4.hpp>
#include <mln/util/logging.hpp>

#include <algorithm>
#include <cmath>

namespace mln {

using namespace shaders;

void TerrainLayerTweaker::execute(LayerGroupBase& layerGroup, const PaintParameters& parameters) {
    if (layerGroup.empty() || !terrain) {
        return;
    }

    auto& context = parameters.context;

#if defined(DEBUG)
    const auto label = layerGroup.getName() + "-update-uniforms";
    const auto debugGroup = parameters.encoder->createDebugGroup(label.c_str());
#endif

    // Get terrain properties; per the style spec, exaggeration is applied as-is
    // (default 1.0 renders true-scale elevation)
    const float exaggeration = terrain->getExaggeration();

    // Skirt depth (u_ele_delta): the shader drops the mesh's skirt vertices by
    // this many metres into a curtain that hides the cracks between neighbouring
    // tiles at different zoom levels. ~1/5 of the tile's world width at this zoom,
    // matching maplibre-gl-js Terrain.getSkirtLength().
    const auto zoom = std::max(parameters.state.getZoom(), 0.0);
    const float elevationOffset = static_cast<float>(util::M2PI * util::EARTH_RADIUS_M / std::pow(2.0, zoom) / 5.0);

    // Populate layer-level UBO with terrain properties
    auto& layerUniforms = layerGroup.mutableUniformBuffers();
    const TerrainEvaluatedPropsUBO propsUBO = {.unpack = terrain->getDEMUnpackVector(),
                                               .exaggeration = exaggeration,
                                               .elevation_offset = elevationOffset,
                                               .pad1 = 0.0f,
                                               .pad2 = 0.0f};
    layerUniforms.createOrUpdate(idTerrainEvaluatedPropsUBO, &propsUBO, context);

#if MLN_UBO_CONSOLIDATION
    int i = 0;
    std::vector<TerrainDrawableUBO> drawableUBOVector(layerGroup.getDrawableCount());
#endif

    // Visit each drawable to populate per-drawable UBOs
    visitLayerGroupDrawables(layerGroup, [&](gfx::Drawable& drawable) {
        if (!drawable.getTileID()) {
            return;
        }

        const UnwrappedTileID tileID = drawable.getTileID()->toUnwrapped();

        // Calculate transformation matrix for this terrain tile. The metres->world pixels
        // elevation scale (pixelsPerMeter) is baked into the projection matrix in
        // TransformState::getProjMatrix, so it applies here and to the elevated symbol /
        // circle layers consistently, matching maplibre-gl-js. The projection is the one
        // fill-extrusions and custom layers share (projMatrix3D), so buildings, hills and a
        // custom layer's models hide each other by one depth; on Vulkan, Metal and WebGPU its
        // clip z is already remapped to their [0, 1], so terrain in the near half of the GL
        // clip volume is not clipped away.
        // The depth pass for symbol occlusion keeps projMatrix: the symbols compare their own
        // depth, projected with it, against what this pass packs.
        mat4 matrix;
        parameters.state.matrixFor(matrix, tileID);
        if (&layerGroup == terrain->getDepthLayerGroup().get()) {
            matrix::multiply(matrix, parameters.transformParams.projMatrix, matrix);
#if !MLN_RENDER_BACKEND_OPENGL
            matrix[2] = 0.5 * (matrix[2] + matrix[3]);
            matrix[6] = 0.5 * (matrix[6] + matrix[7]);
            matrix[10] = 0.5 * (matrix[10] + matrix[11]);
            matrix[14] = 0.5 * (matrix[14] + matrix[15]);
#endif
        } else {
            matrix::multiply(matrix, parameters.projMatrix3D(), matrix);
        }

#if !MLN_UBO_CONSOLIDATION
        auto& drawableUniforms = drawable.mutableUniformBuffers();
#endif

#if MLN_UBO_CONSOLIDATION
        drawableUBOVector[i] = {
#else
        const TerrainDrawableUBO drawableUBO = {
#endif
            .matrix = util::cast<float>(matrix),
            .dem_coords = terrain->getDrawableDemCoords(*drawable.getTileID())
        };

#if !MLN_UBO_CONSOLIDATION
        drawableUniforms.createOrUpdate(idTerrainDrawableUBO, &drawableUBO, context);
#endif

#if MLN_UBO_CONSOLIDATION
        drawable.setUBOIndex(i++);
#endif
    });

#if MLN_UBO_CONSOLIDATION
    const size_t drawableUBOVectorSize = sizeof(TerrainDrawableUBO) * drawableUBOVector.size();
    if (!drawableUniformBuffer || drawableUniformBuffer->getSize() < drawableUBOVectorSize) {
        drawableUniformBuffer = context.createUniformBuffer(
            drawableUBOVector.data(), drawableUBOVectorSize, false, true);
    } else {
        drawableUniformBuffer->update(drawableUBOVector.data(), drawableUBOVectorSize);
    }

    layerUniforms.set(idTerrainDrawableUBO, drawableUniformBuffer);
#endif
}

} // namespace mln
