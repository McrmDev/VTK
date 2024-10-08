// SPDX-FileCopyrightText: Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
// SPDX-License-Identifier: BSD-3-Clause
// This test covers the Sobel edge detection post-processing render pass.
// It renders an opaque actor  The mapper uses color
// interpolation (poor quality).
//
// The command line arguments are:
// -I        => run in interactive mode; unless this is used, the program will
//              not allow interaction and exit

#include "vtkRegressionTestImage.h"
#include "vtkTestUtilities.h"

#include "vtkActor.h"
#include "vtkOpenGLRenderer.h"
#include "vtkRenderWindow.h"
#include "vtkRenderWindowInteractor.h"

#include "vtkCamera.h"
#include "vtkDataSetSurfaceFilter.h"
#include "vtkImageData.h"
#include "vtkImageDataGeometryFilter.h"
#include "vtkImageSinusoidSource.h"
#include "vtkLookupTable.h"
#include "vtkPolyDataMapper.h"

#include "vtkCameraPass.h"
#include "vtkLightsPass.h"
#include "vtkOpaquePass.h"
#include "vtkSequencePass.h"
// #include "vtkDepthPeelingPass.h"
#include "vtkConeSource.h"
#include "vtkOverlayPass.h"
#include "vtkRenderPassCollection.h"
#include "vtkSobelGradientMagnitudePass.h"
#include "vtkTranslucentPass.h"
#include "vtkVolumetricPass.h"

int TestSobelGradientMagnitudePass(int argc, char* argv[])
{
  vtkRenderWindowInteractor* iren = vtkRenderWindowInteractor::New();
  vtkRenderWindow* renWin = vtkRenderWindow::New();
  renWin->SetMultiSamples(0);

  renWin->SetAlphaBitPlanes(1);
  iren->SetRenderWindow(renWin);
  renWin->Delete();

  vtkRenderer* renderer = vtkRenderer::New();
  renWin->AddRenderer(renderer);
  renderer->Delete();

  vtkOpenGLRenderer* glrenderer = vtkOpenGLRenderer::SafeDownCast(renderer);

  vtkCameraPass* cameraP = vtkCameraPass::New();

  vtkSequencePass* seq = vtkSequencePass::New();
  vtkOpaquePass* opaque = vtkOpaquePass::New();
  //  vtkDepthPeelingPass *peeling=vtkDepthPeelingPass::New();
  //  peeling->SetMaximumNumberOfPeels(200);
  //  peeling->SetOcclusionRatio(0.1);

  vtkTranslucentPass* translucent = vtkTranslucentPass::New();
  //  peeling->SetTranslucentPass(translucent);

  vtkVolumetricPass* volume = vtkVolumetricPass::New();
  vtkOverlayPass* overlay = vtkOverlayPass::New();

  vtkLightsPass* lights = vtkLightsPass::New();

  vtkRenderPassCollection* passes = vtkRenderPassCollection::New();
  passes->AddItem(lights);
  passes->AddItem(opaque);

  //  passes->AddItem(peeling);
  passes->AddItem(translucent);

  passes->AddItem(volume);
  passes->AddItem(overlay);
  seq->SetPasses(passes);
  cameraP->SetDelegatePass(seq);

  vtkSobelGradientMagnitudePass* sobelP = vtkSobelGradientMagnitudePass::New();
  sobelP->SetDelegatePass(cameraP);

  glrenderer->SetPass(sobelP);

  //  renderer->SetPass(cameraP);

  opaque->Delete();
  //  peeling->Delete();
  translucent->Delete();
  volume->Delete();
  overlay->Delete();
  seq->Delete();
  passes->Delete();
  cameraP->Delete();
  sobelP->Delete();
  lights->Delete();

  vtkImageSinusoidSource* imageSource = vtkImageSinusoidSource::New();
  imageSource->SetWholeExtent(0, 9, 0, 9, 0, 9);
  imageSource->SetPeriod(5);
  imageSource->Update();

  vtkImageData* image = imageSource->GetOutput();
  double range[2];
  image->GetScalarRange(range);

  vtkDataSetSurfaceFilter* surface = vtkDataSetSurfaceFilter::New();

  surface->SetInputConnection(imageSource->GetOutputPort());
  imageSource->Delete();

  vtkPolyDataMapper* mapper = vtkPolyDataMapper::New();
  mapper->SetInputConnection(surface->GetOutputPort());
  surface->Delete();

  vtkLookupTable* lut = vtkLookupTable::New();
  lut->SetTableRange(range);
  lut->SetAlphaRange(0.5, 0.5);
  lut->SetHueRange(0.2, 0.7);
  lut->SetNumberOfTableValues(256);
  lut->Build();

  mapper->SetScalarVisibility(1);
  mapper->SetLookupTable(lut);
  lut->Delete();

  vtkActor* actor = vtkActor::New();
  renderer->AddActor(actor);
  actor->Delete();
  actor->SetMapper(mapper);
  mapper->Delete();
  actor->SetVisibility(0);

  vtkConeSource* cone = vtkConeSource::New();
  vtkPolyDataMapper* coneMapper = vtkPolyDataMapper::New();
  coneMapper->SetInputConnection(cone->GetOutputPort());
  cone->Delete();
  vtkActor* coneActor = vtkActor::New();
  coneActor->SetMapper(coneMapper);
  coneActor->SetVisibility(1);
  coneMapper->Delete();
  renderer->AddActor(coneActor);
  coneActor->Delete();

  renderer->SetBackground(0.1, 0.3, 0.0);
  renWin->SetSize(400, 400);

  renWin->Render();
  vtkCamera* camera = renderer->GetActiveCamera();
  camera->Azimuth(-40.0);
  camera->Elevation(20.0);
  renWin->Render();

  int retVal = vtkRegressionTestImage(renWin);
  if (retVal == vtkRegressionTester::DO_INTERACTOR)
  {
    iren->Start();
  }
  iren->Delete();

  return !retVal;
}
