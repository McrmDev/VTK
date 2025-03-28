// SPDX-FileCopyrightText: Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
// SPDX-License-Identifier: BSD-3-Clause
#include "vtkHandleRepresentation.h"
#include "vtkCoordinate.h"
#include "vtkInteractorObserver.h"
#include "vtkMath.h"
#include "vtkObjectFactory.h"
#include "vtkPointPlacer.h"
#include "vtkRenderWindow.h"
#include "vtkRenderer.h"

#include <cassert>

VTK_ABI_NAMESPACE_BEGIN
vtkCxxSetObjectMacro(vtkHandleRepresentation, PointPlacer, vtkPointPlacer);

//------------------------------------------------------------------------------
vtkHandleRepresentation::vtkHandleRepresentation()
{
  // Positions are maintained via a vtkCoordinate
  this->DisplayPosition->SetCoordinateSystemToDisplay();
  this->WorldPosition->SetCoordinateSystemToWorld();

  this->InteractionState = vtkHandleRepresentation::Outside;
  this->PointPlacer = vtkPointPlacer::New();

  this->DisplayPositionTime.Modified();
  this->WorldPositionTime.Modified();
}

//------------------------------------------------------------------------------
vtkHandleRepresentation::~vtkHandleRepresentation()
{
  this->SetPointPlacer(nullptr);
}

//------------------------------------------------------------------------------
void vtkHandleRepresentation::SetDisplayPosition(double displyPos[2])
{
  if (this->Renderer && this->PointPlacer)
  {
    if (this->PointPlacer->ValidateDisplayPosition(this->Renderer, displyPos))
    {
      double worldPos[3], worldOrient[9];
      if (this->PointPlacer->ComputeWorldPosition(this->Renderer, displyPos, worldPos, worldOrient))
      {
        this->DisplayPosition->SetValue(displyPos);
        this->WorldPosition->SetValue(worldPos);
        this->DisplayPositionTime.Modified();
      }
    }
  }
  else
  {
    this->DisplayPosition->SetValue(displyPos);
    this->DisplayPositionTime.Modified();
  }
}

//------------------------------------------------------------------------------
void vtkHandleRepresentation::GetDisplayPosition(double pos[2])
{
  // The position is really represented in the world position; the display
  // position is a convenience to go back and forth between coordinate systems.
  // Also note that the window size may have changed, so it's important to
  // update the display position.
  if (this->Renderer &&
    (this->WorldPositionTime > this->DisplayPositionTime ||
      (this->Renderer->GetVTKWindow() &&
        this->Renderer->GetVTKWindow()->GetMTime() > this->BuildTime)))
  {
    int* p = this->WorldPosition->GetComputedDisplayValue(this->Renderer);
    this->DisplayPosition->SetValue(p[0], p[1], 0);
  }
  this->DisplayPosition->GetValue(pos);
}

//------------------------------------------------------------------------------
double* vtkHandleRepresentation::GetDisplayPosition()
{
  // The position is really represented in the world position; the display
  // position is a convenience to go back and forth between coordinate systems.
  // Also note that the window size may have changed, so it's important to
  // update the display position.
  if (this->Renderer &&
    (this->WorldPositionTime > this->DisplayPositionTime ||
      (this->Renderer->GetVTKWindow() &&
        this->Renderer->GetVTKWindow()->GetMTime() > this->BuildTime)))
  {
    int* p = this->WorldPosition->GetComputedDisplayValue(this->Renderer);
    this->DisplayPosition->SetValue(p[0], p[1], 0);
  }
  return this->DisplayPosition->GetValue();
}

//------------------------------------------------------------------------------
void vtkHandleRepresentation::SetWorldPosition(double pos[3])
{
  if (this->Renderer && this->PointPlacer)
  {
    if (this->PointPlacer->ValidateWorldPosition(pos))
    {
      this->WorldPosition->SetValue(pos);
      this->WorldPositionTime.Modified();
    }
  }
  else
  {
    this->WorldPosition->SetValue(pos);
    this->WorldPositionTime.Modified();
  }
}

//------------------------------------------------------------------------------
void vtkHandleRepresentation::GetWorldPosition(double pos[3]) VTK_FUTURE_CONST
{
  this->WorldPosition->GetValue(pos);
}

//------------------------------------------------------------------------------
double* vtkHandleRepresentation::GetWorldPosition()
{
  return this->WorldPosition->GetValue();
}

//------------------------------------------------------------------------------
int vtkHandleRepresentation::CheckConstraint(
  vtkRenderer* vtkNotUsed(renderer), double vtkNotUsed(pos)[2])
{
  return 1;
}

//------------------------------------------------------------------------------
void vtkHandleRepresentation::SetRenderer(vtkRenderer* ren)
{
  this->DisplayPosition->SetViewport(ren);
  this->WorldPosition->SetViewport(ren);
  this->Superclass::SetRenderer(ren);

  // Okay this is weird. If a display position was set previously before
  // the renderer was specified, then the coordinate systems are not
  // synchronized.
  if (this->DisplayPositionTime > this->WorldPositionTime)
  {
    double p[3];
    this->DisplayPosition->GetValue(p);
    this->SetDisplayPosition(p); // side affect updated world pos
  }
}

