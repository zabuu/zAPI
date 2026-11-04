// Exercise both modules through the actual shared registration adapter.
#include "../src/zAPI.cpp"
#include "../src/profiles/Profiles.cpp"
#include <cassert>
#include <iostream>

namespace {
std::vector<std::pair<std::string, LuaCFunction>> registered;
void __fastcall Capture(const char *name, LuaCFunction function) {
  registered.emplace_back(name ? name : "", function);
}
int __fastcall Dummy(void *) { return 0; }
}
int main() {
  g_registerOriginal = &Capture;
  zprofiles::image = 0x400000;
  RegisterFunctionHook("GetBuildInfo", &Dummy);
  assert(registered.size() == 2 && registered[0].first == "GetBuildInfo");
  assert(registered[1].first == "zProfilesNative" && registered[1].second == &zprofiles::Bridge);
  registered.clear();
  RegisterFunctionHook("GetActionBarToggles", zprofiles::At<zprofiles::CFunction>(0x004E7660));
  assert(registered.size() == 1 && registered[0].second == &zprofiles::GetTogglesHook);
  assert(zprofiles::originalGetToggles == zprofiles::At<zprofiles::CFunction>(0x004E7660));
  registered.clear();
  RegisterFunctionHook("GetActionBarToggles", &Dummy);
  assert(registered.size() == 1 && registered[0].second == &Dummy);
  registered.clear();
  g_renderHooksAttempted = true;
  g_textHookInstalled = true;
  RegisterFunctionHook("UnitXP", &Dummy);
  assert(registered.size() == 2 && registered[0].first == "UnitXP");
  assert(registered[1].first == "zAPI" && registered[1].second == &Script_zAPI);
  registered.clear();
  RegisterFunctionHook("UnitName", &Dummy);
  RegisterFunctionHook("UnitXP", &Dummy);
  assert(registered.size() == 4 && registered[1].first == "zProfilesNative" && registered[3].first == "zAPI");
  // Verify deferred hooks follow the mod detours present at world entry.
  // Generated executable stubs stand in for the client and ClassicAPI.
  const uintptr_t previousBase = g_base;
  auto memory = static_cast<unsigned char *>(VirtualAlloc(nullptr, 0x700000, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
  assert(memory);
  g_base = reinterpret_cast<uintptr_t>(memory);
  auto stock = At<unsigned char *>(kTextDrawBuilder);
  auto laterMod = memory + 0x600000;
  auto marker = reinterpret_cast<uint32_t *>(memory + 0x600100);
  *marker = 0;
  laterMod[0] = 0xC7; laterMod[1] = 0x05;
  const uint32_t markerAddress = reinterpret_cast<uint32_t>(marker);
  const uint32_t markerValue = 42;
  std::memcpy(laterMod + 2, &markerAddress, 4);
  std::memcpy(laterMod + 6, &markerValue, 4);
  laterMod[10] = 0xC3;
  stock[0] = 0xE9;
  const int32_t relative = static_cast<int32_t>(laterMod - stock - 5);
  std::memcpy(stock + 1, &relative, 4);
  for (auto address : {kDeviceSetTexture, kPrepareTexture}) {
    auto entry = At<unsigned char *>(address);
    entry[0] = 0xB8; entry[5] = 0xC3;
  }
  assert(MH_Initialize() == MH_OK);
  g_renderHooksAttempted = false; g_textHookInstalled = false;
  assert(InstallRenderingHooks() && g_textHookInstalled);
  assert(stock[0] == 0xE9 && laterMod[0] == 0xE9);
  assert(InstallRenderingHooks());
  reinterpret_cast<TextDrawBuilderFn>(stock)(nullptr);
  assert(*marker == 42);
  std::cout << "PASS original client entry executes the later mod through the shared rendering hook.\n";
  MH_DisableHook(MH_ALL_HOOKS); MH_Uninitialize();
  g_base = previousBase; VirtualFree(memory, 0, MEM_RELEASE);
  std::cout << "PASS shared profile/zAPI registration, bar-toggle wrapping, ReloadUI and deferred rendering-hook chaining.\n";
}
