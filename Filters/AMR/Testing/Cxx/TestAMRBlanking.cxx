// SPDX-FileCopyrightText: Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
// SPDX-License-Identifier: BSD-3-Clause
// .NAME TestAMRBlanking.cxx -- Simple test for AMR blanking (visibility)
//
// .SECTION Description
//  Test blanking for Berger-Collela AMR datasets.

#include "vtkAMRBox.h"
#include "vtkAMRUtilities.h"
#include "vtkOverlappingAMR.h"
#include "vtkStructuredData.h"
#include "vtkUniformGrid.h"

#include <cassert>
#include <sstream>

namespace AMRVisibilityTests
{

//------------------------------------------------------------------------------
vtkUniformGrid* GetGrid(double origin[3], double h[3], int ndims[3])
{
  vtkUniformGrid* grid = vtkUniformGrid::New();
  grid->Initialize();
  grid->SetOrigin(origin);
  grid->SetSpacing(h);
  grid->SetDimensions(ndims);
  return (grid);
}

//------------------------------------------------------------------------------
vtkOverlappingAMR* GetAMRDataSet(const int description)
{
  double origin[3] = { 0, 0, 0 };
  double spacing[3];
  int ndims[3];
  vtkAMRBox box;
  std::vector<unsigned int> blocksPerLevel(2, 1);

  vtkOverlappingAMR* amrDataSet = vtkOverlappingAMR::New();
  amrDataSet->Initialize(blocksPerLevel);
  amrDataSet->SetGridDescription(description);
  amrDataSet->SetOrigin(origin);

  vtkUniformGrid* gridPtr = nullptr;
  switch (description)
  {
    case vtkStructuredData::VTK_STRUCTURED_XY_PLANE:
      // Root block
      ndims[2] = 1;
      origin[0] = origin[1] = origin[2] = 0.0;
      spacing[0] = spacing[1] = spacing[2] = 1.0;
      ndims[0] = ndims[1] = 4;

      gridPtr = GetGrid(origin, spacing, ndims);
      box = vtkAMRBox(origin, ndims, spacing, amrDataSet->GetOrigin(), description);
      amrDataSet->SetSpacing(0, spacing);
      amrDataSet->SetAMRBox(0, 0, box);
      amrDataSet->SetDataSet(0, 0, gridPtr);
      gridPtr->Delete();

      // Refined patch that covers entire root domain
      origin[0] = origin[1] = origin[2] = 0.0;
      spacing[0] = spacing[1] = spacing[2] = 0.5;
      ndims[0] = ndims[1] = 6;
      gridPtr = GetGrid(origin, spacing, ndims);
      box = vtkAMRBox(origin, ndims, spacing, amrDataSet->GetOrigin(), description);
      amrDataSet->SetSpacing(1, spacing);
      amrDataSet->SetAMRBox(1, 0, box);
      amrDataSet->SetDataSet(1, 0, gridPtr);
      gridPtr->Delete();
      break;
    case vtkStructuredData::VTK_STRUCTURED_XZ_PLANE:
      // Root block
      ndims[1] = 1;
      origin[0] = origin[1] = origin[2] = 0.0;
      spacing[0] = spacing[1] = spacing[2] = 1.0;
      ndims[0] = ndims[2] = 4;
      gridPtr = GetGrid(origin, spacing, ndims);
      box = vtkAMRBox(origin, ndims, spacing, amrDataSet->GetOrigin(), description);
      amrDataSet->SetSpacing(0, spacing);
      amrDataSet->SetAMRBox(0, 0, box);
      amrDataSet->SetDataSet(0, 0, gridPtr);
      gridPtr->Delete();

      // Refined patch that covers entire root domain
      origin[0] = origin[1] = origin[2] = 0.0;
      spacing[0] = spacing[1] = spacing[2] = 0.5;
      ndims[0] = ndims[2] = 6;
      gridPtr = GetGrid(origin, spacing, ndims);
      box = vtkAMRBox(origin, ndims, spacing, amrDataSet->GetOrigin(), description);
      amrDataSet->SetSpacing(1, spacing);
      amrDataSet->SetAMRBox(1, 0, box);
      amrDataSet->SetDataSet(1, 0, gridPtr);
      gridPtr->Delete();
      break;
    case vtkStructuredData::VTK_STRUCTURED_YZ_PLANE:
      // Root block
      ndims[0] = 1;
      origin[0] = origin[1] = origin[2] = 0.0;
      spacing[0] = spacing[1] = spacing[2] = 1.0;
      ndims[1] = ndims[2] = 4;
      gridPtr = GetGrid(origin, spacing, ndims);
      box = vtkAMRBox(origin, ndims, spacing, amrDataSet->GetOrigin(), description);
      amrDataSet->SetSpacing(0, spacing);
      amrDataSet->SetAMRBox(0, 0, box);
      amrDataSet->SetDataSet(0, 0, gridPtr);
      gridPtr->Delete();

      // Refined patch that covers entire root domain
      origin[0] = origin[1] = origin[2] = 0.0;
      spacing[0] = spacing[1] = spacing[2] = 0.5;
      ndims[1] = ndims[2] = 6;
      gridPtr = GetGrid(origin, spacing, ndims);
      box = vtkAMRBox(origin, ndims, spacing, amrDataSet->GetOrigin(), description);
      amrDataSet->SetSpacing(1, spacing);
      amrDataSet->SetAMRBox(1, 0, box);
      amrDataSet->SetDataSet(1, 0, gridPtr);
      gridPtr->Delete();
      break;
    case vtkStructuredData::VTK_STRUCTURED_XYZ_GRID:
      // Root block
      origin[0] = origin[1] = origin[2] = 0.0;
      spacing[0] = spacing[1] = spacing[2] = 1.0;
      ndims[0] = ndims[1] = ndims[2] = 4;
      gridPtr = GetGrid(origin, spacing, ndims);
      box = vtkAMRBox(origin, ndims, spacing, amrDataSet->GetOrigin(), description);
      amrDataSet->SetSpacing(0, spacing);
      amrDataSet->SetAMRBox(0, 0, box);
      amrDataSet->SetDataSet(0, 0, gridPtr);
      gridPtr->Delete();

      // Refined patch that covers entire root domain
      origin[0] = origin[1] = origin[2] = 0.0;
      spacing[0] = spacing[1] = spacing[2] = 0.5;
      ndims[0] = ndims[1] = ndims[2] = 6;
      gridPtr = GetGrid(origin, spacing, ndims);
      box = vtkAMRBox(origin, ndims, spacing, amrDataSet->GetOrigin(), description);
      amrDataSet->SetSpacing(1, spacing);
      amrDataSet->SetAMRBox(1, 0, box);
      amrDataSet->SetDataSet(1, 0, gridPtr);
      gridPtr->Delete();
      break;
    default:
      assert("ERROR: Unhandled data description! Code should not reach here!" && false);
  }
  vtkAMRUtilities::BlankCells(amrDataSet);
  return (amrDataSet);
}

//------------------------------------------------------------------------------
int TestAMRVisibility(const int dataDescription)
{
  int rc = 0;
  vtkOverlappingAMR* myAMR = GetAMRDataSet(dataDescription);
  assert("pre: AMR dataset is nullptr" && (myAMR != nullptr));

  vtkUniformGrid* root = myAMR->GetDataSet(0, 0);
  assert("ERROR: root AMR block is nullptr!" && (root != nullptr));

  vtkIdType cellIdx = 0;
  for (; cellIdx < root->GetNumberOfCells(); ++cellIdx)
  {
    if (root->IsCellVisible(cellIdx))
    {
      rc++;
    }
  } // END for all cells

  if (rc != 0)
  {
    std::cerr << rc << "/" << root->GetNumberOfCells() << " are visible!\n";
  }

  myAMR->Delete();
  return (rc);
}

//------------------------------------------------------------------------------
void CheckTestStatus(int rc, const std::string& TestName)
{
  std::cout << "Test " << TestName << "...";
  std::cout.flush();
  if (rc == 0)
  {
    std::cout << "PASSED!\n";
    std::cout.flush();
  }
  else
  {
    std::cout << "FAILED!\n";
    std::cout.flush();
  }
}

} // END namespace AMRVisibilityTests

