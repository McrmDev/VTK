// SPDX-FileCopyrightText: Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
// SPDX-License-Identifier: BSD-3-Clause
#include "vtkImplicitCylinderRepresentation.h"

#include "vtkActor.h"
#include "vtkActorCollection.h"
#include "vtkAssemblyNode.h"
#include "vtkAssemblyPath.h"
#include "vtkBox.h"
#include "vtkCamera.h"
#include "vtkCellArray.h"
#include "vtkCellPicker.h"
#include "vtkCollectionRange.h"
#include "vtkConeSource.h"
#include "vtkCylinder.h"
#include "vtkDoubleArray.h"
#include "vtkFeatureEdges.h"
#include "vtkImageData.h"
#include "vtkInteractorObserver.h"
#include "vtkLineSource.h"
#include "vtkMath.h"
#include "vtkObjectFactory.h"
#include "vtkPickingManager.h"
#include "vtkPlane.h"
#include "vtkPointData.h"
#include "vtkPoints.h"
#include "vtkPolyData.h"
#include "vtkPolyDataMapper.h"
#include "vtkProperty.h"
#include "vtkRenderWindow.h"
#include "vtkRenderWindowInteractor.h"
#include "vtkRenderer.h"
#include "vtkSphereSource.h"
#include "vtkTransform.h"
#include "vtkTubeFilter.h"
#include "vtkWindow.h"

#include <algorithm>
#include <cfloat> //for FLT_EPSILON

VTK_ABI_NAMESPACE_BEGIN
vtkStandardNewMacro(vtkImplicitCylinderRepresentation);

//------------------------------------------------------------------------------
vtkImplicitCylinderRepresentation::vtkImplicitCylinderRepresentation()
{
  this->AlongXAxis = 0;
  this->AlongYAxis = 0;
  this->AlongZAxis = 0;

  // Handle size is in pixels for this widget
  this->HandleSize = 5.0;

  // Pushing operation
  this->BumpDistance = 0.01;

  // Build the representation of the widget
  //
  this->Cylinder = vtkCylinder::New();
  this->Cylinder->SetAxis(0, 0, 1);
  this->Cylinder->SetCenter(0, 0, 0);
  this->Cylinder->SetRadius(0.5);

  this->MinRadius = 0.01;
  this->MaxRadius = 1.00;

  this->Resolution = 128;
  this->ScaleEnabled = 1;

  this->Cyl = vtkPolyData::New();
  vtkPoints* pts = vtkPoints::New();
  pts->SetDataTypeToDouble();
  this->Cyl->SetPoints(pts);
  pts->Delete();
  vtkCellArray* polys = vtkCellArray::New();
  this->Cyl->SetPolys(polys);
  polys->Delete();
  vtkDoubleArray* normals = vtkDoubleArray::New();
  normals->SetNumberOfComponents(3);
  this->Cyl->GetPointData()->SetNormals(normals);
  normals->Delete();
  this->CylMapper = vtkPolyDataMapper::New();
  this->CylMapper->SetInputData(this->Cyl);
  this->CylActor = vtkActor::New();
  this->CylActor->SetMapper(this->CylMapper);
  this->DrawCylinder = 1;

  this->Edges = vtkFeatureEdges::New();
  this->Edges->SetInputData(this->Cyl);
  this->EdgesTuber = vtkTubeFilter::New();
  this->EdgesTuber->SetInputConnection(this->Edges->GetOutputPort());
  this->EdgesTuber->SetNumberOfSides(12);
  this->EdgesMapper = vtkPolyDataMapper::New();
  this->EdgesMapper->SetInputConnection(this->EdgesTuber->GetOutputPort());
  this->EdgesActor = vtkActor::New();
  this->EdgesActor->SetMapper(this->EdgesMapper);
  this->Tubing = 1; // control whether tubing is on
  // The feature edges or tuber turns on scalar viz - we need it off.
  this->EdgesMapper->ScalarVisibilityOff();

  // Create the + cylinder axis
  this->LineSource = vtkLineSource::New();
  this->LineSource->SetResolution(1);
  this->LineMapper = vtkPolyDataMapper::New();
  this->LineMapper->SetInputConnection(this->LineSource->GetOutputPort());
  this->LineActor = vtkActor::New();
  this->LineActor->SetMapper(this->LineMapper);

  this->ConeSource = vtkConeSource::New();
  this->ConeSource->SetResolution(12);
  this->ConeSource->SetAngle(25.0);
  this->ConeMapper = vtkPolyDataMapper::New();
  this->ConeMapper->SetInputConnection(this->ConeSource->GetOutputPort());
  this->ConeActor = vtkActor::New();
  this->ConeActor->SetMapper(this->ConeMapper);

  // Create the - cylinder axis
  this->LineSource2 = vtkLineSource::New();
  this->LineSource2->SetResolution(1);
  this->LineMapper2 = vtkPolyDataMapper::New();
  this->LineMapper2->SetInputConnection(this->LineSource2->GetOutputPort());
  this->LineActor2 = vtkActor::New();
  this->LineActor2->SetMapper(this->LineMapper2);

  this->ConeSource2 = vtkConeSource::New();
  this->ConeSource2->SetResolution(12);
  this->ConeSource2->SetAngle(25.0);
  this->ConeMapper2 = vtkPolyDataMapper::New();
  this->ConeMapper2->SetInputConnection(this->ConeSource2->GetOutputPort());
  this->ConeActor2 = vtkActor::New();
  this->ConeActor2->SetMapper(this->ConeMapper2);

  // Create the center handle
  this->Sphere = vtkSphereSource::New();
  this->Sphere->SetThetaResolution(16);
  this->Sphere->SetPhiResolution(8);
  this->SphereMapper = vtkPolyDataMapper::New();
  this->SphereMapper->SetInputConnection(this->Sphere->GetOutputPort());
  this->SphereActor = vtkActor::New();
  this->SphereActor->SetMapper(this->SphereMapper);

  this->Transform = vtkTransform::New();

  // Define the point coordinates
  double bounds[6];
  bounds[0] = -0.5;
  bounds[1] = 0.5;
  bounds[2] = -0.5;
  bounds[3] = 0.5;
  bounds[4] = -0.5;
  bounds[5] = 0.5;

  // Initial creation of the widget, serves to initialize it
  this->PlaceWidget(bounds);

  // Manage the picking stuff
  this->Picker = vtkCellPicker::New();
  this->Picker->SetTolerance(0.005);
  this->Picker->AddPickList(this->LineActor);
  this->Picker->AddPickList(this->ConeActor);
  this->Picker->AddPickList(this->LineActor2);
  this->Picker->AddPickList(this->ConeActor2);
  this->Picker->AddPickList(this->SphereActor);
  this->Picker->AddPickList(this->GetOutlineActor());
  this->Picker->PickFromListOn();

  this->CylPicker = vtkCellPicker::New();
  this->CylPicker->SetTolerance(0.005);
  this->CylPicker->AddPickList(this->CylActor);
  this->CylPicker->AddPickList(this->EdgesActor);
  this->CylPicker->PickFromListOn();

  // Set up the initial properties
  this->CreateDefaultProperties();

  // Pass the initial properties to the actors.
  this->LineActor->SetProperty(this->AxisProperty);
  this->ConeActor->SetProperty(this->AxisProperty);
  this->LineActor2->SetProperty(this->AxisProperty);
  this->ConeActor2->SetProperty(this->AxisProperty);
  this->SphereActor->SetProperty(this->AxisProperty);
  this->CylActor->SetProperty(this->CylinderProperty);
  this->EdgesActor->SetProperty(this->EdgesProperty);

  // The bounding box
  this->BoundingBox = vtkBox::New();

  this->RepresentationState = vtkImplicitCylinderRepresentation::Outside;
}

