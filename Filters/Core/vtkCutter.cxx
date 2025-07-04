// SPDX-FileCopyrightText: Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
// SPDX-License-Identifier: BSD-3-Clause
#include "vtkCutter.h"

#include "vtk3DLinearGridPlaneCutter.h"
#include "vtkAppendDataSets.h"
#include "vtkCellArray.h"
#include "vtkCellData.h"
#include "vtkCellIterator.h"
#include "vtkCellTypes.h"
#include "vtkContourHelper.h"
#include "vtkDataSet.h"
#include "vtkDoubleArray.h"
#include "vtkEventForwarderCommand.h"
#include "vtkFloatArray.h"
#include "vtkGarbageCollector.h"
#include "vtkGenericCell.h"
#include "vtkGridSynchronizedTemplates3D.h"
#include "vtkImageData.h"
#include "vtkImplicitFunction.h"
#include "vtkIncrementalPointLocator.h"
#include "vtkInformation.h"
#include "vtkInformationVector.h"
#include "vtkMergePoints.h"
#include "vtkNew.h"
#include "vtkObjectFactory.h"
#include "vtkPlane.h"
#include "vtkPlaneCutter.h"
#include "vtkPointData.h"
#include "vtkPolyData.h"
#include "vtkRectilinearGrid.h"
#include "vtkRectilinearSynchronizedTemplates.h"
#include "vtkSmartPointer.h"
#include "vtkStreamingDemandDrivenPipeline.h"
#include "vtkStructuredGrid.h"
#include "vtkSynchronizedTemplates3D.h"
#include "vtkSynchronizedTemplatesCutter3D.h"
#include "vtkUnstructuredGridBase.h"

#include <algorithm>
#include <cmath>

VTK_ABI_NAMESPACE_BEGIN
vtkObjectFactoryNewMacro(vtkCutter);
vtkCxxSetObjectMacro(vtkCutter, CutFunction, vtkImplicitFunction);
vtkCxxSetObjectMacro(vtkCutter, Locator, vtkIncrementalPointLocator);

//------------------------------------------------------------------------------
// Construct with user-specified implicit function; initial value of 0.0; and
// generating cut scalars turned off.
vtkCutter::vtkCutter(vtkImplicitFunction* cf)
{
  this->SortBy = VTK_SORT_BY_VALUE;
  this->CutFunction = cf;
  this->GenerateCutScalars = 0;
  this->Locator = nullptr;
  this->GenerateTriangles = 1;
  this->OutputPointsPrecision = DEFAULT_PRECISION;

  this->PlaneCutter->SetContainerAlgorithm(this);
  this->SynchronizedTemplates3D->SetContainerAlgorithm(this);
  this->SynchronizedTemplatesCutter3D->SetContainerAlgorithm(this);
  this->GridSynchronizedTemplates->SetContainerAlgorithm(this);
  this->RectilinearSynchronizedTemplates->SetContainerAlgorithm(this);
}

//------------------------------------------------------------------------------
vtkCutter::~vtkCutter()
{
  this->SetCutFunction(nullptr);
  this->SetLocator(nullptr);
}

//------------------------------------------------------------------------------
// Overload standard modified time function. If cut functions is modified,
// or contour values modified, then this object is modified as well.
vtkMTimeType vtkCutter::GetMTime()
{
  vtkMTimeType mTime = this->Superclass::GetMTime();
  vtkMTimeType contourValuesMTime = this->ContourValues->GetMTime();
  vtkMTimeType time;

  mTime = (contourValuesMTime > mTime ? contourValuesMTime : mTime);

  if (this->CutFunction != nullptr)
  {
    time = this->CutFunction->GetMTime();
    mTime = (time > mTime ? time : mTime);
  }

  return mTime;
}

