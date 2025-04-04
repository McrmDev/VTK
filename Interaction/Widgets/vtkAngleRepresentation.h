// SPDX-FileCopyrightText: Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
// SPDX-License-Identifier: BSD-3-Clause
/**
 * @class   vtkAngleRepresentation
 * @brief   represent the vtkAngleWidget
 *
 * The vtkAngleRepresentation is a superclass for classes representing the
 * vtkAngleWidget. This representation consists of two rays and three
 * vtkHandleRepresentations to place and manipulate the three points defining
 * the angle representation. (Note: the three points are referred to as Point1,
 * Center, and Point2, at the two end points (Point1 and Point2) and Center
 * (around which the angle is measured).
 *
 * @sa
 * vtkAngleWidget vtkHandleRepresentation vtkAngleRepresentation2D
 */

#ifndef vtkAngleRepresentation_h
#define vtkAngleRepresentation_h

#include "vtkInteractionWidgetsModule.h" // For export macro
#include "vtkWidgetRepresentation.h"
#include "vtkWrappingHints.h" // For VTK_MARSHALAUTO

VTK_ABI_NAMESPACE_BEGIN
class vtkHandleRepresentation;

class VTKINTERACTIONWIDGETS_EXPORT VTK_MARSHALAUTO vtkAngleRepresentation
  : public vtkWidgetRepresentation
{
public:
  ///@{
  /**
   * Standard VTK methods.
   */
  vtkTypeMacro(vtkAngleRepresentation, vtkWidgetRepresentation);
  void PrintSelf(ostream& os, vtkIndent indent) override;
  ///@}

  /**
   * This representation and all subclasses must keep an angle (in degrees)
   * consistent with the state of the widget.
   */
  virtual double GetAngle() = 0;

  ///@{
  /**
   * Methods to Set/Get the coordinates of the three points defining
   * this representation. Note that methods are available for both
   * display and world coordinates.
   */
  virtual void GetPoint1WorldPosition(double pos[3]) VTK_FUTURE_CONST = 0;
  virtual void GetCenterWorldPosition(double pos[3]) VTK_FUTURE_CONST = 0;
  virtual void GetPoint2WorldPosition(double pos[3]) VTK_FUTURE_CONST = 0;
  virtual void SetPoint1DisplayPosition(double pos[3]) = 0;
  virtual void SetCenterDisplayPosition(double pos[3]) = 0;
  virtual void SetPoint2DisplayPosition(double pos[3]) = 0;
  virtual void GetPoint1DisplayPosition(double pos[3]) VTK_FUTURE_CONST = 0;
  virtual void GetCenterDisplayPosition(double pos[3]) VTK_FUTURE_CONST = 0;
  virtual void GetPoint2DisplayPosition(double pos[3]) VTK_FUTURE_CONST = 0;
  ///@}

  ///@{
  /**
   * This method is used to specify the type of handle representation to use
   * for the three internal vtkHandleWidgets within vtkAngleRepresentation.
   * To use this method, create a dummy vtkHandleRepresentation (or
   * subclass), and then invoke this method with this dummy. Then the
   * vtkAngleRepresentation uses this dummy to clone three
   * vtkHandleRepresentations of the same type. Make sure you set the handle
   * representation before the widget is enabled. (The method
   * InstantiateHandleRepresentation() is invoked by the vtkAngle widget.)
   */
  void SetHandleRepresentation(vtkHandleRepresentation* handle);
  void InstantiateHandleRepresentation();
  ///@}

  ///@{
  /**
   * Set/Get the handle representations used for the vtkAngleRepresentation.
   */
  vtkGetObjectMacro(Point1Representation, vtkHandleRepresentation);
  vtkGetObjectMacro(CenterRepresentation, vtkHandleRepresentation);
  vtkGetObjectMacro(Point2Representation, vtkHandleRepresentation);
  ///@}

  ///@{
  /**
   * The tolerance representing the distance to the representation (in
   * pixels) in which the cursor is considered near enough to the end points
   * of the representation to be active.
   */
  vtkSetClampMacro(Tolerance, int, 1, 100);
  vtkGetMacro(Tolerance, int);
  ///@}

  ///@{
  /**
   * Specify the format to use for labeling the angle. Note that an empty
   * string results in no label, or a format string without a "%" character
   * will not print the angle value.
   */
  vtkSetStringMacro(LabelFormat);
  vtkGetStringMacro(LabelFormat);
  ///@}

  ///@{
  /**
   * Set the scale factor from degrees. The label will be defined in terms of the scaled space. For
   * example, to use radians in the label set the scale factor to pi/180.
   */
  vtkSetMacro(Scale, double);
  vtkGetMacro(Scale, double);
  ///@}

  ///@{
  /**
   * Special methods for turning off the rays and arc that define the cone
   * and arc of the angle.
   */
  vtkSetMacro(Ray1Visibility, vtkTypeBool);
  vtkGetMacro(Ray1Visibility, vtkTypeBool);
  vtkBooleanMacro(Ray1Visibility, vtkTypeBool);
  vtkSetMacro(Ray2Visibility, vtkTypeBool);
  vtkGetMacro(Ray2Visibility, vtkTypeBool);
  vtkBooleanMacro(Ray2Visibility, vtkTypeBool);
  vtkSetMacro(ArcVisibility, vtkTypeBool);
  vtkGetMacro(ArcVisibility, vtkTypeBool);
  vtkBooleanMacro(ArcVisibility, vtkTypeBool);
  ///@}

  // Used to communicate about the state of the representation
  enum
  {
    Outside = 0,
    NearP1,
    NearCenter,
    NearP2
  };

  ///@{
  /**
   * These are methods that satisfy vtkWidgetRepresentation's API.
   */
  void BuildRepresentation() override;
  int ComputeInteractionState(int X, int Y, int modify = 0) override;
  void StartWidgetInteraction(double e[2]) override;
  virtual void CenterWidgetInteraction(double e[2]);
  void WidgetInteraction(double e[2]) override;
  void SetRenderer(vtkRenderer* ren) override;
  ///@}

protected:
  vtkAngleRepresentation();
  ~vtkAngleRepresentation() override;

  // The handle and the rep used to close the handles
  vtkHandleRepresentation* HandleRepresentation;
  vtkHandleRepresentation* Point1Representation;
  vtkHandleRepresentation* CenterRepresentation;
  vtkHandleRepresentation* Point2Representation;

  // Selection tolerance for the handles
  int Tolerance = 5;

  // Visibility of the various pieces of the representation
  vtkTypeBool Ray1Visibility;
  vtkTypeBool Ray2Visibility;
  vtkTypeBool ArcVisibility;

  // Format for the label
  char* LabelFormat;

  // Scale to change from degrees to the desired unit system (radians, fractions of pi) for
  // displaying the angle
  double Scale = 1.0;

private:
  vtkAngleRepresentation(const vtkAngleRepresentation&) = delete;
  void operator=(const vtkAngleRepresentation&) = delete;
};

VTK_ABI_NAMESPACE_END
#endif
