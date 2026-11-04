# Profile module (zAPI 3.2)

The zProfiles native module is part of **zAPI.dll**. Install that single DLL
first in `dlls.txt`, remove the old `zProfilesNative.dll` loader entry, and
retain all `WTF/zProfiles` data. The [installation guide](../README.md#installation)
includes the binary download and migration steps.

`zAPI("profilesVersion")` returns `1`. The established
`zProfilesNative(command, ...)` Lua function remains available from this DLL;
its name is retained to preserve the version-1 command contract. The
[zProfiles addon](https://github.com/zabuu/z_Profiles) provides the user interface.

Profile commands cover version/status, account/character enumeration, mode
selection, identification/resume, bar visibility and Revert. Named setup
commands cover listing, Save, Update, Rename, Delete, Apply and Undo with
binding payload validation. Existing storage formats and account modes remain
compatible. New zProfiles checks the merged capability before enabling controls.

One synchronous exported `Load` initializes the shared MinHook instance,
profile file/settings hooks and the sole Lua-registration detour before login.
The module checks the audited x86 client's PE metadata and native entry bytes.
A failed protection installation stops startup. The DLL is pinned, and profile
routing/session state remains alive through other DLLs' exit destructors.

Rendering detours are installed at stock world API registration, after other
startup DLLs have installed their detours. They chain through the later mods'
glyph processing. New glue/world Lua states and `/reload` receive the bridges
again without reinstalling process hooks. The stock `UnitXP` registration
marker does not require loading the separate UnitXP SP3 extension.

The profile module retains the existing `zProfilesNative.log` filename for
diagnostics. Every unconfigured account remains Default. Modes take effect
after a full client restart. Removing only the Lua addon does not disable
native isolation: select Use Default for isolated accounts before removing it.

Validation includes 138 native profile checks with actual Win32 file detours,
generated account fixtures and forced process termination; the shared
registration/reload/render-chain fixture; and the addon Lua regression suite.
No test uses real account data. Live login, interoperability and remote server
verification remain separate checks, documented in the
[addon validation guide](https://github.com/zabuu/z_Profiles/blob/main/docs/VALIDATION.md).

Profile code retains its [MIT license](../src/profiles/LICENSE); MinHook retains
its [upstream license](../vendor/minhook/LICENSE.txt).
