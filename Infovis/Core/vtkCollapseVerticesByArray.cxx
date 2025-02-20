// SPDX-FileCopyrightText: Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
// SPDX-License-Identifier: BSD-3-Clause

#include "vtkCollapseVerticesByArray.h"

#include "vtkAbstractArray.h"
#include "vtkDataArray.h"
#include "vtkDataObject.h"
#include "vtkDataSetAttributes.h"
#include "vtkDoubleArray.h"
#include "vtkEdgeListIterator.h"
#include "vtkGraphEdge.h"
#include "vtkInformation.h"
#include "vtkInformationVector.h"
#include "vtkIntArray.h"
#include "vtkMutableDirectedGraph.h"
#include "vtkObjectFactory.h"
#include "vtkOutEdgeIterator.h"
#include "vtkSmartPointer.h"
#include "vtkStringArray.h"
#include "vtkVertexListIterator.h"

#include <map>    // Using STL.
#include <string> // Using STL.
#include <vector> // Using STL.

VTK_ABI_NAMESPACE_BEGIN
vtkStandardNewMacro(vtkCollapseVerticesByArray);

//------------------------------------------------------------------------------
class vtkCollapseVerticesByArrayInternal
{
public:
  std::vector<std::string> AggregateEdgeArrays;
};

//------------------------------------------------------------------------------
vtkCollapseVerticesByArray::vtkCollapseVerticesByArray()
  : AllowSelfLoops(false)
  , VertexArray(nullptr)
  , CountEdgesCollapsed(false)
  , EdgesCollapsedArray(nullptr)
  , CountVerticesCollapsed(false)
  , VerticesCollapsedArray(nullptr)
{
  // Setting default names.
  this->SetVerticesCollapsedArray("VerticesCollapsedCountArray");
  this->SetEdgesCollapsedArray("EdgesCollapsedCountArray");

  this->Internal = new vtkCollapseVerticesByArrayInternal();
}

//------------------------------------------------------------------------------
vtkCollapseVerticesByArray::~vtkCollapseVerticesByArray()
{
  delete this->Internal;
  delete[] this->VertexArray;
  delete[] this->VerticesCollapsedArray;
  delete[] this->EdgesCollapsedArray;
}

//------------------------------------------------------------------------------
void vtkCollapseVerticesByArray::PrintSelf(ostream& os, vtkIndent indent)
{
  // Base class print.
  vtkGraphAlgorithm::PrintSelf(os, indent);

  os << indent << "AllowSelfLoops: " << this->AllowSelfLoops << endl;
  os << indent << "VertexArray: " << (this->VertexArray ? this->VertexArray : "nullptr") << endl;

  os << indent << "CountEdgesCollapsed: " << this->CountEdgesCollapsed << endl;
  os << indent << "EdgesCollapsedArray: "
     << (this->EdgesCollapsedArray ? this->EdgesCollapsedArray : "nullptr") << endl;

  os << indent << "CountVerticesCollapsed: " << this->CountVerticesCollapsed << endl;
  os << indent << "VerticesCollapsedArray: "
     << (this->VerticesCollapsedArray ? this->VerticesCollapsedArray : "nullptr") << endl;
}

//------------------------------------------------------------------------------
void vtkCollapseVerticesByArray::AddAggregateEdgeArray(const char* arrName)
{
  this->Internal->AggregateEdgeArrays.emplace_back(arrName);
}

//------------------------------------------------------------------------------
void vtkCollapseVerticesByArray::ClearAggregateEdgeArray()
{
  this->Internal->AggregateEdgeArrays.clear();
}

//------------------------------------------------------------------------------
int vtkCollapseVerticesByArray::RequestData(vtkInformation* vtkNotUsed(request),
  vtkInformationVector** inputVector, vtkInformationVector* outputVector)
{
  vtkInformation* inInfo = inputVector[0]->GetInformationObject(0);

  if (!inInfo)
  {
    vtkErrorMacro("Error: nullptr input vtkInformation");
    return 0;
  }

  vtkDataObject* inObj = inInfo->Get(vtkDataObject::DATA_OBJECT());

  if (!inObj)
  {
    vtkErrorMacro(<< "Error: nullptr vtkDataObject");
    return 0;
  }

  vtkInformation* outInfo = outputVector->GetInformationObject(0);
  if (!outInfo)
  {
    vtkErrorMacro("Error: nullptr output vtkInformation");
    return 0;
  }

  vtkDataObject* outObj = outInfo->Get(vtkDataObject::DATA_OBJECT());
  if (!outObj)
  {
    vtkErrorMacro("Error: nullptr output vtkDataObject");
    return 0;
  }

  vtkGraph* outGraph = this->Create(vtkGraph::SafeDownCast(inObj));
  if (outGraph)
  {
    vtkDirectedGraph::SafeDownCast(outObj)->ShallowCopy(outGraph);
    outGraph->Delete();
  }
  else
  {
    return 0;
  }

  return 1;
}

