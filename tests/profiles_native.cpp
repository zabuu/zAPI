// Include the production adapter so tests exercise its fail-closed decisions,
// not a second implementation. No game process or real account is accessed.
#include "../src/profiles/Profiles.cpp"
using namespace zprofiles;
#include <iostream>
#include <cstdarg>

namespace {
int checks = 0, sent = 0, received = 0;
zp::Bars delivered{};
unsigned liveWrites = 0, actionUploads = 0, perSlotEvents = 0, fullBarEvents = 0;
int failLiveSlotOnce = -1;
bool invalidBarEvent = false;
void __fastcall SetLiveAction(uint32_t slot, uint32_t action, uint32_t upload, uint32_t suppressNotify) {
  ++liveWrites;
  if (upload) ++actionUploads;
  if (!suppressNotify) ++perSlotEvents;
  if (slot >= 120) { invalidBarEvent = true; return; }
  if (static_cast<int>(slot) == failLiveSlotOnce) { failLiveSlotOnce = -1; return; }
  At<uint32_t *>(0xBC6980)[slot] = action;
}
void __cdecl BarEvent(uint32_t event, const char *format, ...) {
  va_list args; va_start(args, format);
  const int slot = va_arg(args, int); va_end(args);
  if (event != 0xD5 || !format || std::strcmp(format, "%d") || slot != 0 || liveWrites % 120)
    invalidBarEvent = true;
  ++fullBarEvents;
}
std::array<uint8_t, 0xE70> fakePlayer{};
std::array<uint8_t, 0x1030> fakeFields{};
uintptr_t __stdcall LookupPlayer(uint64_t guid) { return guid == 42 ? reinterpret_cast<uintptr_t>(fakePlayer.data()) : 0; }
unsigned digestReads = 0;
void __fastcall CacheDigest(uint8_t kind, const char *name, uint8_t *output) {
  if (!name || std::strcmp(name, "fixture-cache")) RaiseException(EXCEPTION_ACCESS_VIOLATION, 0, 0, nullptr);
  ++digestReads;
  for (unsigned i = 0; i < 16; ++i) output[i] = static_cast<uint8_t>(kind * 16 + i + 1);
}
struct PlayerFixture {
  void *memory = VirtualAlloc(nullptr, 0x800000, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
  uintptr_t previousImage = image;
  PlayerFixture() {
    if (!memory) throw std::runtime_error("allocating mock player image");
    image = reinterpret_cast<uintptr_t>(memory);
    // Exercise the production PlayerFlags reader/writer against the exact
    // audited offsets and calling convention, with generated memory only.
    *At<uint8_t *>(0xB4B424) = 1;
    auto manager = reinterpret_cast<uintptr_t>(At<void *>(0xB42000));
    *At<uintptr_t *>(0xB41414) = manager;
    *reinterpret_cast<uint64_t *>(manager + 0xC0) = 42;
    *reinterpret_cast<uintptr_t *>(fakePlayer.data() + 0xE68) = reinterpret_cast<uintptr_t>(fakeFields.data());
    auto jump = At<uint8_t *>(0x464870);
    jump[0] = 0x68;
    auto target = reinterpret_cast<uintptr_t>(&LookupPlayer);
    std::memcpy(jump + 1, &target, 4); jump[5] = 0xC3;
    for (unsigned i = 0; i < 8; ++i) {
      auto row = At<uint8_t *>(0x85DCF0) + i * 0x2C;
      row[0] = static_cast<uint8_t>(i);
      *reinterpret_cast<const char **>(row + 8) = "fixture-cache";
    }
    jump = At<uint8_t *>(0x5AF940);
    jump[0] = 0x68; target = reinterpret_cast<uintptr_t>(&CacheDigest);
    std::memcpy(jump + 1, &target, 4); jump[5] = 0xC3;
    jump = At<uint8_t *>(0x4E5D60);
    jump[0] = 0x68; target = reinterpret_cast<uintptr_t>(&SetLiveAction);
    std::memcpy(jump + 1, &target, 4); jump[5] = 0xC3;
    jump = At<uint8_t *>(0x703F50);
    jump[0] = 0x68; target = reinterpret_cast<uintptr_t>(&BarEvent);
    std::memcpy(jump + 1, &target, 4); jump[5] = 0xC3;
    FlushInstructionCache(GetCurrentProcess(), memory, 0x800000);
  }
  ~PlayerFixture() { image = previousImage; VirtualFree(memory, 0, MEM_RELEASE); }
};
void Check(bool condition, const char *label) {
  ++checks;
  if (!condition) throw std::runtime_error(label);
}
void __fastcall Sent(void *, void *, Packet *) { ++sent; }
void __fastcall Received(void *, void *, uint32_t, Packet *packet) {
  ++received;
  uint8_t *data = nullptr; size_t n = 0;
  // The real 0x129 handler writes a login seed, not the live action table.
  // Keeping these separate catches fake restores that only replay this packet.
  if (View(packet, &data, &n) && n == 2 + sizeof(zp::Bars)) std::memcpy(delivered.data(), data + 2, sizeof(zp::Bars));
}
std::string Contents(const zp::fs::path &path) {
  auto bytes = zp::ReadBytes(path);
  return {bytes.begin(), bytes.end()};
}
void Write(const zp::fs::path &path, const std::string &text, bool ansi = false) {
  HANDLE handle = ansi ? CreateFileA(zp::Ansi(path.wstring()).c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr) :
    CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  Check(handle != INVALID_HANDLE_VALUE, "opening a client settings file");
  DWORD written = 0;
  bool ok = WriteFile(handle, text.data(), static_cast<DWORD>(text.size()), &written, nullptr) && written == text.size();
  CloseHandle(handle);
  Check(ok, "writing a client settings file");
}
std::vector<uint8_t> Message(uint32_t opcode, const void *body, size_t length, size_t opcodeSize) {
  std::vector<uint8_t> bytes(opcodeSize + length);
  std::memcpy(bytes.data(), &opcode, opcodeSize);
  if (length) std::memcpy(bytes.data() + opcodeSize, body, length);
  return bytes;
}
Packet Wrap(std::vector<uint8_t> &bytes, bool borrowed = false) {
  return {reinterpret_cast<void *>(1), bytes.data(), 0, borrowed ? 0xFFFFFFFFu : static_cast<uint32_t>(bytes.size()), static_cast<uint32_t>(bytes.size()), 0};
}
void PacketWindowTests() {
  std::vector<uint8_t> bytes(12, 0);
  uint8_t *data = nullptr; size_t length = 0;
  auto packet = Wrap(bytes, true);
  Check(View(&packet, &data, &length) && data == bytes.data() && length == 12, "borrowed CDataStore ownership marker is not an allocation size");
  packet.base = packet.read = 100; packet.size = 112;
  Check(View(&packet, &data, &length) && data == bytes.data() && length == 12, "borrowed buffer supports a nonzero logical base");
  packet.read = 102;
  Check(View(&packet, &data, &length) && data == bytes.data() + 2 && length == 10, "borrowed read cursor preserves the remaining logical window");
  packet.read = 99;
  Check(!View(&packet, &data, &length), "borrowed cursor before base is rejected");
  packet.read = 113;
  Check(!View(&packet, &data, &length), "borrowed cursor beyond size is rejected");
  packet.read = 100; packet.size = 100 + 16 * 1024 * 1024 + 1;
  Check(!View(&packet, &data, &length), "borrowed windows retain the size bound");
  packet = Wrap(bytes); packet.allocated = 11;
  Check(!View(&packet, &data, &length), "owned windows cannot exceed their actual allocation");
  packet.allocated = 16 * 1024 * 1024 + 1;
  Check(!View(&packet, &data, &length), "unknown large allocation values remain rejected");
  packet = Wrap(bytes, true); packet.read = 0xFFFFFFFFu;
  Check(!View(&packet, &data, &length), "unfinished write cursors cannot be mistaken for readable packets");
}
struct HookScope {
  HookScope() {
    Check(MH_Initialize() == MH_OK, "MinHook initializes");
    Check(zp::files::Install(store.get()), "production file hooks install");
    Check(MH_EnableHook(MH_ALL_HOOKS) == MH_OK, "production file hooks enable");
  }
  ~HookScope() { MH_DisableHook(MH_ALL_HOOKS); MH_Uninitialize(); zp::files::store = nullptr; }
};
void Fixture(const zp::fs::path &root) {
  zp::AtomicWrite(root / L"WTF/Account/TESTACCOUNT/macros-cache.txt", "official account macro");
  zp::AtomicWrite(root / L"WTF/Account/TESTACCOUNT/Realm/Testchar/layout-cache.txt", "official character layout");
  zp::AtomicWrite(root / L"WTF/Account/TESTACCOUNT/Realm/Testchar/macros-cache.txt", "official character macro");
  zp::AtomicWrite(root / L"WTF/Account/TESTACCOUNT/Realm/Otherchar/layout-cache.txt", "official other layout");
  zp::AtomicWrite(root / L"WTF/Account/NORMALACCOUNT/bindings-cache.wtf", "default bindings");
  zp::Store initial(root);
  Check(initial.active.empty(), "no profile means Default");
  initial.Mode(L"testaccount", true);
  Check(!initial.Isolated(L"TESTACCOUNT"), "mode changes do not enable isolation mid-session");
}
void UsernameHistoryTests(const zp::fs::path &root) {
  zlogin::UsernameHistory first(root), second(root);
  Check(first.Names().empty(), "new username history starts empty");
  first.Remember("one"); second.Remember("Two");
  Check(first.Names() == std::vector<std::string>({"TWO", "ONE"}), "instances merge current disk history and normalize account casing");
  first.Remember("one");
  Check(second.Names() == std::vector<std::string>({"ONE", "TWO"}), "reused usernames move to the front without duplicates");
  for (const auto &name : {"THREE", "FOUR", "FIVE", "SIX"}) second.Remember(name);
  const auto names = first.Names();
  Check(names == std::vector<std::string>({"SIX", "FIVE", "FOUR", "THREE", "ONE"}), "history retains exactly five most recent unique usernames");
  first.Remember("OLD", true);
  Check(first.Names() == names, "stock saved-account seeding cannot reorder an existing history");
  first.Remember("wrong\nname"); first.Remember("bad|markup"); first.Remember(std::string(65, 'A'));
  Check(first.Names() == names, "control characters, UI markup and oversized names are not persisted");
  first.Clear();
  Check(second.Names().empty(), "clear history persists across instances");
  second.Remember("SavedAccount", true);
  Check(first.Names() == std::vector<std::string>({"SAVEDACCOUNT"}), "existing stock remembered account can seed an empty history");
  auto file = root / L"WTF/zAPI/login-usernames.txt";
  zp::AtomicWrite(file, "corrupt history");
  bool refused = false;
  try { first.Names(); } catch (...) { refused = true; }
  Check(refused, "damaged history is never interpreted as account names or executable code");
  first.Clear();
  Check(second.Names().empty(), "clear can recover a damaged username history");
}
void BindingTemplateTests(const zp::fs::path &root) {
  auto game = root / L"binding-template-fixture";
  const std::string official = "bind ESCAPE TOGGLEGAMEMENU\nbind W MOVEFORWARD\n";
  const std::string current = official + "bind F1 TARGETSELF\n";
  zp::AtomicWrite(game / L"WTF/Account/ZABUU/bindings-cache.wtf", official);
  zp::AtomicWrite(game / L"WTF/Account/NEWACCOUNT/bindings-cache.wtf", "bind SPACE JUMP\n");
  zp::AtomicWrite(game / L"WTF/Account/NEWACCOUNT/Realm/Newchar/bindings-cache.wtf", "");
  zp::AtomicWrite(game / L"WTF/Account/NEWACCOUNT/Realm/Custom/bindings-cache.wtf", "bind E CUSTOM\n");
  zp::AtomicWrite(game / L"WTF/Account/NEWACCOUNT/macros-cache.txt", "own account macros");
  zp::AtomicWrite(game / L"WTF/Account/DEFAULT/bindings-cache.wtf", "default account keys");
  zp::AtomicWrite(game / L"WTF/zProfiles/profiles/ZABUU/bindings-cache.wtf", current);
  zp::Store initial(game);
  initial.SetBindingTemplate(L"zabuu"); initial.Mode(L"NEWACCOUNT", true);
  zp::Store isolated(game);
  Check(isolated.BindingTemplate() == L"ZABUU", "binding template account persists across client sessions");
  isolated.EnsureProfile(L"NEWACCOUNT");
  Check(Contents(isolated.Profile(L"NEWACCOUNT") / L"bindings-cache.wtf") == "bind SPACE JUMP\n", "new profile preserves an account's existing bindings on first login");
  auto empty = game / L"WTF/Account/NEWACCOUNT/Realm/Newchar/bindings-cache.wtf";
  auto seeded = isolated.Route(empty.wstring(), false);
  Check(Contents(seeded) == "bind SPACE JUMP\n", "empty character bindings inherit their own account bindings before any template");
  Check(Contents(empty).empty(), "seeding does not change the official empty character file");
  auto custom = game / L"WTF/Account/NEWACCOUNT/Realm/Custom/bindings-cache.wtf";
  Check(Contents(isolated.Route(custom.wstring(), false)) == "bind E CUSTOM\n", "existing nonempty character bindings are preserved");
  Check(Contents(isolated.Profile(L"NEWACCOUNT") / L"macros-cache.txt") == "own account macros", "binding defaults never copy the template account's macros");
  Check(Contents(game / L"WTF/Account/NEWACCOUNT/bindings-cache.wtf") == "bind SPACE JUMP\n", "template seeding leaves the account's official bindings intact");
  Check(isolated.Route((game / L"WTF/Account/DEFAULT/bindings-cache.wtf").wstring(), false) == (game / L"WTF/Account/DEFAULT/bindings-cache.wtf").wstring(), "Default-mode accounts are not seeded or redirected");
  zp::AtomicWrite(zp::fs::path(seeded), "bind Q USERCHANGED\n");
  Check(Contents(isolated.Route(empty.wstring(), false)) == "bind Q USERCHANGED\n", "a template is a starting point and never overwrites later custom bindings");
  zp::AtomicWrite(game / L"WTF/Account/EMPTY/bindings-cache.wtf", "");
  isolated.Mode(L"EMPTY", true);
  { zp::Store fresh(game); fresh.EnsureProfile(L"EMPTY");
    Check(Contents(fresh.Profile(L"EMPTY") / L"bindings-cache.wtf") == current, "only an empty account inherits the configured template");
    Check(current.find("bind ESCAPE TOGGLEGAMEMENU") != std::string::npos, "template inheritance includes the source Escape binding"); }
}
void SetupTests(zp::Session &session) {
  const auto oldBars = session.bars;
  Check(!zp::SetupName(L"../escape") && !zp::SetupName(L" spec") && !zp::SetupName(L"a.b"), "setup names cannot escape storage");
  Check(zp::SetupName(L"Frost PvP-2") && !zp::SetupName(std::wstring(33, L'x')), "setup names accept readable short labels");
  const std::string bindings = "ZPB1\n31\t414354494f4e425554544f4e31\n53484946542d31\t414354494f4e425554544f4e31\n";
  zp::Setup setup{session.bars, 5, bindings};
  setup.bars[0] = 116;
  setup.bars[2] = 0x80001234; // item
  setup.bars[118] = 0x41000001; // character macro
  setup.bars[119] = 0x40000001; // account macro on a hidden/stance page
  auto path = zp::SetupPath(*store, session.account, session.guid, L"Frost");
  zp::WriteSetup(*store, path, setup);
  auto loaded = zp::ReadSetup(path);
  Check(loaded.bars == setup.bars && loaded.toggles == 5 && loaded.bindings == bindings, "setup round trip retains all 120 slots, flags and secondary bindings");
  auto separate = zp::SetupPath(*store, session.account, 43, L"Frost");
  Check(!zp::fs::exists(separate), "another character cannot see this character's named setup");
  auto undoPath = zp::SetupPath(*store, session.account, session.guid, L"", true);
  zp::WriteSetup(*store, undoPath, setup);
  Check(zp::ListSetups(*store, session.account, session.guid).size() == 1, "Undo is stored separately from the named setup list");
  bool refused = false;
  try { zp::SetupPath(*store, L"NORMALACCOUNT", 42, L"Frost"); } catch (...) { refused = true; }
  Check(refused, "Default accounts cannot create setup storage");
  refused = false;
  try { zp::SetupPath(*store, session.account, 0, L"Frost"); } catch (...) { refused = true; }
  Check(refused, "setup storage requires a character GUID");
  refused = false;
  auto invalid = setup; invalid.bindings = "ZPB1\n31\t41\n31\t42\n";
  try { zp::WriteSetup(*store, path, invalid); } catch (...) { refused = true; }
  Check(refused && zp::ReadSetup(path).bindings == bindings, "duplicate bindings cannot overwrite a valid snapshot");
  auto corrupt = zp::ReadBytes(path); corrupt[20] ^= 1;
  auto badPath = zp::SetupPath(*store, session.account, session.guid, L"Corrupt");
  zp::AtomicWrite(badPath, corrupt.data(), corrupt.size());
  refused = false;
  try { zp::ReadSetup(badPath); } catch (...) { refused = true; }
  Check(refused, "damaged snapshot checksum prevents applying corrupt bars");
  zp::DeleteSetup(*store, badPath);
  auto renamed = zp::SetupPath(*store, session.account, session.guid, L"Frost PvP");
  zp::RenameSetup(*store, path, renamed);
  Check(!zp::fs::exists(path) && zp::ReadSetup(renamed).bars == setup.bars, "renaming preserves snapshot contents");
  zp::WriteSetup(*store, path, setup);
  refused = false;
  try { zp::RenameSetup(*store, path, renamed); } catch (...) { refused = true; }
  Check(refused && zp::fs::exists(path), "renaming never overwrites an existing setup");
  const int previousSent = sent;
  zp::Bars official{};
  store->LoadBars(session.account, session.guid, true, official);
  {
    PlayerFixture player;
    fakeFields[0x102A] = 0xA9;
    zp::Bars unsavedBars = oldBars;
    unsavedBars[0] = unsavedBars[1] = 0; // removed spell and macro
    unsavedBars[8] = 77; // unsaved extra spell, absent in the named setup
    std::memcpy(At<void *>(0xBC6980), unsavedBars.data(), sizeof(unsavedBars));
    const auto unsaved = CaptureSetup(session, bindings);
    Check(unsaved.bars == unsavedBars && unsaved.bars != session.bars,
      "Save and Undo capture the visible live slots even when the session journal differs");
    zp::WriteSetup(*store, undoPath, unsaved);
    const auto stagedBefore = delivered;
    liveWrites = actionUploads = perSlotEvents = fullBarEvents = 0; invalidBarEvent = false;
    ApplySetup(session, setup);
    Check(std::memcmp(At<void *>(0xBC6980), setup.bars.data(), sizeof(zp::Bars)) == 0,
      "Apply restores the live action slots, not only the login staging packet");
    zp::Bars checkpoint{};
    Check(delivered == stagedBefore && session.bars == setup.bars, "setup application updates live slots without replaying the login seed");
    Check(liveWrites == 120 && !actionUploads && !perSlotEvents && fullBarEvents == 1 && !invalidBarEvent,
      "all slots use the local setter with uploads disabled and one final full-bar refresh");
    Check(At<uint32_t *>(0xBC6980)[0] == 116 && At<uint32_t *>(0xBC6980)[2] == 0x80001234 &&
      At<uint32_t *>(0xBC6980)[118] == 0x41000001 && At<uint32_t *>(0xBC6980)[119] == 0x40000001,
      "spell, item, character-macro and account-macro slots are restored in live memory");
    Check(fakeFields[0x102A] == 0xA5 && session.toggles == 5, "setup applies visibility without changing unrelated player bits");
    Check(store->LoadBars(session.account, session.guid, false, checkpoint) && checkpoint == setup.bars, "applied setup is persisted as the local action checkpoint");
    Check(sent == previousSent, "applying a setup sends no server edits");
    zp::Bars afterOfficial{};
    Check(store->LoadBars(session.account, session.guid, true, afterOfficial) && afterOfficial == official, "applying a setup retains the official Revert baseline");
    ApplySetup(session, zp::ReadSetup(undoPath));
    Check(std::memcmp(At<void *>(0xBC6980), unsavedBars.data(), sizeof(unsavedBars)) == 0,
      "Undo restores unsaved removals and extra live slots");
    auto broken = setup; broken.bars[5] = 133;
    failLiveSlotOnce = 5;
    refused = false;
    try { ApplySetup(session, broken); } catch (...) { refused = true; }
    Check(refused && std::memcmp(At<void *>(0xBC6980), unsavedBars.data(), sizeof(unsavedBars)) == 0 && session.bars == unsavedBars,
      "failed live write is detected and rolls back the actual prior live slots");
    Check(store->LoadBars(session.account, session.guid, false, checkpoint) && checkpoint == unsavedBars,
      "failed application also rolls back its local checkpoint");
    ApplySetup(session, setup);
    *At<uint8_t *>(0xB4B424) = 0;
    refused = false;
    auto unavailable = setup; unavailable.bars[0] = 123;
    try { ApplySetup(session, unavailable); } catch (...) { refused = true; }
    Check(refused && session.bars == setup.bars, "missing live player leaves the setup untouched");
    *At<uint8_t *>(0xB4B424) = 1;
    store->SaveBars(session.account, session.guid, false, oldBars);
    Check(DeliverBars(session, oldBars), "live session can be restored after setup tests");
  }
  zp::DeleteSetup(*store, renamed);
  Check(!zp::fs::exists(renamed) && zp::ReadSetup(undoPath).bars[8] == 77, "delete archives the named setup while retaining Undo");
}
void Tests(const zp::fs::path &root) {
  Check(!zp::Segment(L"..") && !zp::Segment(L"CON.txt") && !zp::Segment(L"x\\y"), "unsafe path segments rejected");
  Check(zp::Segment(L"Realm Name") && zp::Account(L"TEST-1"), "normal identities accepted");
  Check(!zp::Within(L"C:\\WTF\\AccountElse\\A", L"C:\\WTF\\Account"), "directory boundary respected");
  store = std::make_unique<zp::Store>(root);
  auto original = store->original / L"TESTACCOUNT";
  auto character = original / L"Realm/Testchar";
  auto profile = store->Profile(L"TESTACCOUNT");
  Check(store->Isolated(L"TESTACCOUNT"), "isolation activates in the next process");
  Check(store->Route((original / L"macros-cache.txt").wstring(), false) == (profile / L"macros-cache.txt").wstring(), "isolated reads select clone");
  Check(Contents(profile / L"macros-cache.txt") == "official account macro", "clone seeded from original");
  Check(store->Route((store->original / L"NORMALACCOUNT/bindings-cache.wtf").wstring(), true) == (store->original / L"NORMALACCOUNT/bindings-cache.wtf").wstring(), "Default files pass through");
  {
    zp::Store other(root);
    bool refused = false;
    try { other.EnsureProfile(L"TESTACCOUNT"); } catch (...) { refused = true; }
    Check(refused, "a second client cannot share an isolated account");
  }
  HookScope hooks;
  Write(original / L"macros-cache.txt", "isolated macro", true);
  Write(character / L"layout-cache.txt", "isolated layout");
  Check(Contents(original / L"macros-cache.txt") == "official account macro", "ANSI writes preserve official macros");
  Check(Contents(profile / L"macros-cache.txt") == "isolated macro", "ANSI writes reach clone");
  {
    std::ofstream stream(original / L"macros-cache.txt", std::ios::binary | std::ios::trunc);
    stream << "isolated macro";
  }
  Check(Contents(original / L"macros-cache.txt") == "official account macro", "C runtime stream writes preserve official macros");
  Check(Contents(profile / L"macros-cache.txt") == "isolated macro", "C runtime stream writes reach clone");
  Check(Contents(character / L"layout-cache.txt") == "official character layout", "Unicode writes preserve official layout");
  Write(store->original / L"NORMALACCOUNT/bindings-cache.wtf", "changed default");
  Check(Contents(store->original / L"NORMALACCOUNT/bindings-cache.wtf") == "changed default", "Default writes keep normal behavior");
  Check(CreateDirectoryW((character / L"NewFolder").c_str(), nullptr) != 0, "new folders route to clone");
  Write(character / L"NewFolder/inside.txt", "new file");
  Check(!zp::fs::exists(profile / L"Realm/Testchar/missing.txt"), "unrelated paths are not fabricated");
  Check(CopyFileW((character / L"layout-cache.txt").c_str(), (character / L"copy.txt").c_str(), TRUE) != 0, "copy routes both paths");
  Check(MoveFileExW((character / L"copy.txt").c_str(), (character / L"renamed.txt").c_str(), MOVEFILE_WRITE_THROUGH) != 0, "rename stays in clone");
  Check(DeleteFileW((character / L"renamed.txt").c_str()) != 0, "delete stays in clone");
  Write(character / L"replacement.txt", "atomic replacement");
  Check(ReplaceFileW((character / L"layout-cache.txt").c_str(), (character / L"replacement.txt").c_str(), nullptr, 0, nullptr, nullptr) != 0, "atomic file replacement stays in clone");
  Check(Contents(character / L"layout-cache.txt") == "official character layout", "atomic file replacement preserves original");
  Check(CopyFile2((character / L"layout-cache.txt").c_str(), (character / L"copy2.txt").c_str(), nullptr) == S_OK, "modern CopyFile2 stays in clone");
  Check(GetFileAttributesW((character / L"NewFolder/inside.txt").c_str()) != INVALID_FILE_ATTRIBUTES, "attribute queries use clone");
  WIN32_FIND_DATAW found{};
  HANDLE find = FindFirstFileW((character / L"NewFolder/*").c_str(), &found);
  Check(find != INVALID_HANDLE_VALUE, "directory enumeration sees clone");
  FindClose(find);
  wchar_t previous[32768]{}; GetCurrentDirectoryW(32768, previous);
  Check(SetCurrentDirectoryW(character.c_str()) != 0, "client can change working directory");
  Write(L"layout-cache.txt", "relative isolated layout");
  Check(SetCurrentDirectoryW(previous) != 0, "working directory restored");
  Check(Contents(character / L"layout-cache.txt") == "official character layout", "bare relative writes preserve original");
  Check(Contents(profile / L"Realm/Testchar/layout-cache.txt") == "relative isolated layout", "bare relative writes reach clone");

  originalSend = &Sent; originalDispatch = &Received;
  int connection = 0;
  currentConnection = &connection;
  sessions.clear(); sessions[&connection].account = L"TESTACCOUNT";
  auto &session = sessions[&connection];
  uint32_t auth = 0;
  auto authBytes = Message(0x1EE, &auth, sizeof(auth), 2); auto authPacket = Wrap(authBytes, true);
  DispatchHook(&connection, nullptr, 7, &authPacket);
  Check(received == 1 && store->problem.empty() && authPacket.read == 0, "borrowed server authentication response forwards unchanged for an isolated account");
  received = 0; // The remaining checks count character/session traffic.
  uint64_t guid = 42;
  auto loginBytes = Message(zp::PlayerLogin, &guid, sizeof(guid), 4); auto login = Wrap(loginBytes);
  SendHook(&connection, nullptr, &login);
  Check(sent == 1 && session.guid == guid, "login forwards and captures exact character GUID");
  zp::Bars official{}; official[0] = 133; official[1] = 0x40000007;
  auto barsBytes = Message(zp::ActionButtons, official.data(), sizeof(official), 2); auto barsPacket = Wrap(barsBytes, true);
  DispatchHook(&connection, nullptr, 7, &barsPacket);
  Check(received == 1 && session.ready && delivered == official, "server action bars seed first profile");
  uint8_t edit[5]{0}; uint32_t action = 116;
  std::memcpy(edit + 1, &action, 4);
  auto editBytes = Message(zp::SetActionButton, edit, sizeof(edit), 4); auto editPacket = Wrap(editBytes);
  SendHook(&connection, nullptr, &editPacket);
  Check(sent == 1 && session.bars[0] == 116, "action edit is checkpointed and never forwarded");
  zp::Bars saved{};
  Check(store->LoadBars(L"TESTACCOUNT", guid, false, saved) && saved[0] == 116 && saved[1] == official[1], "action checkpoint preserves unedited macro slots");
  auto spellBytes = Message(0x12E, &action, 4, 4); auto spellPacket = Wrap(spellBytes);
  SendHook(&connection, nullptr, &spellPacket);
  Check(sent == 2, "normal casting remains untouched");
  auto uploadBytes = Message(zp::UpdateAccountData, &action, 4, 4); auto uploadPacket = Wrap(uploadBytes);
  SendHook(&connection, nullptr, &uploadPacket);
  Check(sent == 2, "account-data uploads blocked");
  auto requestBytes = Message(zp::RequestAccountData, &action, 4, 4); auto requestPacket = Wrap(requestBytes);
  SendHook(&connection, nullptr, &requestPacket);
  Check(sent == 2, "server cache downloads not requested in isolation");
  uint8_t toggle = 9;
  auto toggleBytes = Message(zp::ActionBarToggles, &toggle, 1, 4); auto togglePacket = Wrap(toggleBytes);
  SendHook(&connection, nullptr, &togglePacket);
  Check(sent == 2, "bar-layout server writes blocked");
  uint8_t times[128]{}; std::memset(times, 0xEE, sizeof(times));
  auto timeBytes = Message(zp::AccountDataTimes, times, sizeof(times), 2); auto timePacket = Wrap(timeBytes, true);
  { PlayerFixture client; digestReads = 0; DispatchHook(&connection, nullptr, 7, &timePacket); }
  bool local = true;
  for (size_t i = 0; i < 128; ++i) local = local && timeBytes[i + 2] == static_cast<uint8_t>(i + 1);
  Check(local && received == 2 && digestReads == 8, "eight local MD5 digests preserve cache initialization without server imports");
  uint8_t wrong[32]{};
  auto wrongBytes = Message(zp::AccountDataTimes, wrong, sizeof(wrong), 2); auto wrongPacket = Wrap(wrongBytes, true);
  DispatchHook(&connection, nullptr, 7, &wrongPacket);
  Check(received == 2 && !store->problem.empty(), "unsupported timestamp-shaped packets still fail closed");
  store->problem.clear();
  auto downloadBytes = Message(zp::ServerAccountData, &action, 4, 2); auto downloadPacket = Wrap(downloadBytes, true);
  DispatchHook(&connection, nullptr, 7, &downloadPacket);
  Check(received == 2, "server cache payloads do not overwrite isolated settings");
  auto newer = official; newer[2] = 999;
  barsBytes = Message(zp::ActionButtons, newer.data(), sizeof(newer), 2); barsPacket = Wrap(barsBytes, true);
  DispatchHook(&connection, nullptr, 7, &barsPacket);
  Check(delivered[0] == 116 && delivered[2] == 0, "future logins preserve isolated slots");
  Check(store->LoadBars(L"TESTACCOUNT", guid, true, saved) && saved[2] == 999, "latest server baseline retained for Revert");
  auto badBytes = Message(zp::SetActionButton, edit, 4, 4); auto badPacket = Wrap(badBytes);
  SendHook(&connection, nullptr, &badPacket);
  Check(sent == 2 && !store->problem.empty(), "malformed settings fail closed");
  {
    zp::Bypass guard;
    SetFileAttributesW(store->Journal(L"TESTACCOUNT", guid, false).c_str(), FILE_ATTRIBUTE_READONLY);
  }
  SendHook(&connection, nullptr, &editPacket);
  Check(sent == 2, "checkpoint failures do not leak settings to server");
  {
    zp::Bypass guard;
    SetFileAttributesW(store->Journal(L"TESTACCOUNT", guid, false).c_str(), FILE_ATTRIBUTE_NORMAL);
  }
  session.realm = L"Realm"; session.character = L"Testchar";
  store->Identify(session.account, session.realm, session.character, guid);
  SetupTests(session);
  Write(original / L"Realm/Otherchar/layout-cache.txt", "isolated other layout");
  store->Revert(L"TESTACCOUNT", L"Realm", L"Testchar", true);
  {
    PlayerFixture player;
    Check(Replay(session), "current-character Revert applies official bars to the live action table");
    Check(std::memcmp(At<void *>(0xBC6980), newer.data(), sizeof(newer)) == 0, "Revert restores latest official live action bars");
  }
  SendHook(&connection, nullptr, &editPacket);
  Check(store->LoadBars(L"TESTACCOUNT", guid, false, saved) && saved == newer, "old UI action saves cannot overwrite reverted bar checkpoint");
  Check(Contents(profile / L"Realm/Testchar/layout-cache.txt") == "official character layout", "character Revert restores original files");
  Check(Contents(profile / L"macros-cache.txt") == "isolated macro", "character Revert preserves shared account macros");
  Check(Contents(profile / L"Realm/Otherchar/layout-cache.txt") == "isolated other layout", "character Revert preserves other characters");
  Write(character / L"layout-cache.txt", "stale Lua logout write");
  Check(Contents(profile / L"Realm/Testchar/layout-cache.txt") == "official character layout", "old UI saves cannot overwrite Revert during reload");
  store->muted.clear();
  Write(character / L"layout-cache.txt", "fresh UI layout");
  Check(Contents(profile / L"Realm/Testchar/layout-cache.txt") == "fresh UI layout", "new UI can save after resume");
  store->Revert(L"TESTACCOUNT", L"", L"", false);
  Check(Contents(profile / L"macros-cache.txt") == "official account macro", "account Revert restores shared macros");
  Check(Contents(profile / L"Realm/Otherchar/layout-cache.txt") == "official other layout", "account Revert covers all character files");
  Check(zp::fs::exists(store->data / L"backups"), "Reverts retain backups");
  session.account = L"NORMALACCOUNT";
  SendHook(&connection, nullptr, &editPacket);
  DispatchHook(&connection, nullptr, 7, &downloadPacket);
  Check(sent == 3 && received == 4, "Default account network writes and cache reads pass through");
  store->Mode(L"TESTACCOUNT", false);
  Check(store->Isolated(L"TESTACCOUNT"), "disabling isolation does not upload an in-memory isolated session");
  store->Mode(L"TESTACCOUNT", true); // retain isolation for abrupt-exit child
  Check(Contents(original / L"macros-cache.txt") == "official account macro", "official account macros unchanged after full suite");
  Check(Contents(character / L"layout-cache.txt") == "official character layout", "official layout unchanged after full suite");
}
int CrashChild(const zp::fs::path &root) {
  store = std::make_unique<zp::Store>(root);
  HookScope hooks;
  Write(store->original / L"TESTACCOUNT/macros-cache.txt", "crash-session macro");
  zp::Bars bars{}; bars[3] = 12345;
  store->SaveBars(L"TESTACCOUNT", 42, false, bars);
  zp::Setup setup{bars, 7, "ZPB1\n31\t414354494f4e425554544f4e31\n"};
  zp::WriteSetup(*store, zp::SetupPath(*store, L"TESTACCOUNT", 42, L"Crash saved"), setup);
  // Deliberately bypass destructors and every logout/save event.
  TerminateProcess(GetCurrentProcess(), 123);
  return 1;
}
void AbruptExit(const zp::fs::path &root, const std::wstring &executable) {
  std::wstring command = L"\"" + executable + L"\" --crash \"" + root.wstring() + L"\"";
  STARTUPINFOW startup{}; startup.cb = sizeof(startup);
  PROCESS_INFORMATION process{};
  Check(CreateProcessW(nullptr, &command[0], nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process) != 0, "abrupt-exit child starts");
  Check(WaitForSingleObject(process.hProcess, 30000) == WAIT_OBJECT_0, "abrupt-exit child finishes");
  DWORD status = 0; GetExitCodeProcess(process.hProcess, &status);
  CloseHandle(process.hThread); CloseHandle(process.hProcess);
  Check(status == 123, "child was killed without clean-up");
  zp::Store restarted(root);
  restarted.EnsureProfile(L"TESTACCOUNT");
  Check(Contents(restarted.original / L"TESTACCOUNT/macros-cache.txt") == "official account macro", "forced termination leaves official macros untouched");
  Check(Contents(restarted.Profile(L"TESTACCOUNT") / L"macros-cache.txt") == "crash-session macro", "isolated file survives forced termination");
  zp::Bars bars{};
  Check(restarted.LoadBars(L"TESTACCOUNT", 42, false, bars) && bars[3] == 12345, "action checkpoint survives forced termination and restart");
  auto setup = zp::ReadSetup(zp::SetupPath(restarted, L"TESTACCOUNT", 42, L"Crash saved"));
  Check(setup.bars[3] == 12345 && setup.toggles == 7 && setup.bindings == "ZPB1\n31\t414354494f4e425554544f4e31\n", "explicitly saved setups survive process termination without logout");
  zp::AtomicWrite(restarted.data / L"modes.txt", "corrupt registry");
  bool refused = false;
  try { zp::Store invalid(root); } catch (...) { refused = true; }
  Check(refused, "corrupt registry cannot silently revert to Default");
}
}

int wmain(int argc, wchar_t **argv) {
  try {
    if (argc == 3 && std::wstring(argv[1]) == L"--crash") return CrashChild(argv[2]);
    wchar_t executable[32768]{}; GetModuleFileNameW(nullptr, executable, 32768);
    auto root = zp::fs::absolute(zp::fs::path(executable).parent_path() / L"../.test-data" / std::to_wstring(GetTickCount64()));
    PacketWindowTests();
    Fixture(root);
    UsernameHistoryTests(root);
    BindingTemplateTests(root);
    Tests(root);
    store.reset(); sessions.clear();
    AbruptExit(root, executable);
    std::cout << "PASS: " << checks << " native checks, including real Win32 file detours and forced process termination.\n";
    return 0;
  } catch (const std::exception &error) {
    MH_DisableHook(MH_ALL_HOOKS); MH_Uninitialize(); zp::files::store = nullptr;
    std::cerr << "FAIL after " << checks << " checks: " << error.what() << '\n';
    return 1;
  }
}
