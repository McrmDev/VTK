vtk_module_test_data()

if (NOT vtk_testing_cxx_disabled)
  add_subdirectory(Cxx)
endif ()

if (VTK_WRAP_PYTHON)
  vtk_module_test_data(Data/cube-with-time.vtu)

  add_subdirectory(Python)
endif ()
