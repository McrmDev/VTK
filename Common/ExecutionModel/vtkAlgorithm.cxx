// SPDX-FileCopyrightText: Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
// SPDX-License-Identifier: BSD-3-Clause
#include "vtkAlgorithm.h"

#include "vtkAlgorithmOutput.h"
#include "vtkCellData.h"
#include "vtkCollection.h"
#include "vtkCollectionIterator.h"
#include "vtkCommand.h"
#include "vtkCompositeDataPipeline.h"
#include "vtkDataArray.h"
#include "vtkDataObject.h"
#include "vtkDataSet.h"
#include "vtkErrorCode.h"
#include "vtkFieldData.h"
#include "vtkGarbageCollector.h"
#include "vtkGraph.h"
#include "vtkHyperTreeGrid.h"
#include "vtkInformation.h"
#include "vtkInformationExecutivePortKey.h"
#include "vtkInformationExecutivePortVectorKey.h"
#include "vtkInformationInformationVectorKey.h"
#include "vtkInformationIntegerKey.h"
#include "vtkInformationStringKey.h"
#include "vtkInformationStringVectorKey.h"
#include "vtkInformationVector.h"
#include "vtkMath.h"
#include "vtkNew.h"
#include "vtkObjectFactory.h"
#include "vtkPointData.h"
#include "vtkProgressObserver.h"
#include "vtkSmartPointer.h"
#include "vtkTable.h"
#include "vtkTrivialProducer.h"

#include <set>
#include <vector>
#include <vtksys/SystemTools.hxx>

VTK_ABI_NAMESPACE_BEGIN
vtkStandardNewMacro(vtkAlgorithm);

vtkCxxSetObjectMacro(vtkAlgorithm, Information, vtkInformation);

vtkInformationKeyMacro(vtkAlgorithm, INPUT_REQUIRED_DATA_TYPE, StringVector);
vtkInformationKeyMacro(vtkAlgorithm, INPUT_IS_OPTIONAL, Integer);
vtkInformationKeyMacro(vtkAlgorithm, INPUT_IS_REPEATABLE, Integer);
vtkInformationKeyMacro(vtkAlgorithm, INPUT_REQUIRED_FIELDS, InformationVector);
vtkInformationKeyMacro(vtkAlgorithm, PORT_REQUIREMENTS_FILLED, Integer);
vtkInformationKeyMacro(vtkAlgorithm, INPUT_PORT, Integer);
vtkInformationKeyMacro(vtkAlgorithm, INPUT_CONNECTION, Integer);
vtkInformationKeyMacro(vtkAlgorithm, INPUT_ARRAYS_TO_PROCESS, InformationVector);
vtkInformationKeyMacro(vtkAlgorithm, CAN_PRODUCE_SUB_EXTENT, Integer);
vtkInformationKeyMacro(vtkAlgorithm, CAN_HANDLE_PIECE_REQUEST, Integer);
vtkInformationKeyMacro(vtkAlgorithm, ABORTED, Integer);

vtkExecutive* vtkAlgorithm::DefaultExecutivePrototype = nullptr;
vtkTimeStamp vtkAlgorithm::LastAbortTime;

//------------------------------------------------------------------------------
class vtkAlgorithmInternals
{
public:
  // Proxy object instances for use in establishing connections from
  // the output ports to other algorithms.
  std::vector<vtkSmartPointer<vtkAlgorithmOutput>> Outputs;
};

//------------------------------------------------------------------------------
class vtkAlgorithmToExecutiveFriendship
{
public:
  static void SetAlgorithm(vtkExecutive* executive, vtkAlgorithm* algorithm)
  {
    executive->SetAlgorithm(algorithm);
  }
};

//------------------------------------------------------------------------------
vtkAlgorithm::vtkAlgorithm()
{
  this->AbortExecute = 0;
  this->ErrorCode = 0;
  this->Progress = 0.0;
  this->ProgressText = nullptr;
  this->Executive = nullptr;
  this->ProgressObserver = nullptr;
  this->InputPortInformation = vtkInformationVector::New();
  this->OutputPortInformation = vtkInformationVector::New();
  this->AlgorithmInternal = new vtkAlgorithmInternals;
  this->Information = vtkInformation::New();
  this->Information->Register(this);
  this->Information->Delete();
  this->ProgressShift = 0.0;
  this->ProgressScale = 1.0;
  this->AbortOutput = false;
  this->ContainerAlgorithm = nullptr;
}

//------------------------------------------------------------------------------
vtkAlgorithm::~vtkAlgorithm()
{
  this->SetInformation(nullptr);
  if (this->Executive)
  {
    this->Executive->UnRegister(this);
    this->Executive = nullptr;
  }
  if (this->ProgressObserver)
  {
    this->ProgressObserver->UnRegister(this);
    this->ProgressObserver = nullptr;
  }
  this->InputPortInformation->Delete();
  this->OutputPortInformation->Delete();
  delete this->AlgorithmInternal;
  delete[] this->ProgressText;
  this->ProgressText = nullptr;
}

//------------------------------------------------------------------------------
void vtkAlgorithm::SetProgressObserver(vtkProgressObserver* po)
{
  // This intentionally does not modify the algorithm as it
  // is usually done by executives during execution and we don't
  // want the filter to change its mtime during execution.
  if (po != this->ProgressObserver)
  {
    if (this->ProgressObserver)
    {
      this->ProgressObserver->UnRegister(this);
    }
    this->ProgressObserver = po;
    if (po)
    {
      po->Register(this);
    }
  }
}

//------------------------------------------------------------------------------
void vtkAlgorithm::SetProgressShiftScale(double shift, double scale)
{
  this->ProgressShift = shift;
  this->ProgressScale = scale;
}

//------------------------------------------------------------------------------
// Update the progress of the process object. If a ProgressMethod exists,
// executes it. Then set the Progress ivar to amount. The parameter amount
// should range between (0,1).
void vtkAlgorithm::UpdateProgress(double amount)
{
  amount = this->GetProgressShift() + this->GetProgressScale() * amount;

  // clamp to [0, 1].
  amount = vtkMath::Min(amount, 1.0);
  amount = vtkMath::Max(amount, 0.0);

  if (this->ProgressObserver)
  {
    this->ProgressObserver->UpdateProgress(amount);
  }
  else
  {
    this->Progress = amount;
    this->InvokeEvent(vtkCommand::ProgressEvent, static_cast<void*>(&amount));
  }
}

//------------------------------------------------------------------------------
// Check to see if an input's ABORTED flag is set or if an upstream
// algorithm's AbortExecute is set. If either is set, return true.
bool vtkAlgorithm::CheckAbort()
{
  if (this->GetAbortExecute())
  {
    this->LastAbortCheckTime.Modified();
    this->AbortOutput = true;
    return true;
  }

  if (this->ContainerAlgorithm)
  {
    this->LastAbortCheckTime.Modified();
    bool containerResult = this->ContainerAlgorithm->CheckAbort();
    if (containerResult)
    {
      this->AbortOutput = true;
    }
    return containerResult;
  }

  if (this->LastAbortTime.GetMTime() > this->LastAbortCheckTime.GetMTime())
  {
    this->LastAbortCheckTime.Modified();
    for (int port = 0; port < this->GetNumberOfInputPorts(); port++)
    {
      for (int index = 0; index < this->GetNumberOfInputConnections(port); index++)
      {
        if (this->GetInputAlgorithm(port, index)->CheckUpstreamAbort())
        {
          this->AbortOutput = true;
          return true;
        }
      }
    }
  }

  return this->AbortOutput;
}

//------------------------------------------------------------------------------
// Set AbortExecute flag and update LastAbortTime.
void vtkAlgorithm::SetAbortExecuteAndUpdateTime()
{
  this->AbortExecute = 1;
  this->LastAbortTime.Modified();
}