//------------------------------------------------------------------------------
int TestAMRBlanking(int, char*[])
{
  int rc = 0;
  int status = 0;

  // XYZ test
  status = AMRVisibilityTests::TestAMRVisibility(vtkStructuredData::VTK_STRUCTURED_XYZ_GRID);
  AMRVisibilityTests::CheckTestStatus(
    status, "TestAMRVisibility-vtkStructuredData::VTK_STRUCTURED_XYZ_GRID");
  rc += status;

  // XY PLANE
  status = AMRVisibilityTests::TestAMRVisibility(vtkStructuredData::VTK_STRUCTURED_XY_PLANE);
  AMRVisibilityTests::CheckTestStatus(
    status, "TestAMRVisibility-vtkStructuredData::VTK_STRUCTURED_XY_PLANE");
  rc += status;

  // XZ PLANE
  status = AMRVisibilityTests::TestAMRVisibility(vtkStructuredData::VTK_STRUCTURED_XZ_PLANE);
  AMRVisibilityTests::CheckTestStatus(
    status, "TestAMRVisibility-vtkStructuredData::VTK_STRUCTURED_XZ_PLANE");
  rc += status;

  // YZ PLANE
  status = AMRVisibilityTests::TestAMRVisibility(vtkStructuredData::VTK_STRUCTURED_YZ_PLANE);
  AMRVisibilityTests::CheckTestStatus(
    status, "TestAMRVisibility-vtkStructuredData::VTK_STRUCTURED_YZ_PLANE");
  rc += status;
  return (rc);
}
