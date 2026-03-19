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

  using TriangleCase = int[3];
  static const TriangleCase* GetTriangleCases();
  static const TriangleCase& GetTriangleCase(uint8_t caseIndex)
  {
    return GetTriangleCases()[caseIndex];
  }

  using PixelCase = int[5];
  static const PixelCase* GetPixelCases();
  static const PixelCase& GetPixelCase(uint8_t caseIndex) { return GetPixelCases()[caseIndex]; }

  using QuadCase = int[5];
  static const QuadCase* GetQuadCases();
  static const QuadCase& GetQuadCase(uint8_t caseIndex) { return GetQuadCases()[caseIndex]; }

  using TetraCase = int[7];
  static const TetraCase* GetTetraCases();
  static const TetraCase& GetTetraCase(uint8_t caseIndex) { return GetTetraCases()[caseIndex]; }
};
VTK_ABI_NAMESPACE_END

#endif // vtkMarchingCellsContourCases_h
