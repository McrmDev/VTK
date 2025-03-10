// SPDX-FileCopyrightText: Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
// SPDX-License-Identifier: BSD-3-Clause
/**
 * @class   vtkCleanPolyData
 * @brief   merge duplicate points, and/or remove unused points and/or remove degenerate cells
 *
 * vtkCleanPolyData is a filter that takes polygonal data as input and
 * generates polygonal data as output. vtkCleanPolyData will merge duplicate
 * points (within specified tolerance and if enabled), eliminate points
 * that are not used in any cell, and if enabled, transform degenerate cells into
 * appropriate forms (for example, a triangle is converted into a line
 * if two points of triangle are merged).
 *
 * Conversion of degenerate cells is controlled by the flags
 * ConvertLinesToPoints, ConvertPolysToLines, ConvertStripsToPolys which act
 * cumulatively such that a degenerate strip may become a poly.
 * The full set is
 * Line with 1 points -> Vert (if ConvertLinesToPoints)
 * Poly with 2 points -> Line (if ConvertPolysToLines)
 * Poly with 1 points -> Vert (if ConvertPolysToLines && ConvertLinesToPoints)
 * Strp with 3 points -> Poly (if ConvertStripsToPolys)
 * Strp with 2 points -> Line (if ConvertStripsToPolys && ConvertPolysToLines)
 * Strp with 1 points -> Vert (if ConvertStripsToPolys && ConvertPolysToLines
 *   && ConvertLinesToPoints)
 *
 * Cells of type VTK_POLY_LINE will be converted to a vertex only if
 * ConvertLinesToPoints is on and all points are merged into one. Degenerate line
 * segments (with two identical end points) will be removed.
 *
 * If tolerance is specified precisely=0.0, then vtkCleanPolyData will use
 * the vtkMergePoints object to merge points (which is faster). Otherwise the
 * slower vtkIncrementalPointLocator is used.  Before inserting points into the point
 * locator, this class calls a function OperateOnPoint which can be used (in
 * subclasses) to further refine the cleaning process. See
 * vtkQuantizePolyDataPoints.
 *
 * In addition, if a point global id array is available, then two points are merged
 * if and only if they share the same global id.
 *
 * Note that merging of points can be disabled. In this case, a point locator
 * will not be used, and points that are not used by any cells will be
 * eliminated, but never merged.
 *
 * @warning
 * Merging points can alter topology, including introducing non-manifold
 * forms. The tolerance should be chosen carefully to avoid these problems.
 * Subclasses should handle OperateOnBounds as well as OperateOnPoint
 * to ensure that the locator is correctly initialized (i.e. all modified
 * points must lie inside modified bounds).
 *
 * @warning
 * If you wish to operate on a set of point coordinates that has no cells,
 * you must add a vtkPolyVertex cell with all of the points to the PolyData
 * (or use a vtkVertexGlyphFilter) before using the vtkCleanPolyData filter.
 *
 * @warning
 * The vtkStaticCleanPolyData filter is similar in operation to
 * vtkCleanPolyData. However, vtkStaticCleanPolyData is non-incremental and
 * uses a much faster threading approach (especially for larger datasets, and
 * when merging points with a non-zero tolerance). However because of the
 * difference in the traversal order in the point merging process, the output
 * of the filters may be different.
 *
 * @sa
 * vtkQuantizePolyDataPoints vtkStaticCleanPolyData
 * vtkStaticCleanUnstructuredGrid
 */

#ifndef vtkCleanPolyData_h
#define vtkCleanPolyData_h

#include "vtkFiltersCoreModule.h" // For export macro
#include "vtkPolyDataAlgorithm.h"

#include <unordered_set>

VTK_ABI_NAMESPACE_BEGIN
class vtkIncrementalPointLocator;

class VTKFILTERSCORE_EXPORT vtkCleanPolyData : public vtkPolyDataAlgorithm
{
public:
  static vtkCleanPolyData* New();
  void PrintSelf(ostream& os, vtkIndent indent) override;
  vtkTypeMacro(vtkCleanPolyData, vtkPolyDataAlgorithm);

  ///@{
  /**
   * By default ToleranceIsAbsolute is false and Tolerance is
   * a fraction of Bounding box diagonal, if true, AbsoluteTolerance is
   * used when adding points to locator (merging)
   */
  vtkSetMacro(ToleranceIsAbsolute, vtkTypeBool);
  vtkBooleanMacro(ToleranceIsAbsolute, vtkTypeBool);
  vtkGetMacro(ToleranceIsAbsolute, vtkTypeBool);
  ///@}

  ///@{
  /**
   * Specify tolerance in terms of fraction of bounding box length.
   * Default is 0.0.
   */
  vtkSetClampMacro(Tolerance, double, 0.0, 1.0);
  vtkGetMacro(Tolerance, double);
  ///@}

  ///@{
  /**
   * Specify tolerance in absolute terms. Default is 1.0.
   */
  vtkSetClampMacro(AbsoluteTolerance, double, 0.0, VTK_DOUBLE_MAX);
  vtkGetMacro(AbsoluteTolerance, double);
  ///@}

