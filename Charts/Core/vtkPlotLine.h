// SPDX-FileCopyrightText: Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
// SPDX-License-Identifier: BSD-3-Clause

/**
 * @class   vtkPlotLine
 * @brief   Class for drawing an XY line plot given two columns from
 * a vtkTable.
 *
 *
 *
 */

#ifndef vtkPlotLine_h
#define vtkPlotLine_h

#include "vtkChartsCoreModule.h" // For export macro
#include "vtkPlotPoints.h"
#include "vtkWrappingHints.h" // For VTK_MARSHALAUTO

VTK_ABI_NAMESPACE_BEGIN
class VTKCHARTSCORE_EXPORT VTK_MARSHALAUTO vtkPlotLine : public vtkPlotPoints
{
public:
  vtkTypeMacro(vtkPlotLine, vtkPlotPoints);
  void PrintSelf(ostream& os, vtkIndent indent) override;

  /**
   * Creates a 2D Chart object.
   */
  static vtkPlotLine* New();

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

  ///@{
  /**
   * Turn on/off flag to control whether the points define a poly line
   * (true) or multiple line segments (false).
   * If true (default), a segment is drawn between each points
   * (e.g. [P1P2, P2P3, P3P4...].) If false, a segment is drawn for each pair
   * of points (e.g. [P1P2, P3P4,...].)
   */
  vtkSetMacro(PolyLine, bool);
  vtkGetMacro(PolyLine, bool);
  vtkBooleanMacro(PolyLine, bool);
  ///@}

protected:
  vtkPlotLine();
  ~vtkPlotLine() override;

  /**
   * Poly line (true) or line segments(false).
   */
  bool PolyLine;

private:
  vtkPlotLine(const vtkPlotLine&) = delete;
  void operator=(const vtkPlotLine&) = delete;
};

VTK_ABI_NAMESPACE_END
#endif // vtkPlotLine_h
