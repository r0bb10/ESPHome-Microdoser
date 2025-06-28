# Microdoser for ESPHome

**Microdoser** is a minimal, reliable, and expandable peristaltic dosing system for aquariums and automated liquid control — built as a native [ESPHome](https://esphome.io) external component.

It is designed to work **entirely offline**, without relying on Home Assistant or the cloud for scheduling or logic.
_Based on ideas and code from [Doser](https://github.com/daniele99/Doser), [Aquapi](https://github.com/TheRealFalseReality/aquapi), and [RTC_Scheduler](https://github.com/pebblebed-tech/RTC_Scheduler)._


## 💡 Features (Alpha Stage)

✅ Modular YAML configuration — add 1 or more pumps  
✅ Per-pump calibration via `ml/sec`  
✅ Daily dosing in multiple scheduled times per pump  
✅ Uses DS3231 RTC for reliable offline timekeeping  
✅ Dosing state is persisted in flash — survives reboots  
✅ Designed for PCF8574 I²C GPIO expander (clean and scalable)  
✅ Works with ESP32 3.3V logic  
✅ HA not required — safe even if HA crashes


## 📦 How It Works

- Each pump has:
  - Calibration rate (`ml/sec`)
  - Daily dose target (e.g. 60 ml/day)
  - One or more scheduled times
- At the scheduled time, Microdoser:
  - Checks RTC (DS3231 via `time: ds1307`)
  - Doses the correct amount using timing
  - Marks that slot as “dosed”
- On reboot, it checks flash to avoid repeating doses


## 🛠️ To Do

- [ ] Live calibration: run pump, enter volume, calculate `ml/sec`
- [ ] Store calibration permanently (no YAML editing)
- [ ] Manual override / dose-once buttons
- [ ] Optional sensors for HA integration (e.g. dose logs, last run)
- [ ] More flexible scheduling (every X hours, etc.)


## 🔧 Requirements

- ESP32 board
- DS3231 RTC module (I²C)
- PCF8574 I²C GPIO expander
- 12V peristaltic pumps + MOSFET board
- ESPHome ≥ 2023.12


## 🧪 Example YAML

```yaml
i2c:
  sda: GPIO17
  scl: GPIO16

time:
  - platform: ds1307
    id: rtc_time

external_components:
  - source:
      type: local
      path: components
    components: [microdoser]

microdoser:
  - id: calcium
    i2c_pin: 0
    calibration_ml_per_sec: 1.2
    daily_dose_ml: 60
    schedule:
      - hour: 8
        minute: 0
      - hour: 20
        minute: 0
