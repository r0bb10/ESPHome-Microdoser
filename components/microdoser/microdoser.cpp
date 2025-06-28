// components/microdoser/microdoser.cpp

#include "microdoser.h"
#include "esphome/core/log.h"
#include "esphome/core/preferences.h"
#include "esphome/components/pcf8574/pcf8574.h"

namespace esphome {
namespace microdoser {

// --- LOGGING TAG ---
static const char *const TAG = "microdoser";

// --- Called once on startup ---
void MicrodoserPump::setup() {
  ESP_LOGI(TAG, "Setting up Microdoser on PCF8574 pin %u", i2c_pin_);
}

// --- Main loop: runs every cycle (~ms scale) ---
void MicrodoserPump::loop() {
  if (!this->time_->now().is_valid())
    return;

  auto now = this->time_->now();

  for (auto &entry : schedules_) {
    // --- If time matches and not already dosed ---
    if (entry.hour == now.hour && entry.minute == now.minute && !has_dosed_today(entry)) {
      ESP_LOGI(TAG, "Dosing %f mL on pin %u", dose_total_ml_, i2c_pin_);
      dose_now();
      mark_dosed(entry);
    }
  }
}

// --- Add a scheduled dose time ---
void MicrodoserPump::add_schedule(uint8_t hour, uint8_t minute) {
  schedules_.push_back(ScheduleEntry{hour, minute});
}

// --- Convert ml → duration, pulse pump ---
void MicrodoserPump::dose_now() {
  float seconds = dose_total_ml_ / calibration_;
  auto duration = static_cast<uint32_t>(seconds * 1000);  // in ms

  // Get first available PCF8574 (assumes 1 expander for now)
  auto *pcf = pcf8574::PCF8574Component::get_first();
  if (!pcf) {
    ESP_LOGE(TAG, "No PCF8574 found");
    return;
  }

  // Trigger pump via PCF pin
  pcf->digital_write(i2c_pin_, true);
  delay(duration);
  pcf->digital_write(i2c_pin_, false);
}

// --- Save record that pump was dosed at this time ---
void MicrodoserPump::mark_dosed(ScheduleEntry &entry) {
  uint32_t key = get_today_key(entry);
  global_preferences->make_preference<uint32_t>(key).save(1);
  entry.dosed_today = true;
}

// --- Check persistent storage if already dosed ---
bool MicrodoserPump::has_dosed_today(const ScheduleEntry &entry) {
  uint32_t key = get_today_key(entry);
  uint32_t val = 0;
  auto pref = global_preferences->make_preference<uint32_t>(key);
  pref.load(&val);
  return val == 1;
}

// --- Generate unique key per day+time (used for persistence) ---
uint32_t MicrodoserPump::get_today_key(const ScheduleEntry &entry) {
  auto now = this->time_->now();
  // Combine date + schedule time into unique key
  return (now.day_of_year << 8) | (entry.hour << 4) | entry.minute;
}

}  // namespace microdoser
}  // namespace esphome
