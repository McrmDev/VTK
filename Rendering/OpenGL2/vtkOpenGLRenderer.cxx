// SPDX-FileCopyrightText: Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
// SPDX-License-Identifier: BSD-3-Clause
#include "vtkOpenGLRenderer.h"

#include "vtkOpenGLHelper.h"

#include "vtkCellArray.h"
#include "vtkDataArray.h"
#include "vtkDataArrayRange.h"
#include "vtkDepthPeelingPass.h"
#include "vtkDualDepthPeelingPass.h"
#include "vtkFloatArray.h"
#include "vtkHardwareSelector.h"
#include "vtkHiddenLineRemovalPass.h"
#include "vtkImageData.h"
#include "vtkLight.h"
#include "vtkLightCollection.h"
#include "vtkNew.h"
#include "vtkObjectFactory.h"
#include "vtkOpaquePass.h"
#include "vtkOpenGLCamera.h"
#include "vtkOpenGLError.h"
#include "vtkOpenGLFXAAFilter.h"
#include "vtkOpenGLQuadHelper.h"
#include "vtkOpenGLRenderUtilities.h"
#include "vtkOpenGLRenderWindow.h"
#include "vtkOpenGLShaderCache.h"
#include "vtkOpenGLState.h"
#include "vtkOrderIndependentTranslucentPass.h"
#include "vtkPBRIrradianceTexture.h"
#include "vtkPBRLUTTexture.h"
#include "vtkPBRPrefilterTexture.h"
#include "vtkPointData.h"
#include "vtkPoints.h"
#include "vtkPolyData.h"
#include "vtkPolyDataMapper2D.h"
#include "vtkRenderPass.h"
#include "vtkRenderState.h"
#include "vtkRenderTimerLog.h"
#include "vtkSSAOPass.h"
#include "vtkShaderProgram.h"
#include "vtkShaderProperty.h"
#include "vtkShadowMapBakerPass.h"
#include "vtkShadowMapPass.h"
#include "vtkSphericalHarmonics.h"
#include "vtkTable.h"
#include "vtkTexture.h"
#include "vtkTextureObject.h"
#include "vtkTexturedActor2D.h"
#include "vtkTimerLog.h"
#include "vtkTransform.h"
#include "vtkTranslucentPass.h"
#include "vtkTrivialProducer.h"
#include "vtkUniforms.h"
#include "vtkUnsignedCharArray.h"
#include "vtkVolumetricPass.h"

#include <iterator>
#include <vtksys/RegularExpression.hxx>

#include <cassert>
#include <cmath>
#include <cstdlib>
#include <list>
#include <sstream>
#include <string>

#if defined(__APPLE__)
#include <CoreFoundation/CoreFoundation.h>
#endif

VTK_ABI_NAMESPACE_BEGIN
vtkStandardNewMacro(vtkOpenGLRenderer);

vtkOpenGLRenderer::vtkOpenGLRenderer()
{
  this->FXAAFilter = nullptr;
  this->DepthPeelingPass = nullptr;
  this->SSAOPass = nullptr;
  this->TranslucentPass = nullptr;
  this->ShadowMapPass = nullptr;
  this->DepthPeelingHigherLayer = 0;

  this->LightingCount = -1;
  this->LightingComplexity = -1;

  this->UseSphericalHarmonics = true;

  // Prepare a quadrilateral for the textured/gradient background actor.
  this->BackgroundGradientActor = vtk::TakeSmartPointer(vtkTexturedActor2D::New());
  this->BackgroundTextureActor = vtk::TakeSmartPointer(vtkTexturedActor2D::New());
  this->BackgroundMapper = vtk::TakeSmartPointer(vtkPolyDataMapper2D::New());
  this->BackgroundQuad = vtk::TakeSmartPointer(vtkPolyData::New());

  vtkNew<vtkPoints> points;
  // rest of the points depend on the size of viewport. they are initialized in Clear() method.
  this->BackgroundQuad->SetPoints(points);

  vtkNew<vtkCellArray> tris;
  tris->InsertNextCell({ 0, 1, 2 });
  tris->InsertNextCell({ 0, 2, 3 });
  this->BackgroundQuad->SetPolys(tris);

  vtkNew<vtkFloatArray> tcoords;
  float tmp[2];
  tmp[0] = 0;
  tmp[1] = 0;
  tcoords->SetNumberOfComponents(2);
  tcoords->SetNumberOfTuples(4);
  tcoords->SetTuple(0, tmp);
  tmp[0] = 1.0;
  tcoords->SetTuple(1, tmp);
  tmp[1] = 1.0;
  tcoords->SetTuple(2, tmp);
  tmp[0] = 0.0;
  tcoords->SetTuple(3, tmp);
  this->BackgroundQuad->GetPointData()->SetTCoords(tcoords);

  this->BackgroundGradientActor->SetMapper(this->BackgroundMapper);
  auto shaderProperty = this->BackgroundGradientActor->GetShaderProperty();
  // get rid of conflicting replacements from vtkOpenGLPolyDataMapper2D.
  shaderProperty->AddFragmentShaderReplacement("//VTK::Color::Dec", true, "", false);
  shaderProperty->AddFragmentShaderReplacement("//VTK::Color::Impl", true, "", false);

  // add gradient parameters as uniforms.
  shaderProperty->AddFragmentShaderReplacement("//VTK::CustomUniforms::Dec",
    /*replaceFirst=*/true,
    R"(
uniform bool dither;
uniform int gradientMode;
uniform vec3 stopColors[2];
// Granularity of dither noise set to very small number 0.5 / 255.0 to ensure any shift in color due to dither noise is minimal
const highp float DITHERING_GRANULARITY = 0.001960784313725;
float generateRandom (vec2 st) { return fract(sin(dot(st.xy, vec2(12.9898,78.233))) * 43758.5453123); }
#define GRADIENT_VERTICAL 0
#define GRADIENT_HORIZONTAL 1
#define GRADIENT_RADIAL_VIEWPORT_FARTHEST_SIDE 2
#define GRADIENT_RADIAL_VIEWPORT_FARTHEST_CORNER 3
  )",
    /*replaceAll=*/false);

  // map texture coordinate value into the gradient color function.
  shaderProperty->AddFragmentShaderReplacement("//VTK::TCoord::Impl",
    /*replaceFirst=*/true,
    R"(
float value = 0.0;
if(gradientMode == GRADIENT_VERTICAL)
{
  value = tcoordVCVSOutput.t;
}
else if(gradientMode == GRADIENT_HORIZONTAL)
{
  value = tcoordVCVSOutput.s;
}
else if(gradientMode == GRADIENT_RADIAL_VIEWPORT_FARTHEST_SIDE)
{
  value = clamp(length(tcoordVCVSOutput - vec2(0.5f, 0.5f)) * 2.0f, 0.0f, 1.0f);
}
else if(gradientMode == GRADIENT_RADIAL_VIEWPORT_FARTHEST_CORNER)
{
  value = length(tcoordVCVSOutput - vec2(0.5f, 0.5f)) * sqrt(2.0f);
}
gl_FragData[0] = vec4(mix(stopColors[0].xyz, stopColors[1].xyz, value), 1.0);
if (dither) {
float noise = mix(-DITHERING_GRANULARITY, DITHERING_GRANULARITY, generateRandom(tcoordVCVSOutput));
gl_FragData[0].xyz += vec3(noise);
}
)",
    /*replaceAll=*/false);

  this->BackgroundTextureActor->SetMapper(this->BackgroundMapper);
}