//------------------------------------------------------------------------------
vtkImplicitCylinderRepresentation::~vtkImplicitCylinderRepresentation()
{
  this->Cylinder->Delete();

  this->Cyl->Delete();
  this->CylMapper->Delete();
  this->CylActor->Delete();

  this->Edges->Delete();
  this->EdgesTuber->Delete();
  this->EdgesMapper->Delete();
  this->EdgesActor->Delete();

  this->LineSource->Delete();
  this->LineMapper->Delete();
  this->LineActor->Delete();

  this->ConeSource->Delete();
  this->ConeMapper->Delete();
  this->ConeActor->Delete();

  this->LineSource2->Delete();
  this->LineMapper2->Delete();
  this->LineActor2->Delete();

  this->ConeSource2->Delete();
  this->ConeMapper2->Delete();
  this->ConeActor2->Delete();

  this->Sphere->Delete();
  this->SphereMapper->Delete();
  this->SphereActor->Delete();

  this->Transform->Delete();

  this->Picker->Delete();
  this->CylPicker->Delete();

  this->AxisProperty->Delete();
  this->SelectedAxisProperty->Delete();
  this->CylinderProperty->Delete();
  this->SelectedCylinderProperty->Delete();
  this->EdgesProperty->Delete();
  this->BoundingBox->Delete();
}

//------------------------------------------------------------------------------
int vtkImplicitCylinderRepresentation::ComputeInteractionState(int X, int Y, int vtkNotUsed(modify))
{
  // See if anything has been selected
  vtkAssemblyPath* path = this->GetAssemblyPath(X, Y, 0., this->Picker);

  // The second picker may need to be called. This is done because the cylinder
  // wraps around things that can be picked; thus the cylinder is the selection
  // of last resort.
  if (path == nullptr)
  {
    this->CylPicker->Pick(X, Y, 0., this->Renderer);
    path = this->CylPicker->GetPath();
  }

  if (path == nullptr) // Nothing picked
  {
    this->SetRepresentationState(vtkImplicitCylinderRepresentation::Outside);
    this->InteractionState = vtkImplicitCylinderRepresentation::Outside;
    return this->InteractionState;
  }

  // Something picked, continue
  this->ValidPick = 1;

  // Depending on the interaction state (set by the widget) we modify
  // this state based on what is picked.
  if (this->InteractionState == vtkImplicitCylinderRepresentation::Moving)
  {
    vtkProp* prop = path->GetFirstNode()->GetViewProp();
    if (prop == this->ConeActor || prop == this->LineActor || prop == this->ConeActor2 ||
      prop == this->LineActor2)
    {
      this->InteractionState = vtkImplicitCylinderRepresentation::RotatingAxis;
      this->SetRepresentationState(vtkImplicitCylinderRepresentation::RotatingAxis);
    }
    else if (prop == this->CylActor || prop == EdgesActor)
    {
      this->InteractionState = vtkImplicitCylinderRepresentation::AdjustingRadius;
      this->SetRepresentationState(vtkImplicitCylinderRepresentation::AdjustingRadius);
    }
    else if (prop == this->SphereActor)
    {
      this->InteractionState = vtkImplicitCylinderRepresentation::MovingCenter;
      this->SetRepresentationState(vtkImplicitCylinderRepresentation::MovingCenter);
    }
    else
    {
      if (this->GetOutlineTranslation())
      {
        this->InteractionState = vtkImplicitCylinderRepresentation::MovingOutline;
        this->SetRepresentationState(vtkImplicitCylinderRepresentation::MovingOutline);
      }
      else
      {
        this->InteractionState = vtkImplicitCylinderRepresentation::Outside;
        this->SetRepresentationState(vtkImplicitCylinderRepresentation::Outside);
      }
    }
  }

  // We may add a condition to allow the camera to work IO scaling
  else if (this->InteractionState != vtkImplicitCylinderRepresentation::Scaling)
  {
    this->InteractionState = vtkImplicitCylinderRepresentation::Outside;
  }

  return this->InteractionState;
}