//------------------------------------------------------------------------------
void vtkCutter::StructuredPointsCutter(vtkDataSet* dataSetInput, vtkPolyData* thisOutput,
  vtkInformation* request, vtkInformationVector** inputVector, vtkInformationVector* outputVector)
{
  vtkImageData* input = vtkImageData::SafeDownCast(dataSetInput);
  vtkPolyData* output;
  vtkIdType numPts = input->GetNumberOfPoints();

  if (numPts < 1)
  {
    return;
  }

  vtkIdType numContours = this->GetNumberOfContours();

  // for one contour we use the SyncTempCutter which is faster and has a
  // smaller memory footprint
  if (numContours == 1)
  {
    this->SynchronizedTemplatesCutter3D->SetCutFunction(this->CutFunction);
    this->SynchronizedTemplatesCutter3D->SetValue(0, this->GetValue(0));
    this->SynchronizedTemplatesCutter3D->SetGenerateTriangles(this->GetGenerateTriangles());
    this->SynchronizedTemplatesCutter3D->ProcessRequest(request, inputVector, outputVector);
    return;
  }

  // otherwise compute scalar data then contour
  vtkFloatArray* cutScalars = vtkFloatArray::New();
  cutScalars->SetNumberOfTuples(numPts);
  cutScalars->SetName("cutScalars");

  vtkImageData* contourData = vtkImageData::New();
  contourData->ShallowCopy(input);
  if (this->GenerateCutScalars)
  {
    contourData->GetPointData()->SetScalars(cutScalars);
  }
  else
  {
    contourData->GetPointData()->AddArray(cutScalars);
  }

  double scalar;
  double x[3];
  for (vtkIdType i = 0; i < numPts; i++)
  {
    input->GetPoint(i, x);
    scalar = this->CutFunction->FunctionValue(x);
    cutScalars->SetComponent(i, 0, scalar);
  }

  this->SynchronizedTemplates3D->SetInputData(contourData);
  this->SynchronizedTemplates3D->SetInputArrayToProcess(
    0, 0, 0, vtkDataObject::FIELD_ASSOCIATION_POINTS, "cutScalars");
  this->SynchronizedTemplates3D->SetNumberOfContours(numContours);
  for (int i = 0; i < numContours; i++)
  {
    this->SynchronizedTemplates3D->SetValue(i, this->GetValue(i));
  }
  this->SynchronizedTemplates3D->ComputeScalarsOff();
  this->SynchronizedTemplates3D->ComputeNormalsOff();
  output = this->SynchronizedTemplates3D->GetOutput();
  this->SynchronizedTemplates3D->SetGenerateTriangles(this->GetGenerateTriangles());
  this->SynchronizedTemplates3D->Update();
  output->Register(this);

  thisOutput->CopyStructure(output);
  thisOutput->GetPointData()->ShallowCopy(output->GetPointData());
  thisOutput->GetCellData()->ShallowCopy(output->GetCellData());
  output->UnRegister(this);

  cutScalars->Delete();
  contourData->Delete();
}

//------------------------------------------------------------------------------
void vtkCutter::StructuredGridCutter(vtkDataSet* dataSetInput, vtkPolyData* thisOutput)
{
  vtkStructuredGrid* input = vtkStructuredGrid::SafeDownCast(dataSetInput);
  vtkPolyData* output;
  vtkIdType numPts = input->GetNumberOfPoints();

  if (numPts < 1)
  {
    return;
  }

  vtkFloatArray* cutScalars = vtkFloatArray::New();
  cutScalars->SetName("cutScalars");
  cutScalars->SetNumberOfTuples(numPts);

  vtkStructuredGrid* contourData = vtkStructuredGrid::New();
  contourData->ShallowCopy(input);
  if (this->GenerateCutScalars)
  {
    contourData->GetPointData()->SetScalars(cutScalars);
  }
  else
  {
    contourData->GetPointData()->AddArray(cutScalars);
  }

  vtkDataArray* dataArrayInput = input->GetPoints()->GetData();
  this->CutFunction->FunctionValue(dataArrayInput, cutScalars);
  vtkIdType numContours = this->GetNumberOfContours();

  this->GridSynchronizedTemplates->SetDebug(this->GetDebug());
  this->GridSynchronizedTemplates->SetOutputPointsPrecision(this->OutputPointsPrecision);
  this->GridSynchronizedTemplates->SetInputData(contourData);
  this->GridSynchronizedTemplates->SetInputArrayToProcess(
    0, 0, 0, vtkDataObject::FIELD_ASSOCIATION_POINTS, "cutScalars");
  this->GridSynchronizedTemplates->SetNumberOfContours(numContours);
  for (int i = 0; i < numContours; i++)
  {
    this->GridSynchronizedTemplates->SetValue(i, this->GetValue(i));
  }
  this->GridSynchronizedTemplates->ComputeScalarsOff();
  this->GridSynchronizedTemplates->ComputeNormalsOff();
  this->GridSynchronizedTemplates->SetGenerateTriangles(this->GetGenerateTriangles());
  output = this->GridSynchronizedTemplates->GetOutput();
  this->GridSynchronizedTemplates->Update();
  output->Register(this);

  thisOutput->ShallowCopy(output);
  output->UnRegister(this);

  cutScalars->Delete();
  contourData->Delete();
}

