// SPDX-FileCopyrightText: Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
// SPDX-License-Identifier: BSD-3-Clause
/**
 * @class   vtkPoints
 * @brief   represent and manipulate 3D points
 *
 * vtkPoints represents 3D points. The data model for vtkPoints is an
 * array of vx-vy-vz triplets accessible by (point or cell) id.
 */

#ifndef vtkPoints_h
#define vtkPoints_h

#include "vtkCommonCoreModule.h" // For export macro
#include "vtkObject.h"
#include "vtkWrappingHints.h" // For VTK_MARSHALAUTO

#include "vtkDataArray.h" // Needed for inline methods

VTK_ABI_NAMESPACE_BEGIN
class vtkIdList;

class VTKCOMMONCORE_EXPORT VTK_MARSHALAUTO vtkPoints : public vtkObject
{
public:
  static vtkPoints* New(int dataType);

  static vtkPoints* New();

  vtkTypeMacro(vtkPoints, vtkObject);
  void PrintSelf(ostream& os, vtkIndent indent) override;

  /**
   * Allocate initial memory size. ext is no longer used.
   */
  virtual vtkTypeBool Allocate(vtkIdType sz, vtkIdType ext = 1000);

  /**
   * Return object to instantiated state.
   */
  virtual void Initialize();

  /**
   * Set/Get the underlying data array. This function must be implemented
   * in a concrete subclass to check for consistency. (The tuple size must
   * match the type of data. For example, 3-tuple data array can be assigned to
   * a vector, normal, or points object, but not a tensor object, which has a
   * tuple dimension of 9. Scalars, on the other hand, can have tuple dimension
   * from 1-4, depending on the type of scalar.)
   */
  virtual void SetData(vtkDataArray*);
  vtkDataArray* GetData() { return this->Data; }

  /**
   * Return the underlying data type. An integer indicating data type is
   * returned as specified in vtkSetGet.h.
   */
  virtual int GetDataType() const;

  /**
   * Specify the underlying data type of the object.
   * Default is VTK_FLOAT.
   */
  virtual void SetDataType(int dataType);
  void SetDataTypeToBit() { this->SetDataType(VTK_BIT); }
  void SetDataTypeToChar() { this->SetDataType(VTK_CHAR); }
  void SetDataTypeToUnsignedChar() { this->SetDataType(VTK_UNSIGNED_CHAR); }
  void SetDataTypeToShort() { this->SetDataType(VTK_SHORT); }
  void SetDataTypeToUnsignedShort() { this->SetDataType(VTK_UNSIGNED_SHORT); }
  void SetDataTypeToInt() { this->SetDataType(VTK_INT); }
  void SetDataTypeToUnsignedInt() { this->SetDataType(VTK_UNSIGNED_INT); }
  void SetDataTypeToLong() { this->SetDataType(VTK_LONG); }
  void SetDataTypeToUnsignedLong() { this->SetDataType(VTK_UNSIGNED_LONG); }
  void SetDataTypeToFloat() { this->SetDataType(VTK_FLOAT); }
  void SetDataTypeToDouble() { this->SetDataType(VTK_DOUBLE); }

  /**
   * Return a void pointer. For image pipeline interface and other
   * special pointer manipulation.
   */
  void* GetVoidPointer(const int id) { return this->Data->GetVoidPointer(id); }

  /**
   * Reclaim any extra memory.
   */
  virtual void Squeeze() { this->Data->Squeeze(); }

  /**
   * Make object look empty but do not delete memory.
   */
  virtual void Reset();

  ///@{
  /**
   * Different ways to copy data. Shallow copy does reference count (i.e.,
   * assigns pointers and updates reference count); deep copy runs through
   * entire data array assigning values.
   */
  virtual void DeepCopy(vtkPoints* ad);
  virtual void ShallowCopy(vtkPoints* ad);
  ///@}

  /**
   * Return the memory in kibibytes (1024 bytes) consumed by this attribute data.
   * Used to support streaming and reading/writing data. The value
   * returned is guaranteed to be greater than or equal to the
   * memory required to actually represent the data represented
   * by this object. The information returned is valid only after
   * the pipeline has been updated.
   */
  unsigned long GetActualMemorySize();

  /**
   * Return number of points in array.
   */
  VTK_MARSHALEXCLUDE(VTK_MARSHAL_EXCLUDE_REASON_IS_REDUNDANT)
  vtkIdType GetNumberOfPoints() const { return this->Data->GetNumberOfTuples(); }

  /**
   * Return a pointer to a double point x[3] for a specific id.
   * WARNING: Just don't use this error-prone method, the returned pointer
   * and its values are only valid as long as another method invocation is not
   * performed. Prefer GetPoint() with the return value in argument.
   */
  VTK_MARSHALEXCLUDE(VTK_MARSHAL_EXCLUDE_REASON_IS_REDUNDANT)
  double* GetPoint(vtkIdType id) VTK_EXPECTS(0 <= id && id < GetNumberOfPoints()) VTK_SIZEHINT(3)
  {
    return this->Data->GetTuple(id);
  }

  /**
   * Copy point components into user provided array v[3] for specified
   * id.
   */
  VTK_MARSHALEXCLUDE(VTK_MARSHAL_EXCLUDE_REASON_IS_REDUNDANT)
  void GetPoint(vtkIdType id, double x[3]) VTK_EXPECTS(0 <= id && id < GetNumberOfPoints())
    VTK_SIZEHINT(3)
  {
    this->Data->GetTuple(id, x);
  }

