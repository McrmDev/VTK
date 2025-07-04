// SPDX-FileCopyrightText: Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
// SPDX-License-Identifier: BSD-3-Clause
#include "vtkUniformGridAMRDataIterator.h"
#include "vtkAMRDataInternals.h"
#include "vtkAMRMetaData.h"
#include "vtkDataObject.h"
#include "vtkInformation.h"
#include "vtkObjectFactory.h"
#include "vtkOverlappingAMRMetaData.h"
#include "vtkUniformGrid.h"
#include "vtkUniformGridAMR.h"
#include <cassert>

//----------------------------------------------------------------
VTK_ABI_NAMESPACE_BEGIN
class AMRIndexIterator : public vtkObject
{
public:
  static AMRIndexIterator* New();
  vtkTypeMacro(AMRIndexIterator, vtkObject);

  void Initialize(const std::vector<int>* numBlocks)
  {
    assert(numBlocks && !numBlocks->empty());
    this->Level = 0;
    this->Index = -1;
    this->NumBlocks = numBlocks;
    this->NumLevels = this->GetNumberOfLevels();
    this->Next();
  }
  void Next()
  {
    this->AdvanceIndex();
    // advanc the level either when we are at the right level of out of levels
    while (this->Level < this->NumLevels &&
      static_cast<unsigned int>(this->Index) >= this->GetNumberOfBlocks(this->Level + 1))
    {
      this->Level++;
    }
  }
  virtual bool IsDone() { return this->Level >= this->NumLevels; }
  unsigned int GetLevel() { return this->Level; }
  unsigned int GetId() { return this->Index - this->GetNumberOfBlocks(this->Level); }
  virtual unsigned int GetFlatIndex() { return this->Index; }

protected:
  AMRIndexIterator()
    : Level(0)
    , Index(0)
  {
  }
  ~AMRIndexIterator() override = default;
  unsigned int Level;
  int Index;
  unsigned int NumLevels;
  const std::vector<int>* NumBlocks;
  virtual void AdvanceIndex() { this->Index++; }
  virtual unsigned int GetNumberOfLevels()
  {
    return static_cast<unsigned int>(this->NumBlocks->size() - 1);
  }
  virtual unsigned int GetNumberOfBlocks(int i)
  {
    assert(i < static_cast<int>(this->NumBlocks->size()));
    return (*this->NumBlocks)[i];
  }
};
vtkStandardNewMacro(AMRIndexIterator);

//----------------------------------------------------------------

class AMRLoadedDataIndexIterator : public AMRIndexIterator
{
public:
  static AMRLoadedDataIndexIterator* New();
  vtkTypeMacro(AMRLoadedDataIndexIterator, AMRIndexIterator);
  AMRLoadedDataIndexIterator() = default;
  void Initialize(
    const std::vector<int>* numBlocks, const vtkAMRDataInternals::BlockList* dataBlocks)
  {
    assert(numBlocks && !numBlocks->empty());
    this->Level = 0;
    this->InternalIdx = -1;
    this->NumBlocks = numBlocks;
    this->DataBlocks = dataBlocks;
    this->NumLevels = this->GetNumberOfLevels();
    this->Next();
  }

protected:
  void AdvanceIndex() override
  {
    this->InternalIdx++;
    Superclass::Index = static_cast<size_t>(this->InternalIdx) < this->DataBlocks->size()
      ? (*this->DataBlocks)[this->InternalIdx].Index
      : 0;
  }
  bool IsDone() override
  {
    return static_cast<size_t>(this->InternalIdx) >= this->DataBlocks->size();
  }
  const vtkAMRDataInternals::BlockList* DataBlocks;
  int InternalIdx;

private:
  AMRLoadedDataIndexIterator(const AMRLoadedDataIndexIterator&) = delete;
  void operator=(const AMRLoadedDataIndexIterator&) = delete;
};
vtkStandardNewMacro(AMRLoadedDataIndexIterator);

//----------------------------------------------------------------

vtkStandardNewMacro(vtkUniformGridAMRDataIterator);

vtkUniformGridAMRDataIterator::vtkUniformGridAMRDataIterator()
{
  this->Information = vtkSmartPointer<vtkInformation>::New();
  this->AMR = nullptr;
  this->AMRData = nullptr;
  this->AMRMetaData = nullptr;
}

vtkUniformGridAMRDataIterator::~vtkUniformGridAMRDataIterator() = default;

vtkDataObject* vtkUniformGridAMRDataIterator::GetCurrentDataObject()
{
  unsigned int level, id;
  this->GetCurrentIndexPair(level, id);
  vtkDataObject* obj = this->AMR->GetDataSet(level, id);
  return obj;
}

vtkInformation* vtkUniformGridAMRDataIterator::GetCurrentMetaData()
{
  // XXX: This works only for OverlappingAMR
  double bounds[6] = { 0, 0, 0, 0, 0, 0 };
  vtkOverlappingAMRMetaData* oamrMData = vtkOverlappingAMRMetaData::SafeDownCast(this->AMRMetaData);
  if (oamrMData)
  {
    oamrMData->GetBounds(this->GetCurrentLevel(), this->GetCurrentIndex(), bounds);
  }
  this->Information->Set(vtkDataObject::BOUNDING_BOX(), bounds, 6);
  return this->Information;
}

unsigned int vtkUniformGridAMRDataIterator::GetCurrentFlatIndex()
{
  assert(!this->IsDoneWithTraversal());
  return this->Iter->GetFlatIndex();
}

void vtkUniformGridAMRDataIterator::GetCurrentIndexPair(unsigned int& level, unsigned int& id)
{
  level = this->Iter->GetLevel();
  id = this->Iter->GetId();
}

unsigned int vtkUniformGridAMRDataIterator::GetCurrentLevel()
{
  unsigned int level, id;
  this->GetCurrentIndexPair(level, id);
  return level;
}

unsigned int vtkUniformGridAMRDataIterator::GetCurrentIndex()
{
  unsigned int level, id;
  this->GetCurrentIndexPair(level, id);
  return id;
}

void vtkUniformGridAMRDataIterator::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}

void vtkUniformGridAMRDataIterator::GoToFirstItem()
{
  if (!this->DataSet)
  {
    return;
  }
  this->AMR = vtkUniformGridAMR::SafeDownCast(this->DataSet);
  this->AMRMetaData = this->AMR->GetAMRMetaData();
  this->AMRData = this->AMR->AMRData;

  if (this->AMRMetaData)
  {
    if (this->GetSkipEmptyNodes())
    {
      vtkSmartPointer<AMRLoadedDataIndexIterator> itr =
        vtkSmartPointer<AMRLoadedDataIndexIterator>::New();
      itr->Initialize(&this->AMRMetaData->GetNumBlocks(), &this->AMR->AMRData->GetAllBlocks());
      this->Iter = itr;
    }
    else
    {
      this->Iter = vtkSmartPointer<AMRIndexIterator>::New();
      this->Iter->Initialize(&this->AMRMetaData->GetNumBlocks());
    }
  }
}

void vtkUniformGridAMRDataIterator::GoToNextItem()
{
  this->Iter->Next();
}

//------------------------------------------------------------------------------
int vtkUniformGridAMRDataIterator::IsDoneWithTraversal()
{
  return (!this->Iter) || this->Iter->IsDone();
}
VTK_ABI_NAMESPACE_END
