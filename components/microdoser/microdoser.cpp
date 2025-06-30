#include "microdoser.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include "esphome/core/preferences.h"

namespace esphome {
namespace microdoser {

// --- LOGGING TAG ---
static const char *const TAG = "microdoser";

// --- Called once on startup ---
void MicrodoserPump::setup() {
  ESP_LOGI(TAG, "Setting up Microdoser pump %u.", this->index_);

  // --- EEPROM-safe calibration key using ID string ---
  uint32_t cal_key = fnv1_hash("cal_" + this->id_string_);
  this->pref_calibration_ = global_preferences->make_preference<float>(cal_key);

  float stored;
  if (this->pref_calibration_.load(&stored)) {
    ESP_LOGI(TAG, "Pump %u loaded stored calibration: %.3f ml/sec", this->index_, stored);
    this->calibration_ = stored;
  } else {
    this->pref_calibration_.save(&this->calibration_);
    ESP_LOGI(TAG, "Pump %u saved initial calibration: %.3f ml/sec", this->index_, this->calibration_);
  }
}

// --- Set selector, number input, and button references ---
void MicrodoserHub::set_calibration_entities(select::Select *sel, number::Number *num, button::Button *btn) {
  this->selector_ = sel;
  this->result_input_ = num;
  this->button_ = btn;
}

// --- Register pump with ID for future lookup ---
void MicrodoserHub::register_pump(const std::string &id, MicrodoserPump *pump) {
  this->pumps_[id] = pump;
}

// --- Trigger calibration on selected pump ---
void MicrodoserHub::start_calibration() {
  if (!this->selector_)
    return;

  const std::string &target = this->selector_->state;
  auto it = this->pumps_.find(target);
  if (it == this->pumps_.end()) {
    ESP_LOGW(TAG, "Pump '%s' not found for calibration", target.c_str());
    return;
  }

  ESP_LOGI(TAG, "Starting calibration on pump '%s'", target.c_str());
  it->second->run_calibration_dose();
}

// --- Apply measured result from user ---
void MicrodoserHub::apply_calibration_result(float measured) {
  if (!this->selector_)
    return;

  const std::string &target = this->selector_->state;
  auto it = this->pumps_.find(target);
  if (it == this->pumps_.end()) {
    ESP_LOGW(TAG, "Pump '%s' not found to apply calibration", target.c_str());
    return;
  }

  ESP_LOGI(TAG, "Applying %.2f mL calibration to pump '%s'", measured, target.c_str());
  it->second->update_calibration_from_result(measured);
}

// --- Save calibration persistently ---
void MicrodoserPump::store_calibration(float new_value) {
  this->calibration_ = new_value;
  this->pref_calibration_.save(&new_value);
  ESP_LOGI(TAG, "Pump %u new calibration: %.3f ml/sec", this->index_, new_value);
}

// --- Run test dose of fixed 10 mL ---
void MicrodoserPump::run_calibration_dose() {
  ESP_LOGI(TAG, "Pump %u running calibration: 10.0 mL at %.3f ml/sec", this->index_, this->calibration_);
  float seconds = 10.0 / this->calibration_;
  if (!this->output_) {
    ESP_LOGE(TAG, "Pump %u has no output assigned", this->index_);
    return;
  }
  this->output_->turn_on();
  delay(static_cast<uint32_t>(seconds * 1000));
  this->output_->turn_off();
  ESP_LOGI(TAG, "Calibration dose complete. Enter actual mL measured.");
}

// --- Apply measured mL ---
void MicrodoserPump::update_calibration_from_result(float actual_ml) {
  if (actual_ml <= 0.1f) {
    ESP_LOGW(TAG, "Invalid calibration result (%.2f mL). Ignoring.", actual_ml);
    return;
  }
  float new_cal = (10.0f / actual_ml) * this->calibration_;
  this->store_calibration(new_cal);
}

// --- Main loop: runs every cycle (~ms scale) ---
void MicrodoserPump::loop() {
  auto now = this->time_->now();
  if (!now.is_valid())
    return;

  for (auto &entry : schedules_) {
    if (entry.hour == now.hour && entry.minute == now.minute && !has_dosed_today(entry)) {
      ESP_LOGI(TAG, "Dosing %f mL", dose_total_ml_);
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
  if (!this->output_) {
    ESP_LOGE(TAG, "No output defined for MicrodoserPump!");
    return;
  }

  float ml_per_dose = dose_total_ml_ / schedules_.size();
  float seconds = ml_per_dose / calibration_;
  uint32_t duration = static_cast<uint32_t>(seconds * 1000);

  ESP_LOGI(TAG, "Activating pump for %u ms", duration);
  this->output_->turn_on();
  delay(duration);
  this->output_->turn_off();
  ESP_LOGI(TAG, "Pump off");
}

// --- Save record that pump was dosed at this time ---
void MicrodoserPump::mark_dosed(ScheduleEntry &entry) {
  uint32_t key = make_dose_key(entry);
  uint32_t value = 1;
  global_preferences->make_preference<uint32_t>(key).save(&value);
  entry.dosed_today = true;
}

// --- Check persistent storage if already dosed ---
bool MicrodoserPump::has_dosed_today(const ScheduleEntry &entry) {
  uint32_t key = make_dose_key(entry);
  uint32_t val = 0;
  auto pref = global_preferences->make_preference<uint32_t>(key);
  pref.load(&val);
  return val == 1;
}

// --- Generate string-based EEPROM-safe key ---
uint32_t MicrodoserPump::make_dose_key(const ScheduleEntry &entry) {
  auto now = this->time_->now();

  // Construct unique string to hash
  std::string composite = "dose_" + this->id_string_ +
                          "_" + to_string(entry.hour) +
                          "_" + to_string(entry.minute) +
                          "_" + to_string(now.day_of_year);

  return fnv1_hash(composite);
}

}  // namespace microdoser
}  // namespace esphome
