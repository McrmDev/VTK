vtk_add_test_cxx(vtkInteractionStyleCxxTests tests
  NO_VALID
  TestJoystickFlyIn.cxx,NO_DATA
  TestJoystickFlyOut.cxx,NO_DATA
  TestTableTopRotate.cxx,NO_DATA
  TestTrackballEnvironmentRotate.cxx
  )
vtk_test_cxx_executable(vtkInteractionStyleCxxTests tests)
