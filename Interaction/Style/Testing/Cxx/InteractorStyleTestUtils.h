// SPDX-FileCopyrightText: Copyright (c) Kitware Inc.
// SPDX-License-Identifier: BSD-3-Clause

#include "vtkCameraManipulator.h"
#include "vtkJoystickFly.h"
#include "vtkRenderWindowInteractor.h"

class vtkJoystickFlyObserver : public vtkCommand
{
  int FramesSinceLeftButtonDown = 0;
  int ReleaseLeftButtonAfterFrameCount = -1;

public:
  static vtkJoystickFlyObserver* New() { return new vtkJoystickFlyObserver(); }
  vtkJoystickFlyObserver() = default;
  ~vtkJoystickFlyObserver() override = default;
  vtkTypeMacro(vtkJoystickFlyObserver, vtkCommand);

  int GetFramesSinceLeftButtonDown() const { return this->FramesSinceLeftButtonDown; }
  void ReleaseLeftButtonAfter(int frameCount)
  {
    this->ReleaseLeftButtonAfterFrameCount = frameCount;
  }

  void Execute(vtkObject* vtkNotUsed(caller), unsigned long eventId, void* callData) override
  {
    if (eventId != vtkJoystickFly::FlyAnimationStepEvent)
    {
      return;
    }
    if (auto* interactor =
          vtkRenderWindowInteractor::SafeDownCast(static_cast<vtkObjectBase*>(callData)))
    {
      this->FramesSinceLeftButtonDown++;
      if (this->ReleaseLeftButtonAfterFrameCount > 0 &&
        this->FramesSinceLeftButtonDown >= this->ReleaseLeftButtonAfterFrameCount)
      {
        interactor->InvokeEvent(vtkCommand::LeftButtonReleaseEvent);
      }
      interactor->ProcessEvents();
    }
  }
};
