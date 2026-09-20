# Active Tasks

## IN PROGRESS

- Prepare the reviewed hardware-free minimal single-pass DLSS-NR-on-AMD v0.3.1 implementation state for commit/push.

## TODO

- Run RX 9070 XT A/B: patched v0.2.14 versus patched v0.3.1 under equivalent conditions.
- Compare:
  - maximum job duration;
  - `pending-after-100ms`;
  - `long-pending-retired`;
  - runtime timeout count;
  - stage diagnostics;
  - native completion;
  - D3D12 fence behavior.
- Evaluate production-runtime migration only after successful A/B.

Approved v0.3.1 experiment identity:

- Official v0.3.1 SHA-256: `B108D6407EB7F094A4F9111EDD778EEE7B978B648D413A9FC7AEEDFDD914C154`
- Expected SHA-256 after only the two approved patches: `175AECA9844B5EAD3E092818DF08D2D8B059021940FF44A200A3B81564DF8012`

Approved binary patches:

1. Standalone hook installer disable — RVA `0x6D9D`.
2. Duplicate `ExecuteCommandLists` suppression — RVA `0x9742`.

Do not automatically carry forward:

- old compiler patch;
- residual timeout patch;
- compute-spin cap.

Fail-closed preparation tool for these two approved patches: **IMPLEMENTED / VERIFIED (hardware-free) / NOT HARDWARE VERIFIED**. The final output is published only after deterministic SHA-256 verification.

## BLOCKED / UNVERIFIED

- RDNA3 / RX 7900-class hardware validation.
- NVIDIA hardware regression validation.
- Production v0.3.1 migration until hardware A/B succeeds.

## DONE

- OptiScaler v0.8.4 AMD migration validated.
- Prerequisite diagnostics validated.
- AMD unsupported-Intensity UI validated.
- Multipass graceful fallback validated.
- Long single-pass lifecycle stall reproduced and instrumented.
- v0.3.1 private ABI audit completed with no remaining REQUIRED-but-UNPROVEN blocker for the minimal experiment.
- Minimal single-pass v0.3.1 adapter implemented with fail-closed identity, atomic approved-patch output publication, and hardware-free regression validation.
