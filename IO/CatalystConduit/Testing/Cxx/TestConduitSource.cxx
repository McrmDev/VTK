// SPDX-FileCopyrightText: Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
// SPDX-License-Identifier: BSD-3-Clause

#include <vtkXMLUniformGridAMRWriter.h>

#include "vtkCellData.h"
#include "vtkCellIterator.h"
#include "vtkCompositeDataIterator.h"
#include "vtkConduitSource.h"
#include "vtkImageData.h"
#include "vtkLogger.h"
#include "vtkMultiProcessController.h"
#include "vtkNew.h"
#include "vtkOverlappingAMR.h"
#include "vtkPartitionedDataSet.h"
#include "vtkPointData.h"
#include "vtkRectilinearGrid.h"
#include "vtkSmartPointer.h"
#include "vtkStructuredGrid.h"
#include "vtkTestUtilities.h"
#include "vtkUnstructuredGrid.h"
#include "vtkVector.h"

#if VTK_MODULE_ENABLE_VTK_ParallelMPI
#include "vtkMPIController.h"
#else
#include "vtkDummyController.h"
#endif

#include "Grid.hxx"
#include <catalyst_conduit.hpp>
#include <catalyst_conduit_blueprint.hpp>

#define VERIFY(x, ...)                                                                             \
  if ((x) == false)                                                                                \
  {                                                                                                \
    vtkLogF(ERROR, __VA_ARGS__);                                                                   \
    return false;                                                                                  \
  }

