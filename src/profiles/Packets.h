#pragma once
#include "Store.h"

namespace zp {
struct Session {
  std::wstring account, realm, character;
  uint64_t guid = 0;
  Bars bars{};
  bool ready = false, togglesSeeded = false;
  uint8_t toggles = 0;

  // All errors propagate to the adapter, which drops settings packets and
  // reports the fault. Gameplay packets never enter these disk operations.
  bool Send(Store &store, uint32_t opcode, const uint8_t *body, size_t size) {
    if (opcode == PlayerLogin) {
      ready = false; togglesSeeded = false; guid = 0; realm.clear(); character.clear();
      if (body && size == sizeof(guid)) std::memcpy(&guid, body, sizeof(guid));
      return true;
    }
    if (!store.Isolated(account) || !SettingsUpload(opcode)) return true;
    ++store.blocked;
    for (const auto &scope : store.muted)
      if (Within(scope, (store.original / account).wstring())) return false;
    store.EnsureProfile(account);
    if (opcode == SetActionButton) {
      if (!ready) throw std::runtime_error("Action bars are not initialized; the server edit was blocked.");
      Bars edited = bars;
      if (!EditBars(edited, body, size)) throw std::runtime_error("Unsupported action-bar packet; the server edit was blocked.");
      store.SaveBars(account, guid, false, edited);
      bars = edited;
    } else if (opcode == ActionBarToggles) {
      if (!body || size != 1) throw std::runtime_error("Unsupported bar-layout packet; the server edit was blocked.");
      if (!guid) return false; // FrameXML can send zero at character select.
      auto path = store.Journal(account, guid, false); path += L".toggles";
      AtomicWrite(path, body, size);
      toggles = *body & 0x0F;
    }
    return false;
  }
  bool Receive(Store &store, uint32_t opcode, uint8_t *body, size_t size, const uint8_t *localDigests = nullptr) {
    if (!store.Isolated(account)) return true;
    if (opcode == ServerAccountData) return false;
    if (opcode == AccountDataTimes) {
      // The audited 1.12 handler reads eight 16-byte MD5 digests, not eight
      // u32 timestamps. Match the local files so normal cache initialization
      // proceeds without downloading server settings or uploading local edits.
      if (!body || size != 128 || !localDigests) throw std::runtime_error("Unsupported account-data checksum table; no server settings were imported.");
      std::memcpy(body, localDigests, size);
    } else if (opcode == ActionButtons) {
      if (!guid || !body || size != sizeof(Bars)) throw std::runtime_error("Unsupported action-bar seed packet.");
      Bars official{};
      std::memcpy(official.data(), body, sizeof(Bars));
      store.SaveBars(account, guid, true, official);
      if (!store.LoadBars(account, guid, false, bars)) {
        bars = official;
        store.SaveBars(account, guid, false, bars);
      }
      std::memcpy(body, bars.data(), sizeof(Bars));
      ready = true;
    }
    return true;
  }
};
} // namespace zp
