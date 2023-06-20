from esphome.components import stepper, lcd_pcf8574, binary_sensor, output
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import CONF_ID

CODEOWNERS = ["@kfr"]
DEPENDENCIES = []


curtains_controller_ns = cg.esphome_ns.namespace("curtains_controller")
curtains_controller = curtains_controller_ns.class_("CurtainsController", output.FloatOutput, cg.Component)

CONFIG_SCHEMA = output.FLOAT_OUTPUT_SCHEMA.extend(
    {
        cv.Required(CONF_ID): cv.declare_id(curtains_controller),
        cv.Required("tulle_stepper"): cv.use_id(stepper.Stepper),
        cv.Required("curtains_stepper"): cv.use_id(stepper.Stepper),
        cv.Required("pcf8574_display"): cv.use_id(lcd_pcf8574.display.PCF8574LCDDisplay),
        cv.Required("open_tulle_sensor"): cv.use_id(binary_sensor.BinarySensor),
        cv.Required("close_tulle_sensor"): cv.use_id(binary_sensor.BinarySensor),
        cv.Required("open_curtains_sensor"): cv.use_id(binary_sensor.BinarySensor),
        cv.Required("close_curtains_sensor"): cv.use_id(binary_sensor.BinarySensor),
        cv.Required("set_sensor"): cv.use_id(binary_sensor.BinarySensor),
        cv.Required("mode_sensor"): cv.use_id(binary_sensor.BinarySensor),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await output.register_output(var, config)

    tulle_stepper = await cg.get_variable(config["tulle_stepper"])
    cg.add(var.setTulleMotor(tulle_stepper))
    curtains_stepper = await cg.get_variable(config["curtains_stepper"])
    cg.add(var.setCurtainsMotor(curtains_stepper))
    display = await cg.get_variable(config["pcf8574_display"])
    cg.add(var.setDisplay(display))
    tulle_open = await cg.get_variable(config["open_tulle_sensor"])
    cg.add(var.setTulleOpenSensor(tulle_open))
    close_tulle = await cg.get_variable(config["close_tulle_sensor"])
    cg.add(var.setTulleCloseSensor(close_tulle))
    open_curtains = await cg.get_variable(config["open_curtains_sensor"])
    cg.add(var.setCurtainsOpenSensor(open_curtains))
    close_curtains = await cg.get_variable(config["close_curtains_sensor"])
    cg.add(var.setCurtainsCloseSensor(close_curtains))
    set = await cg.get_variable(config["set_sensor"])
    cg.add(var.setSetSensor(set))
    mode = await cg.get_variable(config["mode_sensor"])
    cg.add(var.setModeSensor(mode))
