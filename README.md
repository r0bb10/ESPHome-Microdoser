# Microdoser for ESPHome

**Microdoser** is a minimal, reliable, and expandable peristaltic dosing system for aquariums and automated liquid control — built as a native [ESPHome](https://esphome.io) external component.

It is designed to work **entirely offline**, without relying on Home Assistant or the cloud for scheduling or logic.
_Based on ideas and code from [Digital-Pump](https://github.com/infotronic218/esphome-digital-pump), [Aquapi](https://github.com/TheRealFalseReality/aquapi), and [RTC_Scheduler](https://github.com/pebblebed-tech/RTC_Scheduler)._


## Features (Alpha Stage)

- Real-Time Clock (DS3231) based daily scheduling
- Per-pump dose scheduling with multiple time slots
- EEPROM tracking of completed doses
- Calibration system using ml/sec and persistent storage
- Shared calibration and prime control across pumps
- Optional safety watchdog for late/missed dose prevention
- Designed to run fully offline, even if HA is down
- YAML configuration with minimal surface and no HA dependency

## Safety and Watchdog

- Stable runtime behavior confirmed
- Persistent calibration and dose tracking
- Safe offline operation by design
- Reboots do not trigger repeat dosing
- Late missed doses are skipped if too old
- Set `watchdog: off` to allow recovering late doses
- Default `watchdog: strict` skips old missed doses safely
