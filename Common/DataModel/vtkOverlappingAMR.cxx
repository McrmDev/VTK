// SPDX-FileCopyrightText: Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
// SPDX-License-Identifier: BSD-3-Clause

#include "vtkOverlappingAMR.h"
#include "vtkAMRInformation.h"
#include "vtkCellData.h"
#include "vtkDataSetAttributes.h"
#include "vtkInformationIdTypeKey.h"
#include "vtkObjectFactory.h"
#include "vtkUniformGrid.h"
#include "vtkUniformGridAMRDataIterator.h"
#include "vtkUnsignedCharArray.h"

#include <utility>
#include <vector>

VTK_ABI_NAMESPACE_BEGIN
vtkStandardNewMacro(vtkOverlappingAMR);

vtkInformationKeyMacro(vtkOverlappingAMR, NUMBER_OF_BLANKED_POINTS, IdType);

//------------------------------------------------------------------------------
vtkOverlappingAMR::vtkOverlappingAMR() = default;

//------------------------------------------------------------------------------
vtkOverlappingAMR::~vtkOverlappingAMR() = default;

//------------------------------------------------------------------------------
void vtkOverlappingAMR::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
  if (this->AMRInfo)
  {
    this->AMRInfo->PrintSelf(os, indent);
  }
}

//------------------------------------------------------------------------------
vtkCompositeDataIterator* vtkOverlappingAMR::NewIterator()
{
  vtkUniformGridAMRDataIterator* iter = vtkUniformGridAMRDataIterator::New();
  iter->SetDataSet(this);
  return iter;
}

//------------------------------------------------------------------------------
void vtkOverlappingAMR::SetRefinementRatio(unsigned int level, int ratio)
{
  this->AMRInfo->SetRefinementRatio(level, ratio);
}

//------------------------------------------------------------------------------
int vtkOverlappingAMR::GetRefinementRatio(unsigned int level)
{
  if (!AMRInfo->HasRefinementRatio())
  {
    AMRInfo->GenerateRefinementRatio();
  }
  return this->AMRInfo->GetRefinementRatio(level);
}

//------------------------------------------------------------------------------
int vtkOverlappingAMR::GetRefinementRatio(vtkCompositeDataIterator* iter)
{
  vtkUniformGridAMRDataIterator* amrIter = vtkUniformGridAMRDataIterator::SafeDownCast(iter);

  unsigned int level = amrIter->GetCurrentLevel();
  return this->AMRInfo->GetRefinementRatio(level);
}

//------------------------------------------------------------------------------
void vtkOverlappingAMR::GenerateParentChildInformation()
{
  this->AMRInfo->GenerateParentChildInformation();
}

//------------------------------------------------------------------------------
bool vtkOverlappingAMR::HasChildrenInformation()
{
  return AMRInfo->HasChildrenInformation();
}

//------------------------------------------------------------------------------
unsigned int* vtkOverlappingAMR::GetParents(
  unsigned int level, unsigned int index, unsigned int& num)
{
  return this->AMRInfo->GetParents(level, index, num);
}

//------------------------------------------------------------------------------
unsigned int* vtkOverlappingAMR::GetChildren(
  unsigned int level, unsigned int index, unsigned int& num)
{
  return this->AMRInfo->GetChildren(level, index, num);
}

//------------------------------------------------------------------------------
void vtkOverlappingAMR::PrintParentChildInfo(unsigned int level, unsigned int index)
{
  this->AMRInfo->PrintParentChildInfo(level, index);
}

//------------------------------------------------------------------------------
void vtkOverlappingAMR::SetAMRBox(unsigned int level, unsigned int id, const vtkAMRBox& box)
{
  this->AMRInfo->SetAMRBox(level, id, box);
}

//------------------------------------------------------------------------------
const vtkAMRBox& vtkOverlappingAMR::GetAMRBox(unsigned int level, unsigned int id)
{
  const vtkAMRBox& box = this->AMRInfo->GetAMRBox(level, id);
  if (box.IsInvalid())
  {
    vtkErrorMacro("Invalid AMR box");
  }
  return box;
}

//------------------------------------------------------------------------------
void vtkOverlappingAMR::SetSpacing(unsigned int level, const double spacing[3])
{
  this->AMRInfo->SetSpacing(level, spacing);
}

