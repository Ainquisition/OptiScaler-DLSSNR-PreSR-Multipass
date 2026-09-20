# Current Project State

## Goal

Stable and broadly compatible AMD/HIP Neural Rendering backend for OptiScaler, prioritizing the normal single-pass DX12 pre-SR path while preserving the existing NVIDIA/default NR path.

## Repository

- Upstream/repository family: `wilsjo2/OptiScaler-DLSSNR-PreSR-Multipass`
- Fork: `Ainquisition/OptiScaler-DLSSNR-PreSR-Multipass`
- OptiScaler base: `v0.8.4`
- Baseline: `8802b2b470db0462fa1ed03a125e793a7c06d735`
- Canonical branch: `amd-v084-minimal`
- Canonical local HEAD: `1e4de0c80c68d1b606896bda6ae86fbb21237bca` (`Show AMD pre-SR runtime status`)
- Control branch: `amd-v083-minimal`
- Experimental branch: `amd-v031-runtime-experiment`
- Checked branch / HEAD: `amd-v031-runtime-experiment` / `df434c4a9f5eee31b842f219574dbfe83febc448`
- Worktree: not clean before this migration because `.codebase-memory/` was already untracked; no pre-existing source or runtime changes were reported by `git status --short`.

Actual Git state confirms the experimental checkpoint and resolves the previously unknown full canonical `1e4de0c8` hash. No local `origin/amd-v084-minimal` tracking ref was available to confirm remote state after the last explicitly confirmed pushed commit `28bc23d318eeb9def7f09f6f8bc229bde3abc32d`.

## Current status

- AMD migration to exact OptiScaler v0.8.4: **IMPLEMENTED / VERIFIED / HARDWARE VERIFIED** on RX 9070 XT / RDNA4.
- AMD prerequisite diagnostics: **IMPLEMENTED / VERIFIED / HARDWARE VERIFIED** — `5b6a53b110b7ef59c74748100c20dba864c5d154`.
- AMD-aware unsupported Intensity UI: **IMPLEMENTED / VERIFIED / HARDWARE VERIFIED** — `eb8f4ad9ad765425e3efa57b30bd68b7f2566850`.
- Optional multipass graceful fallback: **IMPLEMENTED / VERIFIED / HARDWARE VERIFIED** — `28bc23d318eeb9def7f09f6f8bc229bde3abc32d`.
- Validated fallback behavior: `2→1`, `3→2`, full 3-pass, and live switching.
- AMD runtime-status overlay: **IMPLEMENTED / VERIFIED** — `1e4de0c80c68d1b606896bda6ae86fbb21237bca`; not marked HARDWARE VERIFIED.
- Single-pass timeout lifecycle telemetry: **IMPLEMENTED / VERIFIED / HARDWARE VERIFIED AS DIAGNOSTICS** — `df434c4a9f5eee31b842f219574dbfe83febc448`.
- RX 9070 XT telemetry reproduced real multi-second lifecycle stalls. One instrumented job was approximately 4.2 s total, with approximately 119 ms capture wait; host submission showed no major delay, queue identity stayed stable, timeout counter remained 0, and native completion plus the D3D12 fence completed only later. This is not classified as an ordinary runtime timeout/recovery event.
- v0.3.1 ABI research: **VERIFIED ANALYSIS**.
- Minimal single-pass v0.3.1 adapter: **IMPLEMENTED / VERIFIED (hardware-free) / NOT HARDWARE VERIFIED** on `amd-v031-runtime-experiment`.
- Runtime selection is fail-closed by exact size/SHA; the official v0.3.1 image must be prepared with only the two approved patches before loading.
- v0.3.1 remains a **CANDIDATE FIX**, not a confirmed fix.

## Current runtime

Production AMD runtime remains the locally patched runtime based on upstream DLSS-NR-on-AMD `v0.2.14`.

- Size: `7,156,224 bytes`
- SHA-256: `3C9CA13F0F5FC36A690BA424C457003BCFCC1080B4B785974CDD7E9AE2BC1DD8`
- It is not byte-identical to official upstream v0.2.14.

## Current experiment

Minimal single-pass DLSS-NR-on-AMD `v0.3.1` experimental adapter, reviewed and hardware-free verified; ready for implementation-state fixation and hardware A/B. Use only `amd-v031-runtime-experiment` for this work. Do not claim the runtime fixes the stalls before hardware A/B.

## Blockers / Unverified

- Current `weights.bin` compatibility with real v0.3.1 `InitFn` is not runtime-verified.
- v0.3.1 is not hardware-tested on RX 9070 XT.
- A stall fix is not confirmed.
- RDNA3 / RX 7900-class is not hardware validated.
- NVIDIA hardware regression validation is absent.
- Native-equivalent general AMD Intensity mapping is absent.
- Remote state after canonical `1e4de0c8` is not confirmed; the migration checkpoint only explicitly confirmed push through `28bc23d318eeb9def7f09f6f8bc229bde3abc32d`.

## Next

Prepare the reviewed implementation state for commit/push, then run patched v0.2.14 versus patched v0.3.1 RX 9070 XT A/B under equivalent conditions.