namespace
{

//----------------------------------------------------------------------------
vtkSmartPointer<vtkDataObject> Convert(const conduit_cpp::Node& node)
{
  vtkNew<vtkConduitSource> source;
  source->SetNode(conduit_cpp::c_node(&node));
  source->Update();
  return source->GetOutputDataObject(0);
}

//----------------------------------------------------------------------------
void CreateUniformMesh(
  unsigned int nptsX, unsigned int nptsY, unsigned int nptsZ, conduit_cpp::Node& res)
{
  auto controller = vtkMultiProcessController::GetGlobalController();
  auto rank = controller->GetLocalProcessId();

  // Create the structure
  conduit_cpp::Node coords = res["coordsets/coords"];
  coords["type"] = "uniform";
  conduit_cpp::Node dims = coords["dims"];
  dims["i"] = nptsX;
  dims["j"] = nptsY;

  if (nptsZ > 1)
  {
    dims["k"] = nptsZ;
  }

  // -10 to 10 in each dim
  conduit_cpp::Node origin = coords["origin"];
  origin["x"] = -10.0;
  origin["y"] = -10.0 + 20 * rank;

  if (nptsZ > 1)
  {
    origin["z"] = -10.0;
  }

  conduit_cpp::Node spacing = coords["spacing"];
  spacing["dx"] = 20.0 / (double)(nptsX - 1);
  spacing["dy"] = 20.0 / (double)(nptsY - 1);

  if (nptsZ > 1)
  {
    spacing["dz"] = 20.0 / (double)(nptsZ - 1);
  }

  res["topologies/mesh/type"] = "uniform";
  res["topologies/mesh/coordset"] = "coords";
}

//----------------------------------------------------------------------------
bool ValidateMeshTypeUniform()
{
  conduit_cpp::Node mesh;
  CreateUniformMesh(3, 3, 3, mesh);

  auto data = Convert(mesh);
  VERIFY(vtkPartitionedDataSet::SafeDownCast(data) != nullptr,
    "incorrect data type, expected vtkPartitionedDataSet, got %s", vtkLogIdentifier(data));
  auto pds = vtkPartitionedDataSet::SafeDownCast(data);
  VERIFY(pds->GetNumberOfPartitions() == 1, "incorrect number of partitions, expected 1, got %d",
    pds->GetNumberOfPartitions());
  auto img = vtkImageData::SafeDownCast(pds->GetPartition(0));
  VERIFY(img != nullptr, "missing partition 0");
  int dims[3];
  img->GetDimensions(dims);
  VERIFY(dims[0] == 3, "incorrect x dimension expected=3, got=%d", dims[0]);
  VERIFY(dims[1] == 3, "incorrect y dimension expected=3, got=%d", dims[1]);
  VERIFY(dims[2] == 3, "incorrect z dimension expected=3, got=%d", dims[2]);

  return true;
}

//----------------------------------------------------------------------------
void GenerateValues(unsigned int nptsX, unsigned int nptsY, unsigned int nptsZ,
  std::vector<double>& x, std::vector<double>& y, std::vector<double>& z)
{
  x.resize(nptsX);
  y.resize(nptsY);

  if (nptsZ > 1)
  {
    z.resize(nptsZ);
  }

  double dx = 20.0 / (double)(nptsX - 1);
  double dy = 20.0 / (double)(nptsY - 1);
  double dz = 0.0;

  if (nptsZ > 1)
  {
    dz = 20.0 / (double)(nptsZ - 1);
  }

  for (unsigned int i = 0; i < nptsX; i++)
  {
    x[i] = -10.0 + i * dx;
  }

  for (unsigned int j = 0; j < nptsY; j++)
  {
    y[j] = -10.0 + j * dy;
  }

  if (nptsZ > 1)
  {
    for (unsigned int k = 0; k < nptsZ; k++)
    {
      z[k] = -10.0 + k * dz;
    }
  }
}

//----------------------------------------------------------------------------
void CreateRectilinearMesh(
  unsigned int nptsX, unsigned int nptsY, unsigned int nptsZ, conduit_cpp::Node& res)
{
  conduit_cpp::Node coords = res["coordsets/coords"];
  coords["type"] = "rectilinear";

  std::vector<double> x;
  std::vector<double> y;
  std::vector<double> z;
  GenerateValues(nptsX, nptsY, nptsZ, x, y, z);

  conduit_cpp::Node coordVals = coords["values"];
  coordVals["x"].set(x);
  coordVals["y"].set(y);

  if (nptsZ > 1)
  {
    coordVals["z"].set(z);
  }

  res["topologies/mesh/type"] = "rectilinear";
  res["topologies/mesh/coordset"] = "coords";
}

//----------------------------------------------------------------------------
bool ValidateMeshTypeRectilinear()
{
  conduit_cpp::Node mesh;
  CreateRectilinearMesh(3, 3, 3, mesh);
  auto data = Convert(mesh);
  VERIFY(vtkPartitionedDataSet::SafeDownCast(data) != nullptr,
    "incorrect data type, expected vtkPartitionedDataSet, got %s", vtkLogIdentifier(data));
  auto pds = vtkPartitionedDataSet::SafeDownCast(data);
  VERIFY(pds->GetNumberOfPartitions() == 1, "incorrect number of partitions, expected 1, got %d",
    pds->GetNumberOfPartitions());
  auto rg = vtkRectilinearGrid::SafeDownCast(pds->GetPartition(0));
  VERIFY(rg != nullptr, "missing partition 0");
  int dims[3];
  rg->GetDimensions(dims);
  VERIFY(dims[0] == 3, "incorrect x dimension expected=3, got=%d", dims[0]);
  VERIFY(dims[1] == 3, "incorrect y dimension expected=3, got=%d", dims[1]);
  VERIFY(dims[2] == 3, "incorrect z dimension expected=3, got=%d", dims[2]);

  // Expected values
  std::vector<double> x;
  std::vector<double> y;
  std::vector<double> z;
  GenerateValues(3, 3, 3, x, y, z);
  for (unsigned int i = 0; i < 3; i++)
  {
    VERIFY(x[i] == rg->GetXCoordinates()->GetComponent(i, 0),
      "incorrect x value at %d: expected=%g, got=%g", i, x[i],
      rg->GetXCoordinates()->GetComponent(i, 0));
    VERIFY(y[i] == rg->GetYCoordinates()->GetComponent(i, 0),
      "incorrect y value at %d: expected=%g, got=%g", i, y[i],
      rg->GetYCoordinates()->GetComponent(i, 0));
    VERIFY(z[i] == rg->GetZCoordinates()->GetComponent(i, 0),
      "incorrect z value at %d: expected=%g, got=%g", i, z[i],
      rg->GetZCoordinates()->GetComponent(i, 0));
  }

  return true;
}

//----------------------------------------------------------------------------
void CreateCoords(
  unsigned int nptsX, unsigned int nptsY, unsigned int nptsZ, conduit_cpp::Node& res)
{
  conduit_cpp::Node coords = res["coordsets/coords"];
  conduit_cpp::Node coordVals = coords["values"];
  coords["type"] = "explicit";

  unsigned int npts = nptsX * nptsY;

  if (nptsZ > 1)
  {
    npts *= nptsZ;
  }

  std::vector<double> x;
  x.resize(npts);
  std::vector<double> y;
  y.resize(npts);
  std::vector<double> z;

  if (nptsZ > 1)
  {
    z.resize(npts);
  }

  double dx = 20.0 / double(nptsX - 1);
  double dy = 20.0 / double(nptsY - 1);

  double dz = 0.0;

  if (nptsZ > 1)
  {
    dz = 20.0 / double(nptsZ - 1);
  }

  unsigned int idx = 0;
  unsigned int outer = 1;
  if (nptsZ > 1)
  {
    outer = nptsZ;
  }

  for (unsigned int k = 0; k < outer; k++)
  {
    double cz = -10.0 + k * dz;

    for (unsigned int j = 0; j < nptsY; j++)
    {
      double cy = -10.0 + j * dy;

      for (unsigned int i = 0; i < nptsX; i++)
      {
        x[idx] = -10.0 + i * dx;
        y[idx] = cy;

        if (nptsZ > 1)
        {
          z[idx] = cz;
        }

        idx++;
      }
    }
  }

  coordVals["x"].set(x);
  coordVals["y"].set(y);
  if (nptsZ > 1)
  {
    coordVals["z"].set(z);
  }
}

//----------------------------------------------------------------------------
void CreateStructuredMesh(
  unsigned int nptsX, unsigned int nptsY, unsigned int nptsZ, conduit_cpp::Node& res)
{
  CreateCoords(nptsX, nptsY, nptsZ, res);

  res["topologies/mesh/type"] = "structured";
  res["topologies/mesh/coordset"] = "coords";
  res["topologies/mesh/elements/dims/i"] = nptsX - 1;
  res["topologies/mesh/elements/dims/j"] = nptsY - 1;
  if (nptsZ > 0)
  {
    res["topologies/mesh/elements/dims/k"] = nptsZ - 1;
  }
}

//----------------------------------------------------------------------------
bool ValidateMeshTypeStructured()
{
  conduit_cpp::Node mesh;
  CreateStructuredMesh(3, 3, 3, mesh);
  auto data = Convert(mesh);
  VERIFY(vtkPartitionedDataSet::SafeDownCast(data) != nullptr,
    "incorrect data type, expected vtkPartitionedDataSet, got %s", vtkLogIdentifier(data));
  auto pds = vtkPartitionedDataSet::SafeDownCast(data);
  VERIFY(pds->GetNumberOfPartitions() == 1, "incorrect number of partitions, expected 1, got %d",
    pds->GetNumberOfPartitions());
  auto sg = vtkStructuredGrid::SafeDownCast(pds->GetPartition(0));
  VERIFY(sg != nullptr, "missing partition 0");
  int dims[3];
  sg->GetDimensions(dims);
  VERIFY(dims[0] == 3, "incorrect x dimension expected=3, got=%d", dims[0]);
  VERIFY(dims[1] == 3, "incorrect y dimension expected=3, got=%d", dims[1]);
  VERIFY(dims[2] == 3, "incorrect z dimension expected=3, got=%d", dims[2]);

  return true;
}

//----------------------------------------------------------------------------
void CreatePointSet(
  unsigned int nptsX, unsigned int nptsY, unsigned int nptsZ, conduit_cpp::Node& res)
{
  CreateCoords(nptsX, nptsY, nptsZ, res);

  res["topologies/mesh/type"] = "points";
  res["topologies/mesh/coordset"] = "coords";
}

//----------------------------------------------------------------------------
bool ValidateMeshTypePoints()
{
  conduit_cpp::Node mesh;
  CreatePointSet(3, 3, 3, mesh);
  auto data = Convert(mesh);
  VERIFY(vtkPartitionedDataSet::SafeDownCast(data) != nullptr,
    "incorrect data type, expected vtkPartitionedDataSet, got %s", vtkLogIdentifier(data));
  auto pds = vtkPartitionedDataSet::SafeDownCast(data);
  VERIFY(pds->GetNumberOfPartitions() == 1, "incorrect number of partitions, expected 1, got %d",
    pds->GetNumberOfPartitions());
  auto ps = vtkPointSet::SafeDownCast(pds->GetPartition(0));
  VERIFY(ps != nullptr, "missing partition 0");

  VERIFY(ps->GetNumberOfPoints() == 27, "incorrect number of points, expected 27, got %lld",
    ps->GetNumberOfPoints());
  return true;
}

//----------------------------------------------------------------------------
void CreateTrisMesh(unsigned int nptsX, unsigned int nptsY, conduit_cpp::Node& res)
{
  CreateStructuredMesh(nptsX, nptsY, 1, res);

  unsigned int nElementX = nptsX - 1;
  unsigned int nElementY = nptsY - 1;
  unsigned int nElements = nElementX * nElementY;

  res["topologies/mesh/type"] = "unstructured";
  res["topologies/mesh/coordset"] = "coords";
  res["topologies/mesh/elements/shape"] = "tri";

  std::vector<unsigned int> connectivity;
  connectivity.resize(nElements * 6);

  unsigned int idx = 0;
  for (unsigned int j = 0; j < nElementY; j++)
  {
    unsigned int yoff = j * (nElementX + 1);

    for (unsigned int i = 0; i < nElementX; i++)
    {
      // two tris per quad.
      connectivity[idx + 0] = yoff + i;
      connectivity[idx + 1] = yoff + i + (nElementX + 1);
      connectivity[idx + 2] = yoff + i + 1 + (nElementX + 1);

      connectivity[idx + 3] = yoff + i;
      connectivity[idx + 4] = yoff + i + 1;
      connectivity[idx + 5] = yoff + i + 1 + (nElementX + 1);

      idx += 6;
    }
  }

  res["topologies/mesh/elements/connectivity"].set(connectivity);

  // Need also to define 'fields' for cell array
  conduit_cpp::Node resFields = res["fields/field"];
  resFields["association"] = "element";
  resFields["topology"] = "mesh";
  resFields["volume_dependent"] = "false";

  unsigned int numberofValues = nElements * 2;
  std::vector<double> values;
  values.resize(numberofValues);
  for (unsigned int i = 0; i < numberofValues; i++)
  {
    values[i] = i + 0.0;
  }
  resFields["values"].set(values);

  conduit_cpp::Node resFieldsMetaData = res["state/metadata/vtk_fields/field"];
  resFieldsMetaData["attribute_type"] =
    vtkDataSetAttributes::GetAttributeTypeAsString(vtkDataSetAttributes::SCALARS);
}

//----------------------------------------------------------------------------
bool ValidateMeshTypeUnstructured()
{
  conduit_cpp::Node mesh;
  // generate simple explicit tri-based 2d 'basic' mesh
  CreateTrisMesh(3, 3, mesh);

  auto data = Convert(mesh);
  VERIFY(vtkPartitionedDataSet::SafeDownCast(data) != nullptr,
    "incorrect data type, expected vtkPartitionedDataSet, got %s", vtkLogIdentifier(data));
  auto pds = vtkPartitionedDataSet::SafeDownCast(data);
  VERIFY(pds->GetNumberOfPartitions() == 1, "incorrect number of partitions, expected 1, got %d",
    pds->GetNumberOfPartitions());
  auto ug = vtkUnstructuredGrid::SafeDownCast(pds->GetPartition(0));
  VERIFY(ug != nullptr, "missing partition 0");

  VERIFY(ug->GetNumberOfPoints() == 9, "incorrect number of points, expected 9, got %lld",
    ug->GetNumberOfPoints());
  VERIFY(ug->GetNumberOfCells() == 8, "incorrect number of cells, expected 8, got %lld",
    ug->GetNumberOfCells());
  VERIFY(ug->GetCellData()->GetAttribute(vtkDataSetAttributes::SCALARS) != nullptr,
    "missing 'field' cell-data array with attribute '%s'",
    vtkDataSetAttributes::GetAttributeTypeAsString(vtkDataSetAttributes::SCALARS));
  return true;
}

//----------------------------------------------------------------------------
bool CheckFieldData(vtkDataObject* data, int expected_number_of_arrays,
  const std::string& expected_array_name, int expected_number_of_components,
  std::vector<vtkVariant> expected_values)
{
  auto field_data = data->GetFieldData();
  VERIFY(field_data->GetNumberOfArrays() == expected_number_of_arrays,
    "incorrect number of arrays in field data, expected %d, got %d", expected_number_of_arrays,
    field_data->GetNumberOfArrays());

  if (expected_number_of_arrays > 0)
  {
    auto field_array = field_data->GetAbstractArray(0);

    VERIFY(std::string(field_array->GetName()) == expected_array_name,
      "wrong array name, expected %s, got %s", expected_array_name.c_str(), field_array->GetName());
    VERIFY(field_array->GetNumberOfComponents() == expected_number_of_components,
      "wrong number of component");
    VERIFY(static_cast<size_t>(field_array->GetNumberOfTuples()) == expected_values.size(),
      "wrong number of tuples");
    for (size_t i = 0; i < expected_values.size(); ++i)
    {
      VERIFY(field_array->GetVariantValue(i) == expected_values[i], "wrong value");
    }
  }

  return true;
}

//----------------------------------------------------------------------------
bool CheckFieldDataMeshConversion(conduit_cpp::Node& mesh_node, int expected_number_of_arrays,
  const std::string& expected_array_name, int expected_number_of_components,
  std::vector<vtkVariant> expected_values)
{
  auto data = Convert(mesh_node);

  CheckFieldData(data, expected_number_of_arrays, expected_array_name,
    expected_number_of_components, expected_values);
  auto pds = vtkPartitionedDataSet::SafeDownCast(data);
  VERIFY(pds->GetNumberOfPartitions() == 1, "incorrect number of partitions, expected 1, got %d",
    pds->GetNumberOfPartitions());
  auto img = vtkImageData::SafeDownCast(pds->GetPartition(0));

  CheckFieldData(img, expected_number_of_arrays, expected_array_name, expected_number_of_components,
    expected_values);

  return true;
}

//----------------------------------------------------------------------------
bool ValidateDistributedAMR()
{
  auto controller = vtkMultiProcessController::GetGlobalController();
  auto rank = controller->GetLocalProcessId();

  conduit_cpp::Node amrmesh;

  auto domain = amrmesh["domain0"];

  // Each rank contains a new level
  int level = rank;

  domain["state/domain_id"] = rank;
  domain["state/cycle"] = 0;
  domain["state/time"] = 0;
  domain["state/level"] = level;

  auto coords = domain["coordsets/coords"];
  coords["type"] = "uniform";
  coords["dims/i"] = 3;
  coords["dims/j"] = 3;
  coords["dims/k"] = 3;
  // spacing depends on level
  coords["spacing/dx"] = 1. / std::pow(2, level);
  coords["spacing/dy"] = 1. / std::pow(2, level);
  coords["spacing/dz"] = 1. / std::pow(2, level);
  coords["origin/x"] = 0.0;
  coords["origin/y"] = 0.0;
  coords["origin/z"] = 0.0;

  auto topo = domain["topologies/topo"];
  topo["type"] = "uniform";
  topo["coordset"] = "coords";

  vtkNew<vtkConduitSource> source;
  source->SetUseAMRMeshProtocol(true);
  source->SetNode(conduit_cpp::c_node(&amrmesh));
  source->Update();
  auto data = source->GetOutputDataObject(0);

  VERIFY(vtkOverlappingAMR::SafeDownCast(data) != nullptr,
    "Incorrect data type, expected vtkOverlappingAMR, got %s", vtkLogIdentifier(data));

  auto amr = vtkOverlappingAMR::SafeDownCast(data);
  if (!amr->CheckValidity())
  {
    return false;
  }

  int generatedLevels = amr->GetNumberOfLevels();
  auto nbOfProcess = controller->GetNumberOfProcesses();
  VERIFY(generatedLevels == nbOfProcess, "Incorrect number of levels, expexts %d but has %d",
    generatedLevels, nbOfProcess);

  return true;
}

//----------------------------------------------------------------------------
bool ValidateMeshTypeAMR(const std::string& file)
{
  conduit_cpp::Node mesh;
  // read in an example mesh dataset
  conduit_node_load(conduit_cpp::c_node(&mesh), file.c_str(), "");

  // add in point data
  std::string field_name = "pointfield";
  double field_value = 1;
  size_t num_children = mesh["data"].number_of_children();
  for (size_t i = 0; i < num_children; i++)
  {
    conduit_cpp::Node amr_block = mesh["data"].child(i);
    int i_dimension = amr_block["coordsets/coords/dims/i"].to_int32();
    int j_dimension = amr_block["coordsets/coords/dims/j"].to_int32();
    int k_dimension = amr_block["coordsets/coords/dims/k"].to_int32();
    conduit_cpp::Node fields = amr_block["fields"];
    conduit_cpp::Node point_field = fields[field_name];
    point_field["association"] = "vertex";
    point_field["topology"] = "topo";
    std::vector<double> point_values(
      (i_dimension + 1) * (j_dimension + 1) * (k_dimension + 1), field_value);
    point_field["values"] = point_values;
  }

  const auto& meshdata = mesh["data"];
  // run vtk conduit source
  vtkNew<vtkConduitSource> source;
  source->SetUseAMRMeshProtocol(true);
  source->SetNode(conduit_cpp::c_node(&meshdata));
  source->Update();
  auto data = source->GetOutputDataObject(0);

  VERIFY(vtkOverlappingAMR::SafeDownCast(data) != nullptr,
    "Incorrect data type, expected vtkOverlappingAMR, got %s", vtkLogIdentifier(data));

  auto amr = vtkOverlappingAMR::SafeDownCast(data);

  std::vector<double> bounds(6);
  std::vector<double> origin(3);

  amr->GetBounds(bounds.data());
  amr->GetOrigin(0, 0, origin.data());

  VERIFY(bounds[0] == 0 && bounds[1] == 1 && bounds[2] == 0 && bounds[3] == 1 && bounds[4] == 0 &&
      bounds[5] == 1,
    "Incorrect AMR bounds");

  VERIFY(origin[0] == 0 && origin[1] == 0 && origin[2] == 0, "Incorrect AMR origin");

  vtkSmartPointer<vtkCompositeDataIterator> iter;
  iter.TakeReference(amr->NewIterator());
  iter->InitTraversal();
  for (iter->GoToFirstItem(); !iter->IsDoneWithTraversal(); iter->GoToNextItem())
  {
    vtkDataSet* block = vtkDataSet::SafeDownCast(iter->GetCurrentDataObject());
    VERIFY(block->GetCellData()->GetArray("density") != nullptr, "Incorrect AMR cell data");
    double range[2] = { -1, -1 };
    block->GetPointData()->GetArray(field_name.c_str())->GetRange(range);
    VERIFY(range[0] == field_value && range[1] == field_value, "Incorrect AMR point data");
  }

  return true;
}

//----------------------------------------------------------------------------
bool ValidateFieldData()
{
  auto controller = vtkMultiProcessController::GetGlobalController();
  auto rank = controller->GetLocalProcessId();

  conduit_cpp::Node mesh;
  CreateUniformMesh(3, 3, 3, mesh);

  auto field_data_node = mesh["state/fields"];

  auto empty_field_data = field_data_node["empty_field_data"];
  VERIFY(CheckFieldDataMeshConversion(mesh, 0, empty_field_data.name(), 0, {}),
    "Verification failed for empty field data.");

  field_data_node.remove(0);
  auto integer_field_data = field_data_node["integer_field_data"];
  integer_field_data.set_int64(42 + rank);
  VERIFY(CheckFieldDataMeshConversion(mesh, 1, integer_field_data.name(), 1, { 42 + rank }),
    "Verification failed for integer field data.");

  field_data_node.remove(0);
  auto float_field_data = field_data_node["float_field_data"];
  float_field_data.set_float64(5.0);
  VERIFY(CheckFieldDataMeshConversion(mesh, 1, float_field_data.name(), 1, { 5.0 }),
    "Verification failed for float field data.");

  field_data_node.remove(0);
  auto string_field_data = field_data_node["string_field_data"];
  string_field_data.set_string("test");
  VERIFY(CheckFieldDataMeshConversion(mesh, 1, string_field_data.name(), 1, { "test" }),
    "Verification failed for string field data.");

  field_data_node.remove(0);
  auto integer_vector_field_data = field_data_node["integer_vector_field_data"];
  integer_vector_field_data.set_int64_vector({ 1, 2, 3 });
  VERIFY(CheckFieldDataMeshConversion(mesh, 1, integer_vector_field_data.name(), 1, { 1, 2, 3 }),
    "Verification failed for integer vector field data.");

  field_data_node.remove(0);
  auto float_vector_field_data = field_data_node["float_vector_field_data"];
  float_vector_field_data.set_float64_vector({ 4.0, 5.0, 6.0 });
  VERIFY(
    CheckFieldDataMeshConversion(mesh, 1, float_vector_field_data.name(), 1, { 4.0, 5.0, 6.0 }),
    "Verification failed for float vector field data.");

  field_data_node.remove(0);
  std::vector<int> integer_buffer = { 123, 456, 789 };
  auto external_integer_vector_field_data = field_data_node["external_integer_vector"];
  external_integer_vector_field_data.set_external_int32_ptr(
    integer_buffer.data(), integer_buffer.size());
  VERIFY(CheckFieldDataMeshConversion(
           mesh, 1, external_integer_vector_field_data.name(), 1, { 123, 456, 789 }),
    "Verification failed for external integer vector field data.");

  return true;
}

//----------------------------------------------------------------------------
bool ValidateAscentGhostCellData()
{
  conduit_cpp::Node mesh;
  CreateUniformMesh(3, 3, 3, mesh);

  std::vector<int> cellGhosts(8, 0);
  cellGhosts[2] = 1;

  conduit_cpp::Node resCellFields = mesh["fields/ascent_ghosts"];
  resCellFields["association"] = "element";
  resCellFields["topology"] = "mesh";
  resCellFields["volume_dependent"] = "false";
  resCellFields["values"] = cellGhosts;

  std::vector<int> cellGhostValuesToReplace(1, 1);
  std::vector<int> cellGhostReplacementValues(1, vtkDataSetAttributes::HIDDENCELL);

  std::vector<int> cellGhostsMetaData(1, 1);
  conduit_cpp::Node ghostMetaData = mesh["state/metadata/vtk_fields/ascent_ghosts"];
  ghostMetaData["attribute_type"] = "Ghosts";
  ghostMetaData["values_to_replace"] = cellGhostValuesToReplace;
  ghostMetaData["replacement_values"] = cellGhostReplacementValues;

  auto data = Convert(mesh);
  auto pds = vtkPartitionedDataSet::SafeDownCast(data);
  VERIFY(pds->GetNumberOfPartitions() == 1, "incorrect number of partitions, expected 1, got %d",
    pds->GetNumberOfPartitions());
  auto img = vtkImageData::SafeDownCast(pds->GetPartition(0));
  VERIFY(img != nullptr, "missing partition 0");
  auto array = vtkUnsignedCharArray::SafeDownCast(img->GetCellData()->GetGhostArray());
  VERIFY(array != nullptr &&
      array->GetValue(2) == static_cast<unsigned char>(vtkDataSetAttributes::HIDDENCELL),
    "Verification failed for converting Ascent ghost cell data");

  return true;
}

//----------------------------------------------------------------------------
bool ValidateAscentGhostPointData()
{
  conduit_cpp::Node mesh;
  CreateUniformMesh(3, 3, 3, mesh);

  std::vector<int> pointGhosts(27, 0);
  pointGhosts[2] = 1;

  conduit_cpp::Node resPointFields = mesh["fields/ascent_ghosts"];
  resPointFields["association"] = "vertex";
  resPointFields["topology"] = "mesh";
  resPointFields["values"] = pointGhosts;

  std::vector<int> pointGhostValuesToReplace(1, 1);
  std::vector<int> pointGhostReplacementValues(1, vtkDataSetAttributes::HIDDENPOINT);

  std::vector<int> cellGhostsMetaData(1, 1);
  conduit_cpp::Node ghostMetaData = mesh["state/metadata/vtk_fields/ascent_ghosts"];
  ghostMetaData["attribute_type"] = "Ghosts";
  ghostMetaData["values_to_replace"] = pointGhostValuesToReplace;
  ghostMetaData["replacement_values"] = pointGhostReplacementValues;

  auto data = Convert(mesh);
  auto pds = vtkPartitionedDataSet::SafeDownCast(data);
  VERIFY(pds->GetNumberOfPartitions() == 1, "incorrect number of partitions, expected 1, got %d",
    pds->GetNumberOfPartitions());
  auto img = vtkImageData::SafeDownCast(pds->GetPartition(0));
  VERIFY(img != nullptr, "missing partition 0");
  auto array = vtkUnsignedCharArray::SafeDownCast(img->GetPointData()->GetGhostArray());
  VERIFY(array != nullptr &&
      array->GetValue(2) == static_cast<unsigned char>(vtkDataSetAttributes::HIDDENPOINT),
    "Verification failed for converting Ascent ghost point data");

  return true;
}

//----------------------------------------------------------------------------
bool ValidateRectilinearGridWithDifferentDimensions()
{
  conduit_cpp::Node mesh;
  CreateRectilinearMesh(3, 2, 1, mesh);
  auto data = Convert(mesh);
  VERIFY(vtkPartitionedDataSet::SafeDownCast(data) != nullptr,
    "incorrect data type, expected vtkPartitionedDataSet, got %s", vtkLogIdentifier(data));
  auto pds = vtkPartitionedDataSet::SafeDownCast(data);
  VERIFY(pds->GetNumberOfPartitions() == 1, "incorrect number of partitions, expected 1, got %d",
    pds->GetNumberOfPartitions());
  auto rg = vtkRectilinearGrid::SafeDownCast(pds->GetPartition(0));
  VERIFY(rg != nullptr, "invalid partition at index 0");
  int dims[3];
  rg->GetDimensions(dims);
  VERIFY(dims[0] == 3, "incorrect x dimension expected=3, got=%d", dims[0]);
  VERIFY(dims[1] == 2, "incorrect y dimension expected=2, got=%d", dims[1]);
  VERIFY(dims[2] == 1, "incorrect z dimension expected=1, got=%d", dims[2]);

  return true;
}

//----------------------------------------------------------------------------
bool Validate1DRectilinearGrid()
{
  conduit_cpp::Node mesh;
  auto coords = mesh["coordsets/coords"];
  coords["type"] = "rectilinear";
  coords["values/x"].set_float64_vector({ 5.0, 6.0, 7.0 });
  auto topo_mesh = mesh["topologies/mesh"];
  topo_mesh["type"] = "rectilinear";
  topo_mesh["coordset"] = "coords";
  auto field = mesh["fields/field"];
  field["association"] = "element";
  field["topology"] = "mesh";
  field["volume_dependent"] = "false";
  field["values"].set_float64_vector({ 0.0, 1.0 });

  auto data = Convert(mesh);
  VERIFY(vtkPartitionedDataSet::SafeDownCast(data) != nullptr,
    "incorrect data type, expected vtkPartitionedDataSet, got %s", vtkLogIdentifier(data));
  auto pds = vtkPartitionedDataSet::SafeDownCast(data);
  VERIFY(pds->GetNumberOfPartitions() == 1, "incorrect number of partitions, expected 1, got %d",
    pds->GetNumberOfPartitions());
  auto rg = vtkRectilinearGrid::SafeDownCast(pds->GetPartition(0));
  VERIFY(rg != nullptr, "invalid partition at index 0");
  int dims[3];
  rg->GetDimensions(dims);
  VERIFY(dims[0] == 3, "incorrect x dimension expected=3, got=%d", dims[0]);
  VERIFY(dims[1] == 1, "incorrect y dimension expected=1, got=%d", dims[1]);
  VERIFY(dims[2] == 1, "incorrect z dimension expected=1, got=%d", dims[2]);

  return true;
}

//----------------------------------------------------------------------------
inline unsigned int GetLinearIndex3D(unsigned int i, unsigned int j, unsigned int k, unsigned int I,
  unsigned int J, unsigned int K, unsigned int nx, unsigned int ny)
{
  return (i + I) + (j + J) * nx + (k + K) * (nx * ny);
}

//----------------------------------------------------------------------------
void CreateMixedUnstructuredMesh2D(unsigned int npts_x, unsigned int npts_y, conduit_cpp::Node& res)
{
  CreateCoords(npts_x, npts_y, 1, res);

  const unsigned int nele_x = npts_x - 1;
  const unsigned int nele_y = npts_y - 1;

  res["state/time"] = 3.1415;
  res["state/cycle"] = 100UL;

  res["topologies/mesh/type"] = "unstructured";
  res["topologies/mesh/coordset"] = "coords";

  res["topologies/mesh/elements/shape"] = "mixed";
  res["topologies/mesh/elements/shape_map/quad"] = VTK_QUAD;
  res["topologies/mesh/elements/shape_map/tri"] = VTK_TRIANGLE;

  const unsigned int nele_x2 = nele_x / 2;
  const unsigned int nquads = nele_y * nele_x2;
  const unsigned int ntris = nele_y * 2 * (nele_x2 + nele_x % 2);
  const unsigned int nele = nquads + ntris;

  std::vector<unsigned int> connectivity, shapes, sizes, offsets;
  shapes.resize(nele);
  sizes.resize(nele);
  offsets.resize(nele);
  offsets[0] = 0;
  connectivity.resize(nquads * 4 + ntris * 3);

  size_t idx_elem = 0;
  size_t idx = 0;

  for (unsigned int j = 0; j < nele_y; ++j)
  {
    for (unsigned int i = 0; i < nele_x; ++i)
    {
      if (i % 2 == 0)
      {
        constexpr int TrianglePointCount = 3;
        shapes[idx_elem + 0] = VTK_TRIANGLE;
        shapes[idx_elem + 1] = VTK_TRIANGLE;
        sizes[idx_elem + 0] = 3;
        sizes[idx_elem + 1] = 3;

        offsets[idx_elem + 1] = offsets[idx_elem + 0] + TrianglePointCount;
        if (idx_elem + 2 < offsets.size())
        {
          offsets[idx_elem + 2] = offsets[idx_elem + 1] + TrianglePointCount;
        }

        connectivity[idx + 0] = GetLinearIndex3D(0, 0, 0, i, j, 0, npts_x, npts_y);
        connectivity[idx + 1] = GetLinearIndex3D(1, 0, 0, i, j, 0, npts_x, npts_y);
        connectivity[idx + 2] = GetLinearIndex3D(1, 1, 0, i, j, 0, npts_x, npts_y);

        connectivity[idx + 3] = GetLinearIndex3D(0, 0, 0, i, j, 0, npts_x, npts_y);
        connectivity[idx + 4] = GetLinearIndex3D(1, 1, 0, i, j, 0, npts_x, npts_y);
        connectivity[idx + 5] = GetLinearIndex3D(0, 1, 0, i, j, 0, npts_x, npts_y);

        idx_elem += 2;
        idx += 6;
      }
      else
      {
        constexpr int QuadPointCount = 4;
        shapes[idx_elem] = VTK_QUAD;

        sizes[idx_elem] = 4;
        if (idx_elem + 1 < offsets.size())
        {
          offsets[idx_elem + 1] = offsets[idx_elem + 0] + QuadPointCount;
        }

        connectivity[idx + 0] = GetLinearIndex3D(0, 0, 0, i, j, 0, npts_x, npts_y);
        connectivity[idx + 1] = GetLinearIndex3D(1, 0, 0, i, j, 0, npts_x, npts_y);
        connectivity[idx + 2] = GetLinearIndex3D(1, 1, 0, i, j, 0, npts_x, npts_y);
        connectivity[idx + 3] = GetLinearIndex3D(0, 1, 0, i, j, 0, npts_x, npts_y);

        idx_elem += 1;
        idx += 4;
      }
    }
  }

  auto elements = res["topologies/mesh/elements"];
  elements["shapes"].set(shapes);
  elements["sizes"].set(sizes);
  elements["offsets"].set(offsets);
  elements["connectivity"].set(connectivity);
}

//----------------------------------------------------------------------------
bool ValidateMeshTypeMixed2D()
{
  conduit_cpp::Node mesh;
  CreateMixedUnstructuredMesh2D(5, 5, mesh);
  const auto data = Convert(mesh);

  VERIFY(vtkPartitionedDataSet::SafeDownCast(data) != nullptr,
    "incorrect data type, expected vtkPartitionedDataSet, got %s", vtkLogIdentifier(data));
  auto pds = vtkPartitionedDataSet::SafeDownCast(data);
  VERIFY(pds->GetNumberOfPartitions() == 1, "incorrect number of partitions, expected 1, got %d",
    pds->GetNumberOfPartitions());
  auto ug = vtkUnstructuredGrid::SafeDownCast(pds->GetPartition(0));

  // 16 triangles, 4 quads: 24 cells
  VERIFY(ug->GetNumberOfCells() == 24, "expected 24 cells, got %lld", ug->GetNumberOfCells());
  VERIFY(ug->GetNumberOfPoints() == 25, "Expected 25 points, got %lld", ug->GetNumberOfPoints());

  // check cell types
  const auto it = vtkSmartPointer<vtkCellIterator>::Take(ug->NewCellIterator());
  int nTris(0), nQuads(0);
  for (it->InitTraversal(); !it->IsDoneWithTraversal(); it->GoToNextCell())
  {
    const int cellType = it->GetCellType();
    switch (cellType)
    {
      case VTK_TRIANGLE:
      {
        ++nTris;
        break;
      }
      case VTK_QUAD:
      {
        ++nQuads;
        break;
      }
      default:
      {
        vtkLog(ERROR, "Expected only triangles and quads.");
        return false;
      }
    }
  }

  return true;
}

//----------------------------------------------------------------------------
void CreateWedgeAndPyramidUnstructuredMesh(
  unsigned int nptsX, unsigned int nptsY, unsigned int nptsZ, conduit_cpp::Node& res)
{
  conduit_cpp::Node mesh;
  CreateCoords(nptsX, nptsY, nptsZ, res);

  res["topologies/mesh/type"] = "unstructured";
  res["topologies/mesh/coordset"] = "coords";

  const unsigned int nElementX = nptsX - 1;
  const unsigned int nElementY = nptsY - 1;
  const unsigned int nElementZ = nptsZ - 1;
  const unsigned int nElementX2 = nElementX / 2;
  const unsigned int nPyramid = nElementZ * nElementY * (nElementX2 + nElementX % 2);
  const unsigned int nWedge = nElementZ * nElementY * nElementX2;
  const unsigned int nEle = nPyramid + nWedge;

  res["topologies/mesh/elements/shape"] = "mixed";
  res["topologies/mesh/elements/shape_map/pyramid"] = VTK_PYRAMID;
  res["topologies/mesh/elements/shape_map/wedge"] = VTK_WEDGE;

  std::vector<unsigned int> elemConnectivity, elemShapes, elemSizes, elemOffsets;
  elemShapes.resize(nEle);
  elemSizes.resize(nEle);
  elemOffsets.resize(nEle);
  elemConnectivity.resize(nPyramid * 5 + nWedge * 6);
  elemOffsets[0] = 0;

  unsigned int idxElem = 0;
  unsigned int idx = 0;

  for (unsigned int k = 0; k < nElementZ; ++k)
  {
    for (unsigned int j = 0; j < nElementZ; ++j)
    {
      for (unsigned int i = 0; i < nElementX; ++i)
      {
        if (i % 2 == 0) // pyramid
        {
          constexpr int pyramidPointCount = 5;

          elemShapes[idxElem] = VTK_PYRAMID;
          elemSizes[idxElem] = pyramidPointCount;
          if (idxElem + 1 < elemOffsets.size())
          {
            elemOffsets[idxElem + 1] = elemOffsets[idxElem] + pyramidPointCount;
          }

          elemConnectivity[idx + 0] = GetLinearIndex3D(0, 0, 0, i, j, k, nptsX, nptsY);
          elemConnectivity[idx + 1] = GetLinearIndex3D(1, 0, 0, i, j, k, nptsX, nptsY);
          elemConnectivity[idx + 2] = GetLinearIndex3D(1, 1, 0, i, j, k, nptsX, nptsY);
          elemConnectivity[idx + 3] = GetLinearIndex3D(0, 1, 0, i, j, k, nptsX, nptsY);
          elemConnectivity[idx + 4] = GetLinearIndex3D(0, 0, 1, i, j, k, nptsX, nptsY);

          idxElem += 1;
          idx += pyramidPointCount;
        }
        else
        {
          constexpr int wedgePointCount = 6;

          elemShapes[idxElem] = VTK_WEDGE;
          elemSizes[idxElem] = wedgePointCount;
          if (idxElem + 1 < elemOffsets.size())
          {
            elemOffsets[idxElem + 1] = elemOffsets[idxElem] + wedgePointCount;
          }

          elemConnectivity[idx + 0] = GetLinearIndex3D(0, 0, 0, i, j, k, nptsX, nptsY);
          elemConnectivity[idx + 1] = GetLinearIndex3D(1, 0, 0, i, j, k, nptsX, nptsY);
          elemConnectivity[idx + 2] = GetLinearIndex3D(1, 1, 0, i, j, k, nptsX, nptsY);
          elemConnectivity[idx + 3] = GetLinearIndex3D(0, 1, 0, i, j, k, nptsX, nptsY);
          elemConnectivity[idx + 4] = GetLinearIndex3D(0, 0, 1, i, j, k, nptsX, nptsY);
          elemConnectivity[idx + 5] = GetLinearIndex3D(1, 0, 1, i, j, k, nptsX, nptsY);

          idxElem += 1;
          idx += wedgePointCount;
        }
      }
    }
  }

  auto elements = res["topologies/mesh/elements"];
  elements["shapes"].set(elemShapes);
  elements["offsets"].set(elemOffsets);
  elements["sizes"].set(elemSizes);
  elements["connectivity"].set(elemConnectivity);
}

//----------------------------------------------------------------------------
void CreateMixedUnstructuredMesh(
  unsigned int nptsX, unsigned int nptsY, unsigned int nptsZ, conduit_cpp::Node& res)
{
  CreateCoords(nptsX, nptsY, nptsZ, res);

  res["state/time"] = 3.1415;
  res["state/cycle"] = 100UL;

  res["topologies/mesh/type"] = "unstructured";
  res["topologies/mesh/coordset"] = "coords";

  const unsigned int nElementX = nptsX - 1;
  const unsigned int nElementY = nptsY - 1;
  const unsigned int nElementZ = nptsZ - 1;

  const unsigned int nElementX2 = nElementX / 2;
  // one hexa divided into 3 tetras and one polyhedron (prism)
  const unsigned int nTet = 3 * nElementZ * nElementY * (nElementX2 + nElementX % 2);
  const unsigned int nPolyhedra = nElementZ * nElementY * (nElementX2 + nElementX % 2);
  // one hexa as hexahedron
  const unsigned int nHex = nElementZ * nElementY * nElementX2;

  const unsigned int nFaces = 5 * nPolyhedra;
  const unsigned int nEle = nTet + nHex + nPolyhedra;

  res["topologies/mesh/elements/shape"] = "mixed";
  res["topologies/mesh/elements/shape_map/polyhedral"] = VTK_POLYHEDRON;
  res["topologies/mesh/elements/shape_map/tet"] = VTK_TETRA;
  res["topologies/mesh/elements/shape_map/hex"] = VTK_HEXAHEDRON;

  res["topologies/mesh/subelements/shape"] = "mixed";
  res["topologies/mesh/subelements/shape_map/quad"] = VTK_QUAD;
  res["topologies/mesh/subelements/shape_map/tri"] = VTK_TRIANGLE;

  std::vector<unsigned int> elem_connectivity, elem_shapes, elem_sizes, elem_offsets;
  elem_shapes.resize(nEle);
  elem_sizes.resize(nEle);
  elem_offsets.resize(nEle);
  elem_connectivity.resize(nTet * 4 + nPolyhedra * 5 + nHex * 8);
  elem_offsets[0] = 0;

  std::vector<unsigned int> subelem_connectivity, subelem_shapes, subelem_sizes, subelem_offsets;
  subelem_shapes.resize(nFaces);
  subelem_sizes.resize(nFaces);
  subelem_offsets.resize(nFaces);
  subelem_connectivity.resize(nPolyhedra * 18);
  subelem_offsets[0] = 0;

  unsigned int idx_elem = 0;
  unsigned int idx = 0;
  unsigned int idx_elem2 = 0;
  unsigned int idx2 = 0;
  unsigned int polyhedronCounter = 0;

  for (unsigned int k = 0; k < nElementZ; ++k)
  {
    for (unsigned int j = 0; j < nElementZ; ++j)
    {
      for (unsigned int i = 0; i < nElementX; ++i)
      {
        if (i % 2 == 1) // hexahedron
        {
          constexpr int HexaPointCount = 8;

          elem_shapes[idx_elem] = VTK_HEXAHEDRON;
          elem_sizes[idx_elem] = HexaPointCount;
          if (idx_elem + 1 < elem_offsets.size())
          {
            elem_offsets[idx_elem + 1] = elem_offsets[idx_elem] + HexaPointCount;
          }

          elem_connectivity[idx + 0] = GetLinearIndex3D(0, 0, 0, i, j, k, nptsX, nptsY);
          elem_connectivity[idx + 1] = GetLinearIndex3D(1, 0, 0, i, j, k, nptsX, nptsY);
          elem_connectivity[idx + 2] = GetLinearIndex3D(1, 1, 0, i, j, k, nptsX, nptsY);
          elem_connectivity[idx + 3] = GetLinearIndex3D(0, 1, 0, i, j, k, nptsX, nptsY);
          elem_connectivity[idx + 4] = GetLinearIndex3D(0, 0, 1, i, j, k, nptsX, nptsY);
          elem_connectivity[idx + 5] = GetLinearIndex3D(1, 0, 1, i, j, k, nptsX, nptsY);
          elem_connectivity[idx + 6] = GetLinearIndex3D(1, 1, 1, i, j, k, nptsX, nptsY);
          elem_connectivity[idx + 7] = GetLinearIndex3D(0, 1, 1, i, j, k, nptsX, nptsY);

          idx_elem += 1;
          idx += HexaPointCount;
        }
        else // 3 tets, one polyhedron
        {
          elem_shapes[idx_elem + 0] = VTK_TETRA;
          elem_shapes[idx_elem + 1] = VTK_TETRA;
          elem_shapes[idx_elem + 2] = VTK_TETRA;
          elem_shapes[idx_elem + 3] = VTK_POLYHEDRON;

          constexpr int TetraPointCount = 4;
          constexpr int WedgeFaceCount = 5;
          constexpr int TrianglePointCount = 3;
          constexpr int QuadPointCount = 4;

          elem_sizes[idx_elem + 0] = TetraPointCount;
          elem_sizes[idx_elem + 1] = TetraPointCount;
          elem_sizes[idx_elem + 2] = TetraPointCount;
          elem_sizes[idx_elem + 3] = WedgeFaceCount;

          elem_offsets[idx_elem + 1] = elem_offsets[idx_elem + 0] + TetraPointCount;
          elem_offsets[idx_elem + 2] = elem_offsets[idx_elem + 1] + TetraPointCount;
          elem_offsets[idx_elem + 3] = elem_offsets[idx_elem + 2] + TetraPointCount;
          if (idx_elem + 4 < elem_offsets.size())
          {
            elem_offsets[idx_elem + 4] = elem_offsets[idx_elem + 3] + WedgeFaceCount;
          }

          elem_connectivity[idx + 0] = GetLinearIndex3D(0, 0, 0, i, j, k, nptsX, nptsY);
          elem_connectivity[idx + 1] = GetLinearIndex3D(1, 0, 0, i, j, k, nptsX, nptsY);
          elem_connectivity[idx + 2] = GetLinearIndex3D(0, 1, 0, i, j, k, nptsX, nptsY);
          elem_connectivity[idx + 3] = GetLinearIndex3D(0, 0, 1, i, j, k, nptsX, nptsY);

          elem_connectivity[idx + 4] = GetLinearIndex3D(1, 0, 0, i, j, k, nptsX, nptsY);
          elem_connectivity[idx + 5] = GetLinearIndex3D(1, 0, 1, i, j, k, nptsX, nptsY);
          elem_connectivity[idx + 6] = GetLinearIndex3D(0, 0, 1, i, j, k, nptsX, nptsY);
          elem_connectivity[idx + 7] = GetLinearIndex3D(0, 1, 1, i, j, k, nptsX, nptsY);

          elem_connectivity[idx + 8] = GetLinearIndex3D(0, 0, 1, i, j, k, nptsX, nptsY);
          elem_connectivity[idx + 9] = GetLinearIndex3D(0, 1, 1, i, j, k, nptsX, nptsY);
          elem_connectivity[idx + 10] = GetLinearIndex3D(0, 1, 0, i, j, k, nptsX, nptsY);
          elem_connectivity[idx + 11] = GetLinearIndex3D(1, 0, 0, i, j, k, nptsX, nptsY);

          // note: there are no shared faces in this example
          elem_connectivity[idx + 12] = 0 + WedgeFaceCount * polyhedronCounter;
          elem_connectivity[idx + 13] = 1 + WedgeFaceCount * polyhedronCounter;
          elem_connectivity[idx + 14] = 2 + WedgeFaceCount * polyhedronCounter;
          elem_connectivity[idx + 15] = 3 + WedgeFaceCount * polyhedronCounter;
          elem_connectivity[idx + 16] = 4 + WedgeFaceCount * polyhedronCounter;

          subelem_shapes[idx_elem2 + 0] = VTK_QUAD;
          subelem_shapes[idx_elem2 + 1] = VTK_QUAD;
          subelem_shapes[idx_elem2 + 2] = VTK_QUAD;
          subelem_shapes[idx_elem2 + 3] = VTK_TRIANGLE;
          subelem_shapes[idx_elem2 + 4] = VTK_TRIANGLE;

          subelem_sizes[idx_elem2 + 0] = QuadPointCount;
          subelem_sizes[idx_elem2 + 1] = QuadPointCount;
          subelem_sizes[idx_elem2 + 2] = QuadPointCount;
          subelem_sizes[idx_elem2 + 3] = TrianglePointCount;
          subelem_sizes[idx_elem2 + 4] = TrianglePointCount;

          subelem_offsets[idx_elem2 + 1] = subelem_offsets[idx_elem2 + 0] + QuadPointCount;
          subelem_offsets[idx_elem2 + 2] = subelem_offsets[idx_elem2 + 1] + QuadPointCount;
          subelem_offsets[idx_elem2 + 3] = subelem_offsets[idx_elem2 + 2] + QuadPointCount;
          subelem_offsets[idx_elem2 + 4] = subelem_offsets[idx_elem2 + 3] + TrianglePointCount;
          if (idx_elem2 + 5 < subelem_offsets.size())
          {
            subelem_offsets[idx_elem2 + 5] = subelem_offsets[idx_elem2 + 4] + TrianglePointCount;
          }

          subelem_connectivity[idx2 + 0] = GetLinearIndex3D(1, 0, 0, i, j, k, nptsX, nptsY);
          subelem_connectivity[idx2 + 1] = GetLinearIndex3D(1, 0, 1, i, j, k, nptsX, nptsY);
          subelem_connectivity[idx2 + 2] = GetLinearIndex3D(0, 1, 1, i, j, k, nptsX, nptsY);
          subelem_connectivity[idx2 + 3] = GetLinearIndex3D(0, 1, 0, i, j, k, nptsX, nptsY);

          subelem_connectivity[idx2 + 4] = GetLinearIndex3D(1, 0, 0, i, j, k, nptsX, nptsY);
          subelem_connectivity[idx2 + 5] = GetLinearIndex3D(1, 1, 0, i, j, k, nptsX, nptsY);
          subelem_connectivity[idx2 + 6] = GetLinearIndex3D(1, 1, 1, i, j, k, nptsX, nptsY);
          subelem_connectivity[idx2 + 7] = GetLinearIndex3D(1, 0, 1, i, j, k, nptsX, nptsY);

          subelem_connectivity[idx2 + 8] = GetLinearIndex3D(1, 1, 0, i, j, k, nptsX, nptsY);
          subelem_connectivity[idx2 + 9] = GetLinearIndex3D(0, 1, 0, i, j, k, nptsX, nptsY);
          subelem_connectivity[idx2 + 10] = GetLinearIndex3D(0, 1, 1, i, j, k, nptsX, nptsY);
          subelem_connectivity[idx2 + 11] = GetLinearIndex3D(1, 1, 1, i, j, k, nptsX, nptsY);

          subelem_connectivity[idx2 + 12] = GetLinearIndex3D(1, 0, 0, i, j, k, nptsX, nptsY);
          subelem_connectivity[idx2 + 13] = GetLinearIndex3D(0, 1, 0, i, j, k, nptsX, nptsY);
          subelem_connectivity[idx2 + 14] = GetLinearIndex3D(1, 1, 0, i, j, k, nptsX, nptsY);

          subelem_connectivity[idx2 + 15] = GetLinearIndex3D(1, 1, 1, i, j, k, nptsX, nptsY);
          subelem_connectivity[idx2 + 16] = GetLinearIndex3D(0, 1, 1, i, j, k, nptsX, nptsY);
          subelem_connectivity[idx2 + 17] = GetLinearIndex3D(1, 0, 1, i, j, k, nptsX, nptsY);

          idx_elem += 4; // three tets, 1 polyhedron
          idx += 3 * TetraPointCount + WedgeFaceCount;
          polyhedronCounter += 1;
          idx_elem2 += WedgeFaceCount; // five faces on the polyhedron
          idx2 += 3 * QuadPointCount + 2 * TrianglePointCount;
        }
      }
    }
  }

  auto elements = res["topologies/mesh/elements"];
  elements["shapes"].set(elem_shapes);
  elements["offsets"].set(elem_offsets);
  elements["sizes"].set(elem_sizes);
  elements["connectivity"].set(elem_connectivity);

  auto subelements = res["topologies/mesh/subelements"];
  subelements["shapes"].set(subelem_shapes);
  subelements["offsets"].set(subelem_offsets);
  subelements["sizes"].set(subelem_sizes);
  subelements["connectivity"].set(subelem_connectivity);
}

//----------------------------------------------------------------------------
bool ValidateMeshTypeMixed()
{
  conduit_cpp::Node mesh;
  constexpr int nX = 5, nY = 5, nZ = 5;
  CreateMixedUnstructuredMesh(5, 5, 5, mesh);
  auto data = Convert(mesh);

  VERIFY(vtkPartitionedDataSet::SafeDownCast(data) != nullptr,
    "incorrect data type, expected vtkPartitionedDataSet, got %s", vtkLogIdentifier(data));
  auto pds = vtkPartitionedDataSet::SafeDownCast(data);
  VERIFY(pds->GetNumberOfPartitions() == 1, "incorrect number of partitions, expected 1, got %d",
    pds->GetNumberOfPartitions());
  auto ug = vtkUnstructuredGrid::SafeDownCast(pds->GetPartition(0));

  VERIFY(ug->GetNumberOfPoints() == nX * nY * nZ, "expected %d points got %lld", nX * nY * nZ,
    ug->GetNumberOfPoints());

  // 160 cells expected: 4 layers of
  //                     - 2 columns with 4 hexahedra
  //                     - 2 columns with 4 polyhedra (wedges) and 12 tetra
  //                     96 tetras + 32 hexas + 32 polyhedra
  VERIFY(ug->GetNumberOfCells() == 160, "expected 160 cells, got %lld", ug->GetNumberOfCells());

  // check cell types
  auto it = vtkSmartPointer<vtkCellIterator>::Take(ug->NewCellIterator());

  int nPolyhedra(0), nTetra(0), nHexa(0), nCells(0);
  for (it->InitTraversal(); !it->IsDoneWithTraversal(); it->GoToNextCell())
  {
    ++nCells;
    const int cellType = it->GetCellType();
    switch (cellType)
    {
      case VTK_POLYHEDRON:
      {
        ++nPolyhedra;
        const vtkIdType nFaces = it->GetNumberOfFaces();
        VERIFY(nFaces == 5, "Expected 5 faces, got %lld", nFaces);
        break;
      }
      case VTK_HEXAHEDRON:
      {
        ++nHexa;
        break;
      }
      case VTK_TETRA:
      {
        ++nTetra;
        break;
      }
      default:
      {
        vtkLog(ERROR, "Expected only tetras, hexas and polyhedra.");
        return false;
      }
    }
  }

  VERIFY(nCells == 160, "Expected 160 cells, got %d", nCells);
  VERIFY(nTetra == 96, "Expected 96 tetras, got %d", nTetra);
  VERIFY(nHexa == 32, "Expected 32 hexahedra, got %d", nHexa);
  VERIFY(nPolyhedra == 32, "Expected 32 polyhedra, got %d", nPolyhedra);

  // Test Wedge and Pyramid cell type
  conduit_cpp::Node mesh2;
  CreateWedgeAndPyramidUnstructuredMesh(5, 5, 5, mesh2);
  data = Convert(mesh2);

  VERIFY(vtkPartitionedDataSet::SafeDownCast(data) != nullptr,
    "incorrect data type, expected vtkPartitionedDataSet, got %s", vtkLogIdentifier(data));
  pds = vtkPartitionedDataSet::SafeDownCast(data);
  VERIFY(pds->GetNumberOfPartitions() == 1, "incorrect number of partitions, expected 1, got %d",
    pds->GetNumberOfPartitions());
  ug = vtkUnstructuredGrid::SafeDownCast(pds->GetPartition(0));

  VERIFY(ug->GetNumberOfPoints() == nX * nY * nZ, "expected %d points got %lld", nX * nY * nZ,
    ug->GetNumberOfPoints());

  // 64 cells expected: 4 layers of
  //                     - 2 columns with 4 pyramids
  //                     - 2 columns with 4 wedges
  //                     32 pyramids + 32 wedges
  VERIFY(ug->GetNumberOfCells() == 64, "expected 64 cells, got %lld", ug->GetNumberOfCells());

  // check cell types
  it = vtkSmartPointer<vtkCellIterator>::Take(ug->NewCellIterator());

  int nPyramids(0), nWedges(0);
  nCells = 0;
  for (it->InitTraversal(); !it->IsDoneWithTraversal(); it->GoToNextCell())
  {
    ++nCells;
    const int cellType = it->GetCellType();
    switch (cellType)
    {
      case VTK_PYRAMID:
      {
        ++nPyramids;
        break;
      }
      case VTK_WEDGE:
      {
        ++nWedges;
        break;
      }
      default:
      {
        vtkLog(ERROR, "Expected only pyramids and wedges.");
        return false;
      }
    }
  }

  VERIFY(nCells == 64, "Expected 64 cells, got %d", nCells);
  VERIFY(nPyramids == 32, "Expected 32 pyramids, got %d", nPyramids);
  VERIFY(nWedges == 32, "Expected 32 wedges, got %d", nWedges);

  return true;
}

void CreatePolyhedra(Grid& grid, Attributes& attribs, unsigned int nx, unsigned int ny,
  unsigned int nz, conduit_cpp::Node& mesh)
{
  unsigned int numPoints[3] = { nx, ny, nz };
  double spacing[3] = { 1, 1.1, 1.3 };
  grid.Initialize(numPoints, spacing);
  attribs.Initialize(&grid);
  attribs.UpdateFields(0);

  mesh["coordsets/coords/type"].set("explicit");

  mesh["coordsets/coords/values/x"].set_external(const_cast<double*>(grid.GetPoints().data()),
    grid.GetNumberOfPoints(), /*offset=*/0, /*stride=*/3 * sizeof(double));
  mesh["coordsets/coords/values/y"].set_external(const_cast<double*>(grid.GetPoints().data()),
    grid.GetNumberOfPoints(),
    /*offset=*/sizeof(double), /*stride=*/3 * sizeof(double));
  mesh["coordsets/coords/values/z"].set_external(const_cast<double*>(grid.GetPoints().data()),
    grid.GetNumberOfPoints(),
    /*offset=*/2 * sizeof(double), /*stride=*/3 * sizeof(double));

  // Next, add topology
  mesh["topologies/mesh/type"].set("unstructured");
  mesh["topologies/mesh/coordset"].set("coords");

  // add elements
  using VecT = std::vector<unsigned int>;

  mesh["topologies/mesh/elements/shape"].set("polyhedral");
  mesh["topologies/mesh/elements/connectivity"].set_external(
    const_cast<VecT&>(grid.GetPolyhedralCells().Connectivity));
  mesh["topologies/mesh/elements/sizes"].set_external(
    const_cast<VecT&>(grid.GetPolyhedralCells().Sizes));
  mesh["topologies/mesh/elements/offsets"].set_external(
    const_cast<VecT&>(grid.GetPolyhedralCells().Offsets));

  // add faces (aka subelements)
  mesh["topologies/mesh/subelements/shape"].set("polygonal");
  mesh["topologies/mesh/subelements/connectivity"].set_external(
    const_cast<VecT&>(grid.GetPolygonalFaces().Connectivity));
  mesh["topologies/mesh/subelements/sizes"].set_external(
    const_cast<VecT&>(grid.GetPolygonalFaces().Sizes));
  mesh["topologies/mesh/subelements/offsets"].set_external(
    const_cast<VecT&>(grid.GetPolygonalFaces().Offsets));

  // Finally, add fields.
  auto fields = mesh["fields"];
  fields["velocity/association"].set("vertex");
  fields["velocity/topology"].set("mesh");
  fields["velocity/volume_dependent"].set("false");

  // velocity is stored in non-interlaced form (unlike points).
  fields["velocity/values/x"].set_external(
    attribs.GetVelocityArray().data(), grid.GetNumberOfPoints(), /*offset=*/0);
  fields["velocity/values/y"].set_external(attribs.GetVelocityArray().data(),
    grid.GetNumberOfPoints(),
    /*offset=*/grid.GetNumberOfPoints() * sizeof(double));
  fields["velocity/values/z"].set_external(attribs.GetVelocityArray().data(),
    grid.GetNumberOfPoints(),
    /*offset=*/grid.GetNumberOfPoints() * sizeof(double) * 2);

  // pressure is cell-data.
  fields["pressure/association"].set("element");
  fields["pressure/topology"].set("mesh");
  fields["pressure/volume_dependent"].set("false");
  fields["pressure/values"].set_external(
    attribs.GetPressureArray().data(), grid.GetNumberOfCells());
}

bool ValidatePolyhedra()
{
  conduit_cpp::Node mesh;
  constexpr int nX = 4, nY = 4, nZ = 4;
  Grid grid;
  Attributes attribs;
  CreatePolyhedra(grid, attribs, nX, nY, nZ, mesh);
  auto values = mesh["fields/velocity/values"];
  auto data = Convert(mesh);

  VERIFY(vtkPartitionedDataSet::SafeDownCast(data) != nullptr,
    "incorrect data type, expected vtkPartitionedDataSet, got %s", vtkLogIdentifier(data));
  auto pds = vtkPartitionedDataSet::SafeDownCast(data);
  VERIFY(pds->GetNumberOfPartitions() == 1, "incorrect number of partitions, expected 1, got %d",
    pds->GetNumberOfPartitions());
  auto ug = vtkUnstructuredGrid::SafeDownCast(pds->GetPartition(0));

  VERIFY(ug->GetNumberOfPoints() == static_cast<vtkIdType>(grid.GetNumberOfPoints()),
    "expected %zu points got %lld", grid.GetNumberOfPoints(), ug->GetNumberOfPoints());

  VERIFY(ug->GetNumberOfCells() == static_cast<vtkIdType>(grid.GetNumberOfCells()),
    "expected %zu cells, got %lld", grid.GetNumberOfCells(), ug->GetNumberOfCells());

  // check cell types
  auto it = vtkSmartPointer<vtkCellIterator>::Take(ug->NewCellIterator());

  vtkIdType nPolyhedra(0);
  for (it->InitTraversal(); !it->IsDoneWithTraversal(); it->GoToNextCell())
  {
    const int cellType = it->GetCellType();
    switch (cellType)
    {
      case VTK_POLYHEDRON:
      {
        ++nPolyhedra;
        const vtkIdType nFaces = it->GetNumberOfFaces();
        VERIFY(nFaces == 6, "Expected 6 faces, got %lld", nFaces);
        break;
      }
      default:
      {
        vtkLog(ERROR, "Expected only polyhedra.");
        return false;
      }
    }
  }

  VERIFY(nPolyhedra == static_cast<vtkIdType>(grid.GetNumberOfCells()),
    "Expected %zu polyhedra, got %lld", grid.GetNumberOfCells(), nPolyhedra);
  return true;
}

} // end namespace

//----------------------------------------------------------------------------
int TestConduitSource(int argc, char** argv)
{
#if VTK_MODULE_ENABLE_VTK_ParallelMPI
  vtkNew<vtkMPIController> controller;
#else
  vtkNew<vtkDummyController> controller;
#endif
  controller->Initialize(&argc, &argv);
  vtkMultiProcessController::SetGlobalController(controller);

  std::string amrFile =
    vtkTestUtilities::ExpandDataFileName(argc, argv, "Data/Conduit/bp_amr_example.json");

  auto ret = ValidateMeshTypeUniform() && ValidateMeshTypeRectilinear() &&
      ValidateMeshTypeStructured() && ValidateMeshTypeUnstructured() && ValidateFieldData() &&
      ValidateRectilinearGridWithDifferentDimensions() && Validate1DRectilinearGrid() &&
      ValidateMeshTypeMixed() && ValidateMeshTypeMixed2D() && ValidateMeshTypeAMR(amrFile) &&
      ValidateAscentGhostCellData() && ValidateAscentGhostPointData() && ValidateMeshTypePoints() &&
      ValidateDistributedAMR() && ValidatePolyhedra()

    ? EXIT_SUCCESS
    : EXIT_FAILURE;

  controller->Finalize();

  return ret;
}
