// SPDX-FileCopyrightText: Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
// SPDX-License-Identifier: BSD-3-Clause
#include "vtkAbstractMapper.h"

#include "vtkAbstractArray.h"
#include "vtkCellData.h"
#include "vtkDataArray.h"
#include "vtkDataSet.h"
#include "vtkObjectFactory.h"
#include "vtkPlaneCollection.h"
#include "vtkPlanes.h"
#include "vtkPointData.h"
#include "vtkTimerLog.h"
#include "vtkUnsignedCharArray.h"

VTK_ABI_NAMESPACE_BEGIN
vtkCxxSetObjectMacro(vtkAbstractMapper, ClippingPlanes, vtkPlaneCollection);

// Construct object.
vtkAbstractMapper::vtkAbstractMapper()
{
  this->TimeToDraw = 0.0;
  this->LastWindow = nullptr;
  this->ClippingPlanes = nullptr;
  this->Timer = vtkTimerLog::New();
  this->SetNumberOfOutputPorts(0);
  this->SetNumberOfInputPorts(1);
}

vtkAbstractMapper::~vtkAbstractMapper()
{
  this->Timer->Delete();
  if (this->ClippingPlanes)
  {
    this->ClippingPlanes->UnRegister(this);
  }
}

// Description:
// Override Modifiedtime as we have added Clipping planes
vtkMTimeType vtkAbstractMapper::GetMTime()
{
  vtkMTimeType mTime = this->Superclass::GetMTime();
  vtkMTimeType clipMTime;

  if (this->ClippingPlanes != nullptr)
  {
    clipMTime = this->ClippingPlanes->GetMTime();
    mTime = (clipMTime > mTime ? clipMTime : mTime);
  }

  return mTime;
}

void vtkAbstractMapper::AddClippingPlane(vtkPlane* plane)
{
  if (this->ClippingPlanes == nullptr)
  {
    this->ClippingPlanes = vtkPlaneCollection::New();
    this->ClippingPlanes->Register(this);
    this->ClippingPlanes->Delete();
  }

  this->ClippingPlanes->AddItem(plane);
  this->Modified();
}

void vtkAbstractMapper::RemoveClippingPlane(vtkPlane* plane)
{
  if (this->ClippingPlanes == nullptr)
  {
    vtkErrorMacro(<< "Cannot remove clipping plane: mapper has none");
    return;
  }
  this->ClippingPlanes->RemoveItem(plane);
  this->Modified();
}

void vtkAbstractMapper::RemoveAllClippingPlanes()
{
  if (this->ClippingPlanes && this->ClippingPlanes->GetNumberOfItems() > 0)
  {
    this->ClippingPlanes->RemoveAllItems();
    this->Modified();
  }
}

void vtkAbstractMapper::SetClippingPlanes(vtkPlanes* planes)
{
  vtkPlane* plane;
  if (!planes)
  {
    return;
  }

  int numPlanes = planes->GetNumberOfPlanes();

  this->RemoveAllClippingPlanes();
  for (int i = 0; i < numPlanes && i < 6; i++)
  {
    plane = vtkPlane::New();
    planes->GetPlane(i, plane);
    this->AddClippingPlane(plane);
    plane->Delete();
  }
}

//------------------------------------------------------------------------------
vtkUnsignedCharArray* vtkAbstractMapper::GetGhostArray(
  vtkDataSet* input, int scalarMode, unsigned char& ghostsToSkip)
{
  switch (scalarMode)
  {
    case VTK_SCALAR_MODE_DEFAULT:
    {
      vtkUnsignedCharArray* ghosts = input->GetPointData()->GetGhostArray();
      if (!ghosts)
      {
        ghostsToSkip = input->GetCellData()->GetGhostsToSkip();
        return input->GetCellData()->GetGhostArray();
      }
      ghostsToSkip = input->GetPointData()->GetGhostsToSkip();
      return ghosts;
    }
    case VTK_SCALAR_MODE_USE_POINT_DATA:
    case VTK_SCALAR_MODE_USE_POINT_FIELD_DATA:
      ghostsToSkip = input->GetPointData()->GetGhostsToSkip();
      return input->GetPointData()->GetGhostArray();
    case VTK_SCALAR_MODE_USE_CELL_DATA:
    case VTK_SCALAR_MODE_USE_CELL_FIELD_DATA:
      ghostsToSkip = input->GetCellData()->GetGhostsToSkip();
      return input->GetCellData()->GetGhostArray();
    case VTK_SCALAR_MODE_USE_FIELD_DATA:
      ghostsToSkip = input->GetFieldData()->GetGhostsToSkip();
      return input->GetFieldData()->GetGhostArray();
    default:
      return nullptr;
  }
}