  /**
   * Insert point into object. No range checking performed (fast!).
   * Make sure you use SetNumberOfPoints() to allocate memory prior
   * to using SetPoint(). You should call Modified() finally after
   * changing points using this method as it will not do it itself.
   */
  VTK_MARSHALEXCLUDE(VTK_MARSHAL_EXCLUDE_REASON_IS_REDUNDANT)
  void SetPoint(vtkIdType id, const float x[3]) VTK_EXPECTS(0 <= id && id < GetNumberOfPoints())
  {
    this->Data->SetTuple(id, x);
  }
  VTK_MARSHALEXCLUDE(VTK_MARSHAL_EXCLUDE_REASON_IS_REDUNDANT)
  void SetPoint(vtkIdType id, const double x[3]) VTK_EXPECTS(0 <= id && id < GetNumberOfPoints())
  {
    this->Data->SetTuple(id, x);
  }
  VTK_MARSHALEXCLUDE(VTK_MARSHAL_EXCLUDE_REASON_IS_REDUNDANT)
  void SetPoint(vtkIdType id, double x, double y, double z)
    VTK_EXPECTS(0 <= id && id < GetNumberOfPoints());

  ///@{
  /**
   * Insert point into object. Range checking performed and memory
   * allocated as necessary.
   */
  void InsertPoint(vtkIdType id, const float x[3]) VTK_EXPECTS(0 <= id)
  {
    this->Data->InsertTuple(id, x);
  }
  void InsertPoint(vtkIdType id, const double x[3]) VTK_EXPECTS(0 <= id)
  {
    this->Data->InsertTuple(id, x);
  }
  void InsertPoint(vtkIdType id, double x, double y, double z) VTK_EXPECTS(0 <= id);
  ///@}

  /**
   * Copy the points indexed in srcIds from the source array to the tuple
   * locations indexed by dstIds in this array.
   * Note that memory allocation is performed as necessary to hold the data.
   */
  void InsertPoints(vtkIdList* dstIds, vtkIdList* srcIds, vtkPoints* source)
  {
    this->Data->InsertTuples(dstIds, srcIds, source->Data);
  }

  /**
   * Copy n consecutive points starting at srcStart from the source array to
   * this array, starting at the dstStart location.
   * Note that memory allocation is performed as necessary to hold the data.
   */
  void InsertPoints(vtkIdType dstStart, vtkIdType n, vtkIdType srcStart, vtkPoints* source)
  {
    this->Data->InsertTuples(dstStart, n, srcStart, source->Data);
  }

  /**
   * Insert point into next available slot. Returns id of slot.
   */
  vtkIdType InsertNextPoint(const float x[3]) { return this->Data->InsertNextTuple(x); }
  vtkIdType InsertNextPoint(const double x[3]) { return this->Data->InsertNextTuple(x); }
  vtkIdType InsertNextPoint(double x, double y, double z);

  /**
   * Specify the number of points for this object to hold. Does an
   * allocation as well as setting the MaxId ivar. Used in conjunction with
   * SetPoint() method for fast insertion.
   */
  VTK_MARSHALEXCLUDE(VTK_MARSHAL_EXCLUDE_REASON_IS_REDUNDANT)
  void SetNumberOfPoints(vtkIdType numPoints);

  /**
   * Resize the internal array while conserving the data.  Returns 1 if
   * resizing succeeded (including shrinking) and 0 (or throw std::bad_alloc
   * based on VTK_DONT_THROW_BAD_ALLOC configuration) otherwise.
   */
  vtkTypeBool Resize(vtkIdType numPoints);

  /**
   * Given a list of pt ids, return an array of points.
   */
  void GetPoints(vtkIdList* ptId, vtkPoints* outPoints);

  /**
   * Determine (xmin,xmax, ymin,ymax, zmin,zmax) bounds of points.
   */
  virtual void ComputeBounds();

  /**
   * Return the bounds of the points.
   */
  double* GetBounds() VTK_SIZEHINT(6);

  /**
   * Return the bounds of the points.
   */
  void GetBounds(double bounds[6]);

  /**
   * The modified time of the points.
   */
  vtkMTimeType GetMTime() override;

  /**
   * Update the modification time for this object and its Data.
   * As this object acts as a shell around a DataArray and
   * forwards Set methods it needs to forward Modified as well.
   */
  void Modified() override;

protected:
  vtkPoints(int dataType = VTK_FLOAT);
  ~vtkPoints() override;

  double Bounds[6];
  vtkTimeStamp ComputeTime; // Time at which bounds computed
  vtkDataArray* Data;       // Array which represents data

private:
  vtkPoints(const vtkPoints&) = delete;
  void operator=(const vtkPoints&) = delete;
};

inline void vtkPoints::Reset()
{
  this->Data->Reset();
  this->Modified();
}

inline void vtkPoints::SetNumberOfPoints(vtkIdType numPoints)
{
  if (numPoints != this->Data->GetNumberOfTuples())
  {
    this->Data->SetNumberOfComponents(3);
    this->Data->SetNumberOfTuples(numPoints);
    this->Modified();
  }
}

inline vtkTypeBool vtkPoints::Resize(vtkIdType numPoints)
{
  if (numPoints != this->Data->GetNumberOfTuples())
  {
    this->Data->SetNumberOfComponents(3);
    this->Modified();
    return this->Data->Resize(numPoints);
  }
  return 1;
}

inline void vtkPoints::SetPoint(vtkIdType id, double x, double y, double z)
{
  double p[3] = { x, y, z };
  this->Data->SetTuple(id, p);
}

inline void vtkPoints::InsertPoint(vtkIdType id, double x, double y, double z)
{
  double p[3] = { x, y, z };
  this->Data->InsertTuple(id, p);
}

inline vtkIdType vtkPoints::InsertNextPoint(double x, double y, double z)
{
  double p[3] = { x, y, z };
  return this->Data->InsertNextTuple(p);
}

VTK_ABI_NAMESPACE_END
#endif
