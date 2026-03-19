// SPDX-FileCopyrightText: Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
// SPDX-License-Identifier: BSD-3-Clause

#include "vtkMarchingCellsContourCases.h"

namespace
{
//------------------------------------------------------------------------------
// Marching lines case table
constexpr vtkMarchingCellsContourCases::LineCase LineCases[4] = { { -1, -1 }, { 1, 0 }, { 0, 1 },
  { -1, -1 } };
}

VTK_ABI_NAMESPACE_BEGIN
//------------------------------------------------------------------------------
const vtkMarchingCellsContourCases::LineCase* vtkMarchingCellsContourCases::GetLineCases()
{
  return LineCases;
}
VTK_ABI_NAMESPACE_END
