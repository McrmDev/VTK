// SPDX-FileCopyrightText: Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
// SPDX-License-Identifier: BSD-3-Clause
#include "vtkUniformGrid.h"
#include <vtkAMRGaussianPulseSource.h>
#include <vtkAMRInterpolatedVelocityField.h>
#include <vtkCompositeDataPipeline.h>
#include <vtkGradientFilter.h>
#include <vtkMath.h>
#include <vtkNew.h>
#include <vtkOverlappingAMR.h>
#define RETURNONFALSE(b)                                                                           \
  do                                                                                               \
  {                                                                                                \
    if (!(b))                                                                                      \
    {                                                                                              \
      vtkAlgorithm::SetDefaultExecutivePrototype(nullptr);                                         \
      return EXIT_FAILURE;                                                                         \
    }                                                                                              \
  } while (false)

int TestAMRInterpolatedVelocityField(int, char*[])
{
  vtkNew<vtkCompositeDataPipeline> cexec;
  vtkAlgorithm::SetDefaultExecutivePrototype(cexec);

  char name[100] = "Gaussian-Pulse";
  vtkNew<vtkAMRGaussianPulseSource> imageSource;
  vtkNew<vtkGradientFilter> gradientFilter;
  gradientFilter->SetInputConnection(imageSource->GetOutputPort());
  gradientFilter->SetInputScalars(vtkDataObject::FIELD_ASSOCIATION_CELLS, name);
  gradientFilter->SetResultArrayName("Gradient");
  gradientFilter->Update();

  vtkOverlappingAMR* amrGrad =
    vtkOverlappingAMR::SafeDownCast(gradientFilter->GetOutputDataObject(0));
  amrGrad->GenerateParentChildInformation();
  for (unsigned int datasetLevel = 0; datasetLevel < amrGrad->GetNumberOfLevels(); datasetLevel++)
  {
    for (unsigned int id = 0; id < amrGrad->GetNumberOfBlocks(datasetLevel); id++)
    {
      vtkUniformGrid* grid = amrGrad->GetDataSet(datasetLevel, id);
      int numBlankedCells(0);
      for (int i = 0; i < grid->GetNumberOfCells(); i++)
      {
        numBlankedCells += grid->IsCellVisible(i) ? 0 : 1;
      }
      cout << numBlankedCells << " ";
    }
  }
  cout << endl;

  vtkNew<vtkAMRInterpolatedVelocityField> func;
  func->SetAMRData(amrGrad);
  func->SelectVectors(vtkDataObject::FIELD_ASSOCIATION_CELLS, "Gradient");

  double Points[4][3] = {
    { -2.1, -0.51, 1 },
    { -1.9, -0.51, 1 },
    { -0.9, -0.51, 1 },
    { -0.1, -0.51, 1 },
  };

  double v[3];
  bool res;
  unsigned int level, id;
  res = func->FunctionValues(Points[0], v) != 0;
  RETURNONFALSE(!res);
  res = func->FunctionValues(Points[1], v) != 0;
  RETURNONFALSE(res);
  func->GetLastDataSetLocation(level, id);
  RETURNONFALSE(level == 1);
  res = func->FunctionValues(Points[2], v) != 0;
  RETURNONFALSE(res);
  func->GetLastDataSetLocation(level, id);
  RETURNONFALSE(level == 0);
  res = func->FunctionValues(Points[3], v) != 0;
  RETURNONFALSE(res);
  func->GetLastDataSetLocation(level, id);
  RETURNONFALSE(level == 1);

  vtkAlgorithm::SetDefaultExecutivePrototype(nullptr);
  return EXIT_SUCCESS;
}