// Ask lights to load themselves into graphics pipeline.
int vtkOpenGLRenderer::UpdateLights()
{
  // consider the lighting complexity to determine which case applies
  // simple headlight, Light Kit, the whole feature set of VTK
  vtkLightCollection* lc = this->GetLights();
  vtkLight* light;

  int lightingComplexity = 0;
  int lightingCount = 0;

  vtkMTimeType ltime = lc->GetMTime();

  vtkCollectionSimpleIterator sit;
  for (lc->InitTraversal(sit); (light = lc->GetNextLight(sit));)
  {
    float status = light->GetSwitch();
    if (status > 0.0)
    {
      ltime = vtkMath::Max(ltime, light->GetMTime());
      lightingCount++;
      if (lightingComplexity == 0)
      {
        lightingComplexity = 1;
      }
    }

    if (lightingComplexity == 1 &&
      (lightingCount > 1 || light->GetLightType() != VTK_LIGHT_TYPE_HEADLIGHT))
    {
      lightingComplexity = 2;
    }
    if (lightingComplexity < 3 && (light->GetPositional()))
    {
      lightingComplexity = 3;
    }
  }

  if (this->GetUseImageBasedLighting() && lightingComplexity == 0)
  {
    lightingComplexity = 1;
  }

  // create alight if needed
  if (!lightingCount)
  {
    if (this->AutomaticLightCreation)
    {
      vtkDebugMacro(<< "No lights are on, creating one.");
      this->CreateLight();
      lc->InitTraversal(sit);
      light = lc->GetNextLight(sit);
      ltime = lc->GetMTime();
      lightingCount = 1;
      lightingComplexity = light->GetLightType() == VTK_LIGHT_TYPE_HEADLIGHT ? 1 : 2;
      ltime = vtkMath::Max(ltime, light->GetMTime());
    }
  }

  if (lightingComplexity != this->LightingComplexity || lightingCount != this->LightingCount)
  {
    this->LightingComplexity = lightingComplexity;
    this->LightingCount = lightingCount;

    this->LightingUpdateTime = ltime;

    // rebuild the standard declarations
    std::ostringstream toString;
    switch (this->LightingComplexity)
    {
      case 0: // no lighting or RENDER_VALUES
        this->LightingDeclaration = "";
        break;

      case 1: // headlight
        this->LightingDeclaration = "uniform vec3 lightColor0;\n";
        break;

      case 2: // light kit
        toString.clear();
        toString.str("");
        for (int i = 0; i < this->LightingCount; ++i)
        {
          toString << "uniform vec3 lightColor" << i
                   << ";\n"
                      "  uniform vec3 lightDirectionVC"
                   << i << "; // normalized\n";
        }
        this->LightingDeclaration = toString.str();
        break;

      case 3: // positional
        toString.clear();
        toString.str("");
        for (int i = 0; i < this->LightingCount; ++i)
        {
          toString << "uniform vec3 lightColor" << i
                   << ";\n"
                      "uniform vec3 lightDirectionVC"
                   << i
                   << "; // normalized\n"
                      "uniform vec3 lightPositionVC"
                   << i
                   << ";\n"
                      "uniform vec3 lightAttenuation"
                   << i
                   << ";\n"
                      "uniform float lightConeAngle"
                   << i
                   << ";\n"
                      "uniform float lightExponent"
                   << i
                   << ";\n"
                      "uniform int lightPositional"
                   << i << ";";
        }
        this->LightingDeclaration = toString.str();
        break;
    }
  }

  this->LightingUpdateTime = ltime;

  return this->LightingCount;
}

