// SPDX-FileCopyrightText: Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
// SPDX-FileCopyrightText: Copyright (c) Kitware, Inc.
// SPDX-License-Identifier: BSD-3-Clause
#include "vtkMergeArrays.h"

#include "vtkCellData.h"
#include "vtkCompositeDataIterator.h"
#include "vtkCompositeDataSet.h"
#include "vtkDataArray.h"
#include "vtkDataSet.h"
#include "vtkFieldData.h"
#include "vtkInformation.h"
#include "vtkInformationVector.h"
#include "vtkObjectFactory.h"
#include "vtkPointData.h"
#include "vtkSmartPointer.h"
#include "vtkStreamingDemandDrivenPipeline.h"

#include <set>
#include <vector>

VTK_ABI_NAMESPACE_BEGIN
vtkStandardNewMacro(vtkMergeArrays);

//------------------------------------------------------------------------------
vtkMergeArrays::vtkMergeArrays() = default;

//------------------------------------------------------------------------------
vtkMergeArrays::~vtkMergeArrays() = default;

//------------------------------------------------------------------------------
bool vtkMergeArrays::GetOutputArrayName(
  vtkFieldData* arrays, const char* arrayName, int inputIndex, std::string& outputArrayName)
{
  if (arrays->GetAbstractArray(arrayName) == nullptr)
  {
    return false;
  }
  outputArrayName = std::string(arrayName) + "_input_" + std::to_string(inputIndex);
  return true;
}

//------------------------------------------------------------------------------
void vtkMergeArrays::MergeArrays(int inputIndex, vtkFieldData* inputFD, vtkFieldData* outputFD)
{
  if (inputFD == nullptr || outputFD == nullptr)
  {
    return;
  }

  std::string outputArrayName;
  int numArrays = inputFD->GetNumberOfArrays();
  for (int arrayIdx = 0; arrayIdx < numArrays; ++arrayIdx)
  {
    vtkAbstractArray* array = inputFD->GetAbstractArray(arrayIdx);
    if (this->GetOutputArrayName(outputFD, array->GetName(), inputIndex, outputArrayName))
    {
      vtkAbstractArray* newArray = array->NewInstance();
      if (vtkDataArray* newDataArray = vtkDataArray::SafeDownCast(newArray))
      {
        newDataArray->ShallowCopy(vtkDataArray::SafeDownCast(array));
      }
      else
      {
        newArray->DeepCopy(array);
      }
      newArray->SetName(outputArrayName.c_str());
      outputFD->AddArray(newArray);
      newArray->FastDelete();
    }
    else
    {
      outputFD->AddArray(array);
    }
  }
}

//------------------------------------------------------------------------------
int vtkMergeArrays::MergeDataObjectFields(vtkDataObject* input, int idx, vtkDataObject* output)
{
  if (!input || !output)
  {
    return 0;
  }

  int checks[vtkDataObject::NUMBER_OF_ATTRIBUTE_TYPES];
  for (int attr = 0; attr < vtkDataObject::NUMBER_OF_ATTRIBUTE_TYPES; attr++)
  {
    checks[attr] = output->GetNumberOfElements(attr) == input->GetNumberOfElements(attr) ? 0 : 1;
  }
  int globalChecks[vtkDataObject::NUMBER_OF_ATTRIBUTE_TYPES];

  for (int i = 0; i < vtkDataObject::NUMBER_OF_ATTRIBUTE_TYPES; ++i)
  {
    globalChecks[i] = checks[i];
  }

  for (int attr = 0; attr < vtkDataObject::NUMBER_OF_ATTRIBUTE_TYPES; attr++)
  {
    if (globalChecks[attr] == 0)
    {
      // only merge arrays when the number of elements in the input and output are the same
      this->MergeArrays(
        idx, input->GetAttributesAsFieldData(attr), output->GetAttributesAsFieldData(attr));
    }
  }

  return 1;
}

//------------------------------------------------------------------------------
int vtkMergeArrays::FillInputPortInformation(int vtkNotUsed(port), vtkInformation* info)
{
  info->Set(vtkAlgorithm::INPUT_IS_REPEATABLE(), 1);
  return 1;
}

