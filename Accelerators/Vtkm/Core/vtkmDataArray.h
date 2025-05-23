// SPDX-FileCopyrightText: Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
// SPDX-FileCopyrightText: Copyright (c) Kitware, Inc.
// SPDX-FileCopyrightText: Copyright 2019 Sandia Corporation.
// SPDX-FileCopyrightText: Copyright 2019 UT-Battelle, LLC.
// SPDX-FileCopyrightText: Copyright 2019 Los Alamos National Security.
// SPDX-License-Identifier: LicenseRef-BSD-3-Clause-Sandia-LANL-USGov
/**
 * @class   vtkmDataArray
 * @brief   Wraps a Viskores `ArrayHandle` inside a sub-class of `vtkGenericDataArray`.
 *
 * vtkmDataArray<T> can be used to wrap an ArrayHandle with base component type of T. It is mainly
 * intended as a way to pass a Viskores ArrayHandle through a VTK pipeline in a zero-copy manner.
 * This is useful for implicit ArrayHandles or when unified memory is not being used. As long as the
 * underlying data is not accessed, device to host copying of the data is avoided. The ComputeRange
 * and ComputeFiniteRange functions have been overloaded to do the computation on the device side
 * using Viskores. This also avoids device-to-host memory transfers for this commonly used
 * operation. Individual elements of the underlying data can be accessed via the
 * `vtkGenericDataArray` API, but there are some limitations to keep in mind:
 * 1. Access can be quite slow compared to direct memory access and thus, should be avoided.
 * 2. Once the underlying data is accessed though this class, any modifications via the ArrayHandle
 *    interface would result in undefined behaviour.
 * 3. Any modifications made through this class' API is not guaranteed to be reflected via the
 *    ArrayHandle interface.
 *
 * @sa vtkGenericDataArray
 */

#ifndef vtkmDataArray_h
#define vtkmDataArray_h

#include "vtkAcceleratorsVTKmCoreModule.h" // For export macro
#include "vtkGenericDataArray.h"
#include "vtkmConfigCore.h" // For template export

#include <viskores/VecTraits.h>               // For viskores::VecTraits
#include <viskores/cont/ArrayHandle.h>        // For viskores::cont::ArrayHandle
#include <viskores/cont/UnknownArrayHandle.h> // For viskores::cont::UnknownArrayHandle

#include <memory> // For std::unique_ptr<>

namespace fromvtkm
{
VTK_ABI_NAMESPACE_BEGIN

template <typename T>
class ArrayHandleHelperBase;

template <typename T>
struct ArrayHandleHelperSwapper;

VTK_ABI_NAMESPACE_END
} // fromvtkm

VTK_ABI_NAMESPACE_BEGIN
template <typename T>
class vtkmDataArray : public vtkGenericDataArray<vtkmDataArray<T>, T>
{
  static_assert(std::is_arithmetic<T>::value, "T must be an integral or floating-point type");

  using GenericDataArrayType = vtkGenericDataArray<vtkmDataArray<T>, T>;
  using SelfType = vtkmDataArray<T>;
  vtkTemplateTypeMacro(SelfType, GenericDataArrayType);

public:
  using typename Superclass::ValueType;

  static vtkmDataArray* New();

  /// @brief Set the Viskores ArrayHandle to be wrapped
  ///
  void SetVtkmArrayHandle(const viskores::cont::UnknownArrayHandle& ah);

  /// @brief Get the underlying ArrayHandle.
  ///
  viskores::cont::UnknownArrayHandle GetVtkmUnknownArrayHandle() const;

  ///@{
  /// If the data in the ArrayHandle has a basic layout, this does a shallow copy.
  /// Otherwise, it does a deep copy.
  void* GetVoidPointer(vtkIdType valueIdx) override;
  void* WriteVoidPointer(vtkIdType valueIdx, vtkIdType numValues) override;
  ///@}

