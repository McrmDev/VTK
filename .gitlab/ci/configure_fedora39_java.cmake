set(VTK_JAVA_RELEASE_VERSION $ENV{VTK_JAVA_VERSION} CACHE STRING "" FORCE)

include("${CMAKE_CURRENT_LIST_DIR}/configure_fedora39.cmake")

set(MAVEN_LOCAL_NATIVE_NAME "linux-amd" CACHE STRING "" FORCE)
set(VTK_GENERATE_SPDX OFF CACHE BOOL "" FORCE)

set(VTK_MODULE_ENABLE_VTK_fides NO CACHE STRING "")
set(VTK_MODULE_ENABLE_VTK_IOADIOS2 NO CACHE STRING "")
set(VTK_MODULE_ENABLE_VTK_IOFides NO CACHE STRING "")

set(JOGL_GLUE "$ENV{HOME}/.m2/repository/org/jogamp/gluegen/gluegen-rt/2.3.2/gluegen-rt-2.3.2.jar" CACHE FILEPATH "")
set(JOGL_LIB  "$ENV{HOME}/.m2/repository/org/jogamp/jogl/jogl-all/2.3.2/jogl-all-2.3.2.jar" CACHE FILEPATH "")
