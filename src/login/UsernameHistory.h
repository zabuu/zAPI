#pragma once
#include "../profiles/Store.h"

namespace zlogin {
// This format stores account names only. Passwords never enter this API.
class UsernameHistory {
  zp::fs::path directory;
  class Lock {
    HANDLE handle = INVALID_HANDLE_VALUE;
   public:
    explicit Lock(const zp::fs::path &file) {
      for (int retry = 0; retry < 4; ++retry) {
        handle = CreateFileW(file.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (handle != INVALID_HANDLE_VALUE) return;
        if (GetLastError() != ERROR_SHARING_VIOLATION) break;
        Sleep(5);
      }
      throw std::runtime_error("Username history is unavailable.");
    }
    ~Lock() { if (handle != INVALID_HANDLE_VALUE) CloseHandle(handle); }
    Lock(const Lock &) = delete;
    Lock &operator=(const Lock &) = delete;
  };
  zp::fs::path File() const { return directory / L"login-usernames.txt"; }
  void Prepare() const {
    zp::PlainDirectory(directory);
    zp::fs::create_directories(directory);
    zp::PlainDirectory(File());
    zp::PlainDirectory(directory / L"login-usernames.lock");
  }
  std::vector<std::string> Read() const {
    if (!zp::fs::exists(File())) return {};
    const auto bytes = zp::ReadBytes(File(), 1024);
    std::istringstream input(std::string(bytes.begin(), bytes.end()));
    std::string line;
    if (!std::getline(input, line) || line != "ZAPI USERNAMES 1") throw std::runtime_error("Invalid username history.");
    std::vector<std::string> names;
    while (std::getline(input, line)) {
      if (!line.empty() && line.back() == '\r') line.pop_back();
      if (line.empty()) continue;
      const auto name = Normalize(line);
      if (name.empty() || name != line || names.size() == 5 || std::find(names.begin(), names.end(), name) != names.end())
        throw std::runtime_error("Invalid username history.");
      names.push_back(name);
    }
    return names;
  }
  void Write(const std::vector<std::string> &names) const {
    std::string data = "ZAPI USERNAMES 1\n";
    for (const auto &name : names) data += name + "\n";
    zp::AtomicWrite(File(), data);
  }
 public:
  explicit UsernameHistory(const zp::fs::path &game) : directory(game / L"WTF" / L"zAPI") {}
  static std::string Normalize(std::string name) {
    if (name.empty() || name.size() > 64) return {};
    for (auto &c : name) {
      if (c >= 'a' && c <= 'z') c -= 'a' - 'A';
      if (!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '#')) return {};
    }
    return name;
  }
  std::vector<std::string> Names() const {
    zp::Bypass bypass;
    Prepare(); Lock lock(directory / L"login-usernames.lock");
    return Read();
  }
  void Remember(const std::string &value, bool seedOnly = false) const {
    const auto name = Normalize(value);
    if (name.empty()) return;
    zp::Bypass bypass;
    Prepare(); Lock lock(directory / L"login-usernames.lock");
    auto names = Read();
    if (seedOnly && !names.empty()) return;
    names.erase(std::remove(names.begin(), names.end(), name), names.end());
    names.insert(names.begin(), name);
    if (names.size() > 5) names.resize(5);
    Write(names);
  }
  void Clear() const {
    zp::Bypass bypass;
    Prepare(); Lock lock(directory / L"login-usernames.lock");
    Write({});
  }
};
}