  /// Support methods for \c vtkGenericDataArray.
  ValueType GetValue(vtkIdType valueIdx) const;
  void SetValue(vtkIdType valueIdx, ValueType value);
  void GetTypedTuple(vtkIdType tupleIdx, ValueType* tuple) const;
  void SetTypedTuple(vtkIdType tupleIdx, const ValueType* tuple);
  ValueType GetTypedComponent(vtkIdType tupleIdx, int compIdx) const;
  void SetTypedComponent(vtkIdType tupleIdx, int compIdx, ValueType value);

protected:
  vtkmDataArray();
  ~vtkmDataArray() override;

  /// Overrides for range computation. These use Viskores to perform the computations to avoid
  /// memory transfers
  using Superclass::ComputeScalarRange;
  bool ComputeScalarRange(
    double* ranges, const unsigned char* ghosts, unsigned char ghostsToSkip = 0xff) override;
  using Superclass::ComputeVectorRange;
  bool ComputeVectorRange(
    double range[2], const unsigned char* ghosts, unsigned char ghostsToSkip = 0xff) override;
  using Superclass::ComputeFiniteScalarRange;
  bool ComputeFiniteScalarRange(
    double* ranges, const unsigned char* ghosts, unsigned char ghostsToSkip = 0xff) override;
  using Superclass::ComputeFiniteVectorRange;
  bool ComputeFiniteVectorRange(
    double range[2], const unsigned char* ghosts, unsigned char ghostsToSkip = 0xff) override;

  /// concept methods for \c vtkGenericDataArray
  bool AllocateTuples(vtkIdType numberOfTuples);
  bool ReallocateTuples(vtkIdType numberOfTuples);

private:
  // To access concept methods
  friend Superclass;
  friend fromvtkm::ArrayHandleHelperSwapper<T>;

  mutable std::unique_ptr<fromvtkm::ArrayHandleHelperBase<T>> Helper;

  vtkmDataArray(const vtkmDataArray&) = delete;
  void operator=(const vtkmDataArray&) = delete;
};

//=============================================================================
template <typename T, typename S>
inline vtkmDataArray<typename viskores::VecTraits<T>::BaseComponentType>* make_vtkmDataArray(
  const viskores::cont::ArrayHandle<T, S>& ah)
{
  auto ret = vtkmDataArray<typename viskores::VecTraits<T>::BaseComponentType>::New();
  ret->SetVtkmArrayHandle(ah);
  return ret;
}

//=============================================================================
#ifndef vtkmDataArray_cxx
extern template class VTKACCELERATORSVTKMCORE_TEMPLATE_EXPORT vtkmDataArray<char>;
extern template class VTKACCELERATORSVTKMCORE_TEMPLATE_EXPORT vtkmDataArray<double>;
extern template class VTKACCELERATORSVTKMCORE_TEMPLATE_EXPORT vtkmDataArray<float>;
extern template class VTKACCELERATORSVTKMCORE_TEMPLATE_EXPORT vtkmDataArray<int>;
extern template class VTKACCELERATORSVTKMCORE_TEMPLATE_EXPORT vtkmDataArray<long>;
extern template class VTKACCELERATORSVTKMCORE_TEMPLATE_EXPORT vtkmDataArray<long long>;
extern template class VTKACCELERATORSVTKMCORE_TEMPLATE_EXPORT vtkmDataArray<short>;
extern template class VTKACCELERATORSVTKMCORE_TEMPLATE_EXPORT vtkmDataArray<signed char>;
extern template class VTKACCELERATORSVTKMCORE_TEMPLATE_EXPORT vtkmDataArray<unsigned char>;
extern template class VTKACCELERATORSVTKMCORE_TEMPLATE_EXPORT vtkmDataArray<unsigned int>;
extern template class VTKACCELERATORSVTKMCORE_TEMPLATE_EXPORT vtkmDataArray<unsigned long>;
extern template class VTKACCELERATORSVTKMCORE_TEMPLATE_EXPORT vtkmDataArray<unsigned long long>;
extern template class VTKACCELERATORSVTKMCORE_TEMPLATE_EXPORT vtkmDataArray<unsigned short>;
#endif // vtkmDataArray_cxx

VTK_ABI_NAMESPACE_END
#endif // vtkmDataArray_h

#include "vtkmlib/vtkmDataArray.hxx"

// VTK-HeaderTest-Exclude: vtkmDataArray.h
