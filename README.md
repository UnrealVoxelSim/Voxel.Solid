# UnrealVoxelSim.Voxel.Solid

Portable implementation of `UnrealVoxelSim.Voxel.Solid.Api` over one injected logical voxel field.

The implementation owns solid semantics but does not own or inspect physical storage. It translates configured
`MaterialId` values to opaque `Voxel.Api::CellValue` atoms, uses compare-and-set batches for placement and removal, and
publishes one queued `Changed` fact after each successful non-empty mutation.

Changed positions are deterministically sorted and coalesced into X-axis logical runs. Distant edits therefore remain
separate invalidation regions without revealing the backing implementation's chunk boundaries.

V1 uses the standard Dirt, Grass, and Stone identifiers. The configured material span is copied at construction, so
composition may extend the catalog without modifying `Controller`. The controller and all injected field/event
capabilities are thread-affine.