//------------------------------------------------------------------------------
// Check to see if an input's ABORTED flag is set or if an upstream
// algorithm's AbortExecute is set. If either is set, return true.
// This is used by upstream algorithms to check for abort without
// setting any variables.
bool vtkAlgorithm::CheckUpstreamAbort()
{
  if (this->GetAbortExecute())
  {
    this->LastAbortCheckTime.Modified();
    return true;
  }

  if (this->LastAbortTime.GetMTime() > this->LastAbortCheckTime.GetMTime())
  {
    this->LastAbortCheckTime.Modified();
    for (int port = 0; port < this->GetNumberOfInputPorts(); port++)
    {
      for (int index = 0; index < this->GetNumberOfInputConnections(port); index++)
      {
        if (this->GetInputAlgorithm(port, index)->CheckUpstreamAbort())
        {
          return true;
        }
      }
    }
  }

  return this->GetAbortOutput();
}

//------------------------------------------------------------------------------
void vtkAlgorithm::SetNoPriorTemporalAccessInformationKey()
{
  this->SetNoPriorTemporalAccessInformationKey(
    vtkStreamingDemandDrivenPipeline::NO_PRIOR_TEMPORAL_ACCESS_RESET);
}

//------------------------------------------------------------------------------
void vtkAlgorithm::SetNoPriorTemporalAccessInformationKey(int key)
{
  using vtkSDDP = vtkStreamingDemandDrivenPipeline;

  if (key != vtkSDDP::NO_PRIOR_TEMPORAL_ACCESS_CONTINUE &&
    key != vtkSDDP::NO_PRIOR_TEMPORAL_ACCESS_RESET)
  {
    vtkWarningMacro("Setting vtkStreamingDemandDrivenPipeline::NO_PRIOR_TEMPORAL_ACCESS() with"
                    " unsupported value, setting it to"
                    " vtkStreamingDemandDrivenPipeline::NO_PRIOR_TEMPORAL_ACCESS_RESET by default");
    key = vtkStreamingDemandDrivenPipeline::NO_PRIOR_TEMPORAL_ACCESS_RESET;
  }

  for (int port = 0; port < this->GetNumberOfOutputPorts(); ++port)
  {
    if (vtkInformation* outputInfo = this->GetOutputInformation(port))
    {
      outputInfo->Set(vtkSDDP::NO_PRIOR_TEMPORAL_ACCESS(), key);
    }
  }
  this->Modified();
}

//------------------------------------------------------------------------------
void vtkAlgorithm::RemoveNoPriorTemporalAccessInformationKey()
{
  for (int port = 0; this->GetNumberOfOutputPorts(); ++port)
  {
    if (vtkInformation* outputInfo = this->GetOutputInformation(port))
    {
      outputInfo->Remove(vtkStreamingDemandDrivenPipeline::NO_PRIOR_TEMPORAL_ACCESS());
    }
  }
}

//------------------------------------------------------------------------------
vtkInformation* vtkAlgorithm ::GetInputArrayFieldInformation(
  int idx, vtkInformationVector** inputVector)
{
  // first get out association
  vtkInformation* info = this->GetInputArrayInformation(idx);

  // then get the actual info object from the pinfo
  int port = info->Get(INPUT_PORT());
  int connection = info->Get(INPUT_CONNECTION());
  int fieldAssoc = info->Get(vtkDataObject::FIELD_ASSOCIATION());
  vtkInformation* inInfo = inputVector[port]->GetInformationObject(connection);

  if (info->Has(vtkDataObject::FIELD_NAME()))
  {
    const char* name = info->Get(vtkDataObject::FIELD_NAME());
    return vtkDataObject::GetNamedFieldInformation(inInfo, fieldAssoc, name);
  }
  int fType = info->Get(vtkDataObject::FIELD_ATTRIBUTE_TYPE());
  return vtkDataObject::GetActiveFieldInformation(inInfo, fieldAssoc, fType);
}

//------------------------------------------------------------------------------
vtkInformation* vtkAlgorithm::GetInputArrayInformation(int idx)
{
  // add this info into the algorithms info object
  vtkInformationVector* inArrayVec = this->Information->Get(INPUT_ARRAYS_TO_PROCESS());
  if (!inArrayVec)
  {
    inArrayVec = vtkInformationVector::New();
    this->Information->Set(INPUT_ARRAYS_TO_PROCESS(), inArrayVec);
    inArrayVec->Delete();
  }
  vtkInformation* inArrayInfo = inArrayVec->GetInformationObject(idx);
  if (!inArrayInfo)
  {
    inArrayInfo = vtkInformation::New();
    inArrayVec->SetInformationObject(idx, inArrayInfo);
    inArrayInfo->Delete();
  }
  return inArrayInfo;
}

//------------------------------------------------------------------------------
void vtkAlgorithm::SetInputArrayToProcess(int idx, vtkInformation* inInfo)
{
  vtkInformation* info = this->GetInputArrayInformation(idx);
  info->Copy(inInfo, 1);
  this->Modified();
}

//------------------------------------------------------------------------------
void vtkAlgorithm::SetInputArrayToProcess(
  int idx, int port, int connection, int fieldAssociation, int attributeType)
{
  vtkInformation* info = this->GetInputArrayInformation(idx);

  info->Set(INPUT_PORT(), port);
  info->Set(INPUT_CONNECTION(), connection);
  info->Set(vtkDataObject::FIELD_ASSOCIATION(), fieldAssociation);
  info->Set(vtkDataObject::FIELD_ATTRIBUTE_TYPE(), attributeType);

  // remove name if there is one
  info->Remove(vtkDataObject::FIELD_NAME());

  this->Modified();
}

//------------------------------------------------------------------------------
void vtkAlgorithm::SetInputArrayToProcess(int idx, int port, int connection,
  const char* fieldAssociation, const char* fieldAttributeTypeOrName)
{
  if (!fieldAssociation)
  {
    vtkErrorMacro("Association is required");
    return;
  }
  if (!fieldAttributeTypeOrName)
  {
    vtkErrorMacro("Attribute type or array name is required");
    return;
  }

  int i;

  // Try to convert the string argument to an enum value
  int association = -1;
  for (i = 0; i < vtkDataObject::NUMBER_OF_ASSOCIATIONS; i++)
  {
    if (strcmp(fieldAssociation, vtkDataObject::GetAssociationTypeAsString(i)) == 0)
    {
      association = i;
      break;
    }
  }
  if (association == -1)
  {
    vtkErrorMacro("Unrecognized association type: " << fieldAssociation);
    return;
  }

  int attributeType = -1;
  for (i = 0; i < vtkDataSetAttributes::NUM_ATTRIBUTES; i++)
  {
    if (strcmp(fieldAttributeTypeOrName, vtkDataSetAttributes::GetLongAttributeTypeAsString(i)) ==
      0)
    {
      attributeType = i;
      break;
    }
  }
  if (attributeType == -1)
  {
    // Set by association and array name
    this->SetInputArrayToProcess(idx, port, connection, association, fieldAttributeTypeOrName);
    return;
  }

  // Set by association and attribute type
  this->SetInputArrayToProcess(idx, port, connection, association, attributeType);
}

//------------------------------------------------------------------------------
void vtkAlgorithm::SetInputArrayToProcess(
  int idx, int port, int connection, int fieldAssociation, const char* name)
{
  // ignore nullptr string
  if (!name)
  {
    return;
  }

  vtkInformation* info = this->GetInputArrayInformation(idx);

  // remove fieldAttr if there is one
  info->Remove(vtkDataObject::FIELD_ATTRIBUTE_TYPE());

  // Check to see whether the current input array matches -
  // if so we're done.
  if (info->Has(vtkDataObject::FIELD_NAME()) && info->Get(INPUT_PORT()) == port &&
    info->Get(INPUT_CONNECTION()) == connection &&
    info->Get(vtkDataObject::FIELD_ASSOCIATION()) == fieldAssociation &&
    info->Get(vtkDataObject::FIELD_NAME()) &&
    strcmp(info->Get(vtkDataObject::FIELD_NAME()), name) == 0)
  {
    return;
  }

  info->Set(INPUT_PORT(), port);
  info->Set(INPUT_CONNECTION(), connection);
  info->Set(vtkDataObject::FIELD_ASSOCIATION(), fieldAssociation);
  info->Set(vtkDataObject::FIELD_NAME(), name);

  this->Modified();
}

