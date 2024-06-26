// SPDX-FileCopyrightText: Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
// SPDX-License-Identifier: BSD-3-Clause

#include "vtkCPExodusIIElementBlock.h"

#include "vtkCellType.h"
#include "vtkCellTypes.h"
#include "vtkGenericCell.h"
#include "vtkIdTypeArray.h"
#include "vtkObjectFactory.h"
#include "vtkPoints.h"

#include <algorithm>

//------------------------------------------------------------------------------
VTK_ABI_NAMESPACE_BEGIN
vtkStandardNewMacro(vtkCPExodusIIElementBlock);
vtkStandardNewMacro(vtkCPExodusIIElementBlockImpl);

//------------------------------------------------------------------------------
void vtkCPExodusIIElementBlockImpl::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
  os << indent << "Elements: " << this->Elements << endl;
  os << indent << "CellType: " << vtkCellTypes::GetClassNameFromTypeId(this->CellType) << endl;
  os << indent << "CellSize: " << this->CellSize << endl;
  os << indent << "NumberOfCells: " << this->NumberOfCells << endl;
}

//------------------------------------------------------------------------------
bool vtkCPExodusIIElementBlockImpl::SetExodusConnectivityArray(
  int* elements, const std::string& type, int numElements, int nodesPerElement)
{
  if (!elements)
  {
    return false;
  }

  // Try to figure out the vtk cell type:
  if (type.size() < 3)
  {
    vtkErrorMacro(<< "Element type too short, expected at least 3 char: " << type);
    return false;
  }
  std::string typekey = type.substr(0, 3);
  std::transform(typekey.begin(), typekey.end(), typekey.begin(), ::toupper);
  if (typekey == "CIR" || typekey == "SPH")
  {
    this->CellType = VTK_VERTEX;
  }
  else if (typekey == "TRU" || typekey == "BEA")
  {
    this->CellType = VTK_LINE;
  }
  else if (typekey == "TRI")
  {
    this->CellType = VTK_TRIANGLE;
  }
  else if (typekey == "QUA" || typekey == "SHE")
  {
    this->CellType = VTK_QUAD;
  }
  else if (typekey == "TET")
  {
    this->CellType = VTK_TETRA;
  }
  else if (typekey == "WED")
  {
    this->CellType = VTK_WEDGE;
  }
  else if (typekey == "HEX")
  {
    this->CellType = VTK_HEXAHEDRON;
  }
  else
  {
    vtkErrorMacro(<< "Unknown cell type: " << type);
    return false;
  }

  this->CellSize = static_cast<vtkIdType>(nodesPerElement);
  this->NumberOfCells = static_cast<vtkIdType>(numElements);
  this->Elements = elements;
  this->Modified();

  return true;
}

//------------------------------------------------------------------------------
vtkIdType vtkCPExodusIIElementBlockImpl::GetNumberOfCells()
{
  return this->NumberOfCells;
}

//------------------------------------------------------------------------------
int vtkCPExodusIIElementBlockImpl::GetCellType(vtkIdType)
{
  return this->CellType;
}

//------------------------------------------------------------------------------
void vtkCPExodusIIElementBlockImpl::GetCellPoints(vtkIdType cellId, vtkIdList* ptIds)
{
  ptIds->SetNumberOfIds(this->CellSize);

  std::transform(
    this->GetElementStart(cellId), this->GetElementEnd(cellId), ptIds->GetPointer(0), NodeToPoint);
}

//------------------------------------------------------------------------------
void vtkCPExodusIIElementBlockImpl::GetFaceStream(
  vtkIdType vtkNotUsed(cellId), vtkIdList* vtkNotUsed(ptIds))
{
  // vtkCPExodusIIElementBlockImpl does not support polyhedra
  vtkErrorMacro(<< __FUNCTION__ << " is not implemented");
}

//------------------------------------------------------------------------------
void vtkCPExodusIIElementBlockImpl::GetPolyhedronFaces(
  vtkIdType vtkNotUsed(cellId), vtkCellArray* vtkNotUsed(faces))
{
  // vtkCPExodusIIElementBlockImpl does not support polyhedra
  vtkErrorMacro(<< __FUNCTION__ << " is not implemented");
}

//------------------------------------------------------------------------------
void vtkCPExodusIIElementBlockImpl::GetPointCells(vtkIdType ptId, vtkIdList* cellIds)
{
  const int targetElement = PointToNode(ptId);
  int* element = this->GetStart();
  int* elementEnd = this->GetEnd();

  cellIds->Reset();

  element = std::find(element, elementEnd, targetElement);
  while (element != elementEnd)
  {
    cellIds->InsertNextId(static_cast<vtkIdType>((element - this->Elements) / this->CellSize));
    element = std::find(element, elementEnd, targetElement);
  }
}

//------------------------------------------------------------------------------
int vtkCPExodusIIElementBlockImpl::GetMaxCellSize()
{
  return this->CellSize;
}

//------------------------------------------------------------------------------
void vtkCPExodusIIElementBlockImpl::GetIdsOfCellsOfType(int type, vtkIdTypeArray* array)
{
  array->Reset();
  if (type == this->CellType)
  {
    array->SetNumberOfComponents(1);
    array->Allocate(this->NumberOfCells);
    for (vtkIdType i = 0; i < this->NumberOfCells; ++i)
    {
      array->InsertNextValue(i);
    }
  }
}

//------------------------------------------------------------------------------
int vtkCPExodusIIElementBlockImpl::IsHomogeneous()
{
  return 1;
}

//------------------------------------------------------------------------------
void vtkCPExodusIIElementBlockImpl::Allocate(vtkIdType, int)
{
  vtkErrorMacro("Read only container.");
}

//------------------------------------------------------------------------------
vtkIdType vtkCPExodusIIElementBlockImpl::InsertNextCell(int, vtkIdList*)
{
  vtkErrorMacro("Read only container.");
  return -1;
}

//------------------------------------------------------------------------------
vtkIdType vtkCPExodusIIElementBlockImpl::InsertNextCell(int, vtkIdType, const vtkIdType[])
{
  vtkErrorMacro("Read only container.");
  return -1;
}

//------------------------------------------------------------------------------
vtkIdType vtkCPExodusIIElementBlockImpl::InsertNextCell(
  int, vtkIdType, const vtkIdType[], vtkCellArray*)
{
  vtkErrorMacro("Read only container.");
  return -1;
}

//------------------------------------------------------------------------------
void vtkCPExodusIIElementBlockImpl::ReplaceCell(vtkIdType, int, const vtkIdType[])
{
  vtkErrorMacro("Read only container.");
}

//------------------------------------------------------------------------------
vtkCPExodusIIElementBlockImpl::vtkCPExodusIIElementBlockImpl()
  : Elements(nullptr)
  , CellType(VTK_EMPTY_CELL)
  , CellSize(0)
  , NumberOfCells(0)
{
}

//------------------------------------------------------------------------------
vtkCPExodusIIElementBlockImpl::~vtkCPExodusIIElementBlockImpl()
{
  delete[] this->Elements;
}
VTK_ABI_NAMESPACE_END
