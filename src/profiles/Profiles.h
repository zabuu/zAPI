#pragma once
#include <Windows.h>
#include <string>

namespace zprofiles {
using CFunction = int(__fastcall *)(void *);
using Register = void(__fastcall *)(const char *, CFunction);
DWORD Prepare(); // Called before the shared registration hook is installed.
CFunction WrapFunction(const char *name, CFunction function);
void OnRegistered(const char *name, Register registerFunction);
void ProcessDetach(bool processExit);
void Log(const std::string &text) noexcept;
}