//------------------------------------------------------------------------------
void vtkAlgorithm::SetInputArrayToProcess(const char* name, int fieldAssociation)
{
  this->SetInputArrayToProcess(0, 0, 0, fieldAssociation, name);
}

//------------------------------------------------------------------------------
int vtkAlgorithm::GetInputArrayAssociation(int idx, vtkInformationVector** inputVector)
{
  int association = vtkDataObject::FIELD_ASSOCIATION_NONE;
  this->GetInputArrayToProcess(idx, inputVector, association);
  return association;
}

//------------------------------------------------------------------------------
int vtkAlgorithm::GetInputArrayAssociation(
  int idx, int connection, vtkInformationVector** inputVector)
{
  int association = vtkDataObject::FIELD_ASSOCIATION_NONE;
  this->GetInputArrayToProcess(idx, connection, inputVector, association);
  return association;
}

//------------------------------------------------------------------------------
int vtkAlgorithm::GetInputArrayAssociation(int idx, vtkDataObject* input)
{
  int association = vtkDataObject::FIELD_ASSOCIATION_NONE;
  this->GetInputArrayToProcess(idx, input, association);
  return association;
}

//------------------------------------------------------------------------------
vtkDataArray* vtkAlgorithm::GetInputArrayToProcess(int idx, vtkInformationVector** inputVector)
{
  int association = vtkDataObject::FIELD_ASSOCIATION_NONE;
  return this->GetInputArrayToProcess(idx, inputVector, association);
}

//------------------------------------------------------------------------------
vtkDataArray* vtkAlgorithm::GetInputArrayToProcess(
  int idx, vtkInformationVector** inputVector, int& association)
{
  return vtkArrayDownCast<vtkDataArray>(
    this->GetInputAbstractArrayToProcess(idx, inputVector, association));
}

//------------------------------------------------------------------------------
vtkDataArray* vtkAlgorithm::GetInputArrayToProcess(
  int idx, int connection, vtkInformationVector** inputVector)
{
  int association = vtkDataObject::FIELD_ASSOCIATION_NONE;
  return this->GetInputArrayToProcess(idx, connection, inputVector, association);
}

//------------------------------------------------------------------------------
vtkDataArray* vtkAlgorithm::GetInputArrayToProcess(
  int idx, int connection, vtkInformationVector** inputVector, int& association)
{
  return vtkArrayDownCast<vtkDataArray>(
    this->GetInputAbstractArrayToProcess(idx, connection, inputVector, association));
}

//------------------------------------------------------------------------------
vtkDataArray* vtkAlgorithm::GetInputArrayToProcess(int idx, vtkDataObject* input)
{
  int association = vtkDataObject::FIELD_ASSOCIATION_NONE;
  return this->GetInputArrayToProcess(idx, input, association);
}

//------------------------------------------------------------------------------
vtkDataArray* vtkAlgorithm::GetInputArrayToProcess(int idx, vtkDataObject* input, int& association)
{
  return vtkArrayDownCast<vtkDataArray>(
    this->GetInputAbstractArrayToProcess(idx, input, association));
}

//------------------------------------------------------------------------------
vtkAbstractArray* vtkAlgorithm::GetInputAbstractArrayToProcess(
  int idx, vtkInformationVector** inputVector)
{
  int association = vtkDataObject::FIELD_ASSOCIATION_NONE;
  return this->GetInputAbstractArrayToProcess(idx, inputVector, association);
}

//------------------------------------------------------------------------------
vtkAbstractArray* vtkAlgorithm::GetInputAbstractArrayToProcess(
  int idx, vtkInformationVector** inputVector, int& association)
{
  vtkInformationVector* inArrayVec = this->Information->Get(INPUT_ARRAYS_TO_PROCESS());
  if (!inArrayVec)
  {
    vtkErrorMacro("Attempt to get an input array for an index that has not been specified");
    return nullptr;
  }
  vtkInformation* inArrayInfo = inArrayVec->GetInformationObject(idx);
  if (!inArrayInfo)
  {
    vtkErrorMacro("Attempt to get an input array for an index that has not been specified");
    return nullptr;
  }

  int connection = inArrayInfo->Get(INPUT_CONNECTION());
  return this->GetInputAbstractArrayToProcess(idx, connection, inputVector, association);
}

//------------------------------------------------------------------------------
vtkAbstractArray* vtkAlgorithm::GetInputAbstractArrayToProcess(
  int idx, int connection, vtkInformationVector** inputVector)
{
  int association = vtkDataObject::FIELD_ASSOCIATION_NONE;
  return this->GetInputAbstractArrayToProcess(idx, connection, inputVector, association);
}

//------------------------------------------------------------------------------
vtkAbstractArray* vtkAlgorithm::GetInputAbstractArrayToProcess(
  int idx, int connection, vtkInformationVector** inputVector, int& association)
{
  vtkInformationVector* inArrayVec = this->Information->Get(INPUT_ARRAYS_TO_PROCESS());
  if (!inArrayVec)
  {
    vtkErrorMacro("Attempt to get an input array for an index that has not been specified");
    return nullptr;
  }
  vtkInformation* inArrayInfo = inArrayVec->GetInformationObject(idx);
  if (!inArrayInfo)
  {
    vtkErrorMacro("Attempt to get an input array for an index that has not been specified");
    return nullptr;
  }

  int port = inArrayInfo->Get(INPUT_PORT());
  vtkInformation* inInfo = inputVector[port]->GetInformationObject(connection);
  vtkDataObject* input = inInfo->Get(vtkDataObject::DATA_OBJECT());

  return this->GetInputAbstractArrayToProcess(idx, input, association);
}

//------------------------------------------------------------------------------
vtkAbstractArray* vtkAlgorithm::GetInputAbstractArrayToProcess(int idx, vtkDataObject* input)
{
  int association = vtkDataObject::FIELD_ASSOCIATION_NONE;
  return this->GetInputAbstractArrayToProcess(idx, input, association);
}