//------------------------------------------------------------------------------
// Description:
// Is rendering at translucent geometry stage using depth peeling and
// rendering a layer other than the first one? (Boolean value)
// If so, the uniform variables UseTexture and Texture can be set.
// (Used by vtkOpenGLProperty or vtkOpenGLTexture)
int vtkOpenGLRenderer::GetDepthPeelingHigherLayer()
{
  return this->DepthPeelingHigherLayer;
}

//------------------------------------------------------------------------------
// Concrete open gl render method.
void vtkOpenGLRenderer::DeviceRender()
{
  vtkTimerLog::MarkStartEvent("OpenGL Dev Render");

  bool computeIBLTextures =
    !(this->Pass && this->Pass->IsA("vtkOSPRayPass")) && this->UseImageBasedLighting;
  if (computeIBLTextures)
  {
    this->GetEnvMapLookupTable()->Load(this);
    this->GetEnvMapPrefiltered()->Load(this);

    // Complex logic here, different possibilities:
    // - UseSH is ON, EnvTex is provided but is not compatible, fallback to irradiance
    // - UseSH is ON and SH are provided, EnvTex is not, just use the SH as is
    // - UseSH is ON, SH and EnvTex are provided and compatible, check the MTime to recompute SH
    // - UseSH is ON, SH is not provided, EnvTex is compatible, compute SH
    // - UseSH is ON, SH is not provided, EnvTex is compatible but empty, error out
    // - UseSH is OFF, use irradiance
    bool useSH = this->UseSphericalHarmonics;
    if (useSH && this->EnvironmentTexture && this->EnvironmentTexture->GetCubeMap())
    {
      vtkWarningMacro(
        "Cannot compute spherical harmonics of a cubemap, falling back to irradiance texture");
      useSH = false;
    }

    if (useSH)
    {
      vtkImageData* img = nullptr;
      if (this->EnvironmentTexture)
      {
        img = this->EnvironmentTexture->GetInput();
      }

      if (img &&
        (!this->SphericalHarmonics || img->GetMTime() > this->SphericalHarmonics->GetMTime()))
      {
        vtkNew<vtkSphericalHarmonics> sh;
        sh->SetInputData(img);
        sh->Update();
        this->SphericalHarmonics = vtkFloatArray::SafeDownCast(
          vtkTable::SafeDownCast(sh->GetOutputDataObject(0))->GetColumn(0));
      }

      if (!this->SphericalHarmonics)
      {
        vtkErrorMacro("Cannot compute spherical harmonics without an image data texture");
        return;
      }
    }
    else
    {
      this->GetEnvMapIrradiance()->Load(this);
    }
  }

  if (this->Pass != nullptr)
  {
    vtkRenderState s(this);
    s.SetPropArrayAndCount(this->PropArray, this->PropArrayCount);
    s.SetFrameBuffer(nullptr);
    this->Pass->Render(&s);
  }
  else
  {
    // Do not remove this MakeCurrent! Due to Start / End methods on
    // some objects which get executed during a pipeline update,
    // other windows might get rendered since the last time
    // a MakeCurrent was called.
    this->RenderWindow->MakeCurrent();
    vtkOpenGLClearErrorMacro();

    this->UpdateCamera();
    this->UpdateLightGeometry();
    this->UpdateLights();
    this->UpdateGeometry();

    vtkOpenGLCheckErrorMacro("failed after DeviceRender");
  }

  if (computeIBLTextures)
  {
    this->GetEnvMapLookupTable()->PostRender(this);
    this->GetEnvMapIrradiance()->PostRender(this);
    this->GetEnvMapPrefiltered()->PostRender(this);
  }

  vtkTimerLog::MarkEndEvent("OpenGL Dev Render");
}

