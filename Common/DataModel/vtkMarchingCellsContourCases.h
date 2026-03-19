// SPDX-FileCopyrightText: Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
// SPDX-License-Identifier: BSD-3-Clause

#ifndef vtkMarchingCellsContourCases_h
#define vtkMarchingCellsContourCases_h

#include "vtkCommonDataModelModule.h"

#include <cstdint> // For uint8_t

/**
 * The marching cells contour cases are used to determine how to contour a cell.
 */
VTK_ABI_NAMESPACE_BEGIN
class VTKCOMMONDATAMODEL_EXPORT vtkMarchingCellsContourCases
{
public:
  using LineCase = int[2];
  static const LineCase* GetLineCases();
  static const LineCase& GetLineCase(uint8_t caseIndex) { return GetLineCases()[caseIndex]; }
};
VTK_ABI_NAMESPACE_END

#endif // vtkMarchingCellsContourCases_h
