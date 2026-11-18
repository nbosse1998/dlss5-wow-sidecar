#include <catch2/catch_test_macros.hpp>
#include "present/ObsProjectorRules.h"

using namespace sidecar;
TEST_CASE("OBS projector identity requires both executable and projector title", "[unit][obs]") {
  REQUIRE(ObsProjectorMatches(L"C:\\Portable OBS\\bin\\64bit\\obs64.exe", L"Windowed Projector (Preview)"));
  REQUIRE(ObsProjectorMatches(L"OBS64.EXE", L"Fullscreen Projector (Program)"));
  REQUIRE(ObsProjectorMatches(L"obs64.exe", L"Projector - Scene: Test"));
  REQUIRE(ObsProjectorMatches(L"obs32.exe", L"Windowed Projector (Source) - Test"));
  REQUIRE_FALSE(ObsProjectorMatches(L"Wow.exe", L"Windowed Projector (Preview)"));
  REQUIRE_FALSE(ObsProjectorMatches(L"fake-obs64.exe", L"Projector - Preview"));
  REQUIRE_FALSE(ObsProjectorMatches(L"obs64.exe", L"OBS 33.0 - Profile: Projector"));
  REQUIRE_FALSE(ObsProjectorMatches(L"obs64.exe", L"Settings"));
  REQUIRE_FALSE(ObsProjectorMatches(L"obs64.exe", L""));
}
