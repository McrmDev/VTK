// SPDX-FileCopyrightText: Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
// SPDX-License-Identifier: BSD-3-Clause
#include "vtkPOutlineFilterInternals.h"

#include "vtkAppendPolyData.h"
#include "vtkBoundingBox.h"
#include "vtkCompositeDataIterator.h"
#include "vtkDataObjectTree.h"
#include "vtkGraph.h"
#include "vtkMath.h"
#include "vtkMultiProcessController.h"
#include "vtkNew.h"
#include "vtkObjectFactory.h"
#include "vtkOutlineCornerSource.h"
#include "vtkOutlineSource.h"
#include "vtkOverlappingAMR.h"
#include "vtkOverlappingAMRMetaData.h"
#include "vtkPolyData.h"
#include "vtkSmartPointer.h"
#include "vtkUniformGrid.h"

//------------------------------------------------------------------------------
VTK_ABI_NAMESPACE_BEGIN
class AddBoundsListOperator : public vtkCommunicator::Operation
{
  // Description:
  // Performs a "B.AddBounds(A)" operation.
  void Function(const void* A, void* B, vtkIdType length, int datatype) override
  {
    (void)datatype;
    assert((datatype == VTK_DOUBLE) && (length % 6 == 0));
    assert("pre: A vector is nullptr" && (A != nullptr));
    assert("pre: B vector is nullptr" && (B != nullptr));
    vtkBoundingBox box;
    const double* aPtr = reinterpret_cast<const double*>(A);
    double* bPtr = reinterpret_cast<double*>(B);
    for (vtkIdType idx = 0; idx < length; idx += 6)
    {
      box.SetBounds(&bPtr[idx]);
      box.AddBounds(&aPtr[idx]);
      box.GetBounds(&bPtr[idx]);
    }
  }

  // Description:
  // Sets Commutative to true for this operation
  int Commutative() override { return 1; }
};

//------------------------------------------------------------------------------
void vtkPOutlineFilterInternals::SetController(vtkMultiProcessController* controller)
{
  this->Controller = controller;
}

//------------------------------------------------------------------------------
void vtkPOutlineFilterInternals::SetCornerFactor(double cornerFactor)
{
  this->CornerFactor = cornerFactor;
}

//------------------------------------------------------------------------------
void vtkPOutlineFilterInternals::SetIsCornerSource(bool value)
{
  this->IsCornerSource = value;
}

//------------------------------------------------------------------------------
int vtkPOutlineFilterInternals::RequestData(vtkInformation* vtkNotUsed(request),
  vtkInformationVector** inputVector, vtkInformationVector* outputVector)
{
  vtkDataObject* input = vtkDataObject::GetData(inputVector[0], 0);
  vtkPolyData* output = vtkPolyData::GetData(outputVector, 0);

  if (input == nullptr || output == nullptr)
  {
    vtkGenericWarningMacro("Missing input or output.");
    return 0;
  }

  if (this->Controller == nullptr)
  {
    vtkGenericWarningMacro("Missing Controller.");
    return 0;
  }

  vtkOverlappingAMR* oamr = vtkOverlappingAMR::SafeDownCast(input);
  if (oamr)
  {
    return this->RequestData(oamr, output);
  }

  vtkUniformGridAMR* amr = vtkUniformGridAMR::SafeDownCast(input);
  if (amr)
  {
    return this->RequestData(amr, output);
  }

  vtkDataObjectTree* cd = vtkDataObjectTree::SafeDownCast(input);
  if (cd)
  {
    return this->RequestData(cd, output);
  }

  vtkDataSet* ds = vtkDataSet::SafeDownCast(input);
  if (ds)
  {
    return this->RequestData(ds, output);
  }

  vtkGraph* graph = vtkGraph::SafeDownCast(input);
  if (graph)
  {
    return this->RequestData(graph, output);
  }
  return 0;
}

//------------------------------------------------------------------------------
void vtkPOutlineFilterInternals::CollectCompositeBounds(vtkDataObject* input)
{
  vtkDataSet* ds = vtkDataSet::SafeDownCast(input);
  vtkCompositeDataSet* compInput = vtkCompositeDataSet::SafeDownCast(input);
  if (ds != nullptr)
  {
    double bounds[6];
    ds->GetBounds(bounds);
    this->BoundsList.emplace_back(bounds);
  }
  else if (compInput != nullptr)
  {
    vtkCompositeDataIterator* iter = compInput->NewIterator();
    iter->SkipEmptyNodesOff();
    iter->GoToFirstItem();
    for (; !iter->IsDoneWithTraversal(); iter->GoToNextItem())
    {
      this->CollectCompositeBounds(iter->GetCurrentDataObject());
    }
    iter->Delete();
  }
  else
  {
    double bounds[6];
    vtkMath::UninitializeBounds(bounds);
    this->BoundsList.emplace_back(bounds);
  }
}