  ///@{
  /**
   * Turn on/off conversion of degenerate lines to points. Default is On.
   */
  vtkSetMacro(ConvertLinesToPoints, vtkTypeBool);
  vtkBooleanMacro(ConvertLinesToPoints, vtkTypeBool);
  vtkGetMacro(ConvertLinesToPoints, vtkTypeBool);
  ///@}

  ///@{
  /**
   * Turn on/off conversion of degenerate polys to lines. Default is On.
   */
  vtkSetMacro(ConvertPolysToLines, vtkTypeBool);
  vtkBooleanMacro(ConvertPolysToLines, vtkTypeBool);
  vtkGetMacro(ConvertPolysToLines, vtkTypeBool);
  ///@}

  ///@{
  /**
   * Turn on/off conversion of degenerate strips to polys. Default is On.
   */
  vtkSetMacro(ConvertStripsToPolys, vtkTypeBool);
  vtkBooleanMacro(ConvertStripsToPolys, vtkTypeBool);
  vtkGetMacro(ConvertStripsToPolys, vtkTypeBool);
  ///@}

  ///@{
  /**
   * Set/Get a boolean value that controls whether point merging is
   * performed. If on, a locator will be used, and points laying within
   * the appropriate tolerance may be merged. If off, points are never
   * merged. By default, merging is on.
   */
  vtkSetMacro(PointMerging, vtkTypeBool);
  vtkGetMacro(PointMerging, vtkTypeBool);
  vtkBooleanMacro(PointMerging, vtkTypeBool);
  ///@}

  ///@{
  /**
   * Set/Get a spatial locator for speeding the search process. By
   * default an instance of vtkMergePoints is used.
   */
  virtual void SetLocator(vtkIncrementalPointLocator* locator);
  vtkGetObjectMacro(Locator, vtkIncrementalPointLocator);
  ///@}

  /**
   * Create default locator. Used to create one when none is specified.
   */
  void CreateDefaultLocator(vtkPolyData* input = nullptr);

  /**
   * Release locator
   */
  void ReleaseLocator() { this->SetLocator(nullptr); }

  /**
   * Get the MTime of this object also considering the locator.
   */
  vtkMTimeType GetMTime() override;

  /**
   * Perform operation on a point
   */
  virtual void OperateOnPoint(double in[3], double out[3]);

  /**
   * Perform operation on bounds
   */
  virtual void OperateOnBounds(double in[6], double out[6]);

  // This filter is difficult to stream.
  // To get invariant results, the whole input must be processed at once.
  // This flag allows the user to select whether strict piece invariance
  // is required.  By default it is on.  When off, the filter can stream,
  // but results may change.
  vtkSetMacro(PieceInvariant, vtkTypeBool);
  vtkGetMacro(PieceInvariant, vtkTypeBool);
  vtkBooleanMacro(PieceInvariant, vtkTypeBool);

  ///@{
  /**
   * Set/get the desired precision for the output types. See the documentation
   * for the vtkAlgorithm::DesiredOutputPrecision enum for an explanation of
   * the available precision settings.
   */
  vtkSetMacro(OutputPointsPrecision, int);
  vtkGetMacro(OutputPointsPrecision, int);
  ///@}

protected:
  vtkCleanPolyData();
  ~vtkCleanPolyData() override;

  // Usual data generation method
  int RequestData(vtkInformation*, vtkInformationVector**, vtkInformationVector*) override;
  int RequestUpdateExtent(vtkInformation*, vtkInformationVector**, vtkInformationVector*) override;

  vtkTypeBool PointMerging;
  double Tolerance;
  double AbsoluteTolerance;
  vtkTypeBool ConvertLinesToPoints;
  vtkTypeBool ConvertPolysToLines;
  vtkTypeBool ConvertStripsToPolys;
  vtkTypeBool ToleranceIsAbsolute;
  vtkIncrementalPointLocator* Locator;

  vtkTypeBool PieceInvariant;
  int OutputPointsPrecision;

private:
  vtkCleanPolyData(const vtkCleanPolyData&) = delete;
  void operator=(const vtkCleanPolyData&) = delete;

  // Check whether a point is a primary point (as opposed to duplicated ghost point).
  bool IsPrimaryPoint(vtkPolyData* input, vtkIdType ptIndex);
  // Check whether a point's data has already been copied to the output.
  bool IsPointDataAlreadyCopied(vtkIdType ptIndex);
  // Insert point into newPts. If already present, only get its id.
  void InsertUniquePoint(vtkIdTypeArray* globalIdsArray, vtkIdType ptIndex, vtkPoints* newPts,
    std::unordered_map<vtkIdType, vtkIdType>& addedGlobalIdsMap, double* point, vtkIdType& ptId);

  std::unordered_set<vtkIdType> CopiedPoints;
};

VTK_ABI_NAMESPACE_END
#endif