// Ask actors to render themselves. As a side effect will cause
// visualization network to update.
int vtkOpenGLRenderer::UpdateGeometry(vtkFrameBufferObjectBase* fbo)
{
  vtkRenderTimerLog* timer = this->GetRenderWindow()->GetRenderTimer();
  VTK_SCOPED_RENDER_EVENT("vtkOpenGLRenderer::UpdateGeometry", timer);

  int i;

  this->NumberOfPropsRendered = 0;

  if (this->PropArrayCount == 0)
  {
    return 0;
  }

  if (this->Selector)
  {
    VTK_SCOPED_RENDER_EVENT2("Selection", timer, selectionEvent);

    // When selector is present, we are performing a selection,
    // so do the selection rendering pass instead of the normal passes.
    // Delegate the rendering of the props to the selector itself.

    // use pickfromprops ?
    if (this->PickFromProps)
    {
      vtkProp** pa;
      vtkProp* aProp;
      if (this->PickFromProps->GetNumberOfItems() > 0)
      {
        pa = new vtkProp*[this->PickFromProps->GetNumberOfItems()];
        int pac = 0;

        vtkCollectionSimpleIterator pit;
        for (this->PickFromProps->InitTraversal(pit);
             (aProp = this->PickFromProps->GetNextProp(pit));)
        {
          if (aProp->GetVisibility())
          {
            pa[pac++] = aProp;
          }
        }

        this->NumberOfPropsRendered = this->Selector->Render(this, pa, pac);
        delete[] pa;
      }
    }
    else
    {
      this->NumberOfPropsRendered =
        this->Selector->Render(this, this->PropArray, this->PropArrayCount);
    }

    this->RenderTime.Modified();
    vtkDebugMacro("Rendered " << this->NumberOfPropsRendered << " actors");
    return this->NumberOfPropsRendered;
  }

  // if we are using shadows then let the renderpasses handle it
  // for opaque and translucent
  int hasTranslucentPolygonalGeometry = 0;
  if (this->UseShadows)
  {
    VTK_SCOPED_RENDER_EVENT2("Shadows", timer, shadowsEvent);

    if (!this->ShadowMapPass)
    {
      this->ShadowMapPass = vtkShadowMapPass::New();
    }
    vtkRenderState s(this);
    s.SetPropArrayAndCount(this->PropArray, this->PropArrayCount);
    // s.SetFrameBuffer(0);
    this->ShadowMapPass->GetShadowMapBakerPass()->Render(&s);
    this->ShadowMapPass->Render(&s);
  }
  else
  {
    // Opaque geometry first:
    timer->MarkStartEvent("Opaque Geometry");
    this->DeviceRenderOpaqueGeometry(fbo);
    timer->MarkEndEvent();

    // do the render library specific stuff about translucent polygonal geometry.
    // As it can be expensive, do a quick check if we can skip this step
    for (i = 0; !hasTranslucentPolygonalGeometry && i < this->PropArrayCount; i++)
    {
      hasTranslucentPolygonalGeometry = this->PropArray[i]->HasTranslucentPolygonalGeometry();
    }
    if (hasTranslucentPolygonalGeometry)
    {
      timer->MarkStartEvent("Translucent Geometry");
      this->DeviceRenderTranslucentPolygonalGeometry(fbo);
      timer->MarkEndEvent();
    }
  }

  // Apply FXAA before volumes and overlays. Volumes don't need AA, and overlays
  // are usually things like text, which are already antialiased.
  if (this->UseFXAA)
  {
    timer->MarkStartEvent("FXAA");
    if (!this->FXAAFilter)
    {
      this->FXAAFilter = vtkOpenGLFXAAFilter::New();
    }
    if (this->FXAAOptions)
    {
      this->FXAAFilter->UpdateConfiguration(this->FXAAOptions);
    }

    this->FXAAFilter->Execute(this);
    timer->MarkEndEvent();
  }

  // loop through props and give them a chance to
  // render themselves as volumetric geometry.
  if (hasTranslucentPolygonalGeometry == 0 || !this->UseDepthPeeling ||
    !this->UseDepthPeelingForVolumes)
  {
    timer->MarkStartEvent("Volumes");
    for (i = 0; i < this->PropArrayCount; i++)
    {
      this->NumberOfPropsRendered += this->PropArray[i]->RenderVolumetricGeometry(this);
    }
    timer->MarkEndEvent();
  }

  // loop through props and give them a chance to
  // render themselves as an overlay (or underlay)
  timer->MarkStartEvent("Overlay");
  for (i = 0; i < this->PropArrayCount; i++)
  {
    this->NumberOfPropsRendered += this->PropArray[i]->RenderOverlay(this);
  }
  timer->MarkEndEvent();

  this->RenderTime.Modified();

  vtkDebugMacro(<< "Rendered " << this->NumberOfPropsRendered << " actors");

  return this->NumberOfPropsRendered;
}

//------------------------------------------------------------------------------
vtkTexture* vtkOpenGLRenderer::GetCurrentTexturedBackground()
{
  if (!this->GetRenderWindow()->GetStereoRender() && this->BackgroundTexture)
  {
    return this->BackgroundTexture;
  }
  else if (this->GetRenderWindow()->GetStereoRender() &&
    this->GetActiveCamera()->GetLeftEye() == 1 && this->BackgroundTexture)
  {
    return this->BackgroundTexture;
  }
  else if (this->GetRenderWindow()->GetStereoRender() && this->RightBackgroundTexture)
  {
    return this->RightBackgroundTexture;
  }
  else
  {
    return nullptr;
  }
}

//------------------------------------------------------------------------------
void vtkOpenGLRenderer::DeviceRenderOpaqueGeometry(vtkFrameBufferObjectBase* fbo)
{
  // Do we need hidden line removal?
  bool useHLR = this->UseHiddenLineRemoval &&
    vtkHiddenLineRemovalPass::WireframePropsExist(this->PropArray, this->PropArrayCount);

  if (useHLR)
  {
    vtkNew<vtkHiddenLineRemovalPass> hlrPass;
    vtkRenderState s(this);
    s.SetPropArrayAndCount(this->PropArray, this->PropArrayCount);
    s.SetFrameBuffer(fbo);
    hlrPass->Render(&s);
    this->NumberOfPropsRendered += hlrPass->GetNumberOfRenderedProps();
  }
  else
  {
    if (this->UseSSAO)
    {
      if (!this->SSAOPass)
      {
        this->SSAOPass = vtkSSAOPass::New();
        vtkNew<vtkOpaquePass> opaqueP;
        this->SSAOPass->SetDelegatePass(opaqueP);
      }
      vtkRenderState s(this);
      s.SetPropArrayAndCount(this->PropArray, this->PropArrayCount);
      s.SetFrameBuffer(fbo);
      this->SSAOPass->SetRadius(this->SSAORadius);
      this->SSAOPass->SetBias(this->SSAOBias);
      this->SSAOPass->SetKernelSize(this->SSAOKernelSize);
      this->SSAOPass->SetBlur(this->SSAOBlur);
      this->SSAOPass->Render(&s);
      this->NumberOfPropsRendered += this->SSAOPass->GetNumberOfRenderedProps();
    }
    else
    {
      this->Superclass::DeviceRenderOpaqueGeometry();
    }
  }
}

