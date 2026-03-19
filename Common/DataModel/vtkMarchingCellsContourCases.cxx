// SPDX-FileCopyrightText: Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
// SPDX-License-Identifier: BSD-3-Clause

#include "vtkMarchingCellsContourCases.h"

namespace
{
//------------------------------------------------------------------------------
// Marching lines case table
constexpr vtkMarchingCellsContourCases::LineCase LineCases[4] = { { -1, -1 }, { 1, 0 }, { 0, 1 },
  { -1, -1 } };

//------------------------------------------------------------------------------
// Marching triangles case table
constexpr vtkMarchingCellsContourCases::TriangleCase TriangleCases[] = {
  { -1, -1, -1 },
  { 0, 2, -1 },
  { 1, 0, -1 },
  { 1, 2, -1 },
  { 2, 1, -1 },
  { 0, 1, -1 },
  { 2, 0, -1 },
  { -1, -1, -1 },
};

//------------------------------------------------------------------------------
// Marching pixels case table
constexpr vtkMarchingCellsContourCases::PixelCase PixelCases[] = {
  { -1, -1, -1, -1, -1 }, // 0
  { 0, 3, -1, -1, -1 },   // 1
  { 1, 0, -1, -1, -1 },   // 2
  { 1, 3, -1, -1, -1 },   // 3
  { 3, 2, -1, -1, -1 },   // 4
  { 0, 2, -1, -1, -1 },   // 5
  { 1, 0, 3, 2, -1 },     // 6
  { 1, 2, -1, -1, -1 },   // 7
  { 2, 1, -1, -1, -1 },   // 8
  { 0, 3, 2, 1, -1 },     // 9
  { 2, 0, -1, -1, -1 },   // 10
  { 2, 3, -1, -1, -1 },   // 11
  { 3, 1, -1, -1, -1 },   // 12
  { 0, 1, -1, -1, -1 },   // 13
  { 3, 0, -1, -1, -1 },   // 14
  { -1, -1, -1, -1, -1 }, // 15
};

//------------------------------------------------------------------------------
// Marching quads case table
constexpr vtkMarchingCellsContourCases::QuadCase QuadCases[] = {
  { -1, -1, -1, -1, -1 },
  { 0, 3, -1, -1, -1 },
  { 1, 0, -1, -1, -1 },
  { 1, 3, -1, -1, -1 },
  { 2, 1, -1, -1, -1 },
  { 0, 3, 2, 1, -1 },
  { 2, 0, -1, -1, -1 },
  { 2, 3, -1, -1, -1 },
  { 3, 2, -1, -1, -1 },
  { 0, 2, -1, -1, -1 },
  { 1, 0, 3, 2, -1 },
  { 1, 2, -1, -1, -1 },
  { 3, 1, -1, -1, -1 },
  { 0, 1, -1, -1, -1 },
  { 3, 0, -1, -1, -1 },
  { -1, -1, -1, -1, -1 },
};

//------------------------------------------------------------------------------
// Marching tetras case table
constexpr vtkMarchingCellsContourCases::TetraCase TetraCases[] = { { -1, -1, -1, -1, -1, -1, -1 },
  { 3, 0, 2, -1, -1, -1, -1 }, { 1, 0, 4, -1, -1, -1, -1 }, { 2, 3, 4, 2, 4, 1, -1 },
  { 2, 1, 5, -1, -1, -1, -1 }, { 5, 3, 1, 1, 3, 0, -1 }, { 2, 0, 5, 5, 0, 4, -1 },
  { 5, 3, 4, -1, -1, -1, -1 }, { 4, 3, 5, -1, -1, -1, -1 }, { 4, 0, 5, 5, 0, 2, -1 },
  { 5, 0, 3, 1, 0, 5, -1 }, { 2, 5, 1, -1, -1, -1, -1 }, { 4, 3, 1, 1, 3, 2, -1 },
  { 4, 0, 1, -1, -1, -1, -1 }, { 2, 0, 3, -1, -1, -1, -1 }, { -1, -1, -1, -1, -1, -1, -1 } };
//------------------------------------------------------------------------------
}

VTK_ABI_NAMESPACE_BEGIN
//------------------------------------------------------------------------------
const vtkMarchingCellsContourCases::LineCase* vtkMarchingCellsContourCases::GetLineCases()
{
  return LineCases;
}

//------------------------------------------------------------------------------
const vtkMarchingCellsContourCases::TriangleCase* vtkMarchingCellsContourCases::GetTriangleCases()
{
  return TriangleCases;
}

//------------------------------------------------------------------------------
const vtkMarchingCellsContourCases::PixelCase* vtkMarchingCellsContourCases::GetPixelCases()
{
  return PixelCases;
}

//------------------------------------------------------------------------------
const vtkMarchingCellsContourCases::QuadCase* vtkMarchingCellsContourCases::GetQuadCases()
{
  return QuadCases;
}

//------------------------------------------------------------------------------
const vtkMarchingCellsContourCases::TetraCase* vtkMarchingCellsContourCases::GetTetraCases()
{
  return TetraCases;
}
VTK_ABI_NAMESPACE_END
