#include "curtains_controller.h"

namespace esphome {
namespace curtains_controller {

void CurtainsController::setup() {
  initSteps();
  buttonState = btnNone;
  lastStepStarted = millis();
  currentStep = stpInitial;
  tulleOpenButton.add_on_state_callback([this](bool state) { this->onTulleOpenButtonStateChanged(state); });
  tulleCloseButton.add_on_state_callback([this](bool state) { this->onTulleCloseButtonStateChanged(state); });
  curtainsOpenButton.add_on_state_callback([this](bool state) { this->onCurtainsOpenButtonStateChanged(state); });
  curtainsCloseButton.add_on_state_callback([this](bool state) { this->onCurtainsCloseButtonStateChanged(state); });
  setButton.add_on_state_callback([this](bool state) { this->onSetButtonStateChanged(state); });
  modeButton.add_on_state_callback([this](bool state) { this->onModeButtonStateChanged(state); });
  restoreGlobalState();
}

void CurtainsController::loop() {
  unsigned long nowMillis = millis();
  unsigned long millisSinceStart = nowMillis - lastStepStarted;
  StepNode *currentStepNode = step(currentStep);
  auto nextStep = currentStepNode->nextStep(globalState, buttonState, millisSinceStart);
  if (nextStep == stpNone) {
    String text = currentStepNode->getDisplayText(globalState, buttonState, millisSinceStart);
    printText(text);
    return;
  }
  currentStepNode->exit(globalState, buttonState, millisSinceStart);
  currentStep = nextStep;
  currentStepNode = step(currentStep);
  lastStepStarted = nowMillis;
  currentStepNode->enter(globalState, buttonState, 0);
}

void setUpInitialStep(StepNode *step) {
  step->setCustomNextStep(
      [](GlobalState &globalState, ButtonState buttonState, uint32_t millisSInceStart) -> StepId {
        return globalState.setupCompleted ? stpReady : stpCalibrateTulleOpen;
      }, 
      []() -> std::unordered_set<StepId>* {
        auto res = new std::unordered_set<StepId>();
        res += stpReady;
        res += stpCalibrateTulleOpen;
        return res;
      });
}

void setUpReadyStep(StepNode *step) {}

void CurtainsController::initSteps() {
  registerStep(new StepNode(stpInitial));
  registerStep(new StepNode(stpReady, "Ready"));
  registerStep(new StepNode(stpOpeningClosing));
  registerStep(new StepNode(stpAlreadyInMotion, "Tulle or Curtains are already in motion"));
  registerStep(new StepNode(stpUnsupported, "Unsupported"));
  registerStep(new StepNode(stpModePressed, "Press SET withint 3 seconds to enter setup mode"));
  registerStep(new StepNode(stpCalibrateTulleOpen, "Use tulle open and close button to adjust tulle opening position"));
  registerStep(new StepNode(stpCalibrateTulleClosed, "Use tulle open and close button to adjust tulle closing position"));
  registerStep(
      new StepNode(stpCalibrateCurtainsOpen, "Use curtains open and close button to adjust curtains opening position"));
  registerStep(
      new StepNode(stpCalibrateCurtainsClosed, "Use curtains open and close button to adjust curtains closing position"));

  setUpInitialStep(step(stpInitial));
  //setupReadyStep(step(stpReady));
}

void CurtainsController::registerStep(StepNode *step) {
  if (steps.size() < step->getStepId() + 1) {
    steps.resize(step->getStepId() + 1);
  }
  steps[step->getStepId()] = step;
}

};  // namespace curtains_controller
};  // namespace esphome