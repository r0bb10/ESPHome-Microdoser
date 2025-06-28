// components/microdoser/microdoser.h

#pragma once

// --- ESPHome Core ---
#include "esphome/core/component.h"

// --- RTC time support (from ESPHome) ---
#include "esphome/components/time/real_time_clock.h"

namespace esphome {
namespace microdoser {

// --- Main Class: MicrodoserPump ---
class MicrodoserPump : public Component {
 public:
  // ESPHome setup and loop lifecycle
  void setup() override;
  void loop() override;

  // --- Setters from YAML config ---
  void set_i2c_pin(uint8_t pin) { i2c_pin_ = pin; }
  void set_calibration(float ml_per_sec) { calibration_ = ml_per_sec; }
  void set_daily_dose(float ml) { dose_total_ml_ = ml; }
  void add_schedule(uint8_t hour, uint8_t minute);  // Adds a dose time

  void set_time_source(time::RealTimeClock *time) { time_ = time; }

 protected:
  // --- Config parameters ---
  uint8_t i2c_pin_;
  float calibration_;
  float dose_total_ml_;
  time::RealTimeClock *time_;

  // --- One pump can have multiple dose slots per day ---
  struct ScheduleEntry {
    uint8_t hour;
    uint8_t minute;
    bool dosed_today = false;
  };
  std::vector<ScheduleEntry> schedules_;

  // --- Internal methods ---
  void dose_now();  // Actually dose based on ml + calibration
  void mark_dosed(ScheduleEntry &entry);  // Save state after dosing
  bool has_dosed_today(const ScheduleEntry &entry);  // Check preference store
  uint32_t get_today_key(const ScheduleEntry &entry);  // Key = unique time-of-day
};

}  // namespace microdoser
}  // namespace esphome