//------------------------------------------------------------------------------
int vtkCollapseVerticesByArray::FillOutputPortInformation(
  int vtkNotUsed(port), vtkInformation* info)
{
  info->Set(vtkDataObject::DATA_TYPE_NAME(), "vtkDirectedGraph");
  return 1;
}

//------------------------------------------------------------------------------
vtkGraph* vtkCollapseVerticesByArray::Create(vtkGraph* inGraph)
{
  if (!inGraph)
  {
    return nullptr;
  }

  if (!this->VertexArray)
  {
    return nullptr;
  }

  typedef vtkSmartPointer<vtkMutableDirectedGraph> vtkMutableDirectedGraphRefPtr;
  typedef vtkSmartPointer<vtkEdgeListIterator> vtkEdgeListIteratorRefPtr;
  typedef vtkSmartPointer<vtkVertexListIterator> vtkVertexListIteratorRefPtr;
  typedef vtkSmartPointer<vtkIntArray> vtkIntArrayRefPtr;
  typedef std::pair<vtkVariant, vtkIdType> NameIdPair;

  // Create a new merged graph.
  vtkMutableDirectedGraphRefPtr outGraph(vtkMutableDirectedGraphRefPtr::New());

  vtkVertexListIteratorRefPtr itr(vtkVertexListIteratorRefPtr::New());
  itr->SetGraph(inGraph);

  // Copy the input vertex data and edge data arrays to the
  // output graph vertex and edge data.
  outGraph->GetVertexData()->CopyAllocate(inGraph->GetVertexData());
  outGraph->GetEdgeData()->CopyAllocate(inGraph->GetEdgeData());

  vtkDataSetAttributes* inVtxDsAttrs = inGraph->GetVertexData();
  vtkDataSetAttributes* inEgeDsAttrs = inGraph->GetEdgeData();

  if (!inVtxDsAttrs)
  {
    vtkErrorMacro("Error: No vertex data found on the graph.");
    return nullptr;
  }

  // Find the vertex array of interest.
  vtkAbstractArray* inVertexAOI = inVtxDsAttrs->GetAbstractArray(this->VertexArray);

  // Cannot proceed if vertex array of interest is not found.
  if (!inVertexAOI)
  {
    vtkErrorMacro("Error: Could not find the key vertex array.");
    return nullptr;
  }

  // Optional.
  vtkIntArrayRefPtr countEdgesCollapsedArray(nullptr);
  if (this->CountEdgesCollapsed)
  {
    countEdgesCollapsedArray = vtkIntArrayRefPtr::New();
    countEdgesCollapsedArray->SetName(this->EdgesCollapsedArray);
    countEdgesCollapsedArray->SetNumberOfComponents(1);
    outGraph->GetEdgeData()->AddArray(countEdgesCollapsedArray);
  }

  // Optional.
  vtkIntArrayRefPtr countVerticesCollapsedArray(nullptr);
  if (this->CountVerticesCollapsed)
  {
    countVerticesCollapsedArray = vtkIntArrayRefPtr::New();
    countVerticesCollapsedArray->SetName(this->VerticesCollapsedArray);
    countVerticesCollapsedArray->SetNumberOfComponents(1);
    outGraph->GetVertexData()->AddArray(countVerticesCollapsedArray);
  }

  // Arrays of interest.
  std::vector<vtkDataArray*> inEdgeDataArraysOI;
  std::vector<vtkDataArray*> outEdgeDataArraysOI;

  // All other arrays.
  std::vector<vtkAbstractArray*> inEdgeDataArraysAO;
  std::vector<vtkAbstractArray*> outEdgeDataArraysAO;

  std::vector<vtkAbstractArray*> inVertexDataArraysAO;
  std::vector<vtkAbstractArray*> outVertexDataArraysAO;

  //++
  // Find all the input vertex data arrays except the one set as key.
  for (int i = 0; i < inVtxDsAttrs->GetNumberOfArrays(); ++i)
  {
    vtkAbstractArray* absArray = inVtxDsAttrs->GetAbstractArray(i);
    if (strcmp(absArray->GetName(), inVertexAOI->GetName()) == 0)
    {
      continue;
    }

    inVertexDataArraysAO.push_back(absArray);
  }

  for (size_t i = 0; i < inVertexDataArraysAO.size(); ++i)
  {
    if (!inVertexDataArraysAO[i]->GetName())
    {
      vtkErrorMacro("Error: Name on the array is nullptr or not set.");
      return nullptr;
    }

    outVertexDataArraysAO.push_back(
      outGraph->GetVertexData()->GetAbstractArray(inVertexDataArraysAO[i]->GetName()));
    outVertexDataArraysAO.back()->SetNumberOfTuples(inVertexDataArraysAO[i]->GetNumberOfTuples());
  }
  //--

  //++
  // Find all the input edge data arrays of interest or not.
  for (int i = 0; i < inEgeDsAttrs->GetNumberOfArrays(); ++i)
  {
    vtkAbstractArray* absArray = inEgeDsAttrs->GetAbstractArray(i);

    bool alreadyAdded(false);
    for (size_t j = 0; j < this->Internal->AggregateEdgeArrays.size(); ++j)
    {
      if (absArray->GetName() == this->Internal->AggregateEdgeArrays[j])
      {
        vtkDataArray* inDataArray = vtkArrayDownCast<vtkDataArray>(absArray);
        if (inDataArray)
        {
          inEdgeDataArraysOI.push_back(inDataArray);
        }
        else
        {
          inEdgeDataArraysAO.push_back(absArray);
        }
        alreadyAdded = true;
        break;
      }
      else
      {
        continue;
      }
    } // End inner for.

    if (!alreadyAdded)
    {
      inEdgeDataArraysAO.push_back(absArray);
    }
  }

  // Find the corresponding empty arrays in output graph.
  vtkAbstractArray* outVertexAOI(outGraph->GetVertexData()->GetAbstractArray(this->VertexArray));

  // Arrays of interest.
  if (!inEdgeDataArraysOI.empty())
  {
    for (size_t i = 0; i < inEdgeDataArraysOI.size(); ++i)
    {
      if (!inEdgeDataArraysOI[i]->GetName())
      {
        vtkErrorMacro("Error: Name on the array is nullptr or not set.");
        return nullptr;
      }

      vtkDataArray* outDataArray = vtkArrayDownCast<vtkDataArray>(
        outGraph->GetEdgeData()->GetAbstractArray(inEdgeDataArraysOI[i]->GetName()));

      outEdgeDataArraysOI.push_back(outDataArray);
      outDataArray->SetNumberOfTuples(inEdgeDataArraysOI[i]->GetNumberOfTuples());
    }
  }

  // All others.
  if (!inEdgeDataArraysAO.empty())
  {
    for (size_t i = 0; i < inEdgeDataArraysAO.size(); ++i)
    {
      if (!inEdgeDataArraysAO[i]->GetName())
      {
        vtkErrorMacro("Error: Name on the array is nullptr or not set.");
        return nullptr;
      }

      vtkAbstractArray* outAbsArray =
        outGraph->GetEdgeData()->GetAbstractArray(inEdgeDataArraysAO[i]->GetName());

      outEdgeDataArraysAO.push_back(outAbsArray);
      outAbsArray->SetNumberOfTuples(inEdgeDataArraysAO[i]->GetNumberOfTuples());
    }
  }
  //--

  std::map<vtkVariant, vtkIdType> myMap;
  std::map<vtkVariant, vtkIdType>::iterator myItr;

  vtkIdType inSourceId;
  vtkIdType inTargetId;
  vtkIdType outSourceId;
  vtkIdType outTargetId;
  vtkIdType outEdgeId;

  // Iterate over all the vertices.
  while (itr->HasNext())
  {
    inSourceId = itr->Next();
    vtkVariant source = inVertexAOI->GetVariantValue(inSourceId);

    myItr = myMap.find(source);

    if (myItr != myMap.end())
    {
      // If we already have a vertex for this "source" get its id.
      outSourceId = myItr->second;
      if (this->CountVerticesCollapsed)
      {
        countVerticesCollapsedArray->SetValue(
          outSourceId, countVerticesCollapsedArray->GetValue(outSourceId) + 1);
      }
    }
    else
    {
      // If not then add a new vertex to the output graph.
      outSourceId = outGraph->AddVertex();
      outVertexAOI->InsertVariantValue(outSourceId, source);
      myMap.insert(NameIdPair(source, outSourceId));

      if (this->CountVerticesCollapsed)
      {
        countVerticesCollapsedArray->InsertValue(outSourceId, 1);
      }
    }

    for (size_t i = 0; i < inVertexDataArraysAO.size(); ++i)
    {
      outVertexDataArraysAO[i]->SetTuple(outSourceId, inSourceId, inVertexDataArraysAO[i]);
    }
  }

  // Now iterate over all the edges in the graph.
  // Result vary depending on whether the input graph is
  // directed or not.
  vtkEdgeListIteratorRefPtr elItr(vtkEdgeListIteratorRefPtr::New());
  inGraph->GetEdges(elItr);

  while (elItr->HasNext())
  {
    vtkGraphEdge* edge = elItr->NextGraphEdge();
    inSourceId = edge->GetSource();
    inTargetId = edge->GetTarget();

    vtkVariant source = inVertexAOI->GetVariantValue(inSourceId);
    vtkVariant target = inVertexAOI->GetVariantValue(inTargetId);

    // Again find if there is associated vertex with this name.
    myItr = myMap.find(source);
    outSourceId = myItr->second;

    myItr = myMap.find(target);
    outTargetId = myItr->second;

    // Find if there is an edge between the out source and target.
    FindEdge(outGraph, outSourceId, outTargetId, outEdgeId);

    if ((outSourceId == outTargetId) && !this->AllowSelfLoops)
    {
      continue;
    }

    //++
    if (outEdgeId == -1)
    {
      outEdgeId = outGraph->AddEdge(outSourceId, outTargetId).Id;

      // Edge does not exist. Add a new one.
      if (inEdgeDataArraysOI.empty() && inEdgeDataArraysAO.empty())
      {
        continue;
      }

      // Arrays of interest.
      for (size_t i = 0; i < inEdgeDataArraysOI.size(); ++i)
      {

        outEdgeDataArraysOI[i]->SetTuple(outEdgeId, edge->GetId(), inEdgeDataArraysOI[i]);
      }

      // All others. Last entered will override previous ones.
      for (size_t i = 0; i < inEdgeDataArraysAO.size(); ++i)
      {
        outEdgeDataArraysAO[i]->SetTuple(outEdgeId, edge->GetId(), inEdgeDataArraysAO[i]);
      }

      if (this->CountEdgesCollapsed)
      {
        countEdgesCollapsedArray->InsertValue(outEdgeId, 1);
      }
    }
    else
    {
      if (inEdgeDataArraysOI.empty() && inEdgeDataArraysAO.empty())
      {
        continue;
      }

      // Find the data on the out edge and add the data from in edge
      // and set it on the out edge.
      for (size_t i = 0; i < inEdgeDataArraysOI.size(); ++i)
      {
        double* outEdgeData = outEdgeDataArraysOI[i]->GetTuple(outEdgeId);
        double* inEdgeData = inEdgeDataArraysOI[i]->GetTuple(edge->GetId());

        if (!outEdgeData && !inEdgeData)
        {
          continue;
        }
        for (int j = 0; j < inEdgeDataArraysOI[i]->GetNumberOfComponents(); ++j)
        {
          outEdgeData[j] = outEdgeData[j] + inEdgeData[j];
        }

        outEdgeDataArraysOI[i]->SetTuple(outEdgeId, outEdgeData);
      }

      // All others. Last entered will override previous ones.
      for (size_t i = 0; i < inEdgeDataArraysAO.size(); ++i)
      {
        outEdgeDataArraysAO[i]->SetTuple(outEdgeId, edge->GetId(), inEdgeDataArraysAO[i]);
      }

      if (this->CountEdgesCollapsed)
      {
        countEdgesCollapsedArray->SetValue(
          outEdgeId, static_cast<int>(countEdgesCollapsedArray->GetValue(outEdgeId) + 1));
      }
    }
    //--

  } // while(elItr->HasNext())

  outGraph->Register(nullptr);
  return outGraph;
}

//------------------------------------------------------------------------------
void vtkCollapseVerticesByArray::FindEdge(
  vtkGraph* outGraph, vtkIdType source, vtkIdType target, vtkIdType& edgeId)
{
  typedef vtkSmartPointer<vtkOutEdgeIterator> vtkOutEdgeIteratorRefPtr;
  edgeId = -1;

  if (!outGraph)
  {
    return;
  }

  vtkOutEdgeIteratorRefPtr itr(vtkOutEdgeIteratorRefPtr::New());

  outGraph->GetOutEdges(source, itr);
  while (itr->HasNext())
  {
    vtkGraphEdge* edge = itr->NextGraphEdge();
    if (edge->GetTarget() == target)
    {
      edgeId = edge->GetId();
      break;
    }
  }
}
VTK_ABI_NAMESPACE_END
