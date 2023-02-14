set(CCE_VERSION 15.0.0)

include(${CMAKE_CURRENT_LIST_DIR}/../../src/coreComponents/LvArray/host-configs/ORNL/crusher-cce@${CCE_VERSION}.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/crusher-base.cmake)

set( CONDUIT_DIR "${GEOSX_TPL_DIR}/conduit" CACHE PATH "" )
set( HDF5_DIR "${GEOSX_TPL_DIR}/hdf5" CACHE PATH "" )
set( ENABLE_SILO OFF CACHE BOOL "" )

set( BLAS_DIR "/opt/rocm-${ROCM_VERSION}/" CACHE PATH "" )

set( PUGIXML_DIR "${GEOSX_TPL_DIR}/pugixml" CACHE PATH "" )
set( FMT_DIR "${GEOSX_TPL_DIR}/fmt" CACHE PATH "" )
set( SUITESPARSE_DIR "${GEOSX_TPL_DIR}/suite-sparse" CACHE PATH "" )

# HYPRE options
set( ENABLE_HYPRE_DEVICE "HIP" CACHE STRING "" )
if( ${ENABLE_HYPRE_DEVICE} STREQUAL "HIP" )
  set( HYPRE_DIR "/ccs/home/victorapm/projects/hypre-master/install-gpu" CACHE PATH "" ) # victor's build
else()
  set( HYPRE_DIR "${GEOSX_TPL_DIR}/hypre" CACHE PATH "" )
endif()

set( ENABLE_CALIPER ON CACHE BOOL "" FORCE )
set( CALIPER_DIR "${GEOSX_TPL_DIR}/caliper" CACHE PATH "" )
