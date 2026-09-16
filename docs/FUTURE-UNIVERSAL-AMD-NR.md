# Future roadmap: universal AMD Neural Rendering

## Status

Deferred until the OptiScaler AMD backend is migrated to v0.8.4 and the experimental Intensity implementation is validated.

This document records the idea only. It is not part of the current v0.8.4 migration.

## Goal

Investigate a universal AMD Neural Rendering solution that can run in games without native DLSS, FSR, XeSS, or another upscaling integration.

The intended use is image enhancement rather than a guaranteed performance increase. Performance depends primarily on keeping the frame transport GPU-only.

## Recommended foundation

Start from [zmodelerlover/dlss5-neural-amd](https://github.com/zmodelerlover/dlss5-neural-amd) and use ReShade as the first frontend, while keeping the AMD NR backend independent from ReShade.

Do not build a completely new multi-API injector initially.

Suggested components:

- shared 64-bit AMD HIP NR core;
- ReShade frontend for early D3D11/D3D12/Vulkan validation;
- optional native frontends later;
- separate 32-bit frontend and 64-bit host for x86 games;
- GPU-shared transport where supported;
- CPU-copy transport only as a compatibility fallback.

## First prototype

Target one 64-bit D3D11 game that does not expose DLSS/FSR resources.

Requirements:

- no dependency on game upscaling support;
- GPU-only frame path;
- native-resolution processing;
- Working Scale, Passes, and Intensity controls;
- residual application:

  ```cpp
  output = input + intensity * (neuralOutput - baseline);
  ```

- timings for input copy, guide preparation, HIP execution, resolve, and total frame cost;
- safe fallback when depth or motion is unavailable;
- correct resize, Alt+Tab, shutdown, and VRAM cleanup.

## Input-quality modes

1. Full temporal: color, reliable depth, motion, and exposure.
2. Depth-assisted: color and reliable depth.
3. Spatial/safe: current color only, without unsafe temporal reuse.

The UI should explicitly report which inputs were found and which mode is active.

## Later stages

1. Validate and optimize the D3D11 x64 prototype.
2. Add D3D12 using the command-list, COM-identity, fence, timeout, and history-reset lessons from the OptiScaler fork.
3. Add Vulkan if the D3D11/D3D12 cost is acceptable.
4. Improve the x86 bridge:
   - D3D9Ex/shared GPU textures as the fast path;
   - existing CPU round trip as fallback.
5. Consider native proxy frontends only after measurements show ReShade itself is a meaningful bottleneck.

## Important performance note

The roughly 30 FPS observed in NFS Carbon is likely dominated by the 32-bit plain-D3D9 CPU/IPC transport. Removing ReShade without replacing that transport is not expected to produce a large improvement. The important future optimization is a GPU-only x86-to-x64 bridge.

## Return condition

Resume this investigation after:

- the stable `amd-v083-minimal` branch remains preserved;
- the AMD backend has been ported to the exact OptiScaler NR `v0.8.4` tag;
- the port passes runtime, resize, VRAM, timeout, and long-session testing;
- Intensity is reapplied and tested separately at multiple Working Scale values.
