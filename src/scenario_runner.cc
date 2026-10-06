#include "passport_sim/scenario_runner.h"

#include <fstream>
#include <sstream>

#include <nlohmann/json.hpp>

namespace passport_sim {
namespace {

constexpr size_t kMaxEvents = 4096;
constexpr size_t kMaxTextBytes = 8192;

const char* kTopKeys[] = {"scenario_version", "name", "events", "meta"};

std::string EscapeLog(std::string_view s) {
  std::string out;
  for (char c : s) {
    if (c == '"')
      out += "\\\"";
    else if (c == '\\')
      out += "\\\\";
    else if (c == '\n')
      out += "\\n";
    else
      out += c;
  }
  return out;
}

const char* ActivityName(PassportActivity a) {
  switch (a) {
    case PassportActivity::kListening:
      return "Listening";
    case PassportActivity::kThinking:
      return "Thinking";
    case PassportActivity::kSpeaking:
      return "Speaking";
    default:
      return "None";
  }
}

const char* ButtonEventName(ButtonEvent e) {
  switch (e) {
    case ButtonEvent::Press:
      return "press";
    case ButtonEvent::Click:
      return "click";
    case ButtonEvent::Double:
      return "double";
    case ButtonEvent::Long:
      return "long";
    default:
      return "none";
  }
}

bool IsKnownType(const std::string& t) {
  return t == "state" || t == "stt" || t == "tts_start" ||
         t == "tts_sentence" || t == "tts_stop" || t == "key" || t == "note";
}

bool IsKnownState(const std::string& v) {
  return v == "idle" || v == "connecting" || v == "listening" ||
         v == "speaking" || v == "notifying" || v == "thinking";
}

DeviceState ToDeviceState(const std::string& v) {
  if (v == "idle") return kDeviceStateIdle;
  if (v == "connecting") return kDeviceStateConnecting;
  if (v == "listening") return kDeviceStateListening;
  if (v == "speaking") return kDeviceStateSpeaking;
  if (v == "notifying") return kDeviceStateSpeaking;  // same activity mapping
  return kDeviceStateUnknown;
}

}  // namespace

ScenarioRunner::ScenarioRunner(VirtualClock& clock, CaptionBuffers& captions,
                               ButtonInput& buttons, SettingsStore& settings)
    : clock_(clock),
      conversation_(captions),
      buttons_(buttons),
      settings_(settings),
      captions_(captions) {}

LoadResult ScenarioRunner::load(const std::string& path) {
  // Start clean so a failed load never leaves partial state behind.
  events_.clear();
  reset();
  const auto fail = [&](const std::string& reason) {
    events_.clear();
    reset();
    return LoadResult{false, path + " " + reason};
  };

  std::ifstream in(path);
  if (!in) return fail("cannot open file");
  nlohmann::json root;
  try {
    root = nlohmann::json::parse(in);
  } catch (const std::exception& e) {
    return fail(std::string("invalid JSON: ") + e.what());
  }
  if (!root.is_object()) return fail("root must be an object");
  for (const auto& [k, _] : root.items()) {
    bool known = false;
    for (const char* want : kTopKeys) known = known || k == want;
    if (!known) return fail("unexpected field '" + k + "'");
  }
  if (!root.contains("scenario_version") ||
      !root["scenario_version"].is_number_integer() ||
      root["scenario_version"].get<long long>() != 1) {
    return fail("unsupported scenario_version (want 1)");
  }
  if (!root.contains("name") || !root["name"].is_string() ||
      root["name"].get<std::string>().empty()) {
    return fail("missing non-empty 'name'");
  }
  if (root.contains("meta") && !root["meta"].is_object()) {
    return fail("'meta' must be an object");
  }
  if (!root.contains("events") || !root["events"].is_array()) {
    return fail("missing 'events' array");
  }
  if (root["events"].size() > kMaxEvents) {
    return fail("too many events (max 4096)");
  }

  uint32_t prev_ms = 0;
  bool first = true;
  size_t index = 0;
  for (const auto& e : root["events"]) {
    const std::string where = "event " + std::to_string(index);
    if (!e.is_object()) return fail(where + ": must be an object");
    if (!e.contains("at_ms") || !e["at_ms"].is_number_unsigned()) {
      return fail(where + ": missing non-negative integer 'at_ms'");
    }
    const uint64_t wide = e["at_ms"].get<uint64_t>();
    if (wide > 0xFFFFFFFFu) {
      return fail(where + ": 'at_ms' out of range");
    }
    const uint32_t at_ms = static_cast<uint32_t>(wide);
    if (!first && at_ms < prev_ms) {
      return fail(where + ": 'at_ms' goes backwards (" +
                  std::to_string(at_ms) + " < " + std::to_string(prev_ms) +
                  ")");
    }
    first = false;
    prev_ms = at_ms;
    if (!e.contains("type") || !e["type"].is_string()) {
      return fail(where + ": missing string 'type'");
    }
    const std::string type = e["type"].get<std::string>();
    if (!IsKnownType(type)) {
      return fail(where + ": unknown type '" + type + "'");
    }

    ScenarioEvent ev;
    ev.at_ms = at_ms;
    ev.type = type;
    const auto need_text = [&]() -> LoadResult {
      if (!e.contains("text") || !e["text"].is_string()) {
        return fail(where + ": missing string 'text'");
      }
      const std::string t = e["text"].get<std::string>();
      if (!utf8_valid(t)) return fail(where + ": 'text' is not valid UTF-8");
      if (t.size() > kMaxTextBytes) {
        return fail(where + ": 'text' exceeds 8 KiB");
      }
      ev.text = t;
      return {true, ""};
    };
    if (type == "state") {
      if (!e.contains("value") || !e["value"].is_string()) {
        return fail(where + ": missing string 'value'");
      }
      ev.value = e["value"].get<std::string>();
      if (!IsKnownState(ev.value)) {
        return fail(where + ": unknown state '" + ev.value + "'");
      }
      for (const auto& [k, _] : e.items()) {
        if (k != "at_ms" && k != "type" && k != "value") {
          return fail(where + ": unexpected field '" + k + "'");
        }
      }
    } else if (type == "stt" || type == "tts_sentence" || type == "note") {
      const LoadResult r = need_text();
      if (!r.ok) return r;
      for (const auto& [k, _] : e.items()) {
        if (k != "at_ms" && k != "type" && k != "text") {
          return fail(where + ": unexpected field '" + k + "'");
        }
      }
    } else if (type == "tts_start" || type == "tts_stop") {
      for (const auto& [k, _] : e.items()) {
        if (k != "at_ms" && k != "type") {
          return fail(where + ": unexpected field '" + k + "'");
        }
      }
    } else {  // key
      if (!e.contains("key") || !e["key"].is_string()) {
        return fail(where + ": missing string 'key'");
      }
      ev.key = e["key"].get<std::string>();
      if (ev.key != "up" && ev.key != "down" && ev.key != "ok") {
        return fail(where + ": unknown key '" + ev.key + "'");
      }
      if (!e.contains("action") || !e["action"].is_string()) {
        return fail(where + ": missing string 'action'");
      }
      ev.action = e["action"].get<std::string>();
      if (ev.action != "press" && ev.action != "release") {
        return fail(where + ": unknown action '" + ev.action + "'");
      }
      for (const auto& [k, _] : e.items()) {
        if (k != "at_ms" && k != "type" && k != "key" && k != "action") {
          return fail(where + ": unexpected field '" + k + "'");
        }
      }
    }
    events_.push_back(std::move(ev));
    ++index;
  }

  name_ = root["name"].get<std::string>();
  reset();
  log_ = "# SIMULATED fixture replay: " + name_ + "\n";
  return {true, ""};
}

void ScenarioRunner::reset() {
  clock_.reset();
  captions_.reset();
  buttons_.reset();
  settings_.reset();
  cursor_ = 0;
  activity_ = PassportActivity::kNone;
  channel_open_ = false;
  log_.clear();
}

bool ScenarioRunner::step() {
  if (cursor_ >= events_.size()) return false;
  const ScenarioEvent ev = events_[cursor_++];
  clock_.advance_to(ev.at_ms);
  std::ostringstream line;
  line << "[" << ev.at_ms << "] " << ev.type;

  if (ev.type == "state") {
    DeviceState dev = ToDeviceState(ev.value);
    if (ev.value == "thinking") {
      // The thinking gap: idle device, audio channel still open.
      channel_open_ = true;
      dev = kDeviceStateIdle;
    } else if (ev.value == "listening") {
      channel_open_ = true;
    }
    activity_ = PassportResolveActivity(activity_, dev, channel_open_, false);
    conversation_.system_event();
    line << " " << ev.value;
  } else if (ev.type == "stt") {
    const bool kept = conversation_.stt(ev.text);
    line << " \"" << EscapeLog(ev.text) << "\"" << (kept ? "" : " (ignored)");
  } else if (ev.type == "tts_start") {
    // TTS audio playing means the device is speaking, even with no new
    // DeviceState event on the wire.
    activity_ = PassportNextActivity(activity_, kDeviceStateSpeaking);
    conversation_.tts_start();
  } else if (ev.type == "tts_sentence") {
    conversation_.tts_sentence(ev.text);
    line << " \"" << EscapeLog(ev.text) << "\"";
  } else if (ev.type == "tts_stop") {
    activity_ = PassportNextActivity(activity_, kDeviceStateIdle);
    conversation_.tts_stop();
  } else if (ev.type == "key") {
    Button b = ev.key == "up" ? Button::Up
               : ev.key == "down" ? Button::Down
                                  : Button::Ok;
    if (ev.action == "press") {
      buttons_.press(b);
    } else {
      buttons_.release(b);
    }
    line << " " << ev.key << " " << ev.action << " -> "
         << ButtonEventName(buttons_.last_event());
  } else {  // note
    line << " \"" << EscapeLog(ev.text) << "\"";
  }

  line << " -> activity=" << ActivityName(activity_) << " user=\""
       << EscapeLog(captions_.user()) << "\" assistant=\""
       << EscapeLog(captions_.assistant()) << "\"\n";
  log_ += line.str();
  return true;
}

void ScenarioRunner::run() {
  while (step()) {
  }
}

}  // namespace passport_sim
