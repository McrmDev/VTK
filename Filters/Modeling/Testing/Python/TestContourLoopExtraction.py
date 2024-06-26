#!/usr/bin/env python
from vtkmodules.vtkCommonCore import (
    vtkLookupTable,
    vtkPoints,
)
from vtkmodules.vtkCommonDataModel import (
    vtkCellArray,
    vtkPolyData,
)
from vtkmodules.vtkFiltersCore import (
    vtkFlyingEdges2D,
    vtkGlyph3D,
    vtkGenerateIds,
    vtkTriangleFilter,
)
from vtkmodules.vtkFiltersModeling import (
    vtkContourLoopExtraction,
    vtkCookieCutter,
)
from vtkmodules.vtkFiltersSources import vtkPlaneSource
from vtkmodules.vtkIOImage import vtkDEMReader
from vtkmodules.vtkRenderingCore import (
    vtkActor,
    vtkPolyDataMapper,
    vtkRenderWindow,
    vtkRenderWindowInteractor,
    vtkRenderer,
)
import vtkmodules.vtkInteractionStyle
import vtkmodules.vtkRenderingFreeType
import vtkmodules.vtkRenderingOpenGL2
from vtkmodules.util.misc import vtkGetDataRoot
VTK_DATA_ROOT = vtkGetDataRoot()

# create planes
# Create the RenderWindow, Renderer
#
ren = vtkRenderer()
renWin = vtkRenderWindow()
renWin.AddRenderer( ren )

iren = vtkRenderWindowInteractor()
iren.SetRenderWindow(renWin)

# Create pipeline. generate contours from terrain data.
#
lut = vtkLookupTable()
lut.SetHueRange(0.6, 0)
lut.SetSaturationRange(1.0, 0)
lut.SetValueRange(0.5, 1.0)

# Read the data: a height field results
demReader = vtkDEMReader()
demReader.SetFileName(VTK_DATA_ROOT + "/Data/SainteHelens.dem")
demReader.Update()

lo = demReader.GetOutput().GetScalarRange()[0]
hi = demReader.GetOutput().GetScalarRange()[1]

# Generate contours
contours = vtkFlyingEdges2D()
contours.SetInputConnection(demReader.GetOutputPort())
contours.SetValue(0, (hi + lo)/2.0)

# Construct loops
loops = vtkContourLoopExtraction()
loops.SetInputConnection(contours.GetOutputPort())
loops.Update()
bds = loops.GetOutput().GetBounds()

# Place glyphs inside polygons
plane = vtkPlaneSource()
plane.SetXResolution(25)
plane.SetYResolution(25)
plane.SetOrigin(bds[0],bds[2],bds[4]);
plane.SetPoint1(bds[1],bds[2],bds[4]);
plane.SetPoint2(bds[0],bds[3],bds[4]);

# Custom glyph
glyphData = vtkPolyData()
glyphPts = vtkPoints()
glyphVerts = vtkCellArray()
glyphLines = vtkCellArray()
glyphPolys = vtkCellArray()
glyphData.SetPoints(glyphPts)
glyphData.SetVerts(glyphVerts)
glyphData.SetLines(glyphLines)
glyphData.SetPolys(glyphPolys)

glyphPts.InsertPoint(0, -0.5,-0.5,0.0)
glyphPts.InsertPoint(1,  0.5,-0.5,0.0)
glyphPts.InsertPoint(2,  0.5, 0.5,0.0)
glyphPts.InsertPoint(3, -0.5, 0.5,0.0)
glyphPts.InsertPoint(4,  0.0,-0.5,0.0)
glyphPts.InsertPoint(5,  0.0,-0.75,0.0)
glyphPts.InsertPoint(6,  0.5, 0.0,0.0)
glyphPts.InsertPoint(7,  0.75, 0.0,0.0)
glyphPts.InsertPoint(8,  0.0, 0.5,0.0)
glyphPts.InsertPoint(9,  0.0, 0.75,0.0)
glyphPts.InsertPoint(10, -0.5,0.0,0.0)
glyphPts.InsertPoint(11, -0.75,0.0,0.0)
glyphPts.InsertPoint(12, 0.0,0.0,0.0)
glyphPts.InsertPoint(13, -0.9,0.0,0.0)

glyphVerts.InsertNextCell(1)
glyphVerts.InsertCellPoint(12)

glyphLines.InsertNextCell(2)
glyphLines.InsertCellPoint(4)
glyphLines.InsertCellPoint(5)
glyphLines.InsertNextCell(2)
glyphLines.InsertCellPoint(6)
glyphLines.InsertCellPoint(7)
glyphLines.InsertNextCell(2)
glyphLines.InsertCellPoint(8)
glyphLines.InsertCellPoint(9)
# The last line is a polyline
glyphLines.InsertNextCell(3)
glyphLines.InsertCellPoint(10)
glyphLines.InsertCellPoint(11)
glyphLines.InsertCellPoint(13)

glyphPolys.InsertNextCell(4)
glyphPolys.InsertCellPoint(0)
glyphPolys.InsertCellPoint(1)
glyphPolys.InsertCellPoint(2)
glyphPolys.InsertCellPoint(3)

glyph = vtkGlyph3D()
glyph.SetInputConnection(plane.GetOutputPort())
glyph.SetSourceData(glyphData)
glyph.SetScaleFactor( 100 )

ids = vtkGenerateIds()
ids.SetInputConnection(glyph.GetOutputPort())
ids.Update()

cookie = vtkCookieCutter()
cookie.SetInputConnection(ids.GetOutputPort())
cookie.SetLoopsConnection(loops.GetOutputPort())
cookie.PassPointDataOff()
cookie.PassCellDataOff()

tri = vtkTriangleFilter()
tri.SetInputConnection(cookie.GetOutputPort())

mapper = vtkPolyDataMapper()
mapper.SetInputConnection(tri.GetOutputPort())
mapper.ScalarVisibilityOff()

actor = vtkActor()
actor.SetMapper(mapper)

# Show the loop
loopMapper = vtkPolyDataMapper()
loopMapper.SetInputConnection(loops.GetOutputPort())

loopActor = vtkActor()
loopActor.SetMapper(loopMapper)
loopActor.GetProperty().SetRepresentationToWireframe()

ren.AddActor(actor)
ren.AddActor(loopActor)

renWin.Render()
iren.Start()
