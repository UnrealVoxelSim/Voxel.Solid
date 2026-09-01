# UnrealVoxelSim.Voxel.Solid

Portable implementation of `Voxel.Solid.Api` over an injected logical voxel field. Placement and removal execute
synchronously and publish one immediate `Changed` fact after each successful non-empty mutation.

Changed positions are deterministically sorted and coalesced into logical X-axis runs. The implementation is
thread-affine and contains no command queue.