//------------------------------------------------------------------------------
vtkAbstractArray* vtkAlgorithm::GetInputAbstractArrayToProcess(
  int idx, vtkDataObject* input, int& association)
{
  if (!input)
  {
    return nullptr;
  }

  vtkInformationVector* inArrayVec = this->Information->Get(INPUT_ARRAYS_TO_PROCESS());
  if (!inArrayVec)
  {
    vtkErrorMacro("Attempt to get an input array for an index that has not been specified");
    return nullptr;
  }
  vtkInformation* inArrayInfo = inArrayVec->GetInformationObject(idx);
  if (!inArrayInfo)
  {
    vtkErrorMacro("Attempt to get an input array for an index that has not been specified");
    return nullptr;
  }

  int fieldAssoc = inArrayInfo->Get(vtkDataObject::FIELD_ASSOCIATION());
  association = fieldAssoc;

  if (inArrayInfo->Has(vtkDataObject::FIELD_NAME()))
  {
    const char* name = inArrayInfo->Get(vtkDataObject::FIELD_NAME());

    if (fieldAssoc == vtkDataObject::FIELD_ASSOCIATION_NONE)
    {
      vtkFieldData* fd = input->GetFieldData();
      return fd->GetAbstractArray(name);
    }

    if (fieldAssoc == vtkDataObject::FIELD_ASSOCIATION_ROWS)
    {
      vtkTable* inputT = vtkTable::SafeDownCast(input);
      if (!inputT)
      {
        vtkErrorMacro("Attempt to get row data from a non-table");
        return nullptr;
      }
      return inputT->GetColumnByName(name);
    }

    if (fieldAssoc == vtkDataObject::FIELD_ASSOCIATION_VERTICES ||
      fieldAssoc == vtkDataObject::FIELD_ASSOCIATION_EDGES)
    {
      vtkGraph* inputG = vtkGraph::SafeDownCast(input);
      if (!inputG)
      {
        vtkErrorMacro("Attempt to get vertex or edge data from a non-graph");
        return nullptr;
      }
      vtkFieldData* fd = nullptr;
      if (fieldAssoc == vtkDataObject::FIELD_ASSOCIATION_VERTICES)
      {
        association = vtkDataObject::FIELD_ASSOCIATION_VERTICES;
        fd = inputG->GetVertexData();
      }
      else
      {
        association = vtkDataObject::FIELD_ASSOCIATION_EDGES;
        fd = inputG->GetEdgeData();
      }
      return fd->GetAbstractArray(name);
    }

    if (vtkGraph::SafeDownCast(input) && fieldAssoc == vtkDataObject::FIELD_ASSOCIATION_POINTS)
    {
      return vtkGraph::SafeDownCast(input)->GetVertexData()->GetAbstractArray(name);
    }

    if (vtkHyperTreeGrid::SafeDownCast(input))
    {
      return vtkHyperTreeGrid::SafeDownCast(input)->GetCellData()->GetAbstractArray(name);
    }

    vtkDataSet* inputDS = vtkDataSet::SafeDownCast(input);
    if (!inputDS)
    {
      vtkErrorMacro("Attempt to get point or cell data from a data object");
      return nullptr;
    }

    if (fieldAssoc == vtkDataObject::FIELD_ASSOCIATION_POINTS)
    {
      return inputDS->GetPointData()->GetAbstractArray(name);
    }
    if (fieldAssoc == vtkDataObject::FIELD_ASSOCIATION_POINTS_THEN_CELLS &&
      inputDS->GetPointData()->GetAbstractArray(name))
    {
      association = vtkDataObject::FIELD_ASSOCIATION_POINTS;
      return inputDS->GetPointData()->GetAbstractArray(name);
    }

    association = vtkDataObject::FIELD_ASSOCIATION_CELLS;
    return inputDS->GetCellData()->GetAbstractArray(name);
  }
  else if (inArrayInfo->Has(vtkDataObject::FIELD_ATTRIBUTE_TYPE()))
  {
    vtkDataSet* inputDS = vtkDataSet::SafeDownCast(input);
    if (!inputDS)
    {
      if (vtkHyperTreeGrid::SafeDownCast(input))
      {
        int fType = inArrayInfo->Get(vtkDataObject::FIELD_ATTRIBUTE_TYPE());
        return vtkHyperTreeGrid::SafeDownCast(input)->GetCellData()->GetAbstractAttribute(fType);
      }
      vtkErrorMacro("Attempt to get point or cell data from a data object");
      return nullptr;
    }
    int fType = inArrayInfo->Get(vtkDataObject::FIELD_ATTRIBUTE_TYPE());
    if (fieldAssoc == vtkDataObject::FIELD_ASSOCIATION_POINTS)
    {
      return inputDS->GetPointData()->GetAbstractAttribute(fType);
    }
    if (fieldAssoc == vtkDataObject::FIELD_ASSOCIATION_POINTS_THEN_CELLS &&
      inputDS->GetPointData()->GetAbstractAttribute(fType))
    {
      association = vtkDataObject::FIELD_ASSOCIATION_POINTS;
      return inputDS->GetPointData()->GetAbstractAttribute(fType);
    }

    association = vtkDataObject::FIELD_ASSOCIATION_CELLS;
    return inputDS->GetCellData()->GetAbstractAttribute(fType);
  }
  else
  {
    return nullptr;
  }
}

//------------------------------------------------------------------------------
void vtkAlgorithm::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
  if (this->HasExecutive())
  {
    os << indent << "Executive: " << this->Executive << "\n";
  }
  else
  {
    os << indent << "Executive: (none)\n";
  }

  os << indent << "ErrorCode: " << vtkErrorCode::GetStringFromErrorCode(this->ErrorCode) << endl;

  if (this->Information)
  {
    os << indent << "Information: " << this->Information << "\n";
  }
  else
  {
    os << indent << "Information: (none)\n";
  }

  os << indent << "AbortExecute: " << (this->AbortExecute ? "On\n" : "Off\n");
  os << indent << "Progress: " << this->Progress << "\n";
  if (this->ProgressText)
  {
    os << indent << "Progress Text: " << this->ProgressText << "\n";
  }
  else
  {
    os << indent << "Progress Text: (None)\n";
  }
}

//------------------------------------------------------------------------------
vtkTypeBool vtkAlgorithm::HasExecutive()
{
  return this->Executive ? 1 : 0;
}

//------------------------------------------------------------------------------
vtkExecutive* vtkAlgorithm::GetExecutive()
{
  // Create the default executive if we do not have one already.
  if (!this->HasExecutive())
  {
    vtkExecutive* e = this->CreateDefaultExecutive();
    this->SetExecutive(e);
    e->Delete();
  }
  return this->Executive;
}

//------------------------------------------------------------------------------
void vtkAlgorithm::SetExecutive(vtkExecutive* newExecutive)
{
  vtkExecutive* oldExecutive = this->Executive;
  if (newExecutive != oldExecutive)
  {
    if (newExecutive)
    {
      newExecutive->Register(this);
      vtkAlgorithmToExecutiveFriendship::SetAlgorithm(newExecutive, this);
    }
    this->Executive = newExecutive;
    if (oldExecutive)
    {
      vtkAlgorithmToExecutiveFriendship::SetAlgorithm(oldExecutive, nullptr);
      oldExecutive->UnRegister(this);
    }
  }
}

//------------------------------------------------------------------------------
vtkTypeBool vtkAlgorithm::ProcessRequest(
  vtkInformation* request, vtkCollection* inInfo, vtkInformationVector* outInfo)
{
  vtkSmartPointer<vtkCollectionIterator> iter;
  iter.TakeReference(inInfo->NewIterator());

  std::vector<vtkInformationVector*> ivectors;
  for (iter->GoToFirstItem(); !iter->IsDoneWithTraversal(); iter->GoToNextItem())
  {
    vtkInformationVector* iv = vtkInformationVector::SafeDownCast(iter->GetCurrentObject());
    if (!iv)
    {
      return 0;
    }
    ivectors.push_back(iv);
  }
  if (ivectors.empty())
  {
    return this->ProcessRequest(request, (vtkInformationVector**)nullptr, outInfo);
  }
  else
  {
    return this->ProcessRequest(request, ivectors.data(), outInfo);
  }
}

//------------------------------------------------------------------------------
vtkTypeBool vtkAlgorithm::ProcessRequest(
  vtkInformation* /* request */, vtkInformationVector**, vtkInformationVector*)
{
  return 1;
}

//------------------------------------------------------------------------------
int vtkAlgorithm::ComputePipelineMTime(vtkInformation* /* request */, vtkInformationVector**,
  vtkInformationVector*, int /* requestFromOutputPort */, vtkMTimeType* mtime)
{
  // By default algorithms contribute only their own modified time.
  *mtime = this->GetMTime();
  return 1;
}

//------------------------------------------------------------------------------
int vtkAlgorithm::ModifyRequest(vtkInformation* /*request*/, int /*when*/)
{
  return 1;
}

//------------------------------------------------------------------------------
int vtkAlgorithm::GetNumberOfInputPorts()
{
  return this->InputPortInformation->GetNumberOfInformationObjects();
}

//------------------------------------------------------------------------------
void vtkAlgorithm::SetNumberOfInputPorts(int n)
{
  // Sanity check.
  if (n < 0)
  {
    vtkErrorMacro("Attempt to set number of input ports to " << n);
    n = 0;
  }

  // We must remove all connections from ports that are removed.
  for (int i = n; i < this->GetNumberOfInputPorts(); ++i)
  {
    this->SetNumberOfInputConnections(i, 0);
  }

  // Set the number of input port information objects.
  this->InputPortInformation->SetNumberOfInformationObjects(n);
}

//------------------------------------------------------------------------------
int vtkAlgorithm::GetNumberOfOutputPorts()
{
  return this->OutputPortInformation->GetNumberOfInformationObjects();
}

