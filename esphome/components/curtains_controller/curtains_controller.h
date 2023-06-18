#include <unordered_set>
#include "esphome/core/component.h"
#include "components/stepper/stepper.h"
#include "components/binary_sensor/binary_sensor.h"
#include "components/lcd_pcf8574/pcf8574_display.h"

namespace esphome {
namespace curtains_controller {

namespace curtains_state {
enum CurtainState { Opening, Open, Closing, Closed };
};

struct GlobalState {
  uint32_t tulleOpenPosition;
  uint32_t tulleClosePosition;
  uint32_t curtainsOpenPosition;
  uint32_t curtainsClosePosition;
  curtains_state::CurtainState tulleState;
  curtains_state::CurtainState curtainsState;
  bool setupCompleted;
};

namespace button_state {
enum ButtonState { None, TulleOpen, TulleClose, CurtainsOpen, CurtainsClose, Set, Mode };
};

namespace step_id {
// no not assign manual values, we rely on the automatic value assignment by the compiler
enum StepId {
  Initial,  // decide wheater to go to the setup or to the Ready
  Ready,
  OpenTulle,
  CloseTulle,
  OpenCurtains,
  CloseCurtains,
  TulleAlreadyOpen,
  TulleAlreadyClosed,
  CurtainsAlreadyOpen,
  CurtainsAlreadyClosed,
  ReadySetUnsupported,
  OpeningClosing,
  AlreadyInMotion,
  ModePressed,
  CalibrateTulleOpen,
  CalibrateTulleOpenStartAdjustOpen,
  CalibrateTulleOpenAdjustOpen,
  CalibrateTulleOpenStartAdjustClose,
  CalibrateTulleOpenAdjustClose,
  CalibrateTulleClose,
  CalibrateTulleCloseStartAdjustOpen,
  CalibrateTulleCloseAdjustOpen,
  CalibrateTulleCloseStartAdjustClose,
  CalibrateTulleCloseAdjustClose,
  CalibrateCurtainsOpen,
  CalibrateCurtainsOpenStartAdjustOpen,
  CalibrateCurtainsOpenAdjustOpen,
  CalibrateCurtainsOpenStartAdjustClose,
  CalibrateCurtainsOpenAdjustClose,
  CalibrateCurtainsClose,
  CalibrateCurtainsCloseStartAdjustOpen,
  CalibrateCurtainsCloseAdjustOpen,
  CalibrateCurtainsCloseStartAdjustClose,
  CalibrateCurtainsCloseAdjustClose,
  None,  // Special value, means NULL/nothing/no change.
};

};  // namespace step_id

typedef std::function<step_id::StepId(GlobalState &, button_state::ButtonState, uint32_t)> customNextStepFn;
typedef std::function<std::unordered_set<step_id::StepId> *()> customGetNextStepsFn;
typedef std::function<String(GlobalState &, button_state::ButtonState, uint32_t)> customDislayTextFn;
typedef std::function<void(GlobalState &, button_state::ButtonState, uint32_t)> enterExitFn;

class TimeoutAndNextStep {
 public:
  TimeoutAndNextStep(uint32_t timeoutMillis, step_id::StepId &nextStep)
      : timeoutMillis(timeoutMillis), nextStep(nextStep) {}

  const uint32_t timeoutMillis;
  const step_id::StepId &nextStep;
};

class StepNode {
 public:
  StepNode(step_id::StepId stepId) : StepNode(stepId, "<Undefined for " + String(stepId) + ">") {}

  StepNode(step_id::StepId stepId, String displayText) : stepId(stepId), displayText(displayText) {}

  step_id::StepId getStepId() { return stepId; }

  String getDisplayText() { return displayText; }

  StepNode *onTulleOpen(step_id::StepId stepId) {
    tulleOpenStep = stepId;
    return this;
  }

  StepNode *onTulleClose(step_id::StepId stepId) {
    tulleCloseStep = stepId;
    return this;
  }

  StepNode *onCurtainsOpen(step_id::StepId stepId) {
    curtainsOpenStep = stepId;
    return this;
  }

  StepNode *onCurtainsClose(step_id::StepId stepId) {
    curtainsCloseStep = stepId;
    return this;
  }

  StepNode *onSet(step_id::StepId stepId) {
    setStep = stepId;
    return this;
  }

  StepNode *onMode(step_id::StepId stepId) {
    modeStep = stepId;
    return this;
  }

  StepNode *setCustomNextStep(customNextStepFn customNextStep, customGetNextStepsFn customGetNextSteps) {
    this->customNextStep = customNextStep;
    this->customGetNextSteps = customGetNextSteps;
    return this;
  }

  StepNode *setCustomDisplayText(customDislayTextFn customDisplayText) {
    this->customDisplayText = customDisplayText;
    return this;
  }

  StepNode *setTimeout(uint32_t timeoutMillis, step_id::StepId nextStep) {
    timeoutStep = new TimeoutAndNextStep(timeoutMillis, nextStep);
    return this;
  }