//------------------------------------------------------------------------------
// Description:
// Render translucent polygonal geometry. Default implementation just call
// UpdateTranslucentPolygonalGeometry().
// Subclasses of vtkRenderer that can deal with depth peeling must
// override this method.
void vtkOpenGLRenderer::DeviceRenderTranslucentPolygonalGeometry(vtkFrameBufferObjectBase* fbo)
{
  vtkOpenGLClearErrorMacro();

  vtkOpenGLRenderWindow* context = vtkOpenGLRenderWindow::SafeDownCast(this->RenderWindow);

  if (this->UseDepthPeeling && !context)
  {
    vtkErrorMacro("OpenGL render window is required.");
    return;
  }

  if (!this->UseDepthPeeling)
  {
    if (!this->UseOIT)
    {
      this->UpdateTranslucentPolygonalGeometry();
    }
    else
    {
      if (!this->TranslucentPass)
      {
        vtkOrderIndependentTranslucentPass* oit = vtkOrderIndependentTranslucentPass::New();
        this->TranslucentPass = oit;
      }
      vtkTranslucentPass* tp = vtkTranslucentPass::New();
      this->TranslucentPass->SetTranslucentPass(tp);
      tp->Delete();

      vtkRenderState s(this);
      s.SetPropArrayAndCount(this->PropArray, this->PropArrayCount);
      s.SetFrameBuffer(fbo);
      this->LastRenderingUsedDepthPeeling = 0;
      this->TranslucentPass->Render(&s);
      this->NumberOfPropsRendered += this->TranslucentPass->GetNumberOfRenderedProps();
    }
  }
  else // depth peeling.
  {
#ifdef GL_ES_VERSION_3_0
    vtkErrorMacro("Built in Dual Depth Peeling is not supported on ES3. "
                  "Please see TestFramebufferPass.cxx for an example that should work "
                  "on OpenGL ES 3.");
    this->UpdateTranslucentPolygonalGeometry();
#else
    if (!this->DepthPeelingPass)
    {
      if (this->IsDualDepthPeelingSupported())
      {
        vtkDebugMacro("Using dual depth peeling.");
        vtkDualDepthPeelingPass* ddpp = vtkDualDepthPeelingPass::New();
        this->DepthPeelingPass = ddpp;
      }
      else
      {
        vtkDebugMacro("Using standard depth peeling (dual depth peeling not "
                      "supported by the graphics card/driver).");
        this->DepthPeelingPass = vtkDepthPeelingPass::New();
      }
      vtkTranslucentPass* tp = vtkTranslucentPass::New();
      this->DepthPeelingPass->SetTranslucentPass(tp);
      tp->Delete();
    }

    if (this->UseDepthPeelingForVolumes)
    {
      vtkDualDepthPeelingPass* ddpp = vtkDualDepthPeelingPass::SafeDownCast(this->DepthPeelingPass);
      if (!ddpp)
      {
        vtkWarningMacro("UseDepthPeelingForVolumes requested, but unsupported "
                        "since DualDepthPeeling is not available.");
        this->UseDepthPeelingForVolumes = false;
      }
      else if (!ddpp->GetVolumetricPass())
      {
        vtkVolumetricPass* vp = vtkVolumetricPass::New();
        ddpp->SetVolumetricPass(vp);
        vp->Delete();
      }
    }
    else
    {
      vtkDualDepthPeelingPass* ddpp = vtkDualDepthPeelingPass::SafeDownCast(this->DepthPeelingPass);
      if (ddpp)
      {
        ddpp->SetVolumetricPass(nullptr);
      }
    }

    this->DepthPeelingPass->SetMaximumNumberOfPeels(this->MaximumNumberOfPeels);
    this->DepthPeelingPass->SetOcclusionRatio(this->OcclusionRatio);
    vtkRenderState s(this);
    s.SetPropArrayAndCount(this->PropArray, this->PropArrayCount);
    s.SetFrameBuffer(fbo);
    this->LastRenderingUsedDepthPeeling = 1;
    this->DepthPeelingPass->Render(&s);
    this->NumberOfPropsRendered += this->DepthPeelingPass->GetNumberOfRenderedProps();
#endif
  }

  vtkOpenGLCheckErrorMacro("failed after DeviceRenderTranslucentPolygonalGeometry");
}

//------------------------------------------------------------------------------
void vtkOpenGLRenderer::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}

namespace
{
vtkSmartPointer<vtkDataArray> MakeQuadPointsFromViewportSize(int size[2])
{
  std::vector<float> corners = {
    // clang-format off
    0, 0, 0,
    static_cast<float>(size[0]), 0, 0,
    static_cast<float>(size[0]), static_cast<float>(size[1]), 0,
    0, static_cast<float>(size[1]), 0
    // clang-format on
  };
  auto data = vtk::TakeSmartPointer(vtkFloatArray::New());
  data->SetNumberOfComponents(3);
  data->SetNumberOfTuples(4);
  auto range = vtk::DataArrayValueRange(data);
  std::copy(corners.begin(), corners.end(), range.begin());
  return data;
}
}