//------------------------------------------------------------------------------
void vtkAlgorithm::SetNumberOfOutputPorts(int n)
{
  // Sanity check.
  if (n < 0)
  {
    vtkErrorMacro("Attempt to set number of output ports to " << n);
    n = 0;
  }

  // We must remove all connections from ports that are removed.
  for (int i = n; i < this->GetNumberOfOutputPorts(); ++i)
  {
    // Get the producer and its output information for this port.
    vtkExecutive* producer = this->GetExecutive();
    vtkInformation* info = producer->GetOutputInformation(i);

    // Remove all consumers' references to this producer on this port.
    vtkExecutive** consumers = vtkExecutive::CONSUMERS()->GetExecutives(info);
    int* consumerPorts = vtkExecutive::CONSUMERS()->GetPorts(info);
    int consumerCount = vtkExecutive::CONSUMERS()->Length(info);
    for (int j = 0; j < consumerCount; ++j)
    {
      vtkInformationVector* inputs = consumers[j]->GetInputInformation(consumerPorts[j]);
      inputs->Remove(info);
    }

    // Remove this producer's references to all consumers on this port.
    vtkExecutive::CONSUMERS()->Remove(info);
  }

  // Set the number of output port information objects.
  this->OutputPortInformation->SetNumberOfInformationObjects(n);

  // Set the number of connection proxy objects.
  this->AlgorithmInternal->Outputs.resize(n);
}

//------------------------------------------------------------------------------
vtkInformation* vtkAlgorithm::GetInputPortInformation(int port)
{
  if (!this->InputPortIndexInRange(port, "get information object for"))
  {
    return nullptr;
  }

  // Get the input port information object.
  vtkInformation* info = this->InputPortInformation->GetInformationObject(port);

  // Fill it if it has not yet been filled.
  if (!info->Has(PORT_REQUIREMENTS_FILLED()))
  {
    if (this->FillInputPortInformation(port, info))
    {
      info->Set(PORT_REQUIREMENTS_FILLED(), 1);
    }
    else
    {
      info->Clear();
    }
  }

  // Return the information object.
  return info;
}

//------------------------------------------------------------------------------
vtkInformation* vtkAlgorithm::GetOutputPortInformation(int port)
{
  if (!this->OutputPortIndexInRange(port, "get information object for"))
  {
    return nullptr;
  }

  // Get the output port information object.
  vtkInformation* info = this->OutputPortInformation->GetInformationObject(port);

  // Fill it if it has not yet been filled.
  if (!info->Has(PORT_REQUIREMENTS_FILLED()))
  {
    if (this->FillOutputPortInformation(port, info))
    {
      info->Set(PORT_REQUIREMENTS_FILLED(), 1);
    }
    else
    {
      info->Clear();
    }
  }

  // Return the information object.
  return info;
}

//------------------------------------------------------------------------------
int vtkAlgorithm::FillInputPortInformation(int, vtkInformation*)
{
  vtkErrorMacro("FillInputPortInformation is not implemented.");
  return 0;
}

//------------------------------------------------------------------------------
int vtkAlgorithm::FillOutputPortInformation(int, vtkInformation*)
{
  vtkErrorMacro("FillOutputPortInformation is not implemented.");
  return 0;
}

//------------------------------------------------------------------------------
int vtkAlgorithm::InputPortIndexInRange(int index, const char* action)
{
  // Make sure the index of the input port is in range.
  if (index < 0 || index >= this->GetNumberOfInputPorts())
  {
    vtkErrorMacro("Attempt to " << (action ? action : "access") << " input port index " << index
                                << " for an algorithm with " << this->GetNumberOfInputPorts()
                                << " input ports.");
    return 0;
  }
  return 1;
}

//------------------------------------------------------------------------------
int vtkAlgorithm::OutputPortIndexInRange(int index, const char* action)
{
  // Make sure the index of the output port is in range.
  if (index < 0 || index >= this->GetNumberOfOutputPorts())
  {
    vtkErrorMacro("Attempt to " << (action ? action : "access") << " output port index " << index
                                << " for an algorithm with " << this->GetNumberOfOutputPorts()
                                << " output ports.");
    return 0;
  }
  return 1;
}

//------------------------------------------------------------------------------
void vtkAlgorithm::SetDefaultExecutivePrototype(vtkExecutive* proto)
{
  if (vtkAlgorithm::DefaultExecutivePrototype == proto)
  {
    return;
  }
  if (vtkAlgorithm::DefaultExecutivePrototype)
  {
    vtkAlgorithm::DefaultExecutivePrototype->UnRegister(nullptr);
    vtkAlgorithm::DefaultExecutivePrototype = nullptr;
  }
  if (proto)
  {
    proto->Register(nullptr);
  }
  vtkAlgorithm::DefaultExecutivePrototype = proto;
}

//------------------------------------------------------------------------------
vtkExecutive* vtkAlgorithm::CreateDefaultExecutive()
{
  if (vtkAlgorithm::DefaultExecutivePrototype)
  {
    return vtkAlgorithm::DefaultExecutivePrototype->NewInstance();
  }
  return vtkCompositeDataPipeline::New();
}

//------------------------------------------------------------------------------
void vtkAlgorithm::ReportReferences(vtkGarbageCollector* collector)
{
  this->Superclass::ReportReferences(collector);
  vtkGarbageCollectorReport(collector, this->Executive, "Executive");
}

/// These are convenience methods to forward to the executive

//------------------------------------------------------------------------------
vtkDataObject* vtkAlgorithm::GetOutputDataObject(int port)
{
  return this->GetExecutive()->GetOutputData(port);
}

//------------------------------------------------------------------------------
vtkDataObject* vtkAlgorithm::GetInputDataObject(int port, int connection)
{
  return this->GetExecutive()->GetInputData(port, connection);
}

//------------------------------------------------------------------------------
void vtkAlgorithm::RemoveAllInputs()
{
  this->SetInputConnection(0, nullptr);
}

//------------------------------------------------------------------------------
void vtkAlgorithm::RemoveAllInputConnections(int port)
{
  this->SetInputConnection(port, nullptr);
}

//------------------------------------------------------------------------------
void vtkAlgorithm::SetInputConnection(vtkAlgorithmOutput* input)
{
  this->SetInputConnection(0, input);
}

//------------------------------------------------------------------------------
void vtkAlgorithm::SetInputConnection(int port, vtkAlgorithmOutput* input)
{
  if (!this->InputPortIndexInRange(port, "connect"))
  {
    return;
  }

  // Get the producer/consumer pair for the connection.
  vtkExecutive* producer =
    (input && input->GetProducer()) ? input->GetProducer()->GetExecutive() : nullptr;
  int producerPort = producer ? input->GetIndex() : 0;
  vtkExecutive* consumer = this->GetExecutive();
  int consumerPort = port;

  // Get the vector of connected input information objects.
  vtkInformationVector* inputs = consumer->GetInputInformation(consumerPort);

  // Get the information object from the producer of the new input.
  vtkInformation* newInfo = producer ? producer->GetOutputInformation(producerPort) : nullptr;

  // Check if the connection is already present.
  if (!newInfo && inputs->GetNumberOfInformationObjects() == 0)
  {
    return;
  }
  else if (newInfo == inputs->GetInformationObject(0) &&
    inputs->GetNumberOfInformationObjects() == 1)
  {
    return;
  }

  // The connection is not present.
  vtkDebugMacro("Setting connection to input port index "
    << consumerPort << " from output port index " << producerPort << " on algorithm "
    << (producer ? producer->GetObjectDescription() : nullptr) << ".");

  // Add this consumer to the new input's list of consumers.
  if (newInfo)
  {
    vtkExecutive::CONSUMERS()->Append(newInfo, consumer, consumerPort);
  }

  // Remove this consumer from all old inputs' lists of consumers.
  for (int i = 0; i < inputs->GetNumberOfInformationObjects(); ++i)
  {
    if (vtkInformation* oldInfo = inputs->GetInformationObject(i))
    {
      vtkExecutive::CONSUMERS()->Remove(oldInfo, consumer, consumerPort);
    }
  }

  // Make the new input the only connection.
  if (newInfo)
  {
    inputs->SetInformationObject(0, newInfo);
    inputs->SetNumberOfInformationObjects(1);
  }
  else
  {
    inputs->SetNumberOfInformationObjects(0);
  }

  // This algorithm has been modified.
  this->Modified();
}

