# Direct NR compatibility runtime

The RTX20/30/40 compatibility DLL can be rejected by the NGX driver's signed loader. NR now tries a direct backend after `FAIL_UnableToInitializeFeature`, provided the driver returned no feature handle.

The backend accepts only the verified ShortFuse 310.8.0 DLL:
`E67DEE209320CDAFE0E93E45675D7AA34323A53ACC57A72B2E40A181581C989A`.
Other binaries stay on the driver path. No runtime is downloaded or bundled.

`DlssNr_CompatibilityRuntime.*` owns loading and model calls. Its small `Paths.cpp` adapter supplies OptiScaler's NGX search paths. The existing NR proxy retains the backend until GPU retirement, then releases features, shuts down the last device owner, and unloads the runtime.

The model requires its caller's module path to contain `nvngx.dll`. One import in the verified runtime supplies that alias only during direct calls and only for the calling OptiScaler module. Other queries retain their normal behavior. There is no helper DLL, driver modification, or change to files on disk.

## Validation

- Release x64 build; proxy routing, GPU retirement and pipeline capture regressions.
- Real RTX 5090: driver rejection followed by successful direct creation; four init/shutdown/unload cycles, two independent features per cycle, 24 fenced evaluations and finite, non-black image readback.
- Signed and altered-hash DLLs rejected by the direct backend; calls outside its scope still fail the model's caller check.

Run `tests/nr_compatibility/run.ps1 -Driver <installed _nvngx.dll> -RuntimeDirectory <compatibility DLL folder>` from a Visual Studio developer PowerShell. Optional `-RejectRuntimeDirectories` tests other local binaries without loading them.

This covers DX12 NR, including the DX11-to-DX12 bridge. Native Vulkan still uses its existing driver path. RTX 40 hardware, in-game modes, image quality and long-session stability remain untested by this change.
