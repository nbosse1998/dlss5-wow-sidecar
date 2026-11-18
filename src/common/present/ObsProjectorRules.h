#pragma once
#include <string_view>

namespace sidecar {
// English OBS titles: current "Projector - ..." and older full/windowed forms.
// Pure identity rule, kept independent of Win32 so it can be tested anywhere.
inline bool ObsProjectorMatches(std::wstring_view executable, std::wstring_view title) {
  const auto slash = executable.find_last_of(L"/\\");
  const auto basename = executable.substr(slash == executable.npos ? 0 : slash + 1);
  const auto sameName = [basename](std::wstring_view expected) {
    if (basename.size() != expected.size()) return false;
    for (size_t i = 0; i < basename.size(); ++i) {
      const wchar_t c = basename[i];
      const wchar_t lower = c >= L'A' && c <= L'Z' ? c + (L'a' - L'A') : c;
      if (lower != expected[i]) return false;
    }
    return true;
  };
  if (!sameName(L"obs64.exe") && !sameName(L"obs32.exe")) return false;
  return title.starts_with(L"Projector - ") ||
         title.starts_with(L"Windowed Projector (") ||
         title.starts_with(L"Fullscreen Projector (");
}
}  // namespace sidecar