//------------------------------------------------------------------------------
void vtkHandleRepresentation::GetTranslationVector(
  const double* p1, const double* p2, double* v) const
{
  double p12[3];
  vtkMath::Subtract(p2, p1, p12);
  if (this->TranslationAxis == Axis::NONE)
  {
    for (int i = 0; i < 3; ++i)
    {
      v[i] = p12[i];
    }
  }
  else if (this->TranslationAxis == Axis::Custom)
  {
    vtkMath::ProjectVector(p12, this->CustomTranslationAxis, v);
  }
  else
  {
    for (int i = 0; i < 3; ++i)
    {
      if (this->TranslationAxis == i)
      {
        v[i] = p12[i];
      }
      else
      {
        v[i] = 0.0;
      }
    }
  }
}

//------------------------------------------------------------------------------
void vtkHandleRepresentation::Translate(const double* p1, const double* p2)
{
  double v[3];
  this->GetTranslationVector(p1, p2, v);
  this->Translate(v);
}

//------------------------------------------------------------------------------
void vtkHandleRepresentation::Translate(const double* v)
{
  if (this->TranslationAxis == Axis::NONE)
  {
    for (int i = 0; i < 3; ++i)
    {
      this->WorldPosition->GetValue()[i] += v[i];
    }
  }
  else if (this->TranslationAxis == Axis::Custom)
  {
    double dir[3];
    vtkMath::ProjectVector(v, this->CustomTranslationAxis, dir);
    for (int i = 0; i < 3; ++i)
    {
      this->WorldPosition->GetValue()[i] += dir[i];
    }
  }
  else
  {
    assert(this->TranslationAxis > -1 && this->TranslationAxis < 3 &&
      "this->TranslationAxis out of bounds");
    this->WorldPosition->GetValue()[this->TranslationAxis] += v[this->TranslationAxis];
  }
}

//------------------------------------------------------------------------------
void vtkHandleRepresentation::DeepCopy(vtkProp* prop)
{
  vtkHandleRepresentation* rep = vtkHandleRepresentation::SafeDownCast(prop);
  if (rep)
  {
    this->SetTolerance(rep->GetTolerance());
    this->SetActiveRepresentation(rep->GetActiveRepresentation());
    this->SetConstrained(rep->GetConstrained());
    this->SetPointPlacer(rep->GetPointPlacer());
  }
  this->Superclass::ShallowCopy(prop);
}

//------------------------------------------------------------------------------
void vtkHandleRepresentation::ShallowCopy(vtkProp* prop)
{
  vtkHandleRepresentation* rep = vtkHandleRepresentation::SafeDownCast(prop);
  if (rep)
  {
    this->SetTolerance(rep->GetTolerance());
    this->SetActiveRepresentation(rep->GetActiveRepresentation());
    this->SetConstrained(rep->GetConstrained());
  }
  this->Superclass::ShallowCopy(prop);
}

//------------------------------------------------------------------------------
vtkMTimeType vtkHandleRepresentation::GetMTime()
{
  vtkMTimeType mTime = this->Superclass::GetMTime();
  vtkMTimeType wMTime = this->WorldPosition->GetMTime();
  mTime = (wMTime > mTime ? wMTime : mTime);
  vtkMTimeType dMTime = this->DisplayPosition->GetMTime();
  mTime = (dMTime > mTime ? dMTime : mTime);

  return mTime;
}

//------------------------------------------------------------------------------
void vtkHandleRepresentation::PrintSelf(ostream& os, vtkIndent indent)
{
  // Superclass typedef defined in vtkTypeMacro() found in vtkSetGet.h
  this->Superclass::PrintSelf(os, indent);

  double p[3];
  this->GetDisplayPosition(p);
  os << indent << "Display Position: (" << p[0] << ", " << p[1] << ", " << p[2] << ")\n";

  this->GetWorldPosition(p);
  os << indent << "World Position: (" << p[0] << ", " << p[1] << ", " << p[2] << ")\n";

  os << indent << "Constrained: " << (this->Constrained ? "On" : "Off") << "\n";

  os << indent << "Tolerance: " << this->Tolerance << "\n";

  os << indent << "Active Representation: " << (this->ActiveRepresentation ? "On" : "Off") << "\n";

  if (this->PointPlacer)
  {
    os << indent << "PointPlacer:\n";
    this->PointPlacer->PrintSelf(os, indent.GetNextIndent());
  }
  else
  {
    os << indent << "PointPlacer: (none)\n";
  }

  // this->InteractionState is printed in superclass
  // this is commented to avoid PrintSelf errors
}
VTK_ABI_NAMESPACE_END