//------------------------------------------------------------------------------
void vtkCutter::RectilinearGridCutter(vtkDataSet* dataSetInput, vtkPolyData* thisOutput)
{
  vtkRectilinearGrid* input = vtkRectilinearGrid::SafeDownCast(dataSetInput);
  vtkPolyData* output;
  vtkIdType numPts = input->GetNumberOfPoints();

  if (numPts < 1)
  {
    return;
  }

  vtkFloatArray* cutScalars = vtkFloatArray::New();
  cutScalars->SetNumberOfTuples(numPts);
  cutScalars->SetName("cutScalars");

  vtkRectilinearGrid* contourData = vtkRectilinearGrid::New();
  contourData->ShallowCopy(input);
  if (this->GenerateCutScalars)
  {
    contourData->GetPointData()->SetScalars(cutScalars);
  }
  else
  {
    contourData->GetPointData()->AddArray(cutScalars);
  }

  for (vtkIdType i = 0; i < numPts; i++)
  {
    double x[3];
    input->GetPoint(i, x);
    double scalar = this->CutFunction->FunctionValue(x);
    cutScalars->SetComponent(i, 0, scalar);
  }
  vtkIdType numContours = this->GetNumberOfContours();

  this->RectilinearSynchronizedTemplates->SetInputData(contourData);
  this->RectilinearSynchronizedTemplates->SetInputArrayToProcess(
    0, 0, 0, vtkDataObject::FIELD_ASSOCIATION_POINTS, "cutScalars");
  this->RectilinearSynchronizedTemplates->SetNumberOfContours(numContours);
  for (int i = 0; i < numContours; i++)
  {
    this->RectilinearSynchronizedTemplates->SetValue(i, this->GetValue(i));
  }
  this->RectilinearSynchronizedTemplates->ComputeScalarsOff();
  this->RectilinearSynchronizedTemplates->ComputeNormalsOff();
  this->RectilinearSynchronizedTemplates->SetGenerateTriangles(this->GenerateTriangles);
  output = this->RectilinearSynchronizedTemplates->GetOutput();
  this->RectilinearSynchronizedTemplates->Update();
  output->Register(this);

  thisOutput->ShallowCopy(output);
  output->UnRegister(this);

  cutScalars->Delete();
  contourData->Delete();
}

//------------------------------------------------------------------------------
// Cut through data generating surface.
//
int vtkCutter::RequestData(
  vtkInformation* request, vtkInformationVector** inputVector, vtkInformationVector* outputVector)
{
  // get the info objects
  vtkInformation* inInfo = inputVector[0]->GetInformationObject(0);
  vtkInformation* outInfo = outputVector->GetInformationObject(0);

  // get the input and output
  vtkDataSet* input = vtkDataSet::SafeDownCast(inInfo->Get(vtkDataObject::DATA_OBJECT()));
  vtkPolyData* output = vtkPolyData::SafeDownCast(outInfo->Get(vtkDataObject::DATA_OBJECT()));

  vtkDebugMacro(<< "Executing cutter");
  if (!this->CutFunction)
  {
    vtkErrorMacro("No cut function specified");
    return 0;
  }

  if (!input)
  {
    // this could be a table in a multiblock structure, i.e. no cut!
    return 0;
  }

  if (input->GetNumberOfPoints() < 1 || this->GetNumberOfContours() < 1)
  {
    return 1;
  }

  vtkPlane* plane = vtkPlane::SafeDownCast(this->CutFunction);
  auto executePlaneCutter = [&]()
  {
    if (this->Locator == nullptr)
    {
      this->CreateDefaultLocator();
    }

    vtkSmartPointer<vtkAppendDataSets> append;
    if (this->GetNumberOfContours() > 1)
    {
      append = vtkSmartPointer<vtkAppendDataSets>::New();
      append->SetContainerAlgorithm(this);
      append->SetOutputPointsPrecision(this->GetOutputPointsPrecision());
      append->MergePointsOff();
      append->SetOutputDataSetType(VTK_POLY_DATA);
    }
    for (vtkIdType i = 0; i < this->GetNumberOfContours(); ++i)
    {
      // Create a copy of vtkPlane and nudge it by the single contour
      vtkNew<vtkPlane> newPlane;
      newPlane->DeepCopy(plane);
      // Evaluate the distance the origin is from the original plane. This accommodates
      // subclasses of vtkPlane that may have an additional offset parameter not
      // accessible through the vtkPlane interface. Use this distance to adjust the origin
      // in newPlane.
      double d = plane->EvaluateFunction(plane->GetOrigin());
      // In addition. We'll need to shift by the contour value.
      newPlane->Push(-d + this->GetValue(i));

      this->PlaneCutter->SetInputData(input);
      this->PlaneCutter->SetPlane(newPlane);
      bool mergePoints =
        this->GetLocator() && !this->GetLocator()->IsA("vtkNonMergingPointLocator");
      this->PlaneCutter->SetMergePoints(mergePoints);
      this->PlaneCutter->SetOutputPointsPrecision(this->GetOutputPointsPrecision());
      this->PlaneCutter->SetGeneratePolygons(!this->GetGenerateTriangles());
      this->PlaneCutter->SetInputArrayToProcess(0, this->GetInputArrayInformation(0));
      this->PlaneCutter->BuildTreeOff();
      this->PlaneCutter->ComputeNormalsOff();
      this->PlaneCutter->Update();
      if (this->GetNumberOfContours() > 1)
      {
        vtkNew<vtkPolyData> pd;
        pd->ShallowCopy(this->PlaneCutter->GetOutput());
        append->AddInputData(pd);
      }
    }
    if (this->GetNumberOfContours() > 1)
    {
      append->Update();
      output->ShallowCopy(append->GetOutput());
    }
    else
    {
      output->ShallowCopy(this->PlaneCutter->GetOutput());
    }
  };
  if (vtkImageData::SafeDownCast(input) &&
    static_cast<vtkImageData*>(input)->GetDataDimension() == 3)
  {
    if (plane && this->GetGenerateCutScalars() == 0)
    {
      executePlaneCutter();
    }
    else
    {
      if (input->GetDataObjectType() == VTK_UNIFORM_GRID)
      {
        this->DataSetCutter(input, output);
      }
      else
      {
        this->StructuredPointsCutter(input, output, request, inputVector, outputVector);
      }
    }
  }
  else if (vtkStructuredGrid::SafeDownCast(input) &&
    static_cast<vtkStructuredGrid*>(input)->GetDataDimension() == 3)
  {
    if (plane && this->GetGenerateCutScalars() == 0)
    {
      executePlaneCutter();
    }
    else
    {
      this->StructuredGridCutter(input, output);
    }
  }
  else if (vtkRectilinearGrid::SafeDownCast(input) &&
    static_cast<vtkRectilinearGrid*>(input)->GetDataDimension() == 3)
  {
    if (plane && this->GetGenerateCutScalars() == 0)
    {
      executePlaneCutter();
    }
    else
    {
      this->RectilinearGridCutter(input, output);
    }
  }
  else if (vtkUnstructuredGridBase::SafeDownCast(input))
  {
    if (plane && this->GetGenerateCutScalars() == 0 && this->GetGenerateTriangles() == 1)
    {
      executePlaneCutter();
    }
    else
    {
      this->UnstructuredGridCutter(input, output);
    }
  }
  else if (vtkPolyData::SafeDownCast(input))
  {
    if (plane && this->GetGenerateCutScalars() == 0 && this->GetGenerateTriangles() == 1)
    {
      executePlaneCutter();
    }
    else
    {
      this->DataSetCutter(input, output);
    }
  }
  else
  {
    this->DataSetCutter(input, output);
  }

  return 1;
}

