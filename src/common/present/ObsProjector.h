#pragma once
#include <windows.h>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace sidecar {
struct ObsProjector {
  HWND hwnd = nullptr;
  DWORD pid = 0;
  std::wstring title;
  RECT clientScreen{};
};

// English OBS titles: current "Projector - ..." and older full/windowed forms.
// Deliberately excludes the OBS main window, dialogs and multiview.
bool ObsProjectorMatches(std::wstring_view executable, std::wstring_view title);
std::vector<ObsProjector> FindObsProjectors();
// Rechecks process identity, title and availability; never substitutes another HWND.
std::optional<ObsProjector> ValidateObsProjector(HWND hwnd, DWORD expectedPid);
}  // namespace sidecar