//------------------------------------------------------------------------------
void vtkImplicitCylinderRepresentation::SetRepresentationState(int state)
{
  // Clamp the state
  state = std::min<int>(std::max<int>(state, vtkImplicitCylinderRepresentation::Outside),
    vtkImplicitCylinderRepresentation::Scaling);

  if (this->RepresentationState == state)
  {
    return;
  }

  this->RepresentationState = state;
  this->Modified();

  this->HighlightNormal(0);
  this->HighlightCylinder(0);
  this->HighlightOutline(0);
  if (state == vtkImplicitCylinderRepresentation::RotatingAxis)
  {
    this->HighlightNormal(1);
    this->HighlightCylinder(1);
  }
  else if (state == vtkImplicitCylinderRepresentation::AdjustingRadius)
  {
    this->HighlightCylinder(1);
  }
  else if (state == vtkImplicitCylinderRepresentation::MovingCenter)
  {
    this->HighlightNormal(1);
  }
  else if (state == vtkImplicitCylinderRepresentation::MovingOutline)
  {
    this->HighlightOutline(1);
  }
  else if (state == vtkImplicitCylinderRepresentation::Scaling && this->ScaleEnabled)
  {
    this->HighlightNormal(1);
    this->HighlightCylinder(1);
    this->HighlightOutline(1);
  }
  else if (state == vtkImplicitCylinderRepresentation::TranslatingCenter)
  {
    this->HighlightNormal(1);
  }
}

//------------------------------------------------------------------------------
void vtkImplicitCylinderRepresentation::StartWidgetInteraction(double e[2])
{
  this->StartEventPosition[0] = e[0];
  this->StartEventPosition[1] = e[1];
  this->StartEventPosition[2] = 0.0;

  this->LastEventPosition[0] = e[0];
  this->LastEventPosition[1] = e[1];
  this->LastEventPosition[2] = 0.0;
}

//------------------------------------------------------------------------------
void vtkImplicitCylinderRepresentation::WidgetInteraction(double e[2])
{
  vtkCamera* camera = this->Renderer->GetActiveCamera();
  if (!camera)
  {
    return;
  }

  vtkVector3d prevPickPoint = this->GetWorldPoint(this->Picker, this->LastEventPosition);
  vtkVector3d pickPoint = this->GetWorldPoint(this->Picker, e);
  vtkVector3d cylinderPickPoint = this->GetWorldPoint(this->CylPicker, e);

  // Process the motion
  if (this->InteractionState == vtkImplicitCylinderRepresentation::MovingOutline)
  {
    this->TranslateOutline(prevPickPoint.GetData(), pickPoint.GetData());
  }
  else if (this->InteractionState == vtkImplicitCylinderRepresentation::MovingCenter)
  {
    this->TranslateCenter(prevPickPoint.GetData(), pickPoint.GetData());
  }
  else if (this->InteractionState == vtkImplicitCylinderRepresentation::TranslatingCenter)
  {
    this->TranslateCenterOnAxis(prevPickPoint.GetData(), pickPoint.GetData());
  }
  else if (this->InteractionState == vtkImplicitCylinderRepresentation::AdjustingRadius)
  {
    double dummy[3];
    this->AdjustRadius(e[0], e[1], dummy, cylinderPickPoint.GetData());
  }
  else if (this->InteractionState == vtkImplicitCylinderRepresentation::Scaling &&
    this->ScaleEnabled)
  {
    this->Scale(prevPickPoint.GetData(), pickPoint.GetData(), e[0], e[1]);
  }
  else if (this->InteractionState == vtkImplicitCylinderRepresentation::RotatingAxis)
  {
    double vpn[3];
    camera->GetViewPlaneNormal(vpn);
    this->Rotate(e[0], e[1], prevPickPoint.GetData(), pickPoint.GetData(), vpn);
  }

  this->LastEventPosition[0] = e[0];
  this->LastEventPosition[1] = e[1];
  this->LastEventPosition[2] = 0.0;
}

//------------------------------------------------------------------------------
void vtkImplicitCylinderRepresentation::EndWidgetInteraction(double vtkNotUsed(e)[2])
{
  this->SetRepresentationState(vtkImplicitCylinderRepresentation::Outside);
}

//------------------------------------------------------------------------------
double* vtkImplicitCylinderRepresentation::GetBounds()
{
  this->BuildRepresentation();

  vtkNew<vtkActorCollection> actors;
  this->GetActors(actors);
  this->BoundingBox->SetBounds(actors->GetLastActor()->GetBounds());
  for (auto actor : vtk::Range(actors.Get()))
  {
    this->BoundingBox->AddBounds(actor->GetBounds());
  }

  return this->BoundingBox->GetBounds();
}

//------------------------------------------------------------------------------
void vtkImplicitCylinderRepresentation::GetActors(vtkPropCollection* pc)
{
  this->GetOutlineActor()->GetActors(pc);
  this->EdgesActor->GetActors(pc);
  this->ConeActor->GetActors(pc);
  this->LineActor->GetActors(pc);
  this->ConeActor2->GetActors(pc);
  this->LineActor2->GetActors(pc);
  this->SphereActor->GetActors(pc);

  if (this->DrawCylinder)
  {
    this->CylActor->GetActors(pc);
  }
}

//------------------------------------------------------------------------------
void vtkImplicitCylinderRepresentation::ReleaseGraphicsResources(vtkWindow* w)
{
  vtkNew<vtkActorCollection> actors;
  this->GetActors(actors);
  for (auto actor : vtk::Range(actors.Get()))
  {
    actor->ReleaseGraphicsResources(w);
  }
}

