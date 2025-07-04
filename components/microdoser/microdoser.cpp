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

  // --- Load last calibration timestamp (if available) ---
  uint32_t ts_key = fnv1_hash("cal_time_" + this->id_string_);
  this->pref_last_calibration_ = global_preferences->make_preference<uint32_t>(ts_key);
  this->pref_last_calibration_.load(&this->last_calibrated_epoch_);

  // --- Register periodic schedule checker ---
  this->set_interval(30000, [this]() { this->check_schedule(); });  // every 30 seconds
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

// --- Store Prime button reference ---
void MicrodoserHub::set_prime_button(button::Button *btn) {
  this->prime_button_ = btn;
}

// --- Run prime dose on selected pump from dropdown selector ---
void MicrodoserHub::start_prime() {
  if (!this->selector_)
    return;

  const std::string &target = this->selector_->state;
  auto it = this->pumps_.find(target);
  if (it == this->pumps_.end()) {
    ESP_LOGW(TAG, "Pump '%s' not found for priming", target.c_str());
    return;
  }

  ESP_LOGI(TAG, "Priming pump '%s'", target.c_str());
  it->second->prime();
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

  // --- Store timestamp of this calibration event ---
  if (this->time_ && this->time_->now().is_valid()) {
    this->last_calibrated_epoch_ = this->time_->now().timestamp;
    this->pref_last_calibration_.save(&this->last_calibrated_epoch_);
    ESP_LOGI(TAG, "Pump %u calibration timestamp saved: %u", this->index_, this->last_calibrated_epoch_);
  } else {
    ESP_LOGW(TAG, "Time invalid — could not store calibration timestamp.");
  }
}

// --- Run pump manually for 10 seconds (priming, no dosing state written) ---
void MicrodoserPump::prime() {
  if (!this->output_) {
    ESP_LOGW(TAG, "Prime failed: no output defined for pump %u", this->index_);
    return;
  }

  const int prime_ms = 10000;  // default 10s
  ESP_LOGI(TAG, "Priming pump %u for %d ms", this->index_, prime_ms);

  this->output_->turn_on();
  delay(prime_ms);
  this->output_->turn_off();

  ESP_LOGI(TAG, "Pump %u priming complete", this->index_);
}

// --- Add a scheduled dose time ---
void MicrodoserPump::add_schedule(uint8_t hour, uint8_t minute) {
  schedules_.push_back(ScheduleEntry{hour, minute});
}

// --- Scheduler logic: run this periodically instead of loop() ---
void MicrodoserPump::check_schedule() {
  auto now = this->time_->now();
  if (!now.is_valid())
    return;

  if (this->enable_switch_ && !this->enable_switch_->state) {
    ESP_LOGD(TAG, "Pump %u is disabled. Skipping schedule check.", this->index_);
    return;
  }

  for (auto &entry : this->schedules_) {
    bool already_dosed = this->has_dosed_today(entry);

    ESP_LOGD(TAG, "Pump %u checking %02d:%02d — already dosed: %s",
             this->index_, entry.hour, entry.minute,
             already_dosed ? "yes" : "no");

    if (already_dosed)
      continue;

    // --- On-time match ---
    if (entry.hour == now.hour && entry.minute == now.minute) {
      ESP_LOGI(TAG, "Scheduled dosing %f mL", dose_total_ml_);
      this->dose_now();
      this->mark_dosed(entry);
      continue;
    }

    // --- Watchdog disabled: behave as if feature does not exist ---
    if (this->max_late_minutes_ == 0)
      continue;

    // --- Watchdog recovery: late but still acceptable ---
    int sched_min = entry.hour * 60 + entry.minute;
    int curr_min = now.hour * 60 + now.minute;
    int delta = curr_min - sched_min;

    if (delta > 0 && delta <= this->max_late_minutes_) {
      ESP_LOGW(TAG, "Recovered late dose (%d min late)", delta);
      this->dose_now();
      this->mark_dosed(entry);
    } else if (delta > this->max_late_minutes_) {
      ESP_LOGW(TAG, "Missed dose too old, skipping %02d:%02d", entry.hour, entry.minute);
    }
  }
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
