// SPDX-FileCopyrightText: Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
// SPDX-License-Identifier: BSD-3-Clause
#include "vtkSubdivisionFilter.h"

#include "vtkCell.h"
#include "vtkCellArray.h"
#include "vtkCellData.h"
#include "vtkCellIterator.h"
#include "vtkEdgeTable.h"
#include "vtkIdList.h"
#include "vtkInformation.h"
#include "vtkInformationVector.h"
#include "vtkPointData.h"
#include "vtkPolyData.h"
#include "vtkStreamingDemandDrivenPipeline.h"
#include "vtkUnsignedCharArray.h"

#include <map>
#include <sstream>

// Construct object with number of subdivisions set to 1, check for
// triangles set to 1
VTK_ABI_NAMESPACE_BEGIN
vtkSubdivisionFilter::vtkSubdivisionFilter()
{
  this->NumberOfSubdivisions = 1;
  this->CheckForTriangles = 1;
}

int vtkSubdivisionFilter::RequestData(vtkInformation* vtkNotUsed(request),
  vtkInformationVector** inputVector, vtkInformationVector* vtkNotUsed(outputVector))
{
  // validate the input
  vtkInformation* inInfo = inputVector[0]->GetInformationObject(0);

  // get the input
  vtkPolyData* input = vtkPolyData::SafeDownCast(inInfo->Get(vtkDataObject::DATA_OBJECT()));
  if (!input)
  {
    return 0;
  }

  vtkIdType numCells, numPts;
  numPts = input->GetNumberOfPoints();
  numCells = input->GetNumberOfCells();

  if (numPts == 0 || numCells == 0)
  {
    vtkDebugMacro(<< "No data to subdivide");
    return 1;
  }

  if (this->CheckForTriangles)
  {
    std::map<int, int> badCellTypes;
    bool hasOnlyTris = true;
    vtkCellIterator* it = input->NewCellIterator();
    for (it->InitTraversal(); !it->IsDoneWithTraversal(); it->GoToNextCell())
    {
      if (this->CheckAbort())
      {
        break;
      }
      if (it->GetCellType() != VTK_TRIANGLE)
      {
        hasOnlyTris = false;
        badCellTypes[it->GetCellType()] += 1;
        continue;
      }
    }
    it->Delete();

    if (!hasOnlyTris)
    {
      std::ostringstream msg;
      std::map<int, int>::iterator cit;
      for (cit = badCellTypes.begin(); cit != badCellTypes.end(); ++cit)
      {
        msg << "Cell type: " << cit->first << " Count: " << cit->second << "\n";
      }
      vtkErrorMacro(<< this->GetClassName()
                    << " only operates on triangles, but "
                       "this data set has other cell types present.\n"
                    << msg.str());
      return 0;
    }
  }
  return 1;
}
void vtkSubdivisionFilter::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);

  os << indent << "Number of subdivisions: " << this->GetNumberOfSubdivisions() << endl;
  os << indent << "Check for triangles: " << this->GetCheckForTriangles() << endl;
}
VTK_ABI_NAMESPACE_END