  step_id::StepId nextStep(GlobalState &globalState, button_state::ButtonState buttonState,
                           uint32_t millis_since_start) {
    if (customNextStep != NULL) {
      return customNextStep(globalState, buttonState, millis_since_start);
    }
    if (timeoutStep != NULL && timeoutStep->timeoutMillis <= millis_since_start) {
      return timeoutStep->nextStep;
    }
    switch (buttonState) {
      case button_state::TulleOpen:
        return tulleOpenStep;
      case button_state::TulleClose:
        return tulleCloseStep;
      case button_state::CurtainsOpen:
        return curtainsOpenStep;
      case button_state::CurtainsClose:
        return curtainsCloseStep;
      case button_state::Set:
        return setStep;
      case button_state::Mode:
        return modeStep;
    }
    return step_id::None;
  }

  String getDisplayText(GlobalState &globalState, button_state::ButtonState buttonState, uint32_t millis_since_start) {
    // if (customDisplayText.has_value()) {
    //   return customDisplayText.value()(globalState, buttonState, millis_since_start);
    // }
    return displayText;
  }

  void enter(GlobalState &globalState, button_state::ButtonState buttonState, uint32_t millis_since_start) {
    if (enterFn != NULL) {
      enterFn(globalState, buttonState, millis_since_start);
    }
  }

  void exit(GlobalState &globalState, button_state::ButtonState buttonState, uint32_t millis_since_start) {
    if (exitFn != NULL) {
      exitFn(globalState, buttonState, millis_since_start);
    }
  }

  std::unordered_set<step_id::StepId> *getAllOutboundSteps() {
    if (customGetNextSteps != NULL) {
      return customGetNextSteps();
    }
    auto res = new std::unordered_set<step_id::StepId>();
    if (tulleOpenStep != step_id::None) {
      res += tulleOpenStep;
    }
    if (tulleCloseStep != step_id::None) {
      res += tulleCloseStep;
    }
    if (curtainsOpenStep != step_id::None) {
      res += curtainsOpenStep;
    }
    if (curtainsCloseStep != step_id::None) {
      res += curtainsCloseStep;
    }
    if (setStep != step_id::None) {
      res += setStep;
    }
    if (modeStep != step_id::None) {
      res += modeStep;
    }
    if (timeoutStep != NULL) {
      res += timeoutStep->nextStep;
    }
    return res;
  }

  StepNode *onEnter(enterExitFn enterFn) {
    this->enterFn = enterFn;
    return this;
  }
  StepNode *onExit(enterExitFn exitFn) {
    this->exitFn = exitFn;
    return this;
  }

 private:
  const step_id::StepId stepId;
  const String displayText;
  enterExitFn enterFn = NULL;
  enterExitFn exitFn = NULL;
  step_id::StepId tulleOpenStep = step_id::None;
  step_id::StepId tulleCloseStep = step_id::None;
  step_id::StepId curtainsOpenStep = step_id::None;
  step_id::StepId curtainsCloseStep = step_id::None;
  step_id::StepId setStep = step_id::None;
  step_id::StepId modeStep = step_id::None;
  customDislayTextFn customDisplayText = NULL;
  customNextStepFn customNextStep = NULL;
  customGetNextStepsFn customGetNextSteps = NULL;
  TimeoutAndNextStep *timeoutStep = NULL;
};

class CurtainsController : public Component {
 public:
  void setTulleMotor(stepper::Stepper *stepper) { this->tulleStepper = stepper; }
  void setCurtainsMotor(stepper::Stepper *stepper) { this->curtainsStepper = stepper; }
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
  stepper::Stepper *tulleStepper;
  stepper::Stepper *curtainsStepper;
  binary_sensor::BinarySensor &tulleOpenButton;
  binary_sensor::BinarySensor &tulleCloseButton;
  binary_sensor::BinarySensor &curtainsOpenButton;
  binary_sensor::BinarySensor &curtainsCloseButton;
  binary_sensor::BinarySensor &setButton;
  binary_sensor::BinarySensor &modeButton;
  lcd_pcf8574::PCF8574LCDDisplay &display;
  button_state::ButtonState buttonState;
  step_id::StepId currentStep;
  std::vector<StepNode *> steps = std::vector<StepNode *>();
  GlobalState globalState;
  unsigned long lastStepStarted;

  void onTulleOpenButtonStateChanged(bool state) { buttonState = state ? button_state::TulleOpen : button_state::None; }
  void onTulleCloseButtonStateChanged(bool state) {
    buttonState = state ? button_state::TulleClose : button_state::None;
  }
  void onCurtainsOpenButtonStateChanged(bool state) {
    buttonState = state ? button_state::CurtainsOpen : button_state::None;
  }
  void onCurtainsCloseButtonStateChanged(bool state) {
    buttonState = state ? button_state::CurtainsClose : button_state::None;
  }
  void onSetButtonStateChanged(bool state) { buttonState = state ? button_state::Set : button_state::None; }
  void onModeButtonStateChanged(bool state) { buttonState = state ? button_state::Mode : button_state::None; }
  void registerStep(StepNode *step);
  void registerSteps(std::vector<StepNode *> *steps);
  void restoreGlobalState();
  void backupGlobalState();
  void initSteps();
  StepNode *step(step_id::StepId stepId) { return steps[stepId]; }
  void printText(String text);
};
};  // namespace curtains_controller
};  // namespace esphome
