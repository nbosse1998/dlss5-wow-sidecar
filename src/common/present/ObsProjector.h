#pragma once
#include <windows.h>
#include <optional>
#include <string>
#include "present/ObsProjectorRules.h"
#include <vector>

namespace sidecar {
struct ObsProjector {
  HWND hwnd = nullptr;
  DWORD pid = 0;
  std::wstring title;
  RECT clientScreen{};
};

std::vector<ObsProjector> FindObsProjectors();
// Rechecks process identity, title and availability; never substitutes another HWND.
std::optional<ObsProjector> ValidateObsProjector(HWND hwnd, DWORD expectedPid);
}  // namespace sidecar
