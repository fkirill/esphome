#include <unordered_set>
#include "esphome/core/component.h"
#include "components/stepper/stepper.h"
#include "components/binary_sensor/binary_sensor.h"
#include "components/lcd_pcf8574/pcf8574_display.h"

namespace esphome {
namespace curtains_controller {

enum CurtainState { Opening, Open, Closing, Closed };

struct GlobalState {
  uint32_t tulleOpenPosition;
  uint32_t tulleClosePosition;
  uint32_t curtainsOpenPosition;
  uint32_t curtainsClosePosition;
  CurtainState tulleState;
  CurtainState curtainsState;
  bool setupCompleted;
};

enum ButtonState { btnNone, btnTulleOpen, btnTulleClose, btnCurtainsOpen, btnCurtainsClose, btnSet, btnMode };

// no not assign manual values, we rely on the automatic value assignment by the compiler
enum StepId {
  stpInitial,  // decide wheater to go to the setup or to the Ready
  stpReady,
  stpOpeningClosing,
  stpAlreadyInMotion,
  stpUnsupported,
  stpModePressed,
  stpCalibrateTulleOpen,
  stpCalibrateTulleClosed,
  stpCalibrateCurtainsOpen,
  stpCalibrateCurtainsClosed,
  stpNone,      // Special task, equals to NULL
  MUST_BE_LAST  // DO NOT USE!
};

class TimeoutAndNextStep {
 public:
  TimeoutAndNextStep(uint32_t timeoutMillis, StepId &nextStep) : timeoutMillis(timeoutMillis), nextStep(nextStep) {}

  const uint32_t timeoutMillis;
  const StepId &nextStep;
};

class StepNode {
 public:
  StepNode(StepId stepId) : StepNode(stepId, "<Undefined for " + String(stepId) + ">") {}

  StepNode(StepId stepId, String displayText) : stepId(stepId), displayText(displayText) {}

  StepId getStepId() { return stepId; }

  String getDisplayText() { return displayText; }

  StepNode *onTulleOpen(StepId stepId) {
    tulleOpenStep = stepId;
    return this;
  }

  StepNode *onTulleClosed(StepId stepId) {
    tulleCloseStep = stepId;
    return this;
  }

  StepNode *onCurtainsOpen(StepId stepId) {
    curtainsOpenStep = stepId;
    return this;
  }

  StepNode *onCurtainsClosed(StepId stepId) {
    curtainsCloseStep = stepId;
    return this;
  }

  StepNode *onSet(StepId stepId) {
    setStep = stepId;
    return this;
  }

  StepNode *onMode(StepId stepId) {
    modeStep = stepId;
    return this;
  }

  StepNode *setCustomNextStep(std::function<StepId(GlobalState &, ButtonState, uint32_t)> customNextStep,
                              std::function<std::unordered_set<StepId>*()> customGetNextSteps) {
    this->customNextStep = customNextStep;
    this->customGetNextSteps = customGetNextSteps;
    return this;
  }

  StepNode *setCustomDisplayText(std::function<String(GlobalState &, ButtonState, uint32_t)> customDisplayText) {
    this->customDisplayText = customDisplayText;
    return this;
  }

  StepNode *setTimeout(uint32_t timeoutMillis, StepId nextStep) {
    timeoutStep = new TimeoutAndNextStep(timeoutMillis, nextStep);
    return this;
  }

  StepId nextStep(GlobalState &globalState, ButtonState buttonState, uint32_t millis_since_start) {
    if (customNextStep != NULL) {
      return customNextStep(globalState, buttonState, millis_since_start);
    }
    if (timeoutStep != NULL && timeoutStep->timeoutMillis <= millis_since_start) {
      return timeoutStep->nextStep;
    }
    switch (buttonState) {
      case btnTulleOpen:
        return tulleOpenStep;
      case btnTulleClose:
        return tulleCloseStep;
      case btnCurtainsOpen:
        return curtainsOpenStep;
      case btnCurtainsClose:
        return curtainsCloseStep;
      case btnSet:
        return setStep;
      case btnMode:
        return modeStep;
    }
    return stpNone;
  }

  String getDisplayText(GlobalState &globalState, ButtonState buttonState, uint32_t millis_since_start) {
    // if (customDisplayText.has_value()) {
    //   return customDisplayText.value()(globalState, buttonState, millis_since_start);
    // }
    return displayText;
  }

  void enter(GlobalState &globalState, ButtonState buttonState, uint32_t millis_since_start) {
    enterFn(globalState, buttonState, millis_since_start);
  }

