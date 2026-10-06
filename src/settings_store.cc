#include "passport_sim/settings_store.h"

namespace passport_sim {

void SettingsStore::set(const std::string& key, const std::string& value) {
  values_[key] = value;
}

std::string SettingsStore::get(const std::string& key,
                               const std::string& fallback) const {
  const auto it = values_.find(key);
  return it == values_.end() ? fallback : it->second;
}

void SettingsStore::reset() { values_.clear(); }

}  // namespace passport_sim
