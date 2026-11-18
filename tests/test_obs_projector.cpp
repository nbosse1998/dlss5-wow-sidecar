#include <catch2/catch_test_macros.hpp>
#include "present/ObsProjector.h"
#include "core/Config.h"

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
  REQUIRE_FALSE(ValidateObsProjector(nullptr, 1));
}
TEST_CASE("OBS mode defaults to WoW for existing configurations", "[unit][obs]") {
  REQUIRE_FALSE(Config{}.obsProjector);
}

TEST_CASE("OBS mode persists independently of WoW folders", "[unit][obs]") {
  std::vector<std::string> warnings;
  auto config = ParseConfig("obs_projector = true\nwow_dir = 'C:/Games/WoW'", warnings);
  REQUIRE(warnings.empty());
  REQUIRE(config.obsProjector);
  const auto again = ParseConfig(SerializeConfig(config), warnings);
  REQUIRE(warnings.empty());
  REQUIRE(again.obsProjector);
  REQUIRE(again.wowDir == "C:/Games/WoW");
  ResetRenderingSettings(config);
  REQUIRE(config.obsProjector);
  warnings.clear();
  REQUIRE_FALSE(ParseConfig("obs_projector = 'yes'", warnings).obsProjector);
  REQUIRE(warnings.size() == 1);
}