//------------------------------------------------------------------------------
int vtkImplicitCylinderRepresentation::RenderOpaqueGeometry(vtkViewport* v)
{
  int count = 0;
  this->BuildRepresentation();

  vtkNew<vtkActorCollection> actors;
  this->GetActors(actors);
  for (auto actor : vtk::Range(actors.Get()))
  {
    count += actor->RenderOpaqueGeometry(v);
  }

  return count;
}

//------------------------------------------------------------------------------
int vtkImplicitCylinderRepresentation::RenderTranslucentPolygonalGeometry(vtkViewport* v)
{
  int count = 0;
  this->BuildRepresentation();

  vtkNew<vtkActorCollection> actors;
  this->GetActors(actors);
  for (auto actor : vtk::Range(actors.Get()))
  {
    count += actor->RenderTranslucentPolygonalGeometry(v);
  }

  return count;
}

//------------------------------------------------------------------------------
vtkTypeBool vtkImplicitCylinderRepresentation::HasTranslucentPolygonalGeometry()
{
  int result = 0;
  vtkNew<vtkActorCollection> actors;
  this->GetActors(actors);
  for (auto actor : vtk::Range(actors.Get()))
  {
    result |= actor->HasTranslucentPolygonalGeometry();
  }

  return result;
}

//------------------------------------------------------------------------------
void vtkImplicitCylinderRepresentation::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);

  os << indent << "Min Radius: " << this->MinRadius << "\n";
  os << indent << "Max Radius: " << this->MaxRadius << "\n";

  os << indent << "Resolution: " << this->Resolution << "\n";

  if (this->AxisProperty)
  {
    os << indent << "Axis Property: " << this->AxisProperty << "\n";
  }
  else
  {
    os << indent << "Axis Property: (none)\n";
  }
  if (this->SelectedAxisProperty)
  {
    os << indent << "Selected Axis Property: " << this->SelectedAxisProperty << "\n";
  }
  else
  {
    os << indent << "Selected Axis Property: (none)\n";
  }

  if (this->CylinderProperty)
  {
    os << indent << "Cylinder Property: " << this->CylinderProperty << "\n";
  }
  else
  {
    os << indent << "Cylinder Property: (none)\n";
  }
  if (this->SelectedCylinderProperty)
  {
    os << indent << "Selected Cylinder Property: " << this->SelectedCylinderProperty << "\n";
  }
  else
  {
    os << indent << "Selected Cylinder Property: (none)\n";
  }

  if (this->EdgesProperty)
  {
    os << indent << "Edges Property: " << this->EdgesProperty << "\n";
  }
  else
  {
    os << indent << "Edges Property: (none)\n";
  }

  os << indent << "Along X Axis: " << (this->AlongXAxis ? "On" : "Off") << "\n";
  os << indent << "Along Y Axis: " << (this->AlongYAxis ? "On" : "Off") << "\n";
  os << indent << "ALong Z Axis: " << (this->AlongZAxis ? "On" : "Off") << "\n";

  os << indent << "Tubing: " << (this->Tubing ? "On" : "Off") << "\n";
  os << indent << "Scale Enabled: " << (this->ScaleEnabled ? "On" : "Off") << "\n";
  os << indent << "Draw Cylinder: " << (this->DrawCylinder ? "On" : "Off") << "\n";
  os << indent << "Bump Distance: " << this->BumpDistance << "\n";

  os << indent << "Representation State: ";
  switch (this->RepresentationState)
  {
    case Outside:
      os << "Outside\n";
      break;
    case Moving:
      os << "Moving\n";
      break;
    case MovingOutline:
      os << "MovingOutline\n";
      break;
    case MovingCenter:
      os << "MovingCenter\n";
      break;
    case RotatingAxis:
      os << "RotatingAxis\n";
      break;
    case AdjustingRadius:
      os << "AdjustingRadius\n";
      break;
    case Scaling:
      os << "Scaling\n";
      break;
    case TranslatingCenter:
      os << "TranslatingCenter\n";
      break;
  }

  // this->InteractionState is printed in superclass
  // this is commented to avoid PrintSelf errors
}

//------------------------------------------------------------------------------
void vtkImplicitCylinderRepresentation::HighlightNormal(int highlight)
{
  if (highlight)
  {
    this->LineActor->SetProperty(this->SelectedAxisProperty);
    this->ConeActor->SetProperty(this->SelectedAxisProperty);
    this->LineActor2->SetProperty(this->SelectedAxisProperty);
    this->ConeActor2->SetProperty(this->SelectedAxisProperty);
    this->SphereActor->SetProperty(this->SelectedAxisProperty);
  }
  else
  {
    this->LineActor->SetProperty(this->AxisProperty);
    this->ConeActor->SetProperty(this->AxisProperty);
    this->LineActor2->SetProperty(this->AxisProperty);
    this->ConeActor2->SetProperty(this->AxisProperty);
    this->SphereActor->SetProperty(this->AxisProperty);
  }
}

//------------------------------------------------------------------------------
void vtkImplicitCylinderRepresentation::HighlightCylinder(int highlight)
{
  if (highlight)
  {
    this->CylActor->SetProperty(this->SelectedCylinderProperty);
    this->EdgesActor->SetProperty(this->SelectedCylinderProperty);
  }
  else
  {
    this->CylActor->SetProperty(this->CylinderProperty);
    this->EdgesActor->SetProperty(this->EdgesProperty);
  }
}