//------------------------------------------------------------------------------
void vtkOverlappingAMR::GetSpacing(unsigned int level, double spacing[3])
{
  this->AMRInfo->GetSpacing(level, spacing);
}

//------------------------------------------------------------------------------
void vtkOverlappingAMR::GetBounds(unsigned int level, unsigned int id, double bb[6])
{
  this->AMRInfo->GetBounds(level, id, bb);
}

//------------------------------------------------------------------------------
void vtkOverlappingAMR::GetOrigin(unsigned int level, unsigned int id, double origin[3])
{
  double bb[6];
  this->GetBounds(level, id, bb);
  origin[0] = bb[0];
  origin[1] = bb[2];
  origin[2] = bb[4];
}

//------------------------------------------------------------------------------
void vtkOverlappingAMR::SetOrigin(const double origin[3])
{
  this->AMRInfo->SetOrigin(origin);
}

//------------------------------------------------------------------------------
double* vtkOverlappingAMR::GetOrigin()
{
  return this->AMRInfo ? this->AMRInfo->GetOrigin() : nullptr;
}

//------------------------------------------------------------------------------
void vtkOverlappingAMR::SetAMRBlockSourceIndex(unsigned int level, unsigned int id, int sourceId)
{
  unsigned int index = this->AMRInfo->GetIndex(level, id);
  this->AMRInfo->SetAMRBlockSourceIndex(index, sourceId);
}

//------------------------------------------------------------------------------
int vtkOverlappingAMR::GetAMRBlockSourceIndex(unsigned int level, unsigned int id)
{
  unsigned int index = this->AMRInfo->GetIndex(level, id);
  return this->AMRInfo->GetAMRBlockSourceIndex(index);
}

//------------------------------------------------------------------------------
void vtkOverlappingAMR::Audit()
{
  std::ignore = this->CheckValidity();
}

//------------------------------------------------------------------------------
bool vtkOverlappingAMR::CheckValidity()
{
  bool ret = this->AMRInfo->CheckValidity();

  int emptyDimension(-1);
  switch (this->GetGridDescription())
  {
    case VTK_YZ_PLANE:
      emptyDimension = 0;
      break;
    case VTK_XZ_PLANE:
      emptyDimension = 1;
      break;
    case VTK_XY_PLANE:
      emptyDimension = 2;
      break;
  }

  vtkSmartPointer<vtkUniformGridAMRDataIterator> iter;
  iter.TakeReference(vtkUniformGridAMRDataIterator::SafeDownCast(this->NewIterator()));
  iter->SetSkipEmptyNodes(1);
  for (iter->GoToFirstItem(); !iter->IsDoneWithTraversal(); iter->GoToNextItem())
  {
    vtkUniformGrid* grid = vtkUniformGrid::SafeDownCast(iter->GetCurrentDataObject());
    int hasGhost = grid->HasAnyGhostCells();

    unsigned int level = iter->GetCurrentLevel();
    unsigned int id = iter->GetCurrentIndex();
    const vtkAMRBox& box = this->AMRInfo->GetAMRBox(level, id);
    int dims[3];
    box.GetNumberOfNodes(dims);

    double spacing[3];
    this->GetSpacing(level, spacing);

    double origin[3];
    this->GetOrigin(level, id, origin);

    for (int d = 0; d < 3; d++)
    {
      if (d == emptyDimension)
      {
        if (grid->GetSpacing()[d] != spacing[d])
        {
          vtkErrorMacro(
            "The grid spacing does not match AMRInfo at (" << level << ", " << id << ")");
          ret = false;
        }
        if (!hasGhost && grid->GetOrigin()[d] != origin[d])
        {
          vtkErrorMacro(
            "The grid origin does not match AMRInfo at (" << level << ", " << id << ")");
          ret = false;
        }
        if (!hasGhost && grid->GetDimensions()[d] != dims[d])
        {
          vtkErrorMacro(
            "The grid dimensions does not match AMRInfo at (" << level << ", " << id << ")");
          ret = false;
        }
      }
    }
  }
  return ret;
}

bool vtkOverlappingAMR::FindGrid(double q[3], unsigned int& level, unsigned int& gridId)
{
  return this->AMRInfo->FindGrid(q, level, gridId);
}
VTK_ABI_NAMESPACE_END
