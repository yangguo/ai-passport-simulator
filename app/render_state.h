// Single mapping from runner state to widgets (used by both the interactive
// shell and the headless screenshot path so they cannot diverge): captions +
// activity, transport error to the alert overlay, audio waterline to the
// activity-line label. Must be called after runner state settled for the
// frame/checkpoint; the shell owns layout computation.
#pragma once

#include <string>

#include "passport_sim/lvgl_shell.h"
#include "passport_sim/scenario_runner.h"

namespace passport_sim {

inline void RenderScenario(LvglShell& shell, const ScenarioRunner& runner) {
  shell.render(runner.captions(), runner.activity());
  if (runner.transport().has_error()) {
    shell.set_alert(std::string(runner.transport().error_message()).c_str());
  } else {
    shell.clear_alert();
  }
  shell.set_status_suffix(runner.audio().status_line().c_str());
}

}  // namespace passport_sim