//------------------------------------------------------------------------------
void vtkImplicitCylinderRepresentation::Rotate(
  double X, double Y, double* p1, double* p2, double* vpn)
{
  double v[3];    // vector of motion
  double axis[3]; // axis of rotation
  double theta;   // rotation angle

  // mouse motion vector in world space
  v[0] = p2[0] - p1[0];
  v[1] = p2[1] - p1[1];
  v[2] = p2[2] - p1[2];

  double* center = this->Cylinder->GetCenter();
  double* cylAxis = this->Cylinder->GetAxis();

  // Create axis of rotation and angle of rotation
  vtkMath::Cross(vpn, v, axis);
  if (vtkMath::Normalize(axis) == 0.0)
  {
    return;
  }
  const int* size = this->Renderer->GetSize();
  double l2 = (X - this->LastEventPosition[0]) * (X - this->LastEventPosition[0]) +
    (Y - this->LastEventPosition[1]) * (Y - this->LastEventPosition[1]);
  theta = 360.0 * sqrt(l2 / (size[0] * size[0] + size[1] * size[1]));

  // Manipulate the transform to reflect the rotation
  this->Transform->Identity();
  this->Transform->Translate(center[0], center[1], center[2]);
  this->Transform->RotateWXYZ(theta, axis);
  this->Transform->Translate(-center[0], -center[1], -center[2]);

  // Set the new normal
  double aNew[3];
  this->Transform->TransformNormal(cylAxis, aNew);
  this->SetAxis(aNew);
}

//------------------------------------------------------------------------------
// Loop through all points and translate them
void vtkImplicitCylinderRepresentation::TranslateRepresentation(const vtkVector3d& move)
{
  double oNew[3];
  // Translate the cylinder
  double* origin = this->Cylinder->GetCenter();
  oNew[0] = origin[0] + move[0];
  oNew[1] = origin[1] + move[1];
  oNew[2] = origin[2] + move[2];
  this->Cylinder->SetCenter(oNew);
}

//------------------------------------------------------------------------------
// Loop through all points and translate them
void vtkImplicitCylinderRepresentation::TranslateCenter(double* p1, double* p2)
{
  // Get the motion vector
  double v[3] = { 0, 0, 0 };

  if (!this->IsTranslationConstrained())
  {
    v[0] = p2[0] - p1[0];
    v[1] = p2[1] - p1[1];
    v[2] = p2[2] - p1[2];
  }
  else
  {
    v[this->GetTranslationAxis()] = p2[this->GetTranslationAxis()] - p1[this->GetTranslationAxis()];
  }

  // Add to the current point, project back down onto plane
  double* c = this->Cylinder->GetCenter();
  double* a = this->Cylinder->GetAxis();
  double newCenter[3];

  newCenter[0] = c[0] + v[0];
  newCenter[1] = c[1] + v[1];
  newCenter[2] = c[2] + v[2];

  vtkPlane::ProjectPoint(newCenter, c, a, newCenter);
  this->SetCenter(newCenter[0], newCenter[1], newCenter[2]);
  this->BuildRepresentation();
}

//------------------------------------------------------------------------------
// Translate the center on the axis
void vtkImplicitCylinderRepresentation::TranslateCenterOnAxis(double* p1, double* p2)
{
  // Get the motion vector
  double v[3];
  v[0] = p2[0] - p1[0];
  v[1] = p2[1] - p1[1];
  v[2] = p2[2] - p1[2];

  // Add to the current point, project back down onto plane
  double* c = this->Cylinder->GetCenter();
  double* a = this->Cylinder->GetAxis();
  double newCenter[3];

  newCenter[0] = c[0] + v[0];
  newCenter[1] = c[1] + v[1];
  newCenter[2] = c[2] + v[2];

  // Normalize the axis vector
  const double imag = 1. / std::max(1.0e-100, sqrt(a[0] * a[0] + a[1] * a[1] + a[2] * a[2]));
  double an[3];
  an[0] = a[0] * imag;
  an[1] = a[1] * imag;
  an[2] = a[2] * imag;

  // Project the point on the axis vector
  double u[3];
  u[0] = newCenter[0] - c[0];
  u[1] = newCenter[1] - c[1];
  u[2] = newCenter[2] - c[2];
  double dot = an[0] * u[0] + an[1] * u[1] + an[2] * u[2];
  newCenter[0] = c[0] + an[0] * dot;
  newCenter[1] = c[1] + an[1] * dot;
  newCenter[2] = c[2] + an[2] * dot;

  this->SetCenter(newCenter[0], newCenter[1], newCenter[2]);
  this->BuildRepresentation();
}

//------------------------------------------------------------------------------
void vtkImplicitCylinderRepresentation::Scale(
  double* p1, double* p2, double vtkNotUsed(X), double Y)
{
  // Get the motion vector
  double v[3];
  v[0] = p2[0] - p1[0];
  v[1] = p2[1] - p1[1];
  v[2] = p2[2] - p1[2];

  double* o = this->Cylinder->GetCenter();

  // Compute the scale factor
  double sf = vtkMath::Norm(v) / this->GetDiagonalLength();
  if (Y > this->LastEventPosition[1])
  {
    sf = 1.0 + sf;
  }
  else
  {
    sf = 1.0 - sf;
  }

  this->Transform->Identity();
  this->Transform->Translate(o[0], o[1], o[2]);
  this->Transform->Scale(sf, sf, sf);
  this->Transform->Translate(-o[0], -o[1], -o[2]);

  this->TransformBounds(this->Transform);

  this->BuildRepresentation();
}

//------------------------------------------------------------------------------
void vtkImplicitCylinderRepresentation::AdjustRadius(
  double X, double Y, double* vtkNotUsed(p1), double* point)
{
  if (X == this->LastEventPosition[0] && Y == this->LastEventPosition[1])
  {
    return;
  }

  double origin[3];
  this->Cylinder->GetCenter(origin);

  double axis[3];
  this->Cylinder->GetAxis(axis);

  double centerToPoint[3];
  centerToPoint[0] = point[0] - origin[0];
  centerToPoint[1] = point[1] - origin[1];
  centerToPoint[2] = point[2] - origin[2];

  double crossed[3];
  vtkMath::Cross(axis, centerToPoint, crossed);
  this->SetRadius(vtkMath::Norm(crossed));

  this->BuildRepresentation();
}

