// SPDX-FileCopyrightText: Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
// SPDX-License-Identifier: BSD-3-Clause
#include "TestVectorFieldSource.h"
#include "vtkAMRBox.h"
#include "vtkAMREnzoReader.h"
#include "vtkAMRInterpolatedVelocityField.h"
#include "vtkCellArray.h"
#include "vtkCellData.h"
#include "vtkDoubleArray.h"
#include "vtkIdList.h"
#include "vtkInformation.h"
#include "vtkInformationVector.h"
#include "vtkMPIController.h"
#include "vtkMath.h"
#include "vtkNew.h"
#include "vtkObjectFactory.h"
#include "vtkOverlappingAMR.h"
#include "vtkPStreamTracer.h"
#include "vtkPoints.h"
#include "vtkPolyDataMapper.h"
#include "vtkTestUtilities.h"
#include "vtkUniformGrid.h"

inline double ComputeLength(vtkIdList* poly, vtkPoints* pts)
{
  int n = poly->GetNumberOfIds();
  if (n == 0)
    return 0;

  double s(0);
  double p[3];
  pts->GetPoint(poly->GetId(0), p);
  for (int j = 1; j < n; j++)
  {
    int pIndex = poly->GetId(j);
    double q[3];
    pts->GetPoint(pIndex, q);
    s += sqrt(vtkMath::Distance2BetweenPoints(p, q));
    memcpy(p, q, 3 * sizeof(double));
  }
  return s;
}

class TestAMRVectorSource : public vtkOverlappingAMRAlgorithm
{
public:
  vtkTypeMacro(TestAMRVectorSource, vtkAlgorithm);
  enum GenerateMethod
  {
    UseVelocity,
    Circular
  };

  vtkSetMacro(Method, GenerateMethod);
  vtkGetMacro(Method, GenerateMethod);

  static TestAMRVectorSource* New();

  int FillInputPortInformation(int, vtkInformation* info) override
  {
    // now add our info
    info->Set(vtkDataObject::DATA_TYPE_NAME(), "vtkOverlappingAMR");
    info->Set(vtkAlgorithm::INPUT_IS_REPEATABLE(), 1);
    return 1;
  }

  GenerateMethod Method;

protected:
  TestAMRVectorSource()
  {
    SetNumberOfInputPorts(1);
    SetNumberOfOutputPorts(1);
  }

  // Description:
  // This is called by the superclass.
  // This is the method you should override.
  int RequestData(vtkInformation*, vtkInformationVector** inputVector,
    vtkInformationVector* outputVector) override
  {
    vtkInformation* inInfo = inputVector[0]->GetInformationObject(0);
    vtkInformation* outInfo = outputVector->GetInformationObject(0);

    vtkOverlappingAMR* input =
      vtkOverlappingAMR::SafeDownCast(inInfo->Get(vtkDataObject::DATA_OBJECT()));

    vtkOverlappingAMR* output =
      vtkOverlappingAMR::SafeDownCast(outInfo->Get(vtkDataObject::DATA_OBJECT()));

    if (!input || !output)
    {
      assert(false);
      return 0;
    }

    output->DeepCopy(input);

    for (unsigned int level = 0; level < output->GetNumberOfLevels(); ++level)
    {
      for (unsigned int idx = 0; idx < output->GetNumberOfBlocks(level); ++idx)
      {
        vtkUniformGrid* grid = output->GetDataSet(level, idx);
        if (!grid)
        {
          continue;
        }
        vtkCellData* cellData = grid->GetCellData();
        vtkDataArray* xVelocity = cellData->GetArray("x-velocity");
        vtkDataArray* yVelocity = cellData->GetArray("y-velocity");
        vtkDataArray* zVelocity = cellData->GetArray("z-velocity");

        vtkSmartPointer<vtkDoubleArray> velocityVectors = vtkSmartPointer<vtkDoubleArray>::New();
        velocityVectors->SetName("Gradient");
        velocityVectors->SetNumberOfComponents(3);

        int numCells = grid->GetNumberOfCells();
        for (int cellId = 0; cellId < numCells; cellId++)
        {
          assert(xVelocity);
          double velocity[3] = { xVelocity->GetTuple(cellId)[0], yVelocity->GetTuple(cellId)[0],
            zVelocity->GetTuple(cellId)[0] };
          velocityVectors->InsertNextTuple(velocity);
        }
        grid->GetCellData()->AddArray(velocityVectors);
      }
    }

    return 1;
  }
};