//------------------------------------------------------------------------------
void vtkAlgorithm::AddInputConnection(vtkAlgorithmOutput* input)
{
  this->AddInputConnection(0, input);
}

//------------------------------------------------------------------------------
void vtkAlgorithm::AddInputConnection(int port, vtkAlgorithmOutput* input)
{
  if (!this->InputPortIndexInRange(port, "connect"))
  {
    return;
  }

  // If there is no input do nothing.
  if (!input || !input->GetProducer())
  {
    return;
  }

  // Get the producer/consumer pair for the connection.
  vtkExecutive* producer = input->GetProducer()->GetExecutive();
  int producerPort = input->GetIndex();
  vtkExecutive* consumer = this->GetExecutive();
  int consumerPort = port;

  // Get the vector of connected input information objects.
  vtkInformationVector* inputs = consumer->GetInputInformation(consumerPort);

  // Add the new connection.
  vtkDebugMacro("Adding connection to input port index "
    << consumerPort << " from output port index " << producerPort << " on algorithm "
    << producer->GetAlgorithm()->GetObjectDescription() << ".");

  // Get the information object from the producer of the new input.
  vtkInformation* newInfo = producer->GetOutputInformation(producerPort);

  // Add this consumer to the input's list of consumers.
  vtkExecutive::CONSUMERS()->Append(newInfo, consumer, consumerPort);

  // Add the information object to the list of inputs.
  inputs->Append(newInfo);

  // This algorithm has been modified.
  this->Modified();
}

//------------------------------------------------------------------------------
void vtkAlgorithm::RemoveInputConnection(int port, int idx)
{
  if (!this->InputPortIndexInRange(port, "disconnect"))
  {
    return;
  }

  vtkAlgorithmOutput* input = this->GetInputConnection(port, idx);
  if (input)
  {
    // We need to check if this connection exists multiple times.
    // If it does, we can't remove this from the consumers list.
    int numConnections = 0;
    int numInputConnections = this->GetNumberOfInputConnections(0);
    for (int i = 0; i < numInputConnections; i++)
    {
      if (input == this->GetInputConnection(port, i))
      {
        numConnections++;
      }
    }

    vtkExecutive* consumer = this->GetExecutive();
    int consumerPort = port;

    // Get the vector of connected input information objects.
    vtkInformationVector* inputs = consumer->GetInputInformation(consumerPort);

    // Get the producer/consumer pair for the connection.
    vtkExecutive* producer = input->GetProducer()->GetExecutive();
    int producerPort = input->GetIndex();

    // Get the information object from the producer of the old input.
    vtkInformation* oldInfo = producer->GetOutputInformation(producerPort);

    // Only connected once, remove this from inputs consumer list.
    if (numConnections == 1)
    {
      // Remove this consumer from the old input's list of consumers.
      vtkExecutive::CONSUMERS()->Remove(oldInfo, consumer, consumerPort);
    }

    // Remove the information object from the list of inputs.
    inputs->Remove(idx);

    // This algorithm has been modified.
    this->Modified();
  }
}

//------------------------------------------------------------------------------
void vtkAlgorithm::RemoveInputConnection(int port, vtkAlgorithmOutput* input)
{
  if (!this->InputPortIndexInRange(port, "disconnect"))
  {
    return;
  }

  // If there is no input do nothing.
  if (!input || !input->GetProducer())
  {
    return;
  }

  // Get the producer/consumer pair for the connection.
  vtkExecutive* producer = input->GetProducer()->GetExecutive();
  int producerPort = input->GetIndex();
  vtkExecutive* consumer = this->GetExecutive();
  int consumerPort = port;

  // Get the vector of connected input information objects.
  vtkInformationVector* inputs = consumer->GetInputInformation(consumerPort);

  // Remove the connection.
  vtkDebugMacro("Removing connection to input port index "
    << consumerPort << " from output port index " << producerPort << " on algorithm "
    << producer->GetAlgorithm()->GetObjectDescription() << ".");

  // Get the information object from the producer of the old input.
  vtkInformation* oldInfo = producer->GetOutputInformation(producerPort);

  // Remove this consumer from the old input's list of consumers.
  vtkExecutive::CONSUMERS()->Remove(oldInfo, consumer, consumerPort);

  // Remove the information object from the list of inputs.
  inputs->Remove(oldInfo);

  // This algorithm has been modified.
  this->Modified();
}

//------------------------------------------------------------------------------
void vtkAlgorithm::SetNthInputConnection(int port, int index, vtkAlgorithmOutput* input)
{
  if (!this->InputPortIndexInRange(port, "replace connection"))
  {
    return;
  }

  // Get the producer/consumer pair for the connection.
  vtkExecutive* producer =
    (input && input->GetProducer()) ? input->GetProducer()->GetExecutive() : nullptr;
  int producerPort = producer ? input->GetIndex() : 0;
  vtkExecutive* consumer = this->GetExecutive();
  int consumerPort = port;

  // Get the vector of connected input information objects.
  vtkInformationVector* inputs = consumer->GetInputInformation(consumerPort);

  // Check for any existing connection with this index.
  vtkInformation* oldInfo = inputs->GetInformationObject(index);

  // Get the information object from the producer of the input.
  vtkInformation* newInfo = producer ? producer->GetOutputInformation(producerPort) : nullptr;

  // If the connection has not changed, do nothing.
  if (newInfo == oldInfo)
  {
    return;
  }

  // Set the connection.
  vtkDebugMacro("Setting connection index "
    << index << " to input port index " << consumerPort << " from output port index "
    << producerPort << " on algorithm "
    << (producer ? producer->GetAlgorithm()->GetObjectDescription() : "nullptr") << ".");

  // Add the consumer to the new input's list of consumers.
  if (newInfo)
  {
    vtkExecutive::CONSUMERS()->Append(newInfo, consumer, consumerPort);
  }

  // Remove the consumer from the old input's list of consumers.
  if (oldInfo)
  {
    vtkExecutive::CONSUMERS()->Remove(oldInfo, consumer, consumerPort);
  }

  // Store the information object in the vector of input connections.
  inputs->SetInformationObject(index, newInfo);

  // This algorithm has been modified.
  this->Modified();
}

//------------------------------------------------------------------------------
void vtkAlgorithm::SetNumberOfInputConnections(int port, int n)
{
  // Get the consumer executive and port number.
  vtkExecutive* consumer = this->GetExecutive();
  int consumerPort = port;

  // Get the vector of connected input information objects.
  vtkInformationVector* inputs = consumer->GetInputInformation(consumerPort);

  // If the number of connections has not changed, do nothing.
  if (n == inputs->GetNumberOfInformationObjects())
  {
    return;
  }

  // Remove connections beyond the new number.
  for (int i = n; i < inputs->GetNumberOfInformationObjects(); ++i)
  {
    // Remove each input's reference to this consumer.
    if (vtkInformation* oldInfo = inputs->GetInformationObject(i))
    {
      vtkExecutive::CONSUMERS()->Remove(oldInfo, consumer, consumerPort);
    }
  }

  // Set the number of connected inputs.  Non-existing inputs will be
  // empty information objects.
  inputs->SetNumberOfInformationObjects(n);

  // This algorithm has been modified.
  this->Modified();
}

//------------------------------------------------------------------------------
vtkAlgorithmOutput* vtkAlgorithm::GetOutputPort(int port)
{
  if (!this->OutputPortIndexInRange(port, "get"))
  {
    return nullptr;
  }

  // Create the vtkAlgorithmOutput proxy object if there is not one.
  if (!this->AlgorithmInternal->Outputs[port])
  {
    this->AlgorithmInternal->Outputs[port] = vtkSmartPointer<vtkAlgorithmOutput>::New();
    this->AlgorithmInternal->Outputs[port]->SetProducer(this);
    this->AlgorithmInternal->Outputs[port]->SetIndex(port);
  }

  // Return the proxy object instance.
  return this->AlgorithmInternal->Outputs[port];
}

