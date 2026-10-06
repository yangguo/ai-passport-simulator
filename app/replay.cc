// Headless deterministic fixture replay.
// Exit codes: 0 ok, 2 bad usage, 3 invalid fixture.
#include <fstream>
#include <iostream>
#include <string>

#include "passport_sim/button_input.h"
#include "passport_sim/scenario_runner.h"
#include "passport_sim/settings_store.h"
#include "passport_sim/virtual_clock.h"

namespace {

void Usage() {
  std::cerr << "usage: passport-replay --scenario <path> [--log <path>]\n";
}

}  // namespace

int main(int argc, char** argv) {
  std::string scenario;
  std::string log_path;
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--scenario" && i + 1 < argc) {
      scenario = argv[++i];
    } else if (arg == "--log" && i + 1 < argc) {
      log_path = argv[++i];
    } else {
      Usage();
      return 2;
    }
  }
  if (scenario.empty()) {
    Usage();
    return 2;
  }

  passport_sim::VirtualClock clock;
  passport_sim::CaptionBuffers captions;
  passport_sim::ButtonInput buttons(clock);
  passport_sim::SettingsStore settings;
  passport_sim::ScenarioRunner runner(clock, captions, buttons, settings);
  const passport_sim::LoadResult loaded = runner.load(scenario);
  if (!loaded.ok) {
    std::cerr << "invalid fixture: " << loaded.error << "\n";
    return 3;
  }
  runner.run();
  if (log_path.empty()) {
    std::cout << runner.log();
  } else {
    std::ofstream out(log_path);
    if (!out) {
      std::cerr << "cannot write log file: " << log_path << "\n";
      return 2;
    }
    out << runner.log();
  }
  return 0;
}