//------------------------------------------------------------------------------
void vtkImplicitCylinderRepresentation::CreateDefaultProperties()
{
  // Cylinder properties
  this->CylinderProperty = vtkProperty::New();
  this->CylinderProperty->SetAmbient(1.0);
  this->CylinderProperty->SetAmbientColor(1.0, 1.0, 1.0);
  this->CylinderProperty->SetOpacity(0.5);
  this->CylActor->SetProperty(this->CylinderProperty);

  this->SelectedCylinderProperty = vtkProperty::New();
  this->SelectedCylinderProperty->SetAmbient(1.0);
  this->SelectedCylinderProperty->SetAmbientColor(0.0, 1.0, 0.0);
  this->SelectedCylinderProperty->SetOpacity(0.25);

  // Cylinder axis properties
  this->AxisProperty = vtkProperty::New();
  this->AxisProperty->SetColor(1, 1, 1);
  this->AxisProperty->SetLineWidth(2);

  this->SelectedAxisProperty = vtkProperty::New();
  this->SelectedAxisProperty->SetColor(1, 0, 0);
  this->SelectedAxisProperty->SetLineWidth(2);

  // Edge property
  this->EdgesProperty = vtkProperty::New();
  // don't want for 3D tubes: this->EdgesProperty->SetAmbient(1.0);
  this->EdgesProperty->SetColor(1.0, 0.0, 0.0);

  this->Superclass::CreateDefaultProperties();
}

//------------------------------------------------------------------------------
void vtkImplicitCylinderRepresentation::SetInteractionColor(double r, double g, double b)
{
  this->SelectedAxisProperty->SetColor(r, g, b);
  this->SelectedCylinderProperty->SetAmbientColor(r, g, b);
  this->SetSelectedOutlineColor(r, g, b);
}

//------------------------------------------------------------------------------
void vtkImplicitCylinderRepresentation::SetHandleColor(double r, double g, double b)
{
  this->AxisProperty->SetColor(r, g, b);
  this->EdgesProperty->SetColor(r, g, b);
}

//------------------------------------------------------------------------------
void vtkImplicitCylinderRepresentation::SetForegroundColor(double r, double g, double b)
{
  this->CylinderProperty->SetAmbientColor(r, g, b);
  this->SetOutlineColor(r, g, b);
}

//------------------------------------------------------------------------------
void vtkImplicitCylinderRepresentation::PlaceWidget(double bds[6])
{
  int i;
  double bounds[6], origin[3];

  this->AdjustBounds(bds, bounds, origin);
  this->SetOutlineBounds(bds);

  this->LineSource->SetPoint1(this->Cylinder->GetCenter());
  if (this->AlongYAxis)
  {
    this->Cylinder->SetAxis(0, 1, 0);
    this->LineSource->SetPoint2(0, 1, 0);
  }
  else if (this->AlongZAxis)
  {
    this->Cylinder->SetAxis(0, 0, 1);
    this->LineSource->SetPoint2(0, 0, 1);
  }
  else // default or x-normal
  {
    this->Cylinder->SetAxis(1, 0, 0);
    this->LineSource->SetPoint2(1, 0, 0);
  }

  for (i = 0; i < 6; i++)
  {
    this->InitialBounds[i] = bounds[i];
  }
  this->SetWidgetBounds(bounds);

  this->InitialLength = sqrt((bounds[1] - bounds[0]) * (bounds[1] - bounds[0]) +
    (bounds[3] - bounds[2]) * (bounds[3] - bounds[2]) +
    (bounds[5] - bounds[4]) * (bounds[5] - bounds[4]));

  this->ValidPick = 1; // since we have positioned the widget successfully
  this->BuildRepresentation();
}

//------------------------------------------------------------------------------
// Set the center of the cylinder.
void vtkImplicitCylinderRepresentation::SetCenter(double x, double y, double z)
{
  double center[3];
  center[0] = x;
  center[1] = y;
  center[2] = z;
  this->SetCenter(center);
}

//------------------------------------------------------------------------------
// Set the center of the cylinder. Note that the center is clamped slightly inside
// the bounding box or the cylinder tends to disappear as it hits the boundary.
void vtkImplicitCylinderRepresentation::SetCenter(double x[3])
{
  this->Cylinder->SetCenter(x);
  this->BuildRepresentation();
}

//------------------------------------------------------------------------------
// Get the center of the cylinder.
double* vtkImplicitCylinderRepresentation::GetCenter()
{
  return this->Cylinder->GetCenter();
}

//------------------------------------------------------------------------------
void vtkImplicitCylinderRepresentation::GetCenter(double xyz[3])
{
  this->Cylinder->GetCenter(xyz);
}

//------------------------------------------------------------------------------
// Set the axis of the cylinder.
void vtkImplicitCylinderRepresentation::SetAxis(double x, double y, double z)
{
  double n[3], n2[3];
  n[0] = x;
  n[1] = y;
  n[2] = z;
  vtkMath::Normalize(n);

  this->Cylinder->GetAxis(n2);
  if (n[0] != n2[0] || n[1] != n2[1] || n[2] != n2[2])
  {
    this->Cylinder->SetAxis(n);
    this->Modified();
  }
}

//------------------------------------------------------------------------------
// Set the axis the cylinder.
void vtkImplicitCylinderRepresentation::SetAxis(double n[3])
{
  this->SetAxis(n[0], n[1], n[2]);
}

