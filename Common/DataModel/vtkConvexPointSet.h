// SPDX-FileCopyrightText: Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
// SPDX-License-Identifier: BSD-3-Clause
/**
 * @class   vtkConvexPointSet
 * @brief   a 3D cell defined by a set of convex points
 *
 * vtkConvexPointSet is a concrete implementation that represents a 3D cell
 * defined by a convex set of points. An example of such a cell is an octant
 * (from an octree). vtkConvexPointSet uses the ordered triangulations
 * approach (vtkOrderedTriangulator) to create triangulations guaranteed to
 * be compatible across shared faces. This allows a general approach to
 * processing complex, convex cell types.
 *
 * @sa
 * vtkHexahedron vtkPyramid vtkTetra vtkVoxel vtkWedge
 */

#ifndef vtkConvexPointSet_h
#define vtkConvexPointSet_h

#include "vtkCell3D.h"
#include "vtkCommonDataModelModule.h" // For export macro
#include "vtkDeprecation.h"           // For VTK_DEPRECATED_IN_9_5_0

VTK_ABI_NAMESPACE_BEGIN
class vtkUnstructuredGrid;
class vtkCellArray;
class vtkTriangle;
class vtkTetra;
class vtkDoubleArray;

class VTKCOMMONDATAMODEL_EXPORT vtkConvexPointSet : public vtkCell3D
{
public:
  static vtkConvexPointSet* New();
  vtkTypeMacro(vtkConvexPointSet, vtkCell3D);
  void PrintSelf(ostream& os, vtkIndent indent) override;

  /**
   * See vtkCell3D API for description of this method.
   */
#ifndef VTK_LEGACY_REMOVE
  VTK_DEPRECATED_IN_9_5_0("HasFixedTopology() is always 0 and will be removed")
  virtual vtkTypeBool HasFixedTopology() { return 0; }
#endif

  ///@{
  /**
   * See vtkCell3D API for description of these methods.
   * @warning These method are unimplemented in vtkConvexPointSet
   */
  void GetEdgePoints(vtkIdType vtkNotUsed(edgeId), const vtkIdType*& vtkNotUsed(pts)) override
  {
    vtkWarningMacro(<< "vtkConvexPointSet::GetEdgePoints Not Implemented");
  }
  vtkIdType GetFacePoints(vtkIdType vtkNotUsed(faceId), const vtkIdType*& vtkNotUsed(pts)) override
  {
    vtkWarningMacro(<< "vtkConvexPointSet::GetFacePoints Not Implemented");
    return 0;
  }
  void GetEdgeToAdjacentFaces(
    vtkIdType vtkNotUsed(edgeId), const vtkIdType*& vtkNotUsed(pts)) override
  {
    vtkWarningMacro(<< "vtkConvexPointSet::GetEdgeToAdjacentFaces Not Implemented");
  }
  vtkIdType GetFaceToAdjacentFaces(
    vtkIdType vtkNotUsed(faceId), const vtkIdType*& vtkNotUsed(faceIds)) override
  {
    vtkWarningMacro(<< "vtkConvexPointSet::GetFaceToAdjacentFaces Not Implemented");
    return 0;
  }
  vtkIdType GetPointToIncidentEdges(
    vtkIdType vtkNotUsed(pointId), const vtkIdType*& vtkNotUsed(edgeIds)) override
  {
    vtkWarningMacro(<< "vtkConvexPointSet::GetPointToIncidentEdges Not Implemented");
    return 0;
  }
  vtkIdType GetPointToIncidentFaces(
    vtkIdType vtkNotUsed(pointId), const vtkIdType*& vtkNotUsed(faceIds)) override
  {
    vtkWarningMacro(<< "vtkConvexPointSet::GetPointToIncidentFaces Not Implemented");
    return 0;
  }
  vtkIdType GetPointToOneRingPoints(
    vtkIdType vtkNotUsed(pointId), const vtkIdType*& vtkNotUsed(pts)) override
  {
    vtkWarningMacro(<< "vtkConvexPointSet::GetPointToOneRingPoints Not Implemented");
    return 0;
  }
  bool GetCentroid(double vtkNotUsed(centroid)[3]) const override
  {
    vtkWarningMacro(<< "vtkConvexPointSet::GetCentroid Not Implemented");
    return false;
  }
  ///@}

  /**
   * See vtkCell3D API for description of this method.
   */
  double* GetParametricCoords() override;

  /**
   * See the vtkCell API for descriptions of these methods.
   */
  int GetCellType() override { return VTK_CONVEX_POINT_SET; }

