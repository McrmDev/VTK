// SPDX-FileCopyrightText: Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
// SPDX-License-Identifier: BSD-3-Clause

#include "vtkActor.h"
#include "vtkCamera.h"
#include "vtkCullerCollection.h"
#include "vtkLight.h"
#include "vtkNew.h"
#include "vtkOpenGLPolyDataMapper.h"
#include "vtkOpenGLRenderWindow.h"
#include "vtkOpenGLVertexBufferObject.h"
#include "vtkOpenVRCamera.h"
#include "vtkOpenVRRenderWindow.h"
#include "vtkOpenVRRenderWindowInteractor.h"
#include "vtkOpenVRRenderer.h"
#include "vtkPLYReader.h"
#include "vtkProperty.h"
#include "vtkRegressionTestImage.h"
#include "vtkRenderWindowInteractor.h"
#include "vtkRenderer.h"
#include "vtkTestUtilities.h"
#include "vtkTransform.h"
#include "vtkTransformPolyDataFilter.h"

//------------------------------------------------------------------------------
int TestOpenVRInteractiveDragon(int argc, char* argv[])
{
  vtkNew<vtkOpenVRRenderer> renderer;
  vtkNew<vtkOpenVRRenderWindow> renderWindow;
  vtkNew<vtkOpenVRRenderWindowInteractor> iren;
  vtkNew<vtkOpenVRCamera> cam;
  renderer->SetShowFloor(true);
  renderer->SetBackground(0.2, 0.3, 0.4);
  renderWindow->AddRenderer(renderer);
  iren->SetRenderWindow(renderWindow);
  renderer->SetActiveCamera(cam);

  vtkNew<vtkLight> light;
  light->SetLightTypeToSceneLight();
  light->SetPosition(1.0, 1.0, 1.0);
  renderer->AddLight(light);

  // crazy frame rate requirement
  // need to look into that at some point
  renderWindow->SetDesiredUpdateRate(350.0);
  iren->SetDesiredUpdateRate(350.0);
  iren->SetStillUpdateRate(350.0);
  iren->SetActionManifestDirectory("../../");

  renderer->RemoveCuller(renderer->GetCullers()->GetLastItem());

  const char* fileName = vtkTestUtilities::ExpandDataFileName(argc, argv, "Data/dragon.ply");
  vtkNew<vtkPLYReader> reader;
  reader->SetFileName(fileName);

  vtkNew<vtkTransform> trans;
  trans->Translate(10.0, 20.0, 30.0);

  vtkNew<vtkTransformPolyDataFilter> tf;
  tf->SetTransform(trans);
  tf->SetInputConnection(reader->GetOutputPort());

  vtkNew<vtkActor> actor;
  renderer->AddActor(actor);

  vtkNew<vtkOpenGLPolyDataMapper> mapper;
  mapper->SetInputConnection(tf->GetOutputPort());
  mapper->SetVBOShiftScaleMethod(vtkPolyDataMapper::ShiftScaleMethodType::AUTO_SHIFT_SCALE);
  actor->SetMapper(mapper);
  // Red dragon is in world coordinate system (the default)
  actor->GetProperty()->SetAmbientColor(1.0, 0.0, 0.0);
  actor->GetProperty()->SetDiffuseColor(1.0, 0.0, 0.0);
  actor->GetProperty()->SetSpecular(0.5);
  actor->GetProperty()->SetDiffuse(0.7);
  actor->GetProperty()->SetAmbient(0.5);
  actor->GetProperty()->SetSpecularPower(20.0);
  actor->GetProperty()->SetOpacity(1.0);

  vtkNew<vtkActor> pactor;
  renderer->AddActor(pactor);

  vtkNew<vtkTransform> trans2;
  trans2->Scale(4.0, 2.0, 2.0);

  vtkNew<vtkOpenGLPolyDataMapper> pmapper;
  pmapper->SetInputConnection(reader->GetOutputPort());
  pmapper->SetVBOShiftScaleMethod(vtkPolyDataMapper::ShiftScaleMethodType::AUTO_SHIFT_SCALE);
  pactor->SetMapper(pmapper);

  pactor->SetUserMatrix(trans2->GetMatrix());
  // Green dragon is in physical coords
  pactor->GetProperty()->SetAmbientColor(0.0, 1.0, 0.0);
  pactor->GetProperty()->SetDiffuseColor(0.0, 1.0, 0.0);
  pactor->GetProperty()->SetSpecular(0.5);
  pactor->GetProperty()->SetDiffuse(0.7);
  pactor->GetProperty()->SetAmbient(0.5);
  pactor->GetProperty()->SetSpecularPower(20.0);
  pactor->GetProperty()->SetOpacity(1.0);
  pactor->SetCoordinateSystemToPhysical();
  pactor->SetCoordinateSystemRenderer(renderer);
  pactor->UseBoundsOff();

  vtkNew<vtkActor> dactor;
  renderer->AddActor(dactor);

  vtkNew<vtkOpenGLPolyDataMapper> dmapper;
  dmapper->SetInputConnection(reader->GetOutputPort());
  dmapper->SetVBOShiftScaleMethod(vtkPolyDataMapper::ShiftScaleMethodType::AUTO_SHIFT_SCALE);
  dactor->SetMapper(dmapper);
  dactor->SetScale(2.0, 2.0, 2.0);
  dactor->SetPosition(0.0, 0.0, -0.2);
  // Blue dragon is in device coords (of left controller)
  dactor->GetProperty()->SetAmbientColor(0.0, 0.0, 1.0);
  dactor->GetProperty()->SetDiffuseColor(0.0, 0.0, 1.0);
  dactor->GetProperty()->SetSpecular(0.5);
  dactor->GetProperty()->SetDiffuse(0.7);
  dactor->GetProperty()->SetAmbient(0.5);
  dactor->GetProperty()->SetSpecularPower(20.0);
  dactor->GetProperty()->SetOpacity(1.0);
  dactor->SetCoordinateSystemToDevice();
  dactor->SetCoordinateSystemDevice(static_cast<int>(vtkEventDataDevice::LeftController));
  dactor->SetCoordinateSystemRenderer(renderer);
  dactor->UseBoundsOff();

  // the HMD may not be turned on/etc
  renderWindow->Initialize();
  if (renderWindow->GetHMD())
  {
    renderer->ResetCamera();
    renderWindow->Render();
    iren->Start();
  }
  return 0;
}
