from esphome import automation
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import time as time_
from esphome.components import button, number, select, switch, output
from esphome.const import CONF_ID, CONF_TIME_ID

CODEOWNERS = ["@r0bb10"]

DEPENDENCIES = ["time"]

# --- CONSTANTS ---
CONF_SCHEDULE = "schedule"
CONF_CALIBRATION = "calibration_ml_per_sec"
CONF_DAILY_DOSE = "daily_dose_ml"
CONF_PUMPS = "pumps"
CONF_PUMP_OUTPUT = "output_id"
CONF_MIN_DOSE = "min_ml_dose_each"
CONF_OFFSET_MIN = "dosing_offset_min"
CONF_TARGET_SELECT = "calibration_target"
CONF_RESULT_NUMBER = "calibration_result_ml"
CONF_CALIBRATE_BUTTON = "calibrate_pump_btn"
CONF_ENABLE_SWITCH = "enable_switch_id"
CONF_PRIME_BUTTON = "prime_pump_btn"
CONF_WATCHDOG_MODE = "watchdog"

# --- DECLARE NAMESPACE + CLASS ---
microdoser_ns = cg.esphome_ns.namespace("microdoser")
MicrodoserPump = microdoser_ns.class_("MicrodoserPump", cg.Component)
MicrodoserHub = microdoser_ns.class_("MicrodoserHub", cg.Component)

# --- SCHEDULE FORMAT ---
SCHEDULE_SCHEMA = cv.Schema({
    cv.Required("hour"): cv.int_range(min=0, max=23),
    cv.Required("minute"): cv.int_range(min=0, max=59),
})

# --- YAML CONFIG VALIDATION ---
CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(MicrodoserHub),
    cv.Required(CONF_TIME_ID): cv.use_id(time_.RealTimeClock),
    cv.Required(CONF_CALIBRATE_BUTTON): cv.use_id(button.Button),
    cv.Required(CONF_RESULT_NUMBER): cv.use_id(number.Number),
    cv.Required(CONF_TARGET_SELECT): cv.use_id(select.Select),
    cv.Required(CONF_PRIME_BUTTON): cv.use_id(button.Button),
    cv.Optional(CONF_OFFSET_MIN, default=5): cv.positive_int,
    cv.Optional(CONF_WATCHDOG_MODE, default="strict"): cv.one_of("strict", "recover", "off", lower=True),
    cv.Required(CONF_PUMPS): cv.ensure_list(
        cv.Schema({
            cv.GenerateID(): cv.declare_id(MicrodoserPump),
            cv.Required(CONF_PUMP_OUTPUT): cv.use_id(output.BinaryOutput),
            cv.Required(CONF_CALIBRATION): cv.positive_float,
            cv.Required(CONF_DAILY_DOSE): cv.positive_float,
            cv.Required(CONF_SCHEDULE): cv.Any(
                cv.ensure_list(SCHEDULE_SCHEMA),
                cv.one_of("auto", lower=True)
            ),
            cv.Optional(CONF_MIN_DOSE): cv.positive_float,
            cv.Optional(CONF_ENABLE_SWITCH): cv.use_id(switch.Switch),
        })
    )
}).extend(cv.COMPONENT_SCHEMA)

# --- BIND YAML CONFIG TO C++ CLASS ---
async def to_code(config):
    time_var = await cg.get_variable(config[CONF_TIME_ID])

    # --- Create shared hub instance ---
    hub = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(hub, config)

    cal_btn = await cg.get_variable(config[CONF_CALIBRATE_BUTTON])
    cal_num = await cg.get_variable(config[CONF_RESULT_NUMBER])
    cal_sel = await cg.get_variable(config[CONF_TARGET_SELECT])
    prime_btn = await cg.get_variable(config[CONF_PRIME_BUTTON])

    cg.add(hub.set_calibration_entities(cal_sel, cal_num, cal_btn))
    cg.add(hub.set_prime_button(prime_btn))

    offset_min = config[CONF_OFFSET_MIN]
    auto_index = 0

    watchdog_mode = config[CONF_WATCHDOG_MODE]
    if watchdog_mode == "strict":
        grace_minutes = 0
    elif watchdog_mode == "recover":
        grace_minutes = 10
    elif watchdog_mode == "off":
        grace_minutes = 1440  # 24 hours
    else:
        grace_minutes = 0  # fallback safety

    cg.add(hub.set_watchdog_minutes(grace_minutes))

    for pump_conf in config[CONF_PUMPS]:
        var = cg.new_Pvariable(pump_conf[CONF_ID])
        await cg.register_component(var, pump_conf)
        cg.add(var.set_time_source(time_var))
        cg.add(var.set_watchdog_grace(grace_minutes))

        output_pin = await cg.get_variable(pump_conf[CONF_PUMP_OUTPUT])
        cg.add(var.set_output_pin(output_pin))
        cg.add(var.set_calibration(pump_conf[CONF_CALIBRATION]))
        cg.add(var.set_daily_dose(pump_conf[CONF_DAILY_DOSE]))

        # --- Optional per-pump enable switch ---
        if CONF_ENABLE_SWITCH in pump_conf:
            sw = await cg.get_variable(pump_conf[CONF_ENABLE_SWITCH])
            cg.add(var.set_enable_switch(sw))

        # --- Register pump into hub with its string ID ---
        cg.add(hub.register_pump(str(pump_conf[CONF_ID].id), var))

        sched = pump_conf[CONF_SCHEDULE]
        if sched == "auto":
            total_ml = pump_conf[CONF_DAILY_DOSE]
            min_dose = pump_conf.get(CONF_MIN_DOSE, 10.0)
            num_slots = max(1, int(total_ml / min_dose))

            for i in range(num_slots):
                interval = 24 * 60 // num_slots
                total_min = i * interval + auto_index * offset_min
                hour = (total_min // 60) % 24
                minute = total_min % 60
                cg.add(var.add_schedule(hour, minute))

            auto_index += 1
        else:
            for sched_entry in sched:
                cg.add(var.add_schedule(sched_entry["hour"], sched_entry["minute"]))