//------------------------------------------------------------------------------
vtkDataArray* vtkAbstractMapper::GetScalars(vtkDataSet* input, int scalarMode, int arrayAccessMode,
  int arrayId, const char* arrayName, int& cellFlag)
{
  vtkAbstractArray* abstractScalars = vtkAbstractMapper::GetAbstractScalars(
    input, scalarMode, arrayAccessMode, arrayId, arrayName, cellFlag);
  vtkDataArray* scalars = vtkArrayDownCast<vtkDataArray>(abstractScalars);
  return scalars;
}

//------------------------------------------------------------------------------
vtkAbstractArray* vtkAbstractMapper::GetAbstractScalars(vtkDataSet* input, int scalarMode,
  int arrayAccessMode, int arrayId, const char* arrayName, int& cellFlag)
{
  vtkAbstractArray* scalars = nullptr;
  vtkPointData* pd;
  vtkCellData* cd;
  vtkFieldData* fd;

  // make sure we have an input
  if (!input)
  {
    return nullptr;
  }

  // get and scalar data according to scalar mode
  if (scalarMode == VTK_SCALAR_MODE_DEFAULT)
  {
    scalars = input->GetPointData()->GetScalars();
    cellFlag = 0;
    if (!scalars)
    {
      scalars = input->GetCellData()->GetScalars();
      cellFlag = 1;
    }
  }
  else if (scalarMode == VTK_SCALAR_MODE_USE_POINT_DATA)
  {
    scalars = input->GetPointData()->GetScalars();
    cellFlag = 0;
  }
  else if (scalarMode == VTK_SCALAR_MODE_USE_CELL_DATA)
  {
    scalars = input->GetCellData()->GetScalars();
    cellFlag = 1;
  }
  else if (scalarMode == VTK_SCALAR_MODE_USE_POINT_FIELD_DATA)
  {
    pd = input->GetPointData();
    if (arrayAccessMode == VTK_GET_ARRAY_BY_ID)
    {
      scalars = pd->GetAbstractArray(arrayId);
    }
    else
    {
      scalars = pd->GetAbstractArray(arrayName);
    }
    cellFlag = 0;
  }
  else if (scalarMode == VTK_SCALAR_MODE_USE_CELL_FIELD_DATA)
  {
    cd = input->GetCellData();
    if (arrayAccessMode == VTK_GET_ARRAY_BY_ID)
    {
      scalars = cd->GetAbstractArray(arrayId);
    }
    else
    {
      scalars = cd->GetAbstractArray(arrayName);
    }
    cellFlag = 1;
  }
  else if (scalarMode == VTK_SCALAR_MODE_USE_FIELD_DATA)
  {
    fd = input->GetFieldData();
    if (arrayAccessMode == VTK_GET_ARRAY_BY_ID)
    {
      scalars = fd->GetAbstractArray(arrayId);
    }
    else
    {
      scalars = fd->GetAbstractArray(arrayName);
    }
    cellFlag = 2;
  }

  return scalars;
}

// Shallow copy of vtkProp.
void vtkAbstractMapper::ShallowCopy(vtkAbstractMapper* mapper)
{
  this->SetClippingPlanes(mapper->GetClippingPlanes());
}

void vtkAbstractMapper::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);

  os << indent << "TimeToDraw: " << this->TimeToDraw << "\n";

  if (this->ClippingPlanes)
  {
    os << indent << "ClippingPlanes:\n";
    this->ClippingPlanes->PrintSelf(os, indent.GetNextIndent());
  }
  else
  {
    os << indent << "ClippingPlanes: (none)\n";
  }
}

int vtkAbstractMapper::GetNumberOfClippingPlanes()
{
  int n = 0;

  if (this->ClippingPlanes)
  {
    n = this->ClippingPlanes->GetNumberOfItems();
  }

  return n;
}
VTK_ABI_NAMESPACE_END