//------------------------------------------------------------------------------
void vtkCutter::DataSetCutter(vtkDataSet* input, vtkPolyData* output)
{
  vtkIdType cellId;
  int iter;
  vtkPoints* cellPts;
  vtkDoubleArray* cellScalars;
  vtkGenericCell* cell;
  vtkCellArray *newVerts, *newLines, *newPolys;
  vtkPoints* newPoints;
  vtkDoubleArray* cutScalars;
  double value;
  vtkIdType estimatedSize, numCells = input->GetNumberOfCells();
  vtkIdType numPts = input->GetNumberOfPoints();
  vtkPointData *inPD, *outPD;
  vtkCellData *inCD = input->GetCellData(), *outCD = output->GetCellData();
  vtkIdList* cellIds;
  vtkIdType numContours = this->ContourValues->GetNumberOfContours();
  bool abortExecute = false;

  cellScalars = vtkDoubleArray::New();

  // Create objects to hold output of contour operation
  //
  estimatedSize = static_cast<vtkIdType>(pow(static_cast<double>(numCells), .75)) * numContours;
  estimatedSize = estimatedSize / 1024 * 1024; // multiple of 1024
  if (estimatedSize < 1024)
  {
    estimatedSize = 1024;
  }

  newPoints = vtkPoints::New();
  // set precision for the points in the output
  if (this->OutputPointsPrecision == vtkAlgorithm::DEFAULT_PRECISION)
  {
    vtkPointSet* inputPointSet = vtkPointSet::SafeDownCast(input);
    if (inputPointSet)
    {
      newPoints->SetDataType(inputPointSet->GetPoints()->GetDataType());
    }
    else
    {
      newPoints->SetDataType(VTK_FLOAT);
    }
  }
  else if (this->OutputPointsPrecision == vtkAlgorithm::SINGLE_PRECISION)
  {
    newPoints->SetDataType(VTK_FLOAT);
  }
  else if (this->OutputPointsPrecision == vtkAlgorithm::DOUBLE_PRECISION)
  {
    newPoints->SetDataType(VTK_DOUBLE);
  }
  newPoints->Allocate(estimatedSize, estimatedSize / 2);
  newVerts = vtkCellArray::New();
  newVerts->AllocateEstimate(estimatedSize, 1);
  newLines = vtkCellArray::New();
  newLines->AllocateEstimate(estimatedSize, 2);
  newPolys = vtkCellArray::New();
  newPolys->AllocateEstimate(estimatedSize, 4);
  cutScalars = vtkDoubleArray::New();
  cutScalars->SetNumberOfTuples(numPts);

  // Interpolate data along edge. If generating cut scalars, do necessary setup
  if (this->GenerateCutScalars)
  {
    inPD = vtkPointData::New();
    inPD->ShallowCopy(input->GetPointData()); // copies original attributes
    inPD->SetScalars(cutScalars);
  }
  else
  {
    inPD = input->GetPointData();
  }
  outPD = output->GetPointData();
  outPD->InterpolateAllocate(inPD, estimatedSize, estimatedSize / 2);
  outCD->CopyAllocate(inCD, estimatedSize, estimatedSize / 2);

  // locator used to merge potentially duplicate points
  if (this->Locator == nullptr)
  {
    this->CreateDefaultLocator();
  }
  this->Locator->InitPointInsertion(newPoints, input->GetBounds());

  // Loop over all points evaluating scalar function at each point
  //
  for (vtkIdType i = 0; i < numPts; ++i)
  {
    double x[3];
    input->GetPoint(i, x);
    double s = this->CutFunction->FunctionValue(x);
    cutScalars->SetComponent(i, 0, s);
  }

  // Compute some information for progress methods
  //
  cell = vtkGenericCell::New();
  vtkContourHelper helper(this->Locator, newVerts, newLines, newPolys, inPD, inCD, outPD, outCD,
    estimatedSize, this->GenerateTriangles != 0);
  if (this->SortBy == VTK_SORT_BY_CELL)
  {
    vtkIdType numCuts = numContours * numCells;
    vtkIdType progressInterval = numCuts / 20 + 1;
    int cut = 0;

    // Loop over all contour values.  Then for each contour value,
    // loop over all cells.
    //
    // This is going to have a problem if the input has 2D and 3D cells.
    // I am fixing a bug where cell data is scrambled because with
    // vtkPolyData output, verts and lines have lower cell ids than triangles.
    for (iter = 0; iter < numContours && !abortExecute; iter++)
    {
      // Loop over all cells; get scalar values for all cell points
      // and process each cell.
      //
      for (cellId = 0; cellId < numCells && !abortExecute; cellId++)
      {
        if (!(++cut % progressInterval))
        {
          vtkDebugMacro(<< "Cutting #" << cut);
          this->UpdateProgress(static_cast<double>(cut) / numCuts);
          abortExecute = this->CheckAbort();
        }

        input->GetCell(cellId, cell);
        cellPts = cell->GetPoints();
        cellIds = cell->GetPointIds();

        vtkIdType numCellPts = cellPts->GetNumberOfPoints();
        cellScalars->SetNumberOfTuples(numCellPts);
        for (vtkIdType i = 0; i < numCellPts; ++i)
        {
          double s = cutScalars->GetComponent(cellIds->GetId(i), 0);
          cellScalars->SetTuple(i, &s);
        }

        value = this->ContourValues->GetValue(iter);

        helper.Contour(cell, value, cellScalars, cellId);
      } // for all cells
    }   // for all contour values
  }     // sort by cell

  else // VTK_SORT_BY_VALUE:
  {
    // Three passes over the cells to process lower dimensional cells first.
    // For poly data output cells need to be added in the order:
    // verts, lines and then polys, or cell data gets mixed up.
    // A better solution is to have an unstructured grid output.
    // I create a table that maps cell type to cell dimensionality,
    // because I need a fast way to get cell dimensionality.
    // This assumes GetCell is slow and GetCellType is fast.
    // I do not like hard coding a list of cell types here,
    // but I do not want to add GetCellDimension(vtkIdType cellId)
    // to the vtkDataSet API.  Since I anticipate that the output
    // will change to vtkUnstructuredGrid.  This temporary solution
    // is acceptable.
    //
    unsigned char cellType;
    int dimensionality;

    vtkIdType progressInterval = numCells / 20 + 1;

    // We skip 0d cells (points), because they cannot be cut (generate no data).
    for (dimensionality = 1; dimensionality <= 3; ++dimensionality)
    {
      // Loop over all cells; get scalar values for all cell points
      // and process each cell.
      //
      for (cellId = 0; cellId < numCells && !abortExecute; cellId++)
      {
        if (!(cellId % progressInterval))
        {
          vtkDebugMacro(<< "Cutting #" << cellId);
          this->UpdateProgress(static_cast<double>(cellId) / numCells);
          abortExecute = this->CheckAbort();
        }

        // I assume that "GetCellType" is fast.
        cellType = static_cast<unsigned char>(input->GetCellType(cellId));
        if (vtkCellTypes::GetDimension(cellType) != dimensionality)
        {
          continue;
        }
        input->GetCell(cellId, cell);
        cellPts = cell->GetPoints();
        cellIds = cell->GetPointIds();

        vtkIdType numCellPts = cellPts->GetNumberOfPoints();
        cellScalars->SetNumberOfTuples(numCellPts);
        for (vtkIdType i = 0; i < numCellPts; i++)
        {
          double s = cutScalars->GetComponent(cellIds->GetId(i), 0);
          cellScalars->SetTuple(i, &s);
        }

        // Loop over all contour values.
        for (iter = 0; iter < numContours && !abortExecute; iter++)
        {
          value = this->ContourValues->GetValue(iter);
          helper.Contour(cell, value, cellScalars, cellId);
        } // for all contour values
      }   // for all cells
    }     // for all dimensions.
  }       // sort by value

  // Update ourselves.  Because we don't know upfront how many verts, lines,
  // polys we've created, take care to reclaim memory.
  //
  cell->Delete();
  cellScalars->Delete();
  cutScalars->Delete();

  if (this->GenerateCutScalars)
  {
    inPD->Delete();
  }

  output->SetPoints(newPoints);
  newPoints->Delete();

  if (newVerts->GetNumberOfCells())
  {
    output->SetVerts(newVerts);
  }
  newVerts->Delete();

  if (newLines->GetNumberOfCells())
  {
    output->SetLines(newLines);
  }
  newLines->Delete();

  if (newPolys->GetNumberOfCells())
  {
    output->SetPolys(newPolys);
  }
  newPolys->Delete();

  this->Locator->Initialize(); // release any extra memory
  output->Squeeze();
}

