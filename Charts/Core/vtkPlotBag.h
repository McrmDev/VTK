// SPDX-FileCopyrightText: Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
// SPDX-License-Identifier: BSD-3-Clause

/**
 * @class   vtkPlotBag
 * @brief   Class for drawing an a bagplot.
 *
 *
 * This class allows to draw a bagplot given three columns from
 * a vtkTable. The first two columns will represent X,Y as it is for
 * vtkPlotPoints. The third one will have to specify if the density
 * assigned to each point (generally obtained by the
 * vtkHighestDensityRegionsStatistics filter).
 * Points are drawn in a plot points fashion and 2 convex hull polygons
 * are drawn around the median and the 3 quartile of the density field.
 *
 * @sa
 * vtkHighestDensityRegionsStatistics
 */

#ifndef vtkPlotBag_h
#define vtkPlotBag_h

#include "vtkChartsCoreModule.h" // For export macro
#include "vtkPlotPoints.h"
#include "vtkWrappingHints.h" // For VTK_MARSHALAUTO

VTK_ABI_NAMESPACE_BEGIN
class vtkPen;

class VTKCHARTSCORE_EXPORT VTK_MARSHALAUTO vtkPlotBag : public vtkPlotPoints
{
public:
  vtkTypeMacro(vtkPlotBag, vtkPlotPoints);
  void PrintSelf(ostream& os, vtkIndent indent) override;

  /**
   * Creates a new Bag Plot object.
   */
  static vtkPlotBag* New();

  /**
   * Paint event for the XY plot, called whenever the chart needs to be drawn.
   */
  bool Paint(vtkContext2D* painter) override;

  /**
   * Paint legend event for the XY plot, called whenever the legend needs the
   * plot items symbol/mark/line drawn. A rect is supplied with the lower left
   * corner of the rect (elements 0 and 1) and with width x height (elements 2
   * and 3). The plot can choose how to fill the space supplied.
   */
  bool PaintLegend(vtkContext2D* painter, const vtkRectf& rect, int legendIndex) override;

  /**
   * Get the plot labels. If this array has a length greater than 1 the index
   * refers to the stacked objects in the plot. See vtkPlotBar for example.
   */
  vtkStringArray* GetLabels() override;

  /**
   * Generate and return the tooltip label string for this plot
   * The segmentIndex parameter is ignored, except for vtkPlotBar
   */
  vtkStdString GetTooltipLabel(
    const vtkVector2d& plotPos, vtkIdType seriesIndex, vtkIdType segmentIndex) override;

  ///@{
  /**
   * Set the input, we are expecting a vtkTable with three columns. The first
   * column and the second represent the x,y position . The five others
   * columns represent the quartiles used to display the box.
   * Inherited method will call the last SetInputData method with default
   * parameters.
   */
  void SetInputData(vtkTable* table) override;
  void SetInputData(
    vtkTable* table, const vtkStdString& yColumn, const vtkStdString& densityColumn) override;
  virtual void SetInputData(vtkTable* table, const vtkStdString& xColumn,
    const vtkStdString& yColumn, const vtkStdString& densityColumn);
  ///@}

  virtual void SetInputData(
    vtkTable* table, vtkIdType xColumn, vtkIdType yColumn, vtkIdType densityColumn);

  ///@{
  /**
   * Set/get the visibility of the bags.
   * True by default.
   */
  vtkSetMacro(BagVisible, bool);
  vtkGetMacro(BagVisible, bool);
  ///@}

  ///@{
  /**
   * Set/get the vtkPen object that controls how this plot draws boundary lines.
   */
  void SetLinePen(vtkPen* pen);
  vtkGetObjectMacro(LinePen, vtkPen);
  ///@}

  /**
   * Set/get the vtkPen object that controls how this plot draws points.
   * Those are just helpers functions:
   * this pen is actually the default Plot pen.
   */
  void SetPointPen(vtkPen* pen) { this->SetPen(pen); }
  vtkPen* GetPointPen() { return this->GetPen(); }

  /**
   * Update the internal cache. Returns true if cache was successfully updated. Default does
   * nothing.
   * This method is called by Update() when either the plot's data has changed or
   * CacheRequiresUpdate() returns true. It is not necessary to call this method explicitly.
   */
  bool UpdateCache() override;

protected:
  vtkPlotBag();
  ~vtkPlotBag() override;

  bool BagVisible;
  vtkPoints2D* MedianPoints;
  vtkPoints2D* Q3Points;
  vtkPen* LinePen;

private:
  vtkPlotBag(const vtkPlotBag&) = delete;
  void operator=(const vtkPlotBag&) = delete;
};

VTK_ABI_NAMESPACE_END
#endif // vtkPlotBag_h
