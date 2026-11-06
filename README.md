# zAPI

**Native version 3.7 — adds a native pet spell cast for the zPet Focus manager.**

Version 3.7 adds `zAPI("petCast", name[, target])`, which casts a pet spell through the verified pet dispatcher from any addon timer. It retains 3.6 zPie ring-slot snapshots.

Version 3.6 adds optional zPie ring-slot data to the checked, atomically saved setup and Undo files. Existing bar-only setups remain compatible. It retains 3.5 live action-slot restoration, upload suppression, 3.4 cache initialization and existing-binding precedence. Fully restart WoW after replacing the DLL.

Shared native DLL for zSuite on **Windows, 32-bit WoW 1.12.1.5875**, as used by
OctoWoW. Install one copy for all compatible addons. This repository owns the
native source and the downloadable **`bin/zAPI.dll`**.

## Installation

1. Fully close every WoW client using this game folder.
2. [Download **zAPI.dll**](https://github.com/zabuu/zAPI/raw/refs/heads/main/bin/zAPI.dll).
   Alternatively, [download the repository ZIP](https://github.com/zabuu/zAPI/archive/refs/heads/main.zip)
   and take **`zAPI.dll`** from its **`bin`** folder.
3. Copy **`zAPI.dll`** into your **game folder, beside `WoW.exe` and
   `VanillaFixes.exe`**. Replace the previous copy after backing it up.
4. Open **`dlls.txt` in that same game folder** with a text editor. Create it
   if needed. Add **`zAPI.dll` as the first line, exactly once**. Keep the
   existing entries for your other DLLs, in their existing order below it.
5. If upgrading from standalone zProfiles, remove the **`zProfilesNative.dll`**
   line from `dlls.txt`. The merged zAPI replaces that DLL. Do not load both.
   Keep your **`WTF/zProfiles` data folder** and saved addon settings.
6. Launch through **`VanillaFixes.exe`**, or the configured OctoWoW launcher
   that loads the DLLs listed in `dlls.txt`. Fully restart after every DLL
   update; `/reload` only reloads Lua and cannot replace a loaded DLL.

For example, if your existing loader entries include UnitXP, the layout is:

```text
OctoWoW/
  WoW.exe
  VanillaFixes.exe
  dlls.txt
  zAPI.dll
  UnitXP_SP3.dll
  Interface/AddOns/
    z_DNumbers/
    z_Nameplates/
    z_Profiles/
```

Your `dlls.txt` begins with:

```text
zAPI.dll
```

Leave your other existing DLL lines below that line. zAPI uses the stock
client's `UnitXP` Lua registration marker; **zAPI itself does not require the
separate UnitXP SP3 DLL**. Individual addons may require UnitXP SP3 or other
client extensions; follow their own requirements too.

Cloning an addon with Git or installing it through an addon manager only puts
files in `Interface/AddOns`. It does **not** copy `zAPI.dll` beside `WoW.exe`
or register it in `dlls.txt`. With Git, run
`git clone https://github.com/zabuu/zAPI.git` wherever you keep native source,
then copy **`bin/zAPI.dll`** and edit `dlls.txt` as above. `git pull` updates the
checkout; repeat the DLL copy with WoW closed to update the installed copy.

## Verify the installation

After a full restart and login, run:

```lua
/run DEFAULT_CHAT_FRAME:AddMessage(type(zAPI)=="function" and ("zAPI "..tostring(zAPI("version"))) or "zAPI not loaded")
```

If it reports `zAPI not loaded`, check the filename, its game-root location,
the `dlls.txt` entry, launch method and supported client build. The DLL checks
the client build before using native addresses; unsupported executables are
not supported. Use the addon-specific status commands for feature compatibility.

## Addons using zAPI

| Addon | Usage |
| --- | --- |
| [zDNumbers](https://github.com/zabuu/z_DNumbers) | World-space damage projection and native text transforms. |
| [zNameplates](https://github.com/zabuu/z_Nameplates) | Native projection, validation, sight batches and ground effects. |
| [zMap](https://github.com/zabuu/z_Map) | Optional live gathering and HUD projection. |
| [zInspect](https://github.com/zabuu/z_Inspect) | Additional equipment information. |
| [zRangeFinder](https://github.com/zabuu/z_RangeFinder) | Optional exact world projection and terrain calibration. |
| [zProfiles](https://github.com/zabuu/z_Profiles) | Account isolation, local profile storage and named bar/binding setups. |
| [zBox](https://github.com/zabuu/zBox) | World positions, camera projection and terrain traces. |
| zPet | Native pet commands, nearby discovery and positioning. |
| OctoGC / OctoPerf | GC controls and optional native diagnostics. |

## Recent usernames on the login screen

Click the arrow inside the account-name field to choose from the **five most
recent remembered usernames**. Selecting one fills the field and focuses the
password box. You can still type a different name normally.

Leave **Remember Account Name** checked when logging in to add or refresh that
name in the list. Existing stock remembered names seed a new history. **Clear
saved usernames** clears the list and the stock remembered name. The history is
local to this game folder, in `WTF/zAPI/login-usernames.txt`, and works before
any character addon loads. **Passwords are never read or stored by this feature.**
History failures leave ordinary login available and do not alter account modes.

## Default keybindings for new profiles

An optional local template account supplies keybindings **only when the
destination account has none**. Existing account and character bindings always
take priority, including on first login. An empty or missing character-binding
file first inherits its own account's bindings, then the template if that account
is empty too. The template's current isolated bindings take priority over its
original file. Macros, action bars and other addon settings remain specific to
the destination account.

Choose a visible account with
`/run zProfilesNative("setBindingTemplate", "ACCOUNT")` and replace `ACCOUNT`
with its name. The selection persists in `WTF/zProfiles/binding-template.txt`.
Default-mode accounts keep normal behavior. The template is a starting point;
later binding changes are not continuously synchronized between accounts.

## Source and development

See [API.md](API.md) for the existing native command contracts and
[docs/profiles.md](docs/profiles.md) for profile integration. The shared DLL
retains the existing zAPI commands while incorporating the zProfiles module.
Addons distribute their own Lua and artwork and link here for native downloads;
they do not carry copies of this DLL or its implementation.

Build with Visual Studio C++ x86 Build Tools and Windows SDK:

```powershell
.\build.ps1 -Tests -Package
```

Compilation produces `build/zAPI.dll`; `-Package` updates the distributed
**`bin/zAPI.dll`** and **`bin/SHA256SUMS`**. `-Tests` also runs the native profile
and shared-registration fixtures. Run the three `tests/test_native_*.py` files
for projection/sight regressions and `tests/audit_profiles_client.py PATH/TO/WoW.exe`
for the pinned-client audit. Deployment requires closing WoW and explicitly replacing the installed
DLL. MinHook's license is retained in [vendor/minhook/LICENSE.txt](vendor/minhook/LICENSE.txt).

The zProfiles module remains experimental: offline checks do not replace live
login, addon interoperability or cross-computer server verification. Profile
protection starts synchronously before login, uses one MinHook lifecycle, and
retains the existing `zProfilesNative()` Lua name for compatibility. Rendering
hooks install at world API registration so later-loaded client-mod detours remain
in the chain. A failed or unsupported protection install stops startup before
login and writes `zProfilesNative.log`. Account modes default unless explicitly
isolated. `/reload` preserves the active session mode.