//------------------------------------------------------------------------------
void vtkCutter::UnstructuredGridCutter(vtkDataSet* input, vtkPolyData* output)
{
  vtkIdType i;
  int iter;
  vtkDoubleArray* cellScalars;
  vtkCellArray *newVerts, *newLines, *newPolys;
  vtkPoints* newPoints;
  vtkDoubleArray* cutScalars;
  double value;
  vtkIdType estimatedSize, numCells = input->GetNumberOfCells();
  vtkIdType numPts = input->GetNumberOfPoints();
  vtkIdType numCellPts;
  vtkIdType* ptIds;
  vtkPointData *inPD, *outPD;
  vtkCellData *inCD = input->GetCellData(), *outCD = output->GetCellData();
  vtkIdList* cellIds;
  vtkIdType numContours = this->ContourValues->GetNumberOfContours();
  double* contourValues = this->ContourValues->GetValues();
  double* contourValuesEnd = contourValues + numContours;
  double* contourIter;

  bool abortExecute = false;

  double range[2];

  // Create objects to hold output of contour operation
  //
  estimatedSize = static_cast<vtkIdType>(pow(static_cast<double>(numCells), .75)) * numContours;
  estimatedSize = estimatedSize / 1024 * 1024; // multiple of 1024
  if (estimatedSize < 1024)
  {
    estimatedSize = 1024;
  }

  newPoints = vtkPoints::New();
  vtkPointSet* inputPointSet = vtkPointSet::SafeDownCast(input);
  // set precision for the points in the output
  if (this->OutputPointsPrecision == vtkAlgorithm::DEFAULT_PRECISION)
  {
    if (inputPointSet)
    {
      newPoints->SetDataType(inputPointSet->GetPoints()->GetDataType());
    }
    else
    {
      newPoints->SetDataType(VTK_FLOAT);
    }
  }
  else if (this->OutputPointsPrecision == vtkAlgorithm::SINGLE_PRECISION)
  {
    newPoints->SetDataType(VTK_FLOAT);
  }
  else if (this->OutputPointsPrecision == vtkAlgorithm::DOUBLE_PRECISION)
  {
    newPoints->SetDataType(VTK_DOUBLE);
  }
  newPoints->Allocate(estimatedSize, estimatedSize / 2);
  newVerts = vtkCellArray::New();
  newVerts->AllocateEstimate(estimatedSize, 1);
  newLines = vtkCellArray::New();
  newLines->AllocateEstimate(estimatedSize, 2);
  newPolys = vtkCellArray::New();
  newPolys->AllocateEstimate(estimatedSize, 4);
  cutScalars = vtkDoubleArray::New();
  cutScalars->SetNumberOfTuples(numPts);

  // Interpolate data along edge. If generating cut scalars, do necessary setup
  if (this->GenerateCutScalars)
  {
    inPD = vtkPointData::New();
    inPD->ShallowCopy(input->GetPointData()); // copies original attributes
    inPD->SetScalars(cutScalars);
  }
  else
  {
    inPD = input->GetPointData();
  }
  outPD = output->GetPointData();
  outPD->InterpolateAllocate(inPD, estimatedSize, estimatedSize / 2);
  outCD->CopyAllocate(inCD, estimatedSize, estimatedSize / 2);

  // locator used to merge potentially duplicate points
  if (this->Locator == nullptr)
  {
    this->CreateDefaultLocator();
  }
  this->Locator->InitPointInsertion(newPoints, input->GetBounds());

  // Loop over all points evaluating scalar function at each point
  if (inputPointSet)
  {
    vtkDataArray* dataArrayInput = inputPointSet->GetPoints()->GetData();
    this->CutFunction->FunctionValue(dataArrayInput, cutScalars);
  }

  vtkSmartPointer<vtkCellIterator> cellIter =
    vtkSmartPointer<vtkCellIterator>::Take(input->NewCellIterator());
  vtkNew<vtkGenericCell> cell;
  vtkIdList* pointIdList;
  double* scalarArrayPtr = cutScalars->GetPointer(0);
  double tempScalar;
  cellScalars = cutScalars->NewInstance();
  cellScalars->SetNumberOfComponents(cutScalars->GetNumberOfComponents());
  int maxCellSize = input->GetMaxCellSize();
  cellScalars->Allocate(maxCellSize * cutScalars->GetNumberOfComponents());

  vtkContourHelper helper(this->Locator, newVerts, newLines, newPolys, inPD, inCD, outPD, outCD,
    estimatedSize, this->GenerateTriangles != 0);
  if (this->SortBy == VTK_SORT_BY_CELL)
  {
    // Compute some information for progress methods
    //
    vtkIdType numCuts = numContours * numCells;
    vtkIdType progressInterval = numCuts / 20 + 1;
    int cut = 0;

    // Loop over all contour values.  Then for each contour value,
    // loop over all cells.
    //
    for (iter = 0; iter < numContours && !abortExecute; iter++)
    {
      // Loop over all cells; get scalar values for all cell points
      // and process each cell.
      //
      for (cellIter->InitTraversal(); !cellIter->IsDoneWithTraversal() && !abortExecute;
           cellIter->GoToNextCell())
      {
        if (!(++cut % progressInterval))
        {
          vtkDebugMacro(<< "Cutting #" << cut);
          this->UpdateProgress(static_cast<double>(cut) / numCuts);
          abortExecute = this->CheckAbort();
        }

        pointIdList = cellIter->GetPointIds();
        numCellPts = pointIdList->GetNumberOfIds();
        ptIds = pointIdList->GetPointer(0);

        // find min and max values in scalar data
        range[0] = range[1] = scalarArrayPtr[ptIds[0]];

        for (i = 1; i < numCellPts; i++)
        {
          tempScalar = scalarArrayPtr[ptIds[i]];
          range[0] = std::min(range[0], tempScalar);
          range[1] = std::max(range[1], tempScalar);
        } // for all points in this cell

        int needCell = 0;
        double val = this->ContourValues->GetValue(iter);
        if (val >= range[0] && val <= range[1])
        {
          needCell = 1;
        }

        if (needCell)
        {
          cellIter->GetCell(cell);
          vtkIdType cellId = cellIter->GetCellId();
          input->SetCellOrderAndRationalWeights(cellId, cell);
          cellIds = cell->GetPointIds();
          cellScalars->SetNumberOfTuples(numCellPts);
          cutScalars->GetTuples(cellIds, cellScalars);
          // Loop over all contour values.
          for (iter = 0; iter < numContours && !abortExecute; iter++)
          {
            value = this->ContourValues->GetValue(iter);
            helper.Contour(cell, value, cellScalars, cellIter->GetCellId());
          }
        }

      } // for all cells
    }   // for all contour values
  }     // sort by cell

  else // SORT_BY_VALUE:
  {
    // Three passes over the cells to process lower dimensional cells first.
    // For poly data output cells need to be added in the order:
    // verts, lines and then polys, or cell data gets mixed up.
    // A better solution is to have an unstructured grid output.
    // I create a table that maps cell type to cell dimensionality,
    // because I need a fast way to get cell dimensionality.
    // This assumes GetCell is slow and GetCellType is fast.
    // I do not like hard coding a list of cell types here,
    // but I do not want to add GetCellDimension(vtkIdType cellId)
    // to the vtkDataSet API.  Since I anticipate that the output
    // will change to vtkUnstructuredGrid.  This temporary solution
    // is acceptable.
    //
    unsigned char cellType;
    int dimensionality;

    // Compute some information for progress methods
    vtkIdType numCuts = 3 * numCells;
    vtkIdType progressInterval = numCuts / 20 + 1;
    int cellId = 0;

    // We skip 0d cells (points), because they cannot be cut (generate no data).
    for (dimensionality = 1; dimensionality <= 3; ++dimensionality)
    {
      // Loop over all cells; get scalar values for all cell points
      // and process each cell.
      //
      for (cellIter->InitTraversal(); !cellIter->IsDoneWithTraversal() && !abortExecute;
           cellIter->GoToNextCell())
      {
        if (!(++cellId % progressInterval))
        {
          vtkDebugMacro(<< "Cutting #" << cellId);
          this->UpdateProgress(static_cast<double>(cellId) / numCuts);
          abortExecute = this->CheckAbort();
        }

        // Just fetch the cell type -- least expensive.
        cellType = static_cast<unsigned char>(cellIter->GetCellType());
        // Check if the type is valid for this pass
        if (vtkCellTypes::GetDimension(cellType) != dimensionality)
        {
          continue;
        }

        // Just fetch the cell point ids -- moderately expensive.
        pointIdList = cellIter->GetPointIds();
        numCellPts = pointIdList->GetNumberOfIds();
        ptIds = pointIdList->GetPointer(0);

        // find min and max values in scalar data
        range[0] = range[1] = scalarArrayPtr[ptIds[0]];

        for (i = 1; i < numCellPts; ++i)
        {
          tempScalar = scalarArrayPtr[ptIds[i]];
          range[0] = std::min(range[0], tempScalar);
          range[1] = std::max(range[1], tempScalar);
        } // for all points in this cell

        // Check if the full cell is needed
        int needCell = 0;
        for (contourIter = contourValues; contourIter != contourValuesEnd; ++contourIter)
        {
          if (*contourIter >= range[0] && *contourIter <= range[1])
          {
            needCell = 1;
            break;
          }
        }

        if (needCell)
        {
          // Fetch the full cell -- most expensive.
          cellIter->GetCell(cell);
          input->SetCellOrderAndRationalWeights(cellId, cell);
          cellScalars->SetNumberOfTuples(numCellPts);
          cutScalars->GetTuples(pointIdList, cellScalars);
          // Loop over all contour values.
          for (contourIter = contourValues; contourIter != contourValuesEnd; ++contourIter)
          {
            helper.Contour(cell, *contourIter, cellScalars, cellIter->GetCellId());
          } // for all contour values
        }   // if need cell
      }     // for all cells
    }       // for all dimensions (1,2,3).
  }         // sort by value

  // Update ourselves.  Because we don't know upfront how many verts, lines,
  // polys we've created, take care to reclaim memory.
  //
  cellScalars->Delete();
  cutScalars->Delete();

  if (this->GenerateCutScalars)
  {
    inPD->Delete();
  }

  output->SetPoints(newPoints);
  newPoints->Delete();

  if (newVerts->GetNumberOfCells())
  {
    output->SetVerts(newVerts);
  }
  newVerts->Delete();

  if (newLines->GetNumberOfCells())
  {
    output->SetLines(newLines);
  }
  newLines->Delete();

  if (newPolys->GetNumberOfCells())
  {
    output->SetPolys(newPolys);
  }
  newPolys->Delete();

  this->Locator->Initialize(); // release any extra memory
  output->Squeeze();
}