vtkStandardNewMacro(TestAMRVectorSource);

int TestPStreamAMR(int argc, char* argv[])
{

  vtkNew<vtkMPIController> c;
  vtkMultiProcessController::SetGlobalController(c);
  c->Initialize(&argc, &argv);
  int numProcs = c->GetNumberOfProcesses();
  int Rank = c->GetLocalProcessId();
  if (numProcs != 4)
  {
    std::cerr << "Test requires 4 processes." << std::endl;
    c->Finalize();
    return EXIT_FAILURE;
  }

  char* fname =
    vtkTestUtilities::ExpandDataFileName(argc, argv, "Data/AMR/Enzo/DD0010/moving7_0010.hierarchy");

  bool res = true;

  double maximumPropagation = 10;
  double stepSize(0.1);

  vtkNew<vtkAMREnzoReader> imageSource;
  imageSource->SetController(c);
  imageSource->SetFileName(fname);
  imageSource->SetMaxLevel(8);
  imageSource->SetCellArrayStatus("x-velocity", 1);
  imageSource->SetCellArrayStatus("y-velocity", 1);
  imageSource->SetCellArrayStatus("z-velocity", 1);

  delete[] fname;

  vtkNew<TestAMRVectorSource> gradientSource;
  gradientSource->SetInputConnection(imageSource->GetOutputPort());

  vtkNew<vtkPStreamTracer> tracer;
  tracer->SetInputConnection(0, gradientSource->GetOutputPort());
  tracer->SetInputArrayToProcess(0, 0, 0, vtkDataObject::FIELD_ASSOCIATION_CELLS, "Gradient");
  tracer->SetIntegrationDirection(2);
  tracer->SetIntegratorTypeToRungeKutta4();
  tracer->SetMaximumNumberOfSteps(
    4 * maximumPropagation / stepSize); // shouldn't have to do this fix in stream tracer somewhere!
  tracer->SetMinimumIntegrationStep(stepSize * .1);
  tracer->SetMaximumIntegrationStep(stepSize);
  tracer->SetInitialIntegrationStep(stepSize);

  vtkNew<vtkPolyData> seeds;
  vtkNew<vtkPoints> seedPoints;
  for (double t = 0; t < 1; t += 0.1)
  {
    seedPoints->InsertNextPoint(t, t, t);
  }

  seeds->SetPoints(seedPoints);
  tracer->SetInputData(1, seeds);
  tracer->SetMaximumPropagation(maximumPropagation);

  vtkSmartPointer<vtkPolyDataMapper> traceMapper = vtkSmartPointer<vtkPolyDataMapper>::New();
  traceMapper->SetInputConnection(tracer->GetOutputPort());
  traceMapper->SetPiece(Rank);
  traceMapper->SetNumberOfPieces(numProcs);
  traceMapper->Update();

  gradientSource->GetOutputDataObject(0);

  vtkPolyData* out = tracer->GetOutput();

  vtkNew<vtkIdList> polyLine;
  vtkCellArray* lines = out->GetLines();
  double totalLength(0);
  int totalSize(0);
  lines->InitTraversal();
  while (lines->GetNextCell(polyLine))
  {
    double d = ComputeLength(polyLine, out->GetPoints());
    totalLength += d;
    totalSize += polyLine->GetNumberOfIds();
  }
  double totalLengthAll(0);
  c->Reduce(&totalLength, &totalLengthAll, 1, vtkCommunicator::SUM_OP, 0);

  int totalTotalSize(0);
  c->Reduce(&totalSize, &totalTotalSize, 1, vtkCommunicator::SUM_OP, 0);

  if (Rank == 0)
  {
    cout << "Trace Length: " << totalLengthAll << endl;
  }
  res = (totalLengthAll - 17.18) / 17.18 < 0.01;

  c->Finalize();

  return res ? EXIT_SUCCESS : EXIT_FAILURE;
}
