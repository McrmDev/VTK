#!/usr/bin/env python
from GetReader import get_reader
from vtkmodules.vtkFiltersCore import vtkAssignAttribute
from vtkmodules.vtkFiltersGeometry import vtkDataSetSurfaceFilter
from vtkmodules.vtkIONetCDF import vtkNetCDFCFReader
from vtkmodules.vtkRenderingCore import (
    vtkActor,
    vtkPolyDataMapper,
    vtkRenderWindow,
    vtkRenderer,
)
import vtkmodules.vtkInteractionStyle
import vtkmodules.vtkRenderingFreeType
import vtkmodules.vtkRenderingOpenGL2
from vtkmodules.util.misc import vtkGetDataRoot
VTK_DATA_ROOT = vtkGetDataRoot()

# This test checks netCDF CF reader for reading unstructured (p-sided) cells.
renWin = vtkRenderWindow()
renWin.SetSize(400,200)
#############################################################################
# Case 1: Spherical coordinates off.
# Open the file.
reader_cartesian = get_reader(VTK_DATA_ROOT + "/Data/sampleGenGrid3.nc")
# Set the arrays we want to load.
reader_cartesian.UpdateMetaData()
reader_cartesian.SetVariableArrayStatus("sample",1)
reader_cartesian.SphericalCoordinatesOff()
# Assign the field to scalars.
aa_cartesian = vtkAssignAttribute()
aa_cartesian.SetInputConnection(reader_cartesian.GetOutputPort())
aa_cartesian.Assign("sample","SCALARS","CELL_DATA")
# Extract a surface that we can render.
surface_cartesian = vtkDataSetSurfaceFilter()
surface_cartesian.SetInputConnection(aa_cartesian.GetOutputPort())
mapper_cartesian = vtkPolyDataMapper()
mapper_cartesian.SetInputConnection(surface_cartesian.GetOutputPort())
mapper_cartesian.SetScalarRange(100,2500)
actor_cartesian = vtkActor()
actor_cartesian.SetMapper(mapper_cartesian)
ren_cartesian = vtkRenderer()
ren_cartesian.AddActor(actor_cartesian)
ren_cartesian.SetViewport(0.0,0.0,0.5,1.0)
renWin.AddRenderer(ren_cartesian)
#############################################################################
# Case 2: Spherical coordinates on.
# Open the file.
reader_spherical = get_reader(VTK_DATA_ROOT + "/Data/sampleGenGrid3.nc")
# Set the arrays we want to load.
reader_spherical.UpdateMetaData()
reader_spherical.SetVariableArrayStatus("sample",1)
reader_spherical.SphericalCoordinatesOn()
# Assign the field to scalars.
aa_spherical = vtkAssignAttribute()
aa_spherical.SetInputConnection(reader_spherical.GetOutputPort())
aa_spherical.Assign("sample","SCALARS","CELL_DATA")
# Extract a surface that we can render.
surface_spherical = vtkDataSetSurfaceFilter()
surface_spherical.SetInputConnection(aa_spherical.GetOutputPort())
mapper_spherical = vtkPolyDataMapper()
mapper_spherical.SetInputConnection(surface_spherical.GetOutputPort())
mapper_spherical.SetScalarRange(100,2500)
actor_spherical = vtkActor()
actor_spherical.SetMapper(mapper_spherical)
ren_spherical = vtkRenderer()
ren_spherical.AddActor(actor_spherical)
ren_spherical.SetViewport(0.5,0.0,1.0,1.0)
renWin.AddRenderer(ren_spherical)
#############################################################################
# Case 3: Read as structured data.
# The resulting data is garbage, so we won't actually look at the output.
# This is just to verify that nothing errors out or crashes.
reader_structured = get_reader(VTK_DATA_ROOT + "/Data/sampleGenGrid3.nc")
reader_structured.UpdateMetaData()
reader_structured.SetVariableArrayStatus("sample",1)
reader_structured.SphericalCoordinatesOn()
reader_structured.SetOutputTypeToImage()
reader_structured.Update()
reader_structured.SetOutputTypeToRectilinear()
reader_structured.Update()
reader_structured.SetOutputTypeToStructured()
reader_structured.Update()
# --- end of script --