//------------------------------------------------------------------------------
int vtkAlgorithm::GetNumberOfInputConnections(int port)
{
  if (this->Executive)
  {
    return this->Executive->GetNumberOfInputConnections(port);
  }
  return 0;
}

//------------------------------------------------------------------------------
int vtkAlgorithm::GetTotalNumberOfInputConnections()
{
  int i;
  int total = 0;
  for (i = 0; i < this->GetNumberOfInputPorts(); ++i)
  {
    total += this->GetNumberOfInputConnections(i);
  }
  return total;
}

//------------------------------------------------------------------------------
vtkInformation* vtkAlgorithm::GetOutputInformation(int port)
{
  return this->GetExecutive()->GetOutputInformation(port);
}

//------------------------------------------------------------------------------
vtkInformation* vtkAlgorithm::GetInputInformation(int port, int index)
{
  if (index < 0 || index >= this->GetNumberOfInputConnections(port))
  {
    vtkErrorMacro("Attempt to get connection index "
      << index << " for input port " << port << ", which has "
      << this->GetNumberOfInputConnections(port) << " connections.");
    return nullptr;
  }
  return this->GetExecutive()->GetInputInformation(port, index);
}

//------------------------------------------------------------------------------
vtkAlgorithm* vtkAlgorithm::GetInputAlgorithm(int port, int index)
{
  int dummy;
  return this->GetInputAlgorithm(port, index, dummy);
}

//------------------------------------------------------------------------------
vtkAlgorithm* vtkAlgorithm::GetInputAlgorithm(int port, int index, int& algPort)
{
  vtkAlgorithmOutput* aoutput = this->GetInputConnection(port, index);
  if (!aoutput)
  {
    return nullptr;
  }
  algPort = aoutput->GetIndex();
  return aoutput->GetProducer();
}

//------------------------------------------------------------------------------
vtkExecutive* vtkAlgorithm::GetInputExecutive(int port, int index)
{
  if (index < 0 || index >= this->GetNumberOfInputConnections(port))
  {
    vtkErrorMacro("Attempt to get connection index "
      << index << " for input port " << port << ", which has "
      << this->GetNumberOfInputConnections(port) << " connections.");
    return nullptr;
  }
  if (vtkInformation* info = this->GetExecutive()->GetInputInformation(port, index))
  {
    // Get the executive producing this input.  If there is none, then
    // it is a nullptr input.
    vtkExecutive* producer;
    int producerPort;
    vtkExecutive::PRODUCER()->Get(info, producer, producerPort);
    return producer;
  }
  return nullptr;
}

//------------------------------------------------------------------------------
vtkAlgorithmOutput* vtkAlgorithm::GetInputConnection(int port, int index)
{
  if (port < 0 || port >= this->GetNumberOfInputPorts())
  {
    vtkErrorMacro("Attempt to get connection index " << index << " for input port " << port
                                                     << ", for an algorithm with "
                                                     << this->GetNumberOfInputPorts() << " ports.");
    return nullptr;
  }
  if (index < 0 || index >= this->GetNumberOfInputConnections(port))
  {
    return nullptr;
  }
  if (vtkInformation* info = this->GetExecutive()->GetInputInformation(port, index))
  {
    // Get the executive producing this input.  If there is none, then
    // it is a nullptr input.
    vtkExecutive* producer;
    int producerPort;
    vtkExecutive::PRODUCER()->Get(info, producer, producerPort);
    if (producer)
    {
      return producer->GetAlgorithm()->GetOutputPort(producerPort);
    }
  }
  return nullptr;
}

//------------------------------------------------------------------------------
void vtkAlgorithm::Update()
{
  int port = -1;
  if (this->GetNumberOfOutputPorts())
  {
    port = 0;
  }
  this->Update(port);
}

//------------------------------------------------------------------------------
void vtkAlgorithm::Update(int port)
{
  this->GetExecutive()->Update(port);
}

//------------------------------------------------------------------------------
vtkTypeBool vtkAlgorithm::Update(int port, vtkInformationVector* requests)
{
  vtkStreamingDemandDrivenPipeline* sddp =
    vtkStreamingDemandDrivenPipeline::SafeDownCast(this->GetExecutive());
  if (sddp)
  {
    return sddp->Update(port, requests);
  }
  else
  {
    return this->GetExecutive()->Update(port);
  }
}

//------------------------------------------------------------------------------
vtkTypeBool vtkAlgorithm::Update(vtkInformation* requests)
{
  vtkNew<vtkInformationVector> reqs;
  reqs->SetInformationObject(0, requests);
  return this->Update(0, reqs);
}

//------------------------------------------------------------------------------
int vtkAlgorithm::UpdatePiece(int piece, int numPieces, int ghostLevels, const int extents[6])
{
  typedef vtkStreamingDemandDrivenPipeline vtkSDDP;

  vtkNew<vtkInformation> reqs;
  reqs->Set(vtkSDDP::UPDATE_PIECE_NUMBER(), piece);
  reqs->Set(vtkSDDP::UPDATE_NUMBER_OF_PIECES(), numPieces);
  reqs->Set(vtkSDDP::UPDATE_NUMBER_OF_GHOST_LEVELS(), ghostLevels);
  if (extents)
  {
    reqs->Set(vtkSDDP::UPDATE_EXTENT(), extents, 6);
  }
  return this->Update(reqs);
}

//------------------------------------------------------------------------------
int vtkAlgorithm::UpdateExtent(const int extents[6])
{
  typedef vtkStreamingDemandDrivenPipeline vtkSDDP;

  vtkNew<vtkInformation> reqs;
  reqs->Set(vtkSDDP::UPDATE_EXTENT(), extents, 6);
  return this->Update(reqs);
}

//------------------------------------------------------------------------------
int vtkAlgorithm::UpdateTimeStep(
  double time, int piece, int numPieces, int ghostLevels, const int extents[6])
{
  typedef vtkStreamingDemandDrivenPipeline vtkSDDP;

  vtkNew<vtkInformation> reqs;
  reqs->Set(vtkSDDP::UPDATE_TIME_STEP(), time);
  if (piece >= 0)
  {
    reqs->Set(vtkSDDP::UPDATE_PIECE_NUMBER(), piece);
    reqs->Set(vtkSDDP::UPDATE_NUMBER_OF_PIECES(), numPieces);
    reqs->Set(vtkSDDP::UPDATE_NUMBER_OF_GHOST_LEVELS(), ghostLevels);
  }
  if (extents)
  {
    reqs->Set(vtkSDDP::UPDATE_EXTENT(), extents, 6);
  }
  return this->Update(reqs);
}

//------------------------------------------------------------------------------
void vtkAlgorithm::PropagateUpdateExtent()
{
  this->UpdateInformation();

  vtkStreamingDemandDrivenPipeline* sddp =
    vtkStreamingDemandDrivenPipeline::SafeDownCast(this->GetExecutive());
  if (sddp)
  {
    sddp->PropagateUpdateExtent(-1);
  }
}

//------------------------------------------------------------------------------
void vtkAlgorithm::UpdateInformation()
{
  vtkDemandDrivenPipeline* ddp = vtkDemandDrivenPipeline::SafeDownCast(this->GetExecutive());
  if (ddp)
  {
    ddp->UpdateInformation();
  }
}

//------------------------------------------------------------------------------
void vtkAlgorithm::UpdateDataObject()
{
  vtkDemandDrivenPipeline* ddp = vtkDemandDrivenPipeline::SafeDownCast(this->GetExecutive());
  if (ddp)
  {
    ddp->UpdateDataObject();
  }
}

//------------------------------------------------------------------------------
void vtkAlgorithm::UpdateWholeExtent()
{
  vtkStreamingDemandDrivenPipeline* sddp =
    vtkStreamingDemandDrivenPipeline::SafeDownCast(this->GetExecutive());
  if (sddp)
  {
    sddp->UpdateWholeExtent();
  }
  else
  {
    this->Update();
  }
}

