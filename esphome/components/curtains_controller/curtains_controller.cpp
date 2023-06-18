#include "curtains_controller.h"

namespace esphome {
namespace curtains_controller {

String curtainsStateToDisplayText(curtains_state::CurtainState state) {
  switch (state) {
    case curtains_state::Opening:
      return "is opening";
    case curtains_state::Open:
      return "is open";
    case curtains_state::Closing:
      return "is closing";
    case curtains_state::Closed:
      return "is closed";
  }
  return "unexpected";
}

StepNode *modePressedStep() {
  return (new StepNode(step_id::ModePressed, "Press SET within 3 seconds to enter calibration mode"))
      ->onSet(step_id::CalibrateTulleOpen)
      ->setTimeout(3000, step_id::Ready);
}

StepNode *openCloseStep(step_id::StepId stepId, stepper::Stepper *stepper, uint32_t targetPosition,
                        curtains_state::CurtainState currentState, curtains_state::CurtainState expectedState,
                        std::function<void(GlobalState &, curtains_state::CurtainState)> setStateFn,
                        curtains_state::CurtainState setState, step_id::StepId nextStepId,
                        step_id::StepId alreadyAtStateStepId, step_id::StepId alreadyInMotionStepId) {
  return (new StepNode(stepId))
      ->setCustomNextStep(
          [currentState, expectedState, nextStepId, alreadyInMotionStepId, alreadyAtStateStepId](
              GlobalState &globalState, button_state::ButtonState button_state, uint32_t millisSinceStart) {
            if (currentState == expectedState) {
              return nextStepId;
            }
            if (currentState == curtains_state::Opening || currentState == curtains_state::Closing) {
              return alreadyInMotionStepId;
            }
            return alreadyAtStateStepId;
          },
          [nextStepId, alreadyInMotionStepId, alreadyAtStateStepId]() {
            auto res = new std::unordered_set<step_id::StepId>();
            res += nextStepId;
            res += alreadyInMotionStepId;
            res += alreadyAtStateStepId;
            return res;
          })
      ->onEnter([currentState, expectedState, stepper, targetPosition, setStateFn, setState](
                    GlobalState &globalState, button_state::ButtonState button_state, uint32_t millisSinceStart) {
        if (currentState == expectedState) {
          stepper->set_target(targetPosition);
          setStateFn(globalState, setState);
        }
      });
}

StepNode *customRoutingOnlyStep(step_id::StepId stepId, customNextStepFn nextStepFn,
                                customGetNextStepsFn getNextStepsFn) {
  return (new StepNode(stepId))->setCustomNextStep(nextStepFn, getNextStepsFn);
}

StepNode *timeoutTextStep(step_id::StepId stepId, String displayText, uint32_t delayMillis,
                          step_id::StepId returnToStep) {
  return (new StepNode(stepId, displayText))->setTimeout(delayMillis, returnToStep);
}

StepNode *readyStep() {
  return (new StepNode(step_id::Ready, "Ready"))
      ->onTulleOpen(step_id::OpenTulle)
      ->onTulleClose(step_id::CloseTulle)
      ->onCurtainsOpen(step_id::OpenCurtains)
      ->onCurtainsClose(step_id::CloseCurtains)
      ->onMode(step_id::ModePressed)
      ->onSet(step_id::ReadySetUnsupported);
}

StepNode *openingClosingStep(stepper::Stepper *tulleStepper, stepper::Stepper *curtainsStepper) {
  return (new StepNode(step_id::OpeningClosing))
      ->onTulleOpen(step_id::OpenTulle)
      ->onTulleClose(step_id::CloseTulle)
      ->onCurtainsOpen(step_id::OpenCurtains)
      ->onCurtainsClose(step_id::CloseCurtains)
      ->setCustomNextStep(
          [tulleStepper, curtainsStepper](GlobalState &globalState, button_state::ButtonState buttonState,
                                          uint32_t millisSinceStart) {
            if (globalState.tulleState == curtains_state::Opening ||
                globalState.tulleState == curtains_state::Closing || tulleStepper->has_reached_target()) {
              if (globalState.tulleState == curtains_state::Opening) {
                globalState.tulleState = curtains_state::Open;
              }
              if (globalState.tulleState == curtains_state::Closing) {
                globalState.tulleState = curtains_state::Closed;
              }
            }
            if (globalState.curtainsState == curtains_state::Opening ||
                globalState.curtainsState == curtains_state::Closing || curtainsStepper->has_reached_target()) {
              if (globalState.curtainsState == curtains_state::Opening) {
                globalState.curtainsState = curtains_state::Open;
              }
              if (globalState.curtainsState == curtains_state::Closing) {
                globalState.curtainsState = curtains_state::Closed;
              }
            }
            bool allMotorsStopped = tulleStepper->has_reached_target() && curtainsStepper->has_reached_target();
            return allMotorsStopped ? step_id::Ready : step_id::None;
          },
          []() {
            auto res = new std::unordered_set<step_id::StepId>();
            res += step_id::OpenTulle;
            res += step_id::CloseTulle;
            res += step_id::OpenCurtains;
            res += step_id::CloseCurtains;
            res += step_id::Ready;
            return res;
          })
      ->setCustomDisplayText(
          [](GlobalState &globalState, button_state::ButtonState buttonState, uint32_t millisSinceStart) {
            return "Tulle " + curtainsStateToDisplayText(globalState.tulleState) + ", curtains " +
                   curtainsStateToDisplayText(globalState.curtainsState);
          });
}

std::vector<StepNode *> *adjustSteps(
    step_id::StepId mainStepId, String mainDisplayText, step_id::StepId tulleOpenStepId,
    step_id::StepId tulleCloseStepId, step_id::StepId curtainsOpenStepId, step_id::StepId curtainsCloseStepId,
    step_id::StepId nextStepId, step_id::StepId startAdjustOpenStepId, step_id::StepId startAdjustCloseStepId,
    step_id::StepId adjustOpenStepId, step_id::StepId adjustCloseStepId, String adjustOpenDisplayText,
    String adjustCloseDisplayText, stepper::Stepper *stepper, std::function<uint32_t(GlobalState &)> positionGetter,
    std::function<void(GlobalState &, uint32_t)> positionSetter, uint32_t stepsIncrement) {
  auto mainStep = (new StepNode(mainStepId, mainDisplayText))
                      ->onTulleOpen(tulleOpenStepId)
                      ->onTulleClose(tulleCloseStepId)
                      ->onCurtainsOpen(curtainsOpenStepId)
                      ->onCurtainsClose(curtainsCloseStepId)
                      ->onSet(nextStepId);
  auto startAdjustOpenStep =
      (new StepNode(startAdjustOpenStepId))
          ->onEnter([stepper, positionSetter, stepsIncrement](
                        GlobalState &globalState, button_state::ButtonState buttonState, uint32_t millisSinceStart) {
            stepper->report_position(stepsIncrement);
            stepper->set_target(0);
            positionSetter(globalState, 0);
          })
          ->setCustomNextStep([adjustOpenStepId](GlobalState &globalState, button_state::ButtonState buttonState,
                                                 uint32_t millisSinceStart) { return adjustOpenStepId; },
                              [adjustOpenStepId]() {
                                auto res = new std::unordered_set<step_id::StepId>();
                                res += adjustOpenStepId;
                                return res;
                              });
  auto startAdjustCloseStep =
      (new StepNode(startAdjustCloseStepId))
          ->onEnter([stepper, positionGetter, positionSetter, stepsIncrement](
                        GlobalState &globalState, button_state::ButtonState buttonState, uint32_t millisSinceStart) {
            uint32_t curPosition = positionGetter(globalState);
            stepper->set_target(curPosition + stepsIncrement);
            positionSetter(globalState, curPosition + stepsIncrement);
          })
          ->setCustomNextStep([adjustCloseStepId](GlobalState &globalState, button_state::ButtonState buttonState,
                                                  uint32_t millisSinceStart) { return adjustCloseStepId; },
                              [adjustCloseStepId]() {
                                auto res = new std::unordered_set<step_id::StepId>();
                                res += adjustCloseStepId;
                                return res;
                              });
  auto adjustOpenStep = (new StepNode(adjustOpenStepId, adjustOpenDisplayText))
                            ->setCustomNextStep(
                                [stepper, mainStepId](GlobalState &globalState, button_state::ButtonState buttonState,
                                                      uint32_t millisSInceStart) {
                                  return stepper->has_reached_target() ? mainStepId : step_id::None;
                                },
                                [mainStepId]() {
                                  auto res = new std::unordered_set<step_id::StepId>();
                                  res += mainStepId;
                                  return res;
                                });
  auto adjustCloseStep = (new StepNode(adjustCloseStepId, adjustCloseDisplayText))
                             ->setCustomNextStep(
                                 [stepper, mainStepId](GlobalState &globalState, button_state::ButtonState buttonState,
                                                       uint32_t millisSInceStart) {
                                   return stepper->has_reached_target() ? mainStepId : step_id::None;
                                 },
                                 [mainStepId]() {
                                   auto res = new std::unordered_set<step_id::StepId>();
                                   res += mainStepId;
                                   return res;
                                 });

  auto steps = new std::vector<StepNode *>(5);
  steps->assign(0, mainStep);
  steps->assign(1, startAdjustOpenStep);
  steps->assign(2, startAdjustCloseStep);
  steps->assign(3, adjustOpenStep);
  steps->assign(4, adjustCloseStep);
  return steps;
}

void CurtainsController::setup() {
  initSteps();
  buttonState = button_state::None;
  lastStepStarted = millis();
  currentStep = step_id::Initial;
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
  if (nextStep == step_id::None) {
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

void CurtainsController::initSteps() {
  registerStep(customRoutingOnlyStep(
      step_id::Initial,
      [](GlobalState &globalState, button_state::ButtonState button_state, uint32_t millisSinseStart) {
        return globalState.setupCompleted ? step_id::Ready : step_id::CalibrateTulleOpen;
      },
      []() {
        auto res = new std::unordered_set<step_id::StepId>();
        res += step_id::Ready;
        res += step_id::CalibrateTulleOpen;
        return res;
      }));
  registerStep(readyStep());
  std::function<void(GlobalState &, curtains_state::CurtainState)> setTulleState =
      [](GlobalState &globalState, curtains_state::CurtainState state) { globalState.tulleState = state; };
  std::function<void(GlobalState &, curtains_state::CurtainState)> setCurtainsState =
      [](GlobalState &globalState, curtains_state::CurtainState state) { globalState.curtainsState = state; };
  registerStep(openCloseStep(step_id::OpenTulle, tulleStepper, globalState.tulleOpenPosition, globalState.tulleState,
                             curtains_state::Closed, setTulleState, curtains_state::Opening, step_id::OpeningClosing,
                             step_id::TulleAlreadyOpen, step_id::AlreadyInMotion));
  registerStep(openCloseStep(step_id::CloseTulle, tulleStepper, globalState.tulleClosePosition, globalState.tulleState,
                             curtains_state::Open, setTulleState, curtains_state::Closing, step_id::OpeningClosing,
                             step_id::TulleAlreadyClosed, step_id::AlreadyInMotion));
  registerStep(openCloseStep(step_id::OpenCurtains, curtainsStepper, globalState.curtainsOpenPosition,
                             globalState.curtainsState, curtains_state::Closed, setCurtainsState,
                             curtains_state::Opening, step_id::OpeningClosing, step_id::CurtainsAlreadyOpen,
                             step_id::AlreadyInMotion));
  registerStep(openCloseStep(step_id::CloseCurtains, curtainsStepper, globalState.curtainsClosePosition,
                             globalState.curtainsState, curtains_state::Open, setCurtainsState, curtains_state::Closing,
                             step_id::OpeningClosing, step_id::CurtainsAlreadyClosed, step_id::AlreadyInMotion));
  registerStep(openingClosingStep(tulleStepper, curtainsStepper));
  registerStep(timeoutTextStep(step_id::AlreadyInMotion, "Tulle or Curtains are already in motion", 1000,
                               step_id::OpeningClosing));
  registerStep(timeoutTextStep(step_id::ReadySetUnsupported, "Press MODE before pressing SET", 1000, step_id::Ready));
  registerStep(modePressedStep());
  registerSteps(adjustSteps(
      step_id::CalibrateTulleOpen, "Use tulle open and close button to adjust tulle opening position",
      step_id::CalibrateTulleOpenStartAdjustOpen, step_id::CalibrateTulleOpenStartAdjustClose, step_id::None,
      step_id::None, step_id::CalibrateTulleClose, step_id::CalibrateTulleOpenStartAdjustOpen,
      step_id::CalibrateTulleOpenStartAdjustClose, step_id::CalibrateTulleOpenAdjustOpen,
      step_id::CalibrateTulleOpenAdjustClose, "Adjusting tulle open position, opening by 400 steps",
      "Adjusting tulle open position, closing by 400 steps", tulleStepper,
      [](GlobalState &globalState) { return globalState.tulleOpenPosition; },
      [](GlobalState &globalState, uint32_t position) { globalState.tulleOpenPosition = position; }, 400));
  registerSteps(adjustSteps(
      step_id::CalibrateTulleClose, "Use tulle open and close button to adjust tulle closing position",
      step_id::CalibrateTulleCloseStartAdjustOpen, step_id::CalibrateTulleCloseStartAdjustClose, step_id::None,
      step_id::None, step_id::CalibrateCurtainsOpen, step_id::CalibrateTulleCloseStartAdjustOpen,
      step_id::CalibrateTulleCloseStartAdjustClose, step_id::CalibrateTulleCloseAdjustOpen,
      step_id::CalibrateTulleCloseAdjustClose, "Adjusting tulle close position, opening by 400 steps",
      "Adjusting tulle close position, closing by 400 steps", tulleStepper,
      [](GlobalState &globalState) { return globalState.tulleClosePosition; },
      [](GlobalState &globalState, uint32_t position) { globalState.tulleClosePosition = position; }, 400));
  registerSteps(adjustSteps(
      step_id::CalibrateCurtainsOpen, "Use curtains open and close button to adjust curtains opening position",
      step_id::None, step_id::None, step_id::CalibrateCurtainsOpenStartAdjustOpen,
      step_id::CalibrateCurtainsOpenStartAdjustClose, step_id::CalibrateCurtainsClose,
      step_id::CalibrateCurtainsOpenStartAdjustOpen, step_id::CalibrateCurtainsOpenStartAdjustClose,
      step_id::CalibrateCurtainsOpenAdjustOpen, step_id::CalibrateCurtainsOpenAdjustClose,
      "Adjusting curtains open position, opening by 400 steps",
      "Adjusting curtains open position, closing by 400 steps", curtainsStepper,
      [](GlobalState &globalState) { return globalState.curtainsOpenPosition; },
      [](GlobalState &globalState, uint32_t position) { globalState.curtainsOpenPosition = position; }, 400));
  registerSteps(adjustSteps(
      step_id::CalibrateCurtainsClose, "Use curtains open and close button to adjust curtains closing position",
      step_id::None, step_id::None, step_id::CalibrateCurtainsCloseStartAdjustOpen,
      step_id::CalibrateCurtainsCloseStartAdjustClose, step_id::CalibrateCurtainsClose,
      step_id::CalibrateCurtainsCloseStartAdjustOpen, step_id::CalibrateCurtainsCloseStartAdjustClose,
      step_id::CalibrateCurtainsCloseAdjustOpen, step_id::CalibrateCurtainsCloseAdjustClose,
      "Adjusting curtains close position, opening by 400 steps",
      "Adjusting curtains close position, closing by 400 steps", curtainsStepper,
      [](GlobalState &globalState) { return globalState.curtainsClosePosition; },
      [](GlobalState &globalState, uint32_t position) { globalState.curtainsClosePosition = position; }, 400));
}

void CurtainsController::registerStep(StepNode *step) {
  if (steps.size() < step->getStepId() + 1) {
    steps.resize(step->getStepId() + 1);
  }
  steps[step->getStepId()] = step;
}

void CurtainsController::registerSteps(std::vector<StepNode *> *stepsToRegister) {
  for (int i = 0; i < stepsToRegister->size(); i++) {
    registerStep(stepsToRegister->at(i));
  }
}

};  // namespace curtains_controller
};  // namespace esphome