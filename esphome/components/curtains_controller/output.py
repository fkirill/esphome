from esphome.components import stepper, lcd_pcf8574, button, output
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
        cv.Required("open_tulle_button"): cv.use_id(button.Button),
        cv.Required("close_tulle_button"): cv.use_id(button.Button),
        cv.Required("open_curtains_button"): cv.use_id(button.Button),
        cv.Required("close_curtains_button"): cv.use_id(button.Button),
        cv.Required("set_button"): cv.use_id(button.Button),
        cv.Required("mode_button"): cv.use_id(button.Button),
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
    tulle_open = await cg.get_variable(config["open_tulle_button"])
    cg.add(var.setTulleOpenButton(tulle_open))
    close_tulle = await cg.get_variable(config["close_tulle_button"])
    cg.add(var.setTulleCloseButton(close_tulle))
    open_curtains = await cg.get_variable(config["open_curtains_button"])
    cg.add(var.setCurtainsOpenButton(open_curtains))
    close_curtains = await cg.get_variable(config["close_curtains_button"])
    cg.add(var.setCurtainsCloseButton(close_curtains))
    set = await cg.get_variable(config["set_button"])
    cg.add(var.setSetButton(set))
    mode = await cg.get_variable(config["mode_button"])
    cg.add(var.setModeButton(mode))
