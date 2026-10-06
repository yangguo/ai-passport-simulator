// SIMULATED settings store (P0 stub: in-memory only, resettable).
// Never touches physical NVS.
#pragma once

#include <map>
#include <string>

namespace passport_sim {

class SettingsStore {
 public:
  SettingsStore() = default;
  void set(const std::string& key, const std::string& value);
  std::string get(const std::string& key,
                  const std::string& fallback = "") const;
  // Restores defaults (P0: empty).
  void reset();

 private:
  std::map<std::string, std::string> values_;
};

}  // namespace passport_sim