  void exit(GlobalState &globalState, ButtonState buttonState, uint32_t millis_since_start) {
    exitFn(globalState, buttonState, millis_since_start);
  }

  std::unordered_set<StepId>* getAllOutboundSteps() {
    if (customGetNextSteps != NULL) {
      return customGetNextSteps();
    }
    auto res = new std::unordered_set<StepId>();
    if (tulleOpenStep != stpNone) {
      res += tulleOpenStep;
    }
    if (tulleCloseStep != stpNone) {
      res += tulleCloseStep;
    }
    if (curtainsOpenStep != stpNone) {
      res += curtainsOpenStep;
    }
    if (curtainsCloseStep != stpNone) {
      res += curtainsCloseStep;
    }
    if (setStep != stpNone) {
      res += setStep;
    }
    if (modeStep != stpNone) {
      res += modeStep;
    }
    if (timeoutStep != NULL) {
      res += timeoutStep->nextStep;
    }
    return res;
  }

 private:
  const StepId stepId;
  const String displayText;
  const std::function<void(GlobalState &, ButtonState, uint32_t)> enterFn;
  const std::function<void(GlobalState &, ButtonState, uint32_t)> exitFn;
  StepId tulleOpenStep = stpNone;
  StepId tulleCloseStep = stpNone;
  StepId curtainsOpenStep = stpNone;
  StepId curtainsCloseStep = stpNone;
  StepId setStep = stpNone;
  StepId modeStep = stpNone;
  std::function<String(GlobalState &, ButtonState, uint32_t)> customDisplayText = NULL;
  std::function<StepId(GlobalState &, ButtonState, uint32_t)> customNextStep = NULL;
  std::function<std::unordered_set<StepId>*()> customGetNextSteps = NULL;
  TimeoutAndNextStep *timeoutStep = NULL;
};

class CurtainsController : public Component {
 public:
  void setTulleMotor(stepper::Stepper &stepper) { this->tulleStepper = stepper; }
  void setCurtainsMotor(stepper::Stepper &stepper) { this->curtainsStepper = stepper; }
  void setTulleOpenSensor(binary_sensor::BinarySensor &button) { this->tulleOpenButton = button; }
  void setTulleCloseSensor(binary_sensor::BinarySensor &button) { this->tulleCloseButton = button; }
  void setCurtainsOpenSensor(binary_sensor::BinarySensor &button) { this->curtainsOpenButton = button; }
  void setCurtainsCloseSensor(binary_sensor::BinarySensor &button) { this->curtainsCloseButton = button; }
  void setSetSensor(binary_sensor::BinarySensor &button) { this->setButton = button; }
  void setModeSensor(binary_sensor::BinarySensor &button) { this->modeButton = button; }
  void seDisplay(lcd_pcf8574::PCF8574LCDDisplay &display) { this->display = display; }

  void setup();
  void loop();

 private:
  stepper::Stepper &tulleStepper;
  stepper::Stepper &curtainsStepper;
  binary_sensor::BinarySensor &tulleOpenButton;
  binary_sensor::BinarySensor &tulleCloseButton;
  binary_sensor::BinarySensor &curtainsOpenButton;
  binary_sensor::BinarySensor &curtainsCloseButton;
  binary_sensor::BinarySensor &setButton;
  binary_sensor::BinarySensor &modeButton;
  lcd_pcf8574::PCF8574LCDDisplay &display;
  ButtonState buttonState;
  StepId currentStep;
  std::vector<StepNode *> steps = std::vector<StepNode *>();
  GlobalState globalState;
  unsigned long lastStepStarted;

  void onTulleOpenButtonStateChanged(bool state) { buttonState = state ? btnTulleOpen : btnNone; }
  void onTulleCloseButtonStateChanged(bool state) { buttonState = state ? btnTulleClose : btnNone; }
  void onCurtainsOpenButtonStateChanged(bool state) { buttonState = state ? btnCurtainsOpen : btnNone; }
  void onCurtainsCloseButtonStateChanged(bool state) { buttonState = state ? btnCurtainsClose : btnNone; }
  void onSetButtonStateChanged(bool state) { buttonState = state ? btnSet : btnNone; }
  void onModeButtonStateChanged(bool state) { buttonState = state ? btnMode : btnNone; }
  void registerStep(StepNode *step);
  void restoreGlobalState();
  void backupGlobalState();
  void initSteps();
  StepNode *step(StepId stepId) { return steps[stepId]; }
  void printText(String text);
};
};  // namespace curtains_controller
};  // namespace esphome