//------------------------------------------------------------------------------
void vtkOpenGLRenderer::Clear()
{
  vtkOpenGLClearErrorMacro();

  GLbitfield clear_mask = 0;
  vtkOpenGLState* ostate = this->GetState();

  if (!this->Transparent())
  {
    ostate->vtkglClearColor(static_cast<GLclampf>(this->Background[0]),
      static_cast<GLclampf>(this->Background[1]), static_cast<GLclampf>(this->Background[2]),
      static_cast<GLclampf>(this->BackgroundAlpha));
    clear_mask |= GL_COLOR_BUFFER_BIT;
  }

  if (!this->GetPreserveDepthBuffer())
  {
    ostate->vtkglClearDepth(static_cast<GLclampf>(1.0));
    clear_mask |= GL_DEPTH_BUFFER_BIT;
    ostate->vtkglDepthMask(GL_TRUE);
  }

  vtkDebugMacro(<< "glClear\n");
  ostate->vtkglColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  ostate->vtkglClear(clear_mask);

  if (!this->Transparent() &&
    (this->GradientBackground ||
      (this->TexturedBackground && this->GetCurrentTexturedBackground())))
  {
    int size[2];
    size[0] = this->GetSize()[0];
    size[1] = this->GetSize()[1];

    // readjust the corner coordinates to span entire tile viewport.
    this->BackgroundQuad->GetPoints()->SetData(::MakeQuadPointsFromViewportSize(size));

    vtkNew<vtkTrivialProducer> prod;
    prod->SetOutput(this->BackgroundQuad);

    vtkSmartPointer<vtkTexturedActor2D> actor = nullptr;
    this->BackgroundMapper->SetInputConnection(prod->GetOutputPort());

    if (this->TexturedBackground && this->GetCurrentTexturedBackground())
    {
      actor = this->BackgroundTextureActor;
      this->GetCurrentTexturedBackground()->InterpolateOn();
      actor->SetTexture(this->GetCurrentTexturedBackground());
    }
    else if (this->GradientBackground)
    {
      actor = this->BackgroundGradientActor;
      auto* shaderProperty = actor->GetShaderProperty();
      float stopColors[2][3] = {};
      std::copy(this->Background, this->Background + 3, &stopColors[0][0]);
      std::copy(this->Background2, this->Background2 + 3, &stopColors[1][0]);
      auto* fragmentUniforms = shaderProperty->GetFragmentCustomUniforms();
      fragmentUniforms->SetUniformi("dither", this->DitherGradient);
      fragmentUniforms->SetUniformi("gradientMode", static_cast<int>(this->GradientMode));
      fragmentUniforms->SetUniform3fv("stopColors", 2, stopColors);
    }
    if (actor != nullptr)
    {
      ostate->vtkglDisable(GL_DEPTH_TEST);
      actor->RenderOverlay(this);
    }
  }

  ostate->vtkglEnable(GL_DEPTH_TEST);

  vtkOpenGLCheckErrorMacro("failed after Clear");
}

void vtkOpenGLRenderer::ReleaseGraphicsResources(vtkWindow* w)
{
  if (w && this->Pass)
  {
    this->Pass->ReleaseGraphicsResources(w);
  }
  if (this->FXAAFilter)
  {
    this->FXAAFilter->ReleaseGraphicsResources();
  }
  if (w && this->DepthPeelingPass)
  {
    this->DepthPeelingPass->ReleaseGraphicsResources(w);
  }
  if (w && this->SSAOPass)
  {
    this->SSAOPass->ReleaseGraphicsResources(w);
  }
  if (w && this->TranslucentPass)
  {
    this->TranslucentPass->ReleaseGraphicsResources(w);
  }
  if (w && this->ShadowMapPass)
  {
    this->ShadowMapPass->ReleaseGraphicsResources(w);
  }
  if (w && this->EnvMapIrradiance)
  {
    this->EnvMapIrradiance->ReleaseGraphicsResources(w);
  }
  if (w && this->EnvMapLookupTable)
  {
    this->EnvMapLookupTable->ReleaseGraphicsResources(w);
  }
  if (w && this->EnvMapPrefiltered)
  {
    this->EnvMapPrefiltered->ReleaseGraphicsResources(w);
  }

  this->Superclass::ReleaseGraphicsResources(w);
}

vtkOpenGLRenderer::~vtkOpenGLRenderer()
{
  if (this->Pass != nullptr)
  {
    this->Pass->UnRegister(this);
    this->Pass = nullptr;
  }

  if (this->FXAAFilter)
  {
    this->FXAAFilter->Delete();
    this->FXAAFilter = nullptr;
  }

  if (this->ShadowMapPass)
  {
    this->ShadowMapPass->Delete();
    this->ShadowMapPass = nullptr;
  }

  if (this->DepthPeelingPass)
  {
    this->DepthPeelingPass->Delete();
    this->DepthPeelingPass = nullptr;
  }

  if (this->SSAOPass)
  {
    this->SSAOPass->Delete();
    this->SSAOPass = nullptr;
  }

  if (this->TranslucentPass)
  {
    this->TranslucentPass->Delete();
    this->TranslucentPass = nullptr;
  }
}

