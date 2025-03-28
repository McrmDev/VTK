// SPDX-FileCopyrightText: Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
// SPDX-License-Identifier: BSD-3-Clause
/**
 * @class   vtkAbstractElectronicData
 * @brief   Provides access to and storage of
 * chemical electronic data
 *
 */

#ifndef vtkAbstractElectronicData_h
#define vtkAbstractElectronicData_h

#include "vtkCommonDataModelModule.h" // For export macro
#include "vtkDataObject.h"

VTK_ABI_NAMESPACE_BEGIN
class vtkImageData;

class VTKCOMMONDATAMODEL_EXPORT vtkAbstractElectronicData : public vtkDataObject
{
public:
  vtkTypeMacro(vtkAbstractElectronicData, vtkDataObject);
  void PrintSelf(ostream& os, vtkIndent indent) override;

  /**
   * Returns `VTK_ABSTRACT_ELECTRONIC_DATA`.
   */
  int GetDataObjectType() VTK_FUTURE_CONST override { return VTK_ABSTRACT_ELECTRONIC_DATA; }

  /**
   * Returns the number of molecular orbitals available.
   */
  virtual vtkIdType GetNumberOfMOs() = 0;

  /**
   * Returns the number of electrons in the molecule.
   */
  virtual vtkIdType GetNumberOfElectrons() = 0;

  /**
   * Returns the vtkImageData for the requested molecular orbital.
   */
  virtual vtkImageData* GetMO(vtkIdType orbitalNumber) = 0;

  /**
   * Returns vtkImageData for the molecule's electron density. The data
   * will be calculated when first requested, and cached for later requests.
   */
  virtual vtkImageData* GetElectronDensity() = 0;

  /**
   * Returns vtkImageData for the Highest Occupied Molecular Orbital.
   */
  vtkImageData* GetHOMO() { return this->GetMO(this->GetHOMOOrbitalNumber()); }

  /**
   * Returns vtkImageData for the Lowest Unoccupied Molecular Orbital.
   */
  vtkImageData* GetLUMO() { return this->GetMO(this->GetLUMOOrbitalNumber()); }

  // Description:
  // Returns the orbital number of the Highest Occupied Molecular Orbital.
  vtkIdType GetHOMOOrbitalNumber()
  {
    return static_cast<vtkIdType>((this->GetNumberOfElectrons() / 2) - 1);
  }

  // Description:
  // Returns the orbital number of the Lowest Unoccupied Molecular Orbital.
  vtkIdType GetLUMOOrbitalNumber()
  {
    return static_cast<vtkIdType>(this->GetNumberOfElectrons() / 2);
  }

  /**
   * Returns true if the given orbital number is the Highest Occupied
   * Molecular Orbital, false otherwise.
   */
  bool IsHOMO(vtkIdType orbitalNumber) { return (orbitalNumber == this->GetHOMOOrbitalNumber()); }

  /**
   * Returns true if the given orbital number is the Lowest Unoccupied
   * Molecular Orbital, false otherwise.
   */
  bool IsLUMO(vtkIdType orbitalNumber) { return (orbitalNumber == this->GetLUMOOrbitalNumber()); }

  /**
   * Deep copies the data object into this.
   */
  void DeepCopy(vtkDataObject* obj) override;

  ///@{
  /**
   * Get the padding between the molecule and the cube boundaries. This is
   * used to determine the dataset's bounds.
   */
  vtkGetMacro(Padding, double);
  ///@}

protected:
  vtkAbstractElectronicData();
  ~vtkAbstractElectronicData() override;

  double Padding;

private:
  vtkAbstractElectronicData(const vtkAbstractElectronicData&) = delete;
  void operator=(const vtkAbstractElectronicData&) = delete;
};

VTK_ABI_NAMESPACE_END
#endif