//------------------------------------------------------------------------------
void vtkAlgorithm::ConvertTotalInputToPortConnection(int ind, int& port, int& conn)
{
  port = 0;
  conn = 0;
  while (ind && port < this->GetNumberOfInputPorts())
  {
    int pNumCon;
    pNumCon = this->GetNumberOfInputConnections(port);
    if (ind >= pNumCon)
    {
      port++;
      ind -= pNumCon;
    }
    else
    {
      return;
    }
  }
}

//------------------------------------------------------------------------------
void vtkAlgorithm::ReleaseDataFlagOn()
{
  if (vtkDemandDrivenPipeline* ddp = vtkDemandDrivenPipeline::SafeDownCast(this->GetExecutive()))
  {
    for (int i = 0; i < this->GetNumberOfOutputPorts(); ++i)
    {
      ddp->SetReleaseDataFlag(i, 1);
    }
  }
}

//------------------------------------------------------------------------------
void vtkAlgorithm::ReleaseDataFlagOff()
{
  if (vtkDemandDrivenPipeline* ddp = vtkDemandDrivenPipeline::SafeDownCast(this->GetExecutive()))
  {
    for (int i = 0; i < this->GetNumberOfOutputPorts(); ++i)
    {
      ddp->SetReleaseDataFlag(i, 0);
    }
  }
}

//------------------------------------------------------------------------------
void vtkAlgorithm::SetReleaseDataFlag(vtkTypeBool val)
{
  if (vtkDemandDrivenPipeline* ddp = vtkDemandDrivenPipeline::SafeDownCast(this->GetExecutive()))
  {
    for (int i = 0; i < this->GetNumberOfOutputPorts(); ++i)
    {
      ddp->SetReleaseDataFlag(i, val);
    }
  }
}

//------------------------------------------------------------------------------
vtkTypeBool vtkAlgorithm::GetReleaseDataFlag()
{
  if (vtkDemandDrivenPipeline* ddp = vtkDemandDrivenPipeline::SafeDownCast(this->GetExecutive()))
  {
    return ddp->GetReleaseDataFlag(0);
  }
  return 0;
}

//------------------------------------------------------------------------------
int vtkAlgorithm::UpdateExtentIsEmpty(vtkInformation* pinfo, vtkDataObject* output)
{
  if (output == nullptr)
  {
    return 1;
  }

  // get the pinfo object then call the info signature
  return this->UpdateExtentIsEmpty(
    pinfo, output->GetInformation()->Get(vtkDataObject::DATA_EXTENT_TYPE()));
}
//------------------------------------------------------------------------------
int vtkAlgorithm::UpdateExtentIsEmpty(vtkInformation* info, int extentType)
{
  if (!info)
  {
    return 1;
  }

  int* ext;

  switch (extentType)
  {
    case VTK_PIECES_EXTENT:
      // Special way of asking for no input.
      if (info->Get(vtkStreamingDemandDrivenPipeline::UPDATE_NUMBER_OF_PIECES()) == 0)
      {
        return 1;
      }
      break;

    case VTK_3D_EXTENT:
      ext = info->Get(vtkStreamingDemandDrivenPipeline::UPDATE_EXTENT());
      // Special way of asking for no input. (zero volume)
      if (!ext || ext[0] == (ext[1] + 1) || ext[2] == (ext[3] + 1) || ext[4] == (ext[5] + 1))
      {
        return 1;
      }
      break;

      // We should never have this case occur
    default:
      vtkErrorMacro(<< "Internal error - invalid extent type!");
      break;
  }

  return 0;
}

//------------------------------------------------------------------------------
void vtkAlgorithm::SetProgressText(const char* ptext)
{
  if (!this->ProgressText && !ptext)
  {
    return;
  }
  if (this->ProgressText && ptext && (strcmp(this->ProgressText, ptext)) == 0)
  {
    return;
  }
  delete[] this->ProgressText;
  this->ProgressText = vtksys::SystemTools::DuplicateString(ptext);
}

// This is here to shut off warnings about deprecated functions
// calling deprecated functions.
#if defined(__GNUC__) && !defined(__INTEL_COMPILER)
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

#ifdef _MSC_VER
#pragma warning(disable : 4996)
#endif

//------------------------------------------------------------------------------
int* vtkAlgorithm::GetUpdateExtent(int port)
{
  if (this->GetOutputInformation(port))
  {
    return vtkStreamingDemandDrivenPipeline::GetUpdateExtent(this->GetOutputInformation(port));
  }
  return nullptr;
}

//------------------------------------------------------------------------------
void vtkAlgorithm::GetUpdateExtent(int port, int& x0, int& x1, int& y0, int& y1, int& z0, int& z1)
{
  if (this->GetOutputInformation(port))
  {
    int extent[6];
    vtkStreamingDemandDrivenPipeline::GetUpdateExtent(this->GetOutputInformation(port), extent);
    x0 = extent[0];
    x1 = extent[1];
    y0 = extent[2];
    y1 = extent[3];
    z0 = extent[4];
    z1 = extent[5];
  }
}

//------------------------------------------------------------------------------
void vtkAlgorithm::GetUpdateExtent(int port, int extent[6])
{
  if (this->GetOutputInformation(port))
  {
    vtkStreamingDemandDrivenPipeline::GetUpdateExtent(this->GetOutputInformation(port), extent);
  }
}

//------------------------------------------------------------------------------
int vtkAlgorithm::GetUpdatePiece(int port)
{
  if (this->GetOutputInformation(port))
  {
    return vtkStreamingDemandDrivenPipeline::GetUpdatePiece(this->GetOutputInformation(port));
  }
  return 0;
}

//------------------------------------------------------------------------------
int vtkAlgorithm::GetUpdateNumberOfPieces(int port)
{
  if (this->GetOutputInformation(port))
  {
    return vtkStreamingDemandDrivenPipeline::GetUpdateNumberOfPieces(
      this->GetOutputInformation(port));
  }
  return 1;
}

//------------------------------------------------------------------------------
int vtkAlgorithm::GetUpdateGhostLevel(int port)
{
  if (this->GetOutputInformation(port))
  {
    return vtkStreamingDemandDrivenPipeline::GetUpdateGhostLevel(this->GetOutputInformation(port));
  }
  return 0;
}

//------------------------------------------------------------------------------
void vtkAlgorithm::SetInputDataObject(int port, vtkDataObject* input)
{
  if (input == nullptr)
  {
    // Setting a nullptr input removes the connection.
    this->SetInputConnection(port, nullptr);
    return;
  }

  // We need to setup a trivial producer connection. However, we need to ensure
  // that the input is indeed different from what's currently setup otherwise
  // the algorithm will be modified unnecessarily. This will make it possible
  // for users to call SetInputData(..) with the same data-output and not have
  // the filter re-execute unless the data really changed.

  if (!this->InputPortIndexInRange(port, "connect"))
  {
    return;
  }

  if (this->GetNumberOfInputConnections(port) == 1)
  {
    vtkAlgorithmOutput* current = this->GetInputConnection(port, 0);
    vtkAlgorithm* producer = current ? current->GetProducer() : nullptr;
    if (vtkTrivialProducer::SafeDownCast(producer) && producer->GetOutputDataObject(0) == input)
    {
      // the data object is unchanged. Nothing to do here.
      return;
    }
  }

  vtkTrivialProducer* tp = vtkTrivialProducer::New();
  tp->SetOutput(input);
  this->SetInputConnection(port, tp->GetOutputPort());
  tp->Delete();
}

//------------------------------------------------------------------------------
void vtkAlgorithm::AddInputDataObject(int port, vtkDataObject* input)
{
  if (input)
  {
    vtkTrivialProducer* tp = vtkTrivialProducer::New();
    tp->SetOutput(input);
    this->AddInputConnection(port, tp->GetOutputPort());
    tp->Delete();
  }
}
VTK_ABI_NAMESPACE_END
