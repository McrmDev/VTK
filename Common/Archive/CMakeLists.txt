set(classes
  vtkBufferedArchiver
  vtkPartitionedArchiver)

vtk_module_find_package(PRIVATE_IF_SHARED
  PACKAGE LibArchive)

vtk_module_add_module(VTK::CommonArchive
  CLASSES ${classes})

vtk_module_link(VTK::CommonArchive NO_KIT_EXPORT_IF_SHARED PRIVATE LibArchive::LibArchive)
vtk_add_test_mangling(VTK::CommonArchive)
