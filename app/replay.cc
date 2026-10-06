// Headless deterministic fixture replay.
// Exit codes: 0 ok, 2 bad usage, 3 invalid fixture.
#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>

#include "passport_sim/button_input.h"
#include "passport_sim/lvgl_shell.h"
#include "passport_sim/scenario_runner.h"
#include "passport_sim/screenshot_png.h"
#include "passport_sim/settings_store.h"
#include "passport_sim/virtual_clock.h"

namespace {

void Usage() {
  std::cerr << "usage: passport-replay --scenario <path> [--log <path>] "
               "[--screenshot <png> [--at-ms <n>] [--alert <text>]]\n";
}

}  // namespace

int main(int argc, char** argv) {
  std::string scenario;
  std::string log_path;
  std::string shot_path;
  std::string alert_text;
  uint32_t at_ms = UINT32_MAX;
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--scenario" && i + 1 < argc) {
      scenario = argv[++i];
    } else if (arg == "--log" && i + 1 < argc) {
      log_path = argv[++i];
    } else if (arg == "--screenshot" && i + 1 < argc) {
      shot_path = argv[++i];
    } else if (arg == "--alert" && i + 1 < argc) {
      alert_text = argv[++i];
    } else if (arg == "--at-ms" && i + 1 < argc) {
      at_ms = static_cast<uint32_t>(std::stoul(argv[++i]));
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
  if (!shot_path.empty()) {
    passport_sim::LvglShell shell;
    // Re-run to the checkpoint so the screenshot matches --at-ms exactly.
    if (at_ms != UINT32_MAX) {
      passport_sim::VirtualClock clock2;
      passport_sim::CaptionBuffers captions2;
      passport_sim::ButtonInput buttons2(clock2);
      passport_sim::SettingsStore settings2;
      passport_sim::ScenarioRunner run2(clock2, captions2, buttons2, settings2);
      run2.load(scenario);
      while (run2.next_at_ms() <= at_ms && run2.step()) {
      }
      shell.render(captions2, run2.activity());
    } else {
      shell.render(captions, runner.activity());
    }
    if (!alert_text.empty()) shell.set_alert(alert_text.c_str());
    shell.tick(100);
    if (!passport_sim::SaveViewportPng(shot_path, shell.framebuffer(),
                                       passport_sim::LvglShell::kWidth,
                                       passport_sim::LvglShell::kHeight)) {
      std::cerr << "cannot write screenshot: " << shot_path << "\n";
      return 2;
    }
  }
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
