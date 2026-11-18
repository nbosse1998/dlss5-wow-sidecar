#include "present/ObsProjector.h"
#include "present/WindowTracker.h"
#include <cwchar>
#include <iterator>

namespace sidecar {
std::optional<ObsProjector> ValidateObsProjector(HWND hwnd, DWORD expectedPid) {
  if (!hwnd || !IsWindow(hwnd) || !IsWindowVisible(hwnd) || IsIconic(hwnd) ||
      GetAncestor(hwnd, GA_ROOT) != hwnd) return std::nullopt;
  std::wstring title(static_cast<size_t>(GetWindowTextLengthW(hwnd)) + 1, L'\0');
  const int count = GetWindowTextW(hwnd, title.data(), static_cast<int>(title.size()));
  title.resize(static_cast<size_t>(count));
  // Reject non-projector titles before querying any process.
  if (!ObsProjectorMatches(L"obs64.exe", title)) return std::nullopt;
  DWORD pid = 0;
  GetWindowThreadProcessId(hwnd, &pid);
  if (!pid || (expectedPid && pid != expectedPid)) return std::nullopt;
  // No VM access rights, no executable metadata, no install-directory assumption.
  HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
  if (!process) return std::nullopt;
  wchar_t path[32768]{};
  DWORD length = static_cast<DWORD>(std::size(path));
  const bool queried = QueryFullProcessImageNameW(process, 0, path, &length) != FALSE;
  CloseHandle(process);
  if (!queried || !ObsProjectorMatches(std::wstring_view(path, length), title)) return std::nullopt;
  auto rect = ClientRectInScreen(hwnd);
  if (!rect || rect->right <= rect->left || rect->bottom <= rect->top) return std::nullopt;
  return ObsProjector{hwnd, pid, std::move(title), *rect};
}

std::vector<ObsProjector> FindObsProjectors() {
  std::vector<ObsProjector> windows;
  EnumWindows([](HWND hwnd, LPARAM data) -> BOOL {
    auto& found = *reinterpret_cast<std::vector<ObsProjector>*>(data);
    if (auto candidate = ValidateObsProjector(hwnd, 0)) found.push_back(std::move(*candidate));
    return TRUE;
  }, reinterpret_cast<LPARAM>(&windows));
  return windows;
}
}  // namespace sidecar