  /**
   * This cell requires that it be initialized prior to access.
   */
  int RequiresInitialization() override { return 1; }
  void Initialize() override;

  ///@{
  /**
   * A convex point set has no explicit cell edge or faces; however
   * implicitly (after triangulation) it does. Currently the method
   * GetNumberOfEdges() always returns 0 while the GetNumberOfFaces() returns
   * the number of boundary triangles of the triangulation of the convex
   * point set. The method GetNumberOfFaces() triggers a triangulation of the
   * convex point set; repeated calls to GetFace() then return the boundary
   * faces. (Note: GetNumberOfEdges() currently returns 0 because it is a
   * rarely used method and hard to implement. It can be changed in the future.
   */
  int GetNumberOfEdges() override { return 0; }
  vtkCell* GetEdge(int) override { return nullptr; }
  int GetNumberOfFaces() override;
  vtkCell* GetFace(int faceId) override;
  ///@}

  /**
   * Satisfy the vtkCell API. This method contours by triangulating the
   * cell and then contouring the resulting tetrahedra.
   */
  void Contour(double value, vtkDataArray* cellScalars, vtkIncrementalPointLocator* locator,
    vtkCellArray* verts, vtkCellArray* lines, vtkCellArray* polys, vtkPointData* inPd,
    vtkPointData* outPd, vtkCellData* inCd, vtkIdType cellId, vtkCellData* outCd) override;

  /**
   * Satisfy the vtkCell API. This method contours by triangulating the
   * cell and then adding clip-edge intersection points into the
   * triangulation; extracting the clipped region.
   */
  void Clip(double value, vtkDataArray* cellScalars, vtkIncrementalPointLocator* locator,
    vtkCellArray* connectivity, vtkPointData* inPd, vtkPointData* outPd, vtkCellData* inCd,
    vtkIdType cellId, vtkCellData* outCd, int insideOut) override;

  /**
   * Satisfy the vtkCell API. This method determines the subId, pcoords,
   * and weights by triangulating the convex point set, and then
   * determining which tetrahedron the point lies in.
   */
  int EvaluatePosition(const double x[3], double closestPoint[3], int& subId, double pcoords[3],
    double& dist2, double weights[]) override;

  /**
   * The inverse of EvaluatePosition.
   */
  void EvaluateLocation(int& subId, const double pcoords[3], double x[3], double* weights) override;

  /**
   * Triangulates the cells and then intersects them to determine the
   * intersection point.
   */
  int IntersectWithLine(const double p1[3], const double p2[3], double tol, double& t, double x[3],
    double pcoords[3], int& subId) override;

  /**
   * Triangulate using methods of vtkOrderedTriangulator.
   */
  int TriangulateLocalIds(int index, vtkIdList* ptIds) override;

  /**
   * Computes derivatives by triangulating and from subId and pcoords,
   * evaluating derivatives on the resulting tetrahedron.
   */
  void Derivatives(
    int subId, const double pcoords[3], const double* values, int dim, double* derivs) override;

  /**
   * Returns the set of points forming a face of the triangulation of these
   * points that are on the boundary of the cell that are closest
   * parametrically to the point specified.
   */
  int CellBoundary(int subId, const double pcoords[3], vtkIdList* pts) override;

  /**
   * Return the center of the cell in parametric coordinates.
   */
  int GetParametricCenter(double pcoords[3]) override;

  /**
   * A convex point set is triangulated prior to any operations on it so
   * it is not a primary cell, it is a composite cell.
   */
  int IsPrimaryCell() VTK_FUTURE_CONST override { return 0; }

  ///@{
  /**
   * Compute the interpolation functions/derivatives
   * (aka shape functions/derivatives)
   */
  void InterpolateFunctions(const double pcoords[3], double* sf) override;
  void InterpolateDerivs(const double pcoords[3], double* derivs) override;
  ///@}

protected:
  vtkConvexPointSet();
  ~vtkConvexPointSet() override;

  vtkTetra* Tetra;
  vtkIdList* TetraIds;
  vtkPoints* TetraPoints;
  vtkDoubleArray* TetraScalars;

  vtkCellArray* BoundaryTris;
  vtkTriangle* Triangle;
  vtkDoubleArray* ParametricCoords;

private:
  vtkConvexPointSet(const vtkConvexPointSet&) = delete;
  void operator=(const vtkConvexPointSet&) = delete;
};

//----------------------------------------------------------------------------
inline int vtkConvexPointSet::GetParametricCenter(double pcoords[3])
{
  pcoords[0] = pcoords[1] = pcoords[2] = 0.5;
  return 0;
}

VTK_ABI_NAMESPACE_END
#endif
