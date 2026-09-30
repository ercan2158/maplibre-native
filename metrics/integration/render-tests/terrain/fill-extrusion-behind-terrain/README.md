# terrain/fill-extrusion-behind-terrain

Guards that the terrain and fill-extrusions test depth on one scale, so a slope
between the camera and a building hides it.

Thirty 200 m buildings stand in a grid over the Innsbruck DEM fixture (the same
`terrain-shading` z12 tiles the other terrain tests use), seen at a 60 degree
pitch with the terrain exaggerated 1.5x. Several stand behind slopes that rise
toward the camera. Before the fix the terrain was projected with `projMatrix`
(near plane 1) and fill-extrusions with `nearClippedProjMatrix` (near plane at a
tenth of the camera distance), and on Metal, Vulkan and WebGPU only the
terrain's clip z was remapped to [0, 1]: the two wrote the same depth buffer on
different scales, and those buildings were drawn over the slopes in front of
them.

## expected.png

Generated with the macOS Metal render-test runner (`cmake --preset macos-metal`),
as the other terrain baselines on this branch do not yet pass on Linux OpenGL.
Treat it as provisional until CI regenerates or confirms it.