//------------------------------------------------------------------------------
int vtkMergeArrays::RequestInformation(vtkInformation* vtkNotUsed(request),
  vtkInformationVector** inputVector, vtkInformationVector* outputVector)
{
  int numberOfInputs = inputVector[0]->GetNumberOfInformationObjects();
  if (numberOfInputs < 2)
  {
    vtkErrorMacro(<< "This filter needs at least 2 inputs.");
    return 0;
  }

  vtkInformation* outInfo = outputVector->GetInformationObject(0);

  // Aggregates time values
  std::set<double> allTimeSteps;
  for (int idx = 0; idx < numberOfInputs; ++idx)
  {
    vtkInformation* inInfo = inputVector[0]->GetInformationObject(idx);
    if (!inInfo)
    {
      continue;
    }

    if (!inInfo->Has(vtkStreamingDemandDrivenPipeline::TIME_STEPS()))
    {
      continue;
    }

    int numberOfTimeSteps = inInfo->Length(vtkStreamingDemandDrivenPipeline::TIME_STEPS());
    double* values = inInfo->Get(vtkStreamingDemandDrivenPipeline::TIME_STEPS());

    for (int step = 0; step < numberOfTimeSteps; step++)
    {
      allTimeSteps.insert(values[step]);
    }
  }

  if (allTimeSteps.empty())
  {
    // Not having any timesteps is fine, just return.
    return 1;
  }

  // Forward these timesteps to the output
  std::vector<double> allTimeStepsVec(allTimeSteps.begin(), allTimeSteps.end());
  outInfo->Set(vtkStreamingDemandDrivenPipeline::TIME_STEPS(), allTimeStepsVec.data(),
    static_cast<int>(allTimeStepsVec.size()));

  double timeRange[2];
  timeRange[0] = allTimeStepsVec.front();
  timeRange[1] = allTimeStepsVec.back();
  outInfo->Set(vtkStreamingDemandDrivenPipeline::TIME_RANGE(), timeRange, 2);

  return 1;
}

//------------------------------------------------------------------------------
int vtkMergeArrays::RequestData(vtkInformation* vtkNotUsed(request),
  vtkInformationVector** inputVector, vtkInformationVector* outputVector)
{
  int num = inputVector[0]->GetNumberOfInformationObjects();
  if (num < 1)
  {
    return 0;
  }
  // get the output info object
  vtkInformation* outInfo = outputVector->GetInformationObject(0);
  vtkDataObject* output = outInfo->Get(vtkDataObject::DATA_OBJECT());

  vtkInformation* inInfo = inputVector[0]->GetInformationObject(0);
  vtkDataObject* input = inInfo->Get(vtkDataObject::DATA_OBJECT());

  vtkCompositeDataSet* cOutput = vtkCompositeDataSet::SafeDownCast(output);
  if (cOutput)
  {
    vtkCompositeDataSet* cInput = vtkCompositeDataSet::SafeDownCast(input);
    cOutput->CopyStructure(cInput);
    vtkSmartPointer<vtkCompositeDataIterator> iter;
    iter.TakeReference(cInput->NewIterator());
    iter->InitTraversal();
    for (; !iter->IsDoneWithTraversal(); iter->GoToNextItem())
    {
      if (vtkDataSet* tmpIn = vtkDataSet::SafeDownCast(iter->GetCurrentDataObject()))
      {
        vtkDataSet* tmpOut = tmpIn->NewInstance();
        tmpOut->ShallowCopy(tmpIn);
        cOutput->SetDataSet(iter, tmpOut);
        tmpOut->Delete();
      }
    }
  }
  else
  {
    output->ShallowCopy(input);
  }

  for (int idx = 1; idx < num; ++idx)
  {
    if (this->CheckAbort())
    {
      break;
    }
    inInfo = inputVector[0]->GetInformationObject(idx);
    input = inInfo->Get(vtkDataObject::DATA_OBJECT());
    if (!this->MergeDataObjectFields(input, idx, output))
    {
      return 0;
    }
    vtkCompositeDataSet* cInput = vtkCompositeDataSet::SafeDownCast(input);
    if (cOutput && cInput)
    {
      vtkSmartPointer<vtkCompositeDataIterator> iter;
      iter.TakeReference(cInput->NewIterator());
      iter->InitTraversal();
      for (; !iter->IsDoneWithTraversal(); iter->GoToNextItem())
      {
        vtkDataObject* tmpIn = iter->GetCurrentDataObject();
        vtkDataObject* tmpOut = cOutput->GetDataSet(iter);
        if (!this->MergeDataObjectFields(tmpIn, idx, tmpOut))
        {
          return 0;
        }
      }
    }
  }
  return 1;
}

//------------------------------------------------------------------------------
void vtkMergeArrays::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}
VTK_ABI_NAMESPACE_END
