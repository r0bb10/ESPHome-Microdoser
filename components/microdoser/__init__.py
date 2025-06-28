# components/microdoser/__init__.py

# --- IMPORTS ---
from esphome import automation
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import time as time_, pcf8574
from esphome.const import CONF_ID

# --- CONSTANTS & CUSTOM FIELDS ---
CONF_SCHEDULE = "schedule"
CONF_I2C_PIN = "i2c_pin"
CONF_CALIBRATION = "calibration_ml_per_sec"
CONF_DAILY_DOSE = "daily_dose_ml"

# --- DECLARE NAMESPACE + CLASS ---
microdoser_ns = cg.esphome_ns.namespace("microdoser")
MicrodoserPump = microdoser_ns.class_("MicrodoserPump", cg.Component)

# --- SCHEDULE FORMAT (hour, minute) ---
SCHEDULE_SCHEMA = cv.Schema({
    cv.Required("hour"): cv.int_range(min=0, max=23),
    cv.Required("minute"): cv.int_range(min=0, max=59),
})

# --- YAML CONFIG VALIDATION ---
CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(MicrodoserPump),
    cv.Required(CONF_I2C_PIN): cv.int_,
    cv.Required(CONF_CALIBRATION): cv.positive_float,
    cv.Required(CONF_DAILY_DOSE): cv.positive_float,
    cv.Required(CONF_SCHEDULE): cv.ensure_list(SCHEDULE_SCHEMA),
}).extend(cv.COMPONENT_SCHEMA)

# --- BIND YAML CONFIG TO C++ CLASS ---
async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    cg.add(var.set_i2c_pin(config[CONF_I2C_PIN]))
    cg.add(var.set_calibration(config[CONF_CALIBRATION]))
    cg.add(var.set_daily_dose(config[CONF_DAILY_DOSE]))

    for sched in config[CONF_SCHEDULE]:
        cg.add(var.add_schedule(sched["hour"], sched["minute"]))

    time_var = await cg.get_variable("rtc_time")  # Must be declared in YAML
    cg.add(var.set_time_source(time_var))
