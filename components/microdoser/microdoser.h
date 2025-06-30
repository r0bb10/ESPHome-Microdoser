#pragma once

#include "esphome/core/component.h"
#include "esphome/core/preferences.h"
#include "esphome/components/select/select.h"
#include "esphome/components/number/number.h"
#include "esphome/components/button/button.h"
#include "esphome/components/output/binary_output.h"
#include "esphome/components/time/real_time_clock.h"

#include <vector>
#include <map>
#include <string>

namespace esphome {
namespace number {
class TemplateNumber;
}
namespace button {
class TemplateButton;
}
namespace microdoser {

// --- Main Class: MicrodoserPump ---
class MicrodoserPump : public Component {
 public:
  // ESPHome setup and loop lifecycle
  void setup() override;
  void loop() override;

  // --- Setters from YAML config ---
  void set_output_pin(output::BinaryOutput *output) { output_ = output; }
  void set_calibration(float ml_per_sec) { calibration_ = ml_per_sec; }
  void set_daily_dose(float ml) { dose_total_ml_ = ml; }
  void add_schedule(uint8_t hour, uint8_t minute);  // Adds a dose time
  void set_time_source(time::RealTimeClock *time) { time_ = time; }
  void set_index(uint8_t index) { this->index_ = index; }
  void set_id_string(const std::string &id) { this->id_string_ = id; }

  // --- Calibration handling ---
  void store_calibration(float new_value);
  void load_calibration();  // Not used yet, but useful if split needed
  void run_calibration_dose();
  void update_calibration_from_result(float measured_ml);

 protected:
  // --- Config parameters ---
  output::BinaryOutput *output_{nullptr};
  time::RealTimeClock *time_{nullptr};
  float calibration_{1.0f};
  float dose_total_ml_{0.0f};
  uint8_t index_{0};
  std::string id_string_;  // --- EEPROM-safe persistent key
  ESPPreferenceObject pref_calibration_;

  // --- One pump can have multiple dose slots per day ---
  struct ScheduleEntry {
    uint8_t hour;
    uint8_t minute;
    bool dosed_today;

    ScheduleEntry(uint8_t h, uint8_t m) : hour(h), minute(m), dosed_today(false) {}
  };
  std::vector<ScheduleEntry> schedules_;

  // --- Internal methods ---
  void dose_now();  // Actually dose based on ml + calibration
  void mark_dosed(ScheduleEntry &entry);  // Save state after dosing
  bool has_dosed_today(const ScheduleEntry &entry);  // Check preference store

  // --- Key generator for EEPROM safety ---
  uint32_t make_dose_key(const ScheduleEntry &entry);
};

// --- MicrodoserHub: calibration lifecycle handler ---
class MicrodoserHub : public Component {
 public:
  void set_calibration_entities(select::Select *sel,
                                number::Number *num,
                                button::Button *btn);
  void register_pump(const std::string &id, MicrodoserPump *pump);
  void start_calibration();
  void apply_calibration_result(float measured);

 protected:
  std::map<std::string, MicrodoserPump *> pumps_;
  select::Select *selector_{nullptr};
  number::Number *result_input_{nullptr};
  button::Button *button_{nullptr};
};

}  // namespace microdoser
}  // namespace esphome
