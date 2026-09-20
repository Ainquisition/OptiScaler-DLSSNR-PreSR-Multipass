# Durable Decisions

- Canonical host/base is exact OptiScaler `v0.8.4` at `8802b2b470db0462fa1ed03a125e793a7c06d735`.
- The AMD path is a minimal AMD/HIP backend inside OptiScaler; do not wholesale-merge external forks.
- Preserve NVIDIA/default NR behavior.
- The production AMD route is DX12 pre-SR.
- Target generations are RDNA4 and RDNA3; current physical validation covers RX 9070 XT / RDNA4 only.
- Do not hard-code RX 9070 XT, `gfx1201`, or one Device ID.
- Map the D3D12 adapter to the HIP device by LUID.
- Pass 1 is required; Pass 2 and Pass 3 are optional.
- Select the maximal contiguous available pass prefix.
- Production multipass uses a separate runtime instance per pass.
- Do not migrate production to a shared runtime without proof of temporal equivalence.
- Single-pass currently has priority.
- Do not emulate NVIDIA `DLSSNR.Intensity` on AMD through residual blend, `TransferStrength`, or unknown offsets.
- Do not change `ResTrack_dx12.cpp`, submission/lifetime logic, or private AMD ABI without direct evidence of necessity.
- Patched v0.2.14 remains the production runtime until v0.3.1 validation succeeds.
- v0.3.1 is allowed only as an isolated single-pass experiment before hardware A/B.
- x86 Generic Depth, D3D11, Vulkan, and post-SR are separate workstreams and are not automatically part of the current scope.