//------------------------------------------------------------------------------
int vtkPOutlineFilterInternals::RequestData(vtkDataObjectTree* input, vtkPolyData* output)
{
  // Collect local bounds.
  this->CollectCompositeBounds(input);

  // Make an array of bounds from collected bounds
  std::vector<double> boundsList;
  boundsList.resize(6 * this->BoundsList.size());

  for (size_t i = 0; i < this->BoundsList.size(); ++i)
  {
    this->BoundsList[i].GetBounds(&boundsList[i * 6]);
  }

  // Collect global bounds and copy into the array
  if (this->Controller && this->Controller->GetNumberOfProcesses() > 1)
  {
    AddBoundsListOperator operation;
    double* temp = new double[6 * this->BoundsList.size()];
    this->Controller->Reduce(
      boundsList.data(), temp, static_cast<vtkIdType>(6 * this->BoundsList.size()), &operation, 0);
    memcpy(boundsList.data(), temp, 6 * this->BoundsList.size() * sizeof(double));
    delete[] temp;

    if (this->Controller->GetLocalProcessId() > 0)
    {
      // only root node will produce the output.
      return 1;
    }
  }

  // Make output with collected bounds
  vtkNew<vtkAppendPolyData> appender;
  for (size_t i = 0; i < 6 * this->BoundsList.size(); i += 6)
  {
    appender->AddInputData(this->GenerateOutlineGeometry(&boundsList[i]));
  }

  appender->Update();
  output->ShallowCopy(appender->GetOutput());
  return 1;
}

//------------------------------------------------------------------------------
int vtkPOutlineFilterInternals::RequestData(vtkOverlappingAMR* input, vtkPolyData* output)
{
  // For Overlapping AMR, we have meta-data on all processes for the complete
  // AMR structure. Hence root node can build the outlines using that
  // meta-data, itself.
  int procid = this->Controller->GetLocalProcessId();
  if (procid != 0)
  {
    // we only generate output on the root node.
    return 1;
  }

  vtkNew<vtkAppendPolyData> appender;
  for (unsigned int level = 0; level < input->GetNumberOfLevels(); ++level)
  {
    unsigned int num_datasets = input->GetNumberOfBlocks(level);
    for (unsigned int dataIdx = 0; dataIdx < num_datasets; ++dataIdx)
    {
      double bounds[6];
      input->GetBounds(level, dataIdx, bounds);
      appender->AddInputData(this->GenerateOutlineGeometry(bounds));
    }
  }

  appender->Update();
  output->ShallowCopy(appender->GetOutput());
  return 1;
}

//------------------------------------------------------------------------------
int vtkPOutlineFilterInternals::RequestData(vtkUniformGridAMR* input, vtkPolyData* output)
{
  // All processes simply produce the outline for the non-null blocks that exist
  // on the process.
  vtkNew<vtkAppendPolyData> appender;
  unsigned int block_id = 0;
  for (unsigned int level = 0; level < input->GetNumberOfLevels(); ++level)
  {
    unsigned int num_datasets = input->GetNumberOfBlocks(level);
    for (unsigned int dataIdx = 0; dataIdx < num_datasets; ++dataIdx, block_id++)
    {
      vtkUniformGrid* ug = input->GetDataSet(level, dataIdx);
      if (ug)
      {
        double bounds[6];
        ug->GetBounds(bounds);
        appender->AddInputData(this->GenerateOutlineGeometry(bounds));
      }
    }
  }

  appender->Update();
  output->ShallowCopy(appender->GetOutput());
  return 1;
}

//------------------------------------------------------------------------------
int vtkPOutlineFilterInternals::RequestData(vtkDataSet* input, vtkPolyData* output)
{
  double bounds[6];
  input->GetBounds(bounds);
  if (this->Controller->GetNumberOfProcesses() > 1)
  {
    double reduced_bounds[6];
    int procid = this->Controller->GetLocalProcessId();
    AddBoundsListOperator operation;
    this->Controller->Reduce(bounds, reduced_bounds, 6, &operation, 0);
    if (procid > 0)
    {
      // Satellite node
      return 1;
    }
    memcpy(bounds, reduced_bounds, 6 * sizeof(double));
  }

  output->ShallowCopy(this->GenerateOutlineGeometry(bounds));
  return 1;
}

//------------------------------------------------------------------------------
int vtkPOutlineFilterInternals::RequestData(vtkGraph* input, vtkPolyData* output)
{
  double bounds[6];
  input->GetBounds(bounds);

  if (this->Controller->GetNumberOfProcesses() > 1)
  {
    double reduced_bounds[6];
    int procid = this->Controller->GetLocalProcessId();
    AddBoundsListOperator operation;
    this->Controller->Reduce(bounds, reduced_bounds, 6, &operation, 0);
    if (procid > 0)
    {
      // Satellite node
      return 1;
    }
    memcpy(bounds, reduced_bounds, 6 * sizeof(double));
  }

  output->ShallowCopy(this->GenerateOutlineGeometry(bounds));
  return 1;
}

//------------------------------------------------------------------------------
vtkSmartPointer<vtkPolyData> vtkPOutlineFilterInternals::GenerateOutlineGeometry(double bounds[6])
{
  vtkSmartPointer<vtkPolyData> output = nullptr;
  if (vtkMath::AreBoundsInitialized(bounds))
  {
    if (this->IsCornerSource)
    {
      vtkNew<vtkOutlineCornerSource> corner;
      corner->SetBounds(bounds);
      corner->SetCornerFactor(this->CornerFactor);
      corner->Update();
      output = corner->GetOutput();
    }
    else
    {
      vtkNew<vtkOutlineSource> corner;
      corner->SetBounds(bounds);
      corner->Update();
      output = corner->GetOutput();
    }
  }
  return output;
}
VTK_ABI_NAMESPACE_END