//------------------------------------------------------------------------------
// Specify a spatial locator for merging points. By default,
// an instance of vtkMergePoints is used.
void vtkCutter::CreateDefaultLocator()
{
  if (this->Locator == nullptr)
  {
    this->Locator = vtkMergePoints::New();
    this->Locator->Register(this);
    this->Locator->Delete();
  }
}

//------------------------------------------------------------------------------
int vtkCutter::RequestUpdateExtent(
  vtkInformation*, vtkInformationVector** inputVector, vtkInformationVector*)
{
  vtkInformation* inInfo = inputVector[0]->GetInformationObject(0);
  inInfo->Set(vtkStreamingDemandDrivenPipeline::EXACT_EXTENT(), 1);
  return 1;
}

//------------------------------------------------------------------------------
int vtkCutter::FillInputPortInformation(int, vtkInformation* info)
{
  info->Set(vtkAlgorithm::INPUT_REQUIRED_DATA_TYPE(), "vtkDataSet");
  return 1;
}

//------------------------------------------------------------------------------
void vtkCutter::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);

  os << indent << "Cut Function: " << this->CutFunction << "\n";
  os << indent << "Sort By: " << this->GetSortByAsString() << "\n";

  if (this->Locator)
  {
    os << indent << "Locator: " << this->Locator << "\n";
  }
  else
  {
    os << indent << "Locator: (none)\n";
  }

  this->ContourValues->PrintSelf(os, indent.GetNextIndent());

  os << indent << "Generate Cut Scalars: " << (this->GenerateCutScalars ? "On\n" : "Off\n");

  os << indent << "Precision of the output points: " << this->OutputPointsPrecision << "\n";
}

//------------------------------------------------------------------------------
void vtkCutter::ReportReferences(vtkGarbageCollector* collector)
{
  this->Superclass::ReportReferences(collector);
  // These share our input and might participate in a reference loop
  vtkGarbageCollectorReport(collector, this->SynchronizedTemplates3D, "SynchronizedTemplates3D");
  vtkGarbageCollectorReport(
    collector, this->SynchronizedTemplatesCutter3D, "SynchronizedTemplatesCutter3D");
  vtkGarbageCollectorReport(
    collector, this->GridSynchronizedTemplates, "GridSynchronizedTemplates");
  vtkGarbageCollectorReport(
    collector, this->RectilinearSynchronizedTemplates, "RectilinearSynchronizedTemplates");
  vtkGarbageCollectorReport(collector, this->PlaneCutter, "PlaneCutter");
}
VTK_ABI_NAMESPACE_END