//------------------------------------------------------------------------------
// Get the axis of the cylinder.
double* vtkImplicitCylinderRepresentation::GetAxis()
{
  return this->Cylinder->GetAxis();
}

//------------------------------------------------------------------------------
void vtkImplicitCylinderRepresentation::GetAxis(double xyz[3])
{
  this->Cylinder->GetAxis(xyz);
}

//------------------------------------------------------------------------------
// Set the radius the cylinder. The radius must be a positive number.
void vtkImplicitCylinderRepresentation::SetRadius(double radius)
{
  if (this->GetConstrainToWidgetBounds())
  {
    double minRadius = this->GetDiagonalLength() * this->MinRadius;
    double maxRadius = this->GetDiagonalLength() * this->MaxRadius;

    radius = std::min(maxRadius, std::max(minRadius, radius));
  }
  this->Cylinder->SetRadius(radius);
  this->BuildRepresentation();
}

//------------------------------------------------------------------------------
// Get the radius the cylinder.
double vtkImplicitCylinderRepresentation::GetRadius()
{
  return this->Cylinder->GetRadius();
}

//------------------------------------------------------------------------------
void vtkImplicitCylinderRepresentation::SetDrawCylinder(vtkTypeBool drawCyl)
{
  if (drawCyl == this->DrawCylinder)
  {
    return;
  }

  this->Modified();
  this->DrawCylinder = drawCyl;
  this->BuildRepresentation();
}

//------------------------------------------------------------------------------
void vtkImplicitCylinderRepresentation::SetAlongXAxis(vtkTypeBool var)
{
  if (this->AlongXAxis != var)
  {
    this->AlongXAxis = var;
    this->Modified();
  }
  if (var)
  {
    this->AlongYAxisOff();
    this->AlongZAxisOff();
  }
}

//------------------------------------------------------------------------------
void vtkImplicitCylinderRepresentation::SetAlongYAxis(vtkTypeBool var)
{
  if (this->AlongYAxis != var)
  {
    this->AlongYAxis = var;
    this->Modified();
  }
  if (var)
  {
    this->AlongXAxisOff();
    this->AlongZAxisOff();
  }
}

//------------------------------------------------------------------------------
void vtkImplicitCylinderRepresentation::SetAlongZAxis(vtkTypeBool var)
{
  if (this->AlongZAxis != var)
  {
    this->AlongZAxis = var;
    this->Modified();
  }
  if (var)
  {
    this->AlongXAxisOff();
    this->AlongYAxisOff();
  }
}

//------------------------------------------------------------------------------
void vtkImplicitCylinderRepresentation::GetPolyData(vtkPolyData* pd)
{
  pd->ShallowCopy(this->Cyl);
}

//------------------------------------------------------------------------------
void vtkImplicitCylinderRepresentation::GetCylinder(vtkCylinder* cyl)
{
  if (cyl == nullptr)
  {
    return;
  }

  cyl->SetAxis(this->Cylinder->GetAxis());
  cyl->SetCenter(this->Cylinder->GetCenter());
  cyl->SetRadius(this->Cylinder->GetRadius());
}

//------------------------------------------------------------------------------
void vtkImplicitCylinderRepresentation::UpdatePlacement()
{
  this->BuildRepresentation();
  this->UpdateOutline();
  this->Edges->Update();
}

//------------------------------------------------------------------------------
void vtkImplicitCylinderRepresentation::BumpCylinder(int dir, double factor)
{
  // Compute the distance
  double d = this->InitialLength * this->BumpDistance * factor;

  // Push the cylinder
  this->PushCylinder((dir > 0 ? d : -d));
}

//------------------------------------------------------------------------------
void vtkImplicitCylinderRepresentation::PushCylinder(double d)
{
  vtkCamera* camera = this->Renderer->GetActiveCamera();
  if (!camera)
  {
    return;
  }
  double vpn[3], center[3];
  camera->GetViewPlaneNormal(vpn);
  this->Cylinder->GetCenter(center);

  center[0] += d * vpn[0];
  center[1] += d * vpn[1];
  center[2] += d * vpn[2];

  this->Cylinder->SetCenter(center);
  this->BuildRepresentation();
}

//------------------------------------------------------------------------------
void vtkImplicitCylinderRepresentation::BuildRepresentation()
{
  if (!this->Renderer || !this->Renderer->GetRenderWindow())
  {
    return;
  }

  vtkInformation* info = this->GetPropertyKeys();
  this->GetOutlineActor()->SetPropertyKeys(info);
  this->CylActor->SetPropertyKeys(info);
  this->EdgesActor->SetPropertyKeys(info);
  this->ConeActor->SetPropertyKeys(info);
  this->LineActor->SetPropertyKeys(info);
  this->ConeActor2->SetPropertyKeys(info);
  this->LineActor2->SetPropertyKeys(info);
  this->SphereActor->SetPropertyKeys(info);

  if (this->GetMTime() > this->BuildTime || this->Cylinder->GetMTime() > this->BuildTime ||
    this->Renderer->GetRenderWindow()->GetMTime() > this->BuildTime)
  {
    double* center = this->Cylinder->GetCenter();
    double* axis = this->Cylinder->GetAxis();
    double p2[3];

    this->UpdateCenterAndBounds(center);

    // Setup the cylinder axis
    double d = this->GetDiagonalLength();

    p2[0] = center[0] + 0.30 * d * axis[0];
    p2[1] = center[1] + 0.30 * d * axis[1];
    p2[2] = center[2] + 0.30 * d * axis[2];

    this->LineSource->SetPoint1(center);
    this->LineSource->SetPoint2(p2);
    this->ConeSource->SetCenter(p2);
    this->ConeSource->SetDirection(axis);

    p2[0] = center[0] - 0.30 * d * axis[0];
    p2[1] = center[1] - 0.30 * d * axis[1];
    p2[2] = center[2] - 0.30 * d * axis[2];

    this->LineSource2->SetPoint1(center[0], center[1], center[2]);
    this->LineSource2->SetPoint2(p2);
    this->ConeSource2->SetCenter(p2);
    this->ConeSource2->SetDirection(axis[0], axis[1], axis[2]);

    // Set up the position handle
    this->Sphere->SetCenter(center[0], center[1], center[2]);

    // Control the look of the edges
    if (this->Tubing)
    {
      this->EdgesMapper->SetInputConnection(this->EdgesTuber->GetOutputPort());
    }
    else
    {
      this->EdgesMapper->SetInputConnection(this->Edges->GetOutputPort());
    }

    // Construct intersected cylinder
    this->BuildCylinder();

    this->SizeHandles();
    this->BuildTime.Modified();
  }
}

