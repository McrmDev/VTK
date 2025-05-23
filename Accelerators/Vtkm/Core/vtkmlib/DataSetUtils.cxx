// SPDX-FileCopyrightText: Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
// SPDX-FileCopyrightText: Copyright (c) Kitware, Inc.
// SPDX-FileCopyrightText: Copyright 2012 Sandia Corporation.
// SPDX-License-Identifier: LicenseRef-BSD-3-Clause-Sandia-USGov
#include "DataSetUtils.h"

#include <algorithm>
#include <numeric>
#include <vector>

VTK_ABI_NAMESPACE_BEGIN
std::vector<viskores::Id> GetFieldsIndicesWithoutCoords(const viskores::cont::DataSet& input)
{
  std::vector<viskores::Id> allFields(input.GetNumberOfFields());
  std::vector<viskores::Id> coords(input.GetNumberOfCoordinateSystems());

  std::iota(allFields.begin(), allFields.end(), 0);
  std::iota(coords.begin(), coords.end(), 0);
  std::transform(coords.begin(), coords.end(), coords.begin(),
    [&input](auto idx) { return input.GetFieldIndex(input.GetCoordinateSystemName(idx)); });

  std::vector<viskores::Id> fields;
  std::set_difference(
    allFields.begin(), allFields.end(), coords.begin(), coords.end(), std::back_inserter(fields));

  return fields;
}
VTK_ABI_NAMESPACE_END
