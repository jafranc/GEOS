include(${CMAKE_CURRENT_LIST_DIR}/../../src/coreComponents/LvArray/host-configs/LLNL/tioga-cce@15.0.0.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/tioga-base.cmake)

set( CONDUIT_DIR "${GEOSX_TPL_DIR}/conduit-0.8.6" CACHE PATH "" )
set( HDF5_DIR "${GEOSX_TPL_DIR}/hdf5-1.12.2" CACHE PATH "" )

set( BLAS_DIR "/opt/rocm-${HIP_VERSION_STRING}/" CACHE PATH "" )

set( PUGIXML_DIR "${GEOSX_TPL_DIR}/pugixml-1.11.4" CACHE PATH "" )
set( FMT_DIR "${GEOSX_TPL_DIR}/fmt-8.0.1" CACHE PATH "" )
set( SUITESPARSE_DIR "${GEOSX_TPL_DIR}/suite-sparse-5.10.1" CACHE PATH "" )

# HYPRE options
set( ENABLE_HYPRE_DEVICE "HIP" CACHE STRING "" )
set( HYPRE_DIR "${GEOSX_TPL_DIR}/hypre-develop" CACHE PATH "" )

set( ENABLE_CALIPER ON CACHE BOOL "" FORCE )
set( CALIPER_DIR "${GEOSX_TPL_DIR}/caliper-2.8.0" CACHE PATH "" )