//------------------------------------------------------------------------------
void vtkImplicitCylinderRepresentation::SizeHandles()
{
  double radius =
    this->vtkWidgetRepresentation::SizeHandlesInPixels(1.5, this->Sphere->GetCenter());

  this->ConeSource->SetHeight(2.0 * radius);
  this->ConeSource->SetRadius(radius);
  this->ConeSource2->SetHeight(2.0 * radius);
  this->ConeSource2->SetRadius(radius);

  this->Sphere->SetRadius(radius);

  this->EdgesTuber->SetRadius(0.25 * radius);
}

//------------------------------------------------------------------------------
// Create cylinder polydata.  Basically build an oriented cylinder of
// specified resolution.  Trim cylinder facets by performing
// intersection tests. Note that some facets may be outside the
// bounding box, in which cases they are discarded.
void vtkImplicitCylinderRepresentation::BuildCylinder()
{
  // Initialize the polydata
  this->Cyl->Reset();
  vtkPoints* pts = this->Cyl->GetPoints();
  vtkDataArray* normals = this->Cyl->GetPointData()->GetNormals();
  vtkCellArray* polys = this->Cyl->GetPolys();

  // Retrieve relevant parameters
  double* center = this->Cylinder->GetCenter();
  double* axis = this->Cylinder->GetAxis();
  double radius = this->Cylinder->GetRadius();
  int res = this->Resolution;
  double d = this->GetDiagonalLength();

  // We're gonna need a local coordinate system. Find a normal to the
  // cylinder axis. Then use cross product to find a third orthogonal
  // axis.
  int i;
  double n1[3], n2[3];
  for (i = 0; i < 3; i++)
  {
    // a little trick to find an orthogonal normal
    if (axis[i] != 0.0)
    {
      n1[(i + 2) % 3] = 0.0;
      n1[(i + 1) % 3] = 1.0;
      n1[i] = -axis[(i + 1) % 3] / axis[i];
      break;
    }
  }
  vtkMath::Normalize(n1);
  vtkMath::Cross(axis, n1, n2);

  // Now create Resolution line segments. Initially the line segments
  // are made a little long to extend outside of the bounding
  // box. Later on we'll trim them to the bounding box.
  pts->SetNumberOfPoints(2 * res);
  normals->SetNumberOfTuples(2 * res);

  vtkIdType pid;
  double x[3], n[3], theta;
  double v[3];
  v[0] = d * axis[0];
  v[1] = d * axis[1];
  v[2] = d * axis[2];
  for (pid = 0; pid < res; ++pid)
  {
    theta = static_cast<double>(pid) / static_cast<double>(res) * 2.0 * vtkMath::Pi();
    for (i = 0; i < 3; ++i)
    {
      n[i] = n1[i] * cos(theta) + n2[i] * sin(theta);
      x[i] = center[i] + radius * n[i] + v[i];
    }
    pts->SetPoint(pid, x);
    normals->SetTuple(pid, n);

    for (i = 0; i < 3; ++i)
    {
      x[i] = center[i] + radius * n[i] - v[i];
    }
    pts->SetPoint(res + pid, x);
    normals->SetTuple(res + pid, n);
  }

  // Now trim the cylinder against the bounding box. Mark edges that do not
  // intersect the bounding box.
  bool edgeInside[VTK_MAX_CYL_RESOLUTION];
  double x1[3], x2[3], p1[3], p2[3], t1, t2;

  double bounds[6];
  this->GetOutlineBounds(bounds);
  int plane1, plane2;
  for (pid = 0; pid < res; ++pid)
  {
    pts->GetPoint(pid, x1);
    pts->GetPoint(pid + res, x2);
    if (!vtkBox::IntersectWithLine(bounds, x1, x2, t1, t2, p1, p2, plane1, plane2))
    {
      edgeInside[pid] = false;
    }
    else
    {
      edgeInside[pid] = true;
      pts->SetPoint(pid, p1);
      pts->SetPoint(pid + res, p2);
    }
  }

  // Create polygons around cylinder. Make sure the edges of the polygon
  // are inside the widget's bounding box.
  vtkIdType ptIds[4];
  for (pid = 0; pid < res; ++pid)
  {
    if (edgeInside[pid] && edgeInside[(pid + 1) % res])
    {
      ptIds[0] = pid;
      ptIds[3] = (pid + 1) % res;
      ptIds[1] = ptIds[0] + res;
      ptIds[2] = ptIds[3] + res;
      polys->InsertNextCell(4, ptIds);
    }
  }
  polys->Modified();
}

//------------------------------------------------------------------------------
void vtkImplicitCylinderRepresentation::RegisterPickers()
{
  vtkPickingManager* pm = this->GetPickingManager();
  if (!pm)
  {
    return;
  }
  pm->AddPicker(this->Picker, this);
}
VTK_ABI_NAMESPACE_END
