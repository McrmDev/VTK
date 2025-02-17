// SPDX-FileCopyrightText: Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
// SPDX-License-Identifier: BSD-3-Clause
/**
 * @class   vtkCaptionRepresentation
 * @brief   represents vtkCaptionWidget in the scene
 *
 * This class represents vtkCaptionWidget. A caption is defined by some text
 * with a leader (e.g., arrow) that points from the text to a point in the
 * scene. The caption is defined by an instance of vtkCaptionActor2D. It uses
 * the event bindings of its superclass (vtkBorderWidget) to control the
 * placement of the text, and adds the ability to move the attachment point
 * around. In addition, when the caption text is selected, the widget emits a
 * ActivateEvent that observers can watch for. This is useful for opening GUI
 * dialogoues to adjust font characteristics, etc. (Please see the superclass
 * for a description of event bindings.)
 *
 * Note that this widget extends the behavior of its superclass
 * vtkBorderRepresentation.
 *
 * @sa
 * vtkCaptionWidget vtkBorderWidget vtkBorderRepresentation vtkCaptionActor
 */

#ifndef vtkCaptionRepresentation_h
#define vtkCaptionRepresentation_h

#include "vtkBorderRepresentation.h"
#include "vtkInteractionWidgetsModule.h" // For export macro
#include "vtkWrappingHints.h"            // For VTK_MARSHALAUTO

VTK_ABI_NAMESPACE_BEGIN
class vtkRenderer;
class vtkCaptionActor2D;
class vtkConeSource;
class vtkPointHandleRepresentation3D;

class VTKINTERACTIONWIDGETS_EXPORT VTK_MARSHALAUTO vtkCaptionRepresentation
  : public vtkBorderRepresentation
{
public:
  /**
   * Instantiate this class.
   */
  static vtkCaptionRepresentation* New();

  ///@{
  /**
   * Standard VTK class methods.
   */
  vtkTypeMacro(vtkCaptionRepresentation, vtkBorderRepresentation);
  void PrintSelf(ostream& os, vtkIndent indent) override;
  ///@}

  ///@{
  /**
   * Specify the position of the anchor (i.e., the point that the caption is anchored to).
   * Note that the position should be specified in world coordinates.
   */
  void SetAnchorPosition(double pos[3]);
  void GetAnchorPosition(double pos[3]);
  ///@}

  ///@{
  /**
   * Specify the vtkCaptionActor2D to manage. If not specified, then one
   * is automatically created.
   */
  void SetCaptionActor2D(vtkCaptionActor2D* captionActor);
  vtkGetObjectMacro(CaptionActor2D, vtkCaptionActor2D);
  ///@}

  ///@{
  /**
   * Set and get the instances of vtkPointHandleRepresentation3D used to implement this
   * representation. Normally default representations are created, but you can
   * specify the ones you want to use.
   */
  void SetAnchorRepresentation(vtkPointHandleRepresentation3D*);
  vtkGetObjectMacro(AnchorRepresentation, vtkPointHandleRepresentation3D);
  ///@}

  /**
   * Satisfy the superclasses API.
   */
  void BuildRepresentation() override;
  void GetSize(double size[2]) override
  {
    size[0] = 2.0;
    size[1] = 2.0;
  }

  ///@{
  /**
   * These methods are necessary to make this representation behave as
   * a vtkProp.
   */
  void GetActors2D(vtkPropCollection*) override;
  void ReleaseGraphicsResources(vtkWindow*) override;
  int RenderOverlay(vtkViewport*) override;
  int RenderOpaqueGeometry(vtkViewport*) override;
  int RenderTranslucentPolygonalGeometry(vtkViewport*) override;
  vtkTypeBool HasTranslucentPolygonalGeometry() override;
  ///@}

  ///@{
  /**
   * Set/Get the factor that controls the overall size of the fonts
   * of the caption when the text actor's ScaledText is OFF. This
   * simply is a way of controlling the text size.
   */
  vtkSetClampMacro(FontFactor, double, 0.1, 10.0);
  vtkGetMacro(FontFactor, double);
  ///@}

  /**
   * Control the relationship between the size of the text and the border.
   * By default, the text is sized to fit in the border (defined by this
   * class's superclass vtkBorderRepresentation). However, it is also
   * possible to size the border to fit around the text. In typical
   * applications (SetFitToBorder()), sizing the text to fit within the
   * border means that the text changes size as the rendering window changes
   * in size. However, by choosing SetFitToText(), the text always remains
   * the specified font size (as specified by the text actor) and the border
   * will not scale as the rendering window size changes.
   */
  enum FitType
  {
    VTK_FIT_TO_BORDER = 0,
    VTK_FIT_TO_TEXT
  };
  vtkSetClampMacro(Fit, int, VTK_FIT_TO_BORDER, VTK_FIT_TO_TEXT);
  vtkGetMacro(Fit, int);
  void SetFitToBorder() { this->SetFit(VTK_FIT_TO_BORDER); }
  void SetFitToText() { this->SetFit(VTK_FIT_TO_TEXT); }
  const char* GetFitAsString();

protected:
  vtkCaptionRepresentation();
  ~vtkCaptionRepresentation() override;

  // the text to manage
  vtkCaptionActor2D* CaptionActor2D;
  vtkConeSource* CaptionGlyph;

  int PointWidgetState;
  int DisplayAttachmentPoint[2];
  double FontFactor;
  int Fit;

  // Internal representation for the anchor
  vtkPointHandleRepresentation3D* AnchorRepresentation;

  // Check and adjust boundaries according to the size of the caption text
  virtual void AdjustCaptionBoundary();

private:
  vtkCaptionRepresentation(const vtkCaptionRepresentation&) = delete;
  void operator=(const vtkCaptionRepresentation&) = delete;
};

VTK_ABI_NAMESPACE_END
#endif
