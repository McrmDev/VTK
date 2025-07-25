// SPDX-FileCopyrightText: Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
// SPDX-License-Identifier: BSD-3-Clause
// .NAME Generate3DAMRDataSetWithPulse.cxx -- Generated sample 3D AMR dataset
//
// .SECTION Description
//  This utility code generates a simple 3D AMR dataset with a gaussian
//  pulse at the center. The resulting AMR dataset is written using the
//  vtkXMLHierarchicalBoxDataSetWriter.

// disable linking warning due to inclusion of vtkXML*
#if defined(_MSC_VER)
#pragma warning(disable : 4089)
#endif

#include <cassert>
#include <cmath>
#include <iostream>
#include <sstream>

#include "AMRCommon.h"
#include "vtkAMRBox.h"
#include "vtkAMRUtilities.h"
#include "vtkCell.h"
#include "vtkCellData.h"
#include "vtkDataArray.h"
#include "vtkDoubleArray.h"
#include "vtkOverlappingAMR.h"
#include "vtkPointData.h"
#include "vtkPoints.h"
#include "vtkUniformGrid.h"

static struct PulseAttributes
{
  double origin[3]; // xyz for the center of the pulse
  double width[3];  // the width of the pulse
  double amplitude; // the amplitude of the pulse
} Pulse;

//
// Function prototype declarations
//

// Description:
// Sets the pulse attributes
void SetPulse();

// Description:
// Constructs the vtkOverlappingAMR.
vtkOverlappingAMR* GetAMRDataSet();

// Description:
// Attaches the pulse to the given grid.
void AttachPulseToGrid(vtkUniformGrid* grid);

//
// Program main
//
int main(int argc, char** argv)
{
  // Fix compiler warning for unused variables
  static_cast<void>(argc);
  static_cast<void>(argv);

  // STEP 0: Initialize gaussian pulse parameters
  SetPulse();

  // STEP 1: Get the AMR dataset
  vtkSmartPointer<vtkOverlappingAMR> amrDataSet;
  amrDataSet.TakeReference(GetAMRDataSet());

  AMRCommon::WriteAMRData(amrDataSet, "Gaussian3D");
  return 0;
}

//=============================================================================
//                    Function Prototype Implementation
//=============================================================================

void SetPulse()
{
  Pulse.origin[0] = Pulse.origin[1] = Pulse.origin[2] = -1.0;
  Pulse.width[0] = Pulse.width[1] = Pulse.width[2] = 6.0;
  Pulse.amplitude = 0.0001;
}

//------------------------------------------------------------------------------
void AttachPulseToGrid(vtkUniformGrid* grid)
{
  assert("pre: grid is nullptr!" && (grid != nullptr));

  vtkNew<vtkDoubleArray> xyz;
  xyz->SetName("GaussianPulse");
  xyz->SetNumberOfComponents(1);
  xyz->SetNumberOfTuples(grid->GetNumberOfCells());

  for (int cellIdx = 0; cellIdx < grid->GetNumberOfCells(); ++cellIdx)
  {
    double center[3];
    AMRCommon::ComputeCellCenter(grid, cellIdx, center);

    double r = 0.0;
    for (int i = 0; i < 3; ++i)
    {
      double dx = center[i] - Pulse.origin[i];
      r += (dx * dx) / (Pulse.width[i] * Pulse.width[i]);
    }
    double f = Pulse.amplitude * std::exp(-r);

    xyz->SetTuple1(cellIdx, f);
  } // END for all cells

  grid->GetCellData()->AddArray(xyz);
}
//------------------------------------------------------------------------------
vtkOverlappingAMR* GetAMRDataSet()
{
  vtkNew<vtkOverlappingAMR> data;
  int blocksPerLevel[2] = { 1, 3 };
  double globalOrigin[3] = { -2.0, -2.0, -2.0 };
  data->Initialize(2, blocksPerLevel);
  data->SetOrigin(globalOrigin);
  data->SetGridDescription(vtkStructuredData::VTK_STRUCTURED_XYZ_GRID);

  double origin[3];
  double h[3];
  int ndim[3];

  // Root Block -- Block 0
  ndim[0] = 6;
  ndim[1] = ndim[2] = 5;
  h[0] = h[1] = h[2] = 1.0;
  origin[0] = origin[1] = origin[2] = -2.0;
  int blockId = 0;
  int level = 0;
  vtkSmartPointer<vtkUniformGrid> root;
  root.TakeReference(AMRCommon::GetGrid(origin, h, ndim));
  vtkAMRBox box(origin, ndim, h, data->GetOrigin(), data->GetGridDescription());
  AttachPulseToGrid(root);
  data->SetAMRBox(level, blockId, box);
  data->SetDataSet(level, blockId, root);

  // Block 1
  ndim[0] = 3;
  ndim[1] = ndim[2] = 5;
  h[0] = h[1] = h[2] = 0.5;
  origin[0] = origin[1] = origin[2] = -2.0;
  blockId = 0;
  level = 1;
  vtkSmartPointer<vtkUniformGrid> grid1;
  root.TakeReference(AMRCommon::GetGrid(origin, h, ndim));
  AttachPulseToGrid(grid1);
  vtkAMRBox box1(origin, ndim, h, data->GetOrigin(), data->GetGridDescription());
  data->SetAMRBox(level, blockId, box1);
  data->SetDataSet(level, blockId, grid1);

  // Block 2
  ndim[0] = 3;
  ndim[1] = ndim[2] = 5;
  h[0] = h[1] = h[2] = 0.5;
  origin[0] = 0.0;
  origin[1] = origin[2] = -1.0;
  blockId = 1;
  level = 1;
  vtkSmartPointer<vtkUniformGrid> grid2;
  grid2.TakeReference(AMRCommon::GetGrid(origin, h, ndim));
  AttachPulseToGrid(grid2);
  vtkAMRBox box2(origin, ndim, h, data->GetOrigin(), data->GetGridDescription());
  data->SetAMRBox(level, blockId, box2);
  data->SetDataSet(level, blockId, grid2);

  // Block 3
  ndim[0] = 3;
  ndim[1] = ndim[2] = 7;
  h[0] = h[1] = h[2] = 0.5;
  origin[0] = 2.0;
  origin[1] = origin[2] = -1.0;
  blockId = 2;
  level = 1;
  vtkSmartPointer<vtkUniformGrid> grid3;
  grid3.TakeReference(AMRCommon::GetGrid(origin, h, ndim));
  vtkAMRBox box3(origin, ndim, h, data->GetOrigin(), data->GetGridDescription());
  AttachPulseToGrid(grid3);
  data->SetAMRBox(level, blockId, box3);
  data->SetDataSet(level, blockId, grid3);

  vtkAMRUtilities::BlankCells(data);

  return (data);
}