//------------------------------------------------------------------------------
bool vtkOpenGLRenderer::HaveAppleQueryAllocationBug()
{
#if defined(__APPLE__)
  enum class QueryAllocStatus
  {
    NotChecked,
    Yes,
    No
  };
  static QueryAllocStatus hasBug = QueryAllocStatus::NotChecked;

  if (hasBug == QueryAllocStatus::NotChecked)
  {
    // We can restrict this to a specific version, etc, as we get more
    // information about the bug, but for now just disable query allocations on
    // all apple NVIDIA cards.
    std::string v = reinterpret_cast<const char*>(glGetString(GL_VENDOR));
    hasBug = (v.find("NVIDIA") != std::string::npos) ? QueryAllocStatus::Yes : QueryAllocStatus::No;
  }

  return hasBug == QueryAllocStatus::Yes;
#else
  return false;
#endif
}

//------------------------------------------------------------------------------
bool vtkOpenGLRenderer::IsDualDepthPeelingSupported()
{
  vtkOpenGLRenderWindow* context = vtkOpenGLRenderWindow::SafeDownCast(this->RenderWindow);
  if (!context)
  {
    vtkDebugMacro("Cannot determine if dual depth peeling is support -- no "
                  "vtkRenderWindow set.");
    return false;
  }

  // Dual depth peeling requires:
  // - float textures (ARB_texture_float)
  // - RG textures (ARB_texture_rg)
  // - MAX blending (added in ES3).
  // requires that RG textures be color renderable (they are not in ES3)
#ifdef GL_ES_VERSION_3_0
  // ES3 is not supported, see TestFramebufferPass.cxx for how to do it
  bool dualDepthPeelingSupported = false;
#else
  bool dualDepthPeelingSupported = true;
#endif

  // There's a bug on current mesa master that prevents dual depth peeling
  // from functioning properly, something in the texture sampler is causing
  // all lookups to return NaN. See discussion on
  // https://bugs.freedesktop.org/show_bug.cgi?id=94955
  // This has been fixed in Mesa 17.2.
  const char* glVersionC = reinterpret_cast<const char*>(glGetString(GL_VERSION));
  std::string glVersion = std::string(glVersionC ? glVersionC : "");
  if (dualDepthPeelingSupported && glVersion.find("Mesa") != std::string::npos)
  {
    bool mesaCompat = false;
    // The bug has been fixed with mesa 17.2.0. The version string is approx:
    // 3.3 (Core Profile) Mesa 17.2.0-devel (git-08cb8cf256)
    vtksys::RegularExpression re("Mesa ([0-9]+)\\.([0-9]+)\\.");
    if (re.find(glVersion))
    {
      int majorVersion;
      std::string majorStr = re.match(1);
      std::istringstream majorParse(majorStr);
      majorParse >> majorVersion;
      if (majorVersion > 17)
      {
        mesaCompat = true;
      }
      else if (majorVersion == 17)
      {
        int minorVersion;
        std::string minorStr = re.match(2);
        std::istringstream minorParse(minorStr);
        minorParse >> minorVersion;
        if (minorVersion >= 2)
        {
          mesaCompat = true;
        }
      }
    }

    if (!mesaCompat)
    {
      vtkDebugMacro("Disabling dual depth peeling -- mesa bug detected. "
                    "GL_VERSION = '"
        << glVersion << "'.");
      dualDepthPeelingSupported = false;
    }
  }

  // The old implementation can be forced by defining the environment var
  // "VTK_USE_LEGACY_DEPTH_PEELING":
  if (dualDepthPeelingSupported)
  {
    const char* forceLegacy = getenv("VTK_USE_LEGACY_DEPTH_PEELING");
    if (forceLegacy)
    {
      vtkDebugMacro("Disabling dual depth peeling -- "
                    "VTK_USE_LEGACY_DEPTH_PEELING defined in environment.");
      dualDepthPeelingSupported = false;
    }
  }

  return dualDepthPeelingSupported;
}

vtkOpenGLState* vtkOpenGLRenderer::GetState()
{
  return this->VTKWindow ? static_cast<vtkOpenGLRenderWindow*>(this->VTKWindow)->GetState()
                         : nullptr;
}

const char* vtkOpenGLRenderer::GetLightingUniforms()
{
  return this->LightingDeclaration.c_str();
}

