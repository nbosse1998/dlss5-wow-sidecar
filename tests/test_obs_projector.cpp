#include <catch2/catch_test_macros.hpp>
#include "present/ObsProjector.h"
#include "core/Config.h"

using namespace sidecar;
TEST_CASE("OBS rejects a missing window", "[unit][obs]") {
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