void vtkOpenGLRenderer::UpdateLightingUniforms(vtkShaderProgram* program)
{
  vtkMTimeType ptime = program->GetUniformGroupUpdateTime(vtkShaderProgram::LightingGroup);
  vtkMTimeType ltime = this->LightingUpdateTime;

  // for lighting complexity 2,3 camera has an impact
  vtkCamera* cam = this->GetActiveCamera();
  if (this->LightingComplexity > 1)
  {
    ltime = vtkMath::Max(ltime, cam->GetMTime());
  }

  if (ltime <= ptime)
  {
    return;
  }

  // for lightkit case there are some parameters to set
  vtkTransform* viewTF = cam->GetModelViewTransformObject();

  // bind some light settings
  int numberOfLights = 0;
  vtkLightCollection* lc = this->GetLights();
  vtkLight* light;

  vtkCollectionSimpleIterator sit;
  float lightColor[3];
  float lightDirection[3];
  std::string lcolor("lightColor");
  std::string ldir("lightDirectionVC");
  std::string latten("lightAttenuation");
  std::string lpositional("lightPositional");
  std::string lpos("lightPositionVC");
  std::string lexp("lightExponent");
  std::string lcone("lightConeAngle");

  std::ostringstream toString;
  for (lc->InitTraversal(sit); (light = lc->GetNextLight(sit));)
  {
    float status = light->GetSwitch();
    if (status > 0.0)
    {
      toString.str("");
      toString << numberOfLights;
      std::string count = toString.str();

      double* dColor = light->GetDiffuseColor();
      double intensity = light->GetIntensity();
      // if (renderLuminance)
      // {
      //   lightColor[0] = intensity;
      //   lightColor[1] = intensity;
      //   lightColor[2] = intensity;
      // }
      // else
      {
        lightColor[0] = dColor[0] * intensity;
        lightColor[1] = dColor[1] * intensity;
        lightColor[2] = dColor[2] * intensity;
      }
      program->SetUniform3f((lcolor + count).c_str(), lightColor);

      // we are done unless we have non headlights
      if (this->LightingComplexity >= 2)
      {
        // get required info from light
        double* lfp = light->GetTransformedFocalPoint();
        double* lp = light->GetTransformedPosition();
        double lightDir[3];
        vtkMath::Subtract(lfp, lp, lightDir);
        vtkMath::Normalize(lightDir);
        double tDirView[3];
        viewTF->TransformNormal(lightDir, tDirView);

        if (!light->LightTypeIsSceneLight() && this->UserLightTransform.GetPointer() != nullptr)
        {
          double* tDir = this->UserLightTransform->TransformNormal(tDirView);
          lightDirection[0] = tDir[0];
          lightDirection[1] = tDir[1];
          lightDirection[2] = tDir[2];
        }
        else
        {
          lightDirection[0] = tDirView[0];
          lightDirection[1] = tDirView[1];
          lightDirection[2] = tDirView[2];
        }

        program->SetUniform3f((ldir + count).c_str(), lightDirection);

        // we are done unless we have positional lights
        if (this->LightingComplexity >= 3)
        {
          // if positional lights pass down more parameters
          float lightAttenuation[3];
          float lightPosition[3];
          double* attn = light->GetAttenuationValues();
          lightAttenuation[0] = attn[0];
          lightAttenuation[1] = attn[1];
          lightAttenuation[2] = attn[2];
          double tlpView[3];
          viewTF->TransformPoint(lp, tlpView);
          if (!light->LightTypeIsSceneLight() && this->UserLightTransform.GetPointer() != nullptr)
          {
            double* tlp = this->UserLightTransform->TransformPoint(tlpView);
            lightPosition[0] = tlp[0];
            lightPosition[1] = tlp[1];
            lightPosition[2] = tlp[2];
          }
          else
          {
            lightPosition[0] = tlpView[0];
            lightPosition[1] = tlpView[1];
            lightPosition[2] = tlpView[2];
          }

          program->SetUniform3f((latten + count).c_str(), lightAttenuation);
          program->SetUniformi((lpositional + count).c_str(), light->GetPositional());
          program->SetUniform3f((lpos + count).c_str(), lightPosition);
          program->SetUniformf((lexp + count).c_str(), light->GetExponent());
          program->SetUniformf((lcone + count).c_str(), light->GetConeAngle());
        }
      }
      numberOfLights++;
    }
  }

  program->SetUniformGroupUpdateTime(vtkShaderProgram::LightingGroup, ltime);
}

//------------------------------------------------------------------------------
void vtkOpenGLRenderer::SetUserLightTransform(vtkTransform* transform)
{
  this->UserLightTransform = transform;
}

//------------------------------------------------------------------------------
vtkTransform* vtkOpenGLRenderer::GetUserLightTransform()
{
  return this->UserLightTransform;
}

//------------------------------------------------------------------------------
vtkFloatArray* vtkOpenGLRenderer::GetSphericalHarmonics()
{
  return this->SphericalHarmonics;
}

//------------------------------------------------------------------------------
void vtkOpenGLRenderer::SetEnvironmentTexture(vtkTexture* texture, bool isSRGB)
{
  this->Superclass::SetEnvironmentTexture(texture);

  vtkOpenGLTexture* oglTexture = vtkOpenGLTexture::SafeDownCast(texture);

  if (oglTexture)
  {
    this->GetEnvMapIrradiance()->SetInputTexture(oglTexture);
    this->GetEnvMapPrefiltered()->SetInputTexture(oglTexture);

    this->GetEnvMapIrradiance()->SetConvertToLinear(isSRGB);
    this->GetEnvMapPrefiltered()->SetConvertToLinear(isSRGB);
  }
  else
  {
    this->GetEnvMapIrradiance()->SetInputTexture(nullptr);
    this->GetEnvMapPrefiltered()->SetInputTexture(nullptr);
  }
}

//------------------------------------------------------------------------------
vtkPBRLUTTexture* vtkOpenGLRenderer::GetEnvMapLookupTable()
{
  if (!this->EnvMapLookupTable)
  {
    this->EnvMapLookupTable = vtkSmartPointer<vtkPBRLUTTexture>::New();
  }
  return this->EnvMapLookupTable;
}

//------------------------------------------------------------------------------
vtkPBRIrradianceTexture* vtkOpenGLRenderer::GetEnvMapIrradiance()
{
  if (!this->EnvMapIrradiance)
  {
    this->EnvMapIrradiance = vtkSmartPointer<vtkPBRIrradianceTexture>::New();
  }
  return this->EnvMapIrradiance;
}

//------------------------------------------------------------------------------
vtkPBRPrefilterTexture* vtkOpenGLRenderer::GetEnvMapPrefiltered()
{
  if (!this->EnvMapPrefiltered)
  {
    this->EnvMapPrefiltered = vtkSmartPointer<vtkPBRPrefilterTexture>::New();
  }
  return this->EnvMapPrefiltered;
}
VTK_ABI_NAMESPACE_END
