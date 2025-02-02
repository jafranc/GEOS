/*
 * ------------------------------------------------------------------------------------------------------------
 * SPDX-License-Identifier: LGPL-2.1-only
 *
 * Copyright (c) 2018-2020 Lawrence Livermore National Security LLC
 * Copyright (c) 2018-2020 The Board of Trustees of the Leland Stanford Junior University
 * Copyright (c) 2018-2020 TotalEnergies
 * Copyright (c) 2019-     GEOSX Contributors
 * All rights reserved
 *
 * See top level LICENSE, COPYRIGHT, CONTRIBUTORS, NOTICE, and ACKNOWLEDGEMENTS files for details.
 * ------------------------------------------------------------------------------------------------------------
 */


/**
 * @file ElasticWaveEquationSEM.hpp
 */

#ifndef SRC_CORECOMPONENTS_PHYSICSSOLVERS_WAVEPROPAGATION_ELASTICWAVEEQUATIONSEM_HPP_
#define SRC_CORECOMPONENTS_PHYSICSSOLVERS_WAVEPROPAGATION_ELASTICWAVEEQUATIONSEM_HPP_

#include "mesh/ExtrinsicMeshData.hpp"
#include "physicsSolvers/SolverBase.hpp"
#include "WaveSolverBase.hpp"



namespace geosx
{

class ElasticWaveEquationSEM : public WaveSolverBase
{
public:

  using EXEC_POLICY = parallelDevicePolicy< 32 >;
  using ATOMIC_POLICY = parallelDeviceAtomic;

  /**
   * @brief Safeguard for timeStep. Used to avoid memory issue due to too small value.
   */
  static constexpr real64 epsilonLoc = 1e-8;

  ElasticWaveEquationSEM( const std::string & name,
                          Group * const parent );

  virtual ~ElasticWaveEquationSEM() override;

  ElasticWaveEquationSEM() = delete;
  ElasticWaveEquationSEM( ElasticWaveEquationSEM const & ) = delete;
  ElasticWaveEquationSEM( ElasticWaveEquationSEM && ) = default;

  ElasticWaveEquationSEM & operator=( ElasticWaveEquationSEM const & ) = delete;
  ElasticWaveEquationSEM & operator=( ElasticWaveEquationSEM && ) = delete;


  static string catalogName() { return "ElasticSEM"; }

  virtual void initializePreSubGroups() override;

  virtual void registerDataOnMesh( Group & meshBodies ) override final;


  /**
   * @defgroup Solver Interface Functions
   *
   * These functions provide the primary interface that is required for derived classes
   */
  /**@{*/
  virtual
  real64 solverStep( real64 const & time_n,
                     real64 const & dt,
                     integer const cycleNumber,
                     DomainPartition & domain ) override;

  virtual
  real64 explicitStep( real64 const & time_n,
                       real64 const & dt,
                       integer const cycleNumber,
                       DomainPartition & domain ) override;

  /**
   * @brief Multiply the precomputed term by the Ricker and add to the right-hand side
   * @param cycleNumber the cycle number/step number of evaluation of the source
   * @param rhsx the right hand side vector to be computed (x-component)
   * @param rhsy the right hand side vector to be computed (x-component)
   * @param rhsz the right hand side vector to be computed (x-component)
   */
  void addSourceToRightHandSide( integer const & cycleNumber, arrayView1d< real32 > const rhsx, arrayView1d< real32 > const rhsy, arrayView1d< real32 > const rhsz );

  /**
   * TODO: move implementation into WaveSolverBase
   * @brief Compute the sesimic traces for a given variable at each receiver coordinate at a given time, using the field values at the
   * last two timesteps.
   * @param time_n the time corresponding to the field values at iteration n
   * @param dt the simulation timestep
   * @param timeSeismo the time at which the seismogram is computed
   * @param iSeismo the index of the seismogram time in the seismogram array
   * @param var_np1 the field values at time_n + dt
   * @param var_n the field values at time_n
   * @param varAtreceivers the array holding the trace values, where the output is written
   */
  virtual void computeSeismoTrace( real64 const time_n,
                                   real64 const dt,
                                   real64 const timeSeismo,
                                   localIndex const iSeismo,
                                   arrayView1d< real32 const > const var_np1,
                                   arrayView1d< real32 const > const var_n,
                                   arrayView2d< real32 > varAtReceivers ) override;

  /**
   * TODO: move implementation into WaveSolverBase
   * @brief Computes the traces on all receivers (see @computeSeismoTraces) up to time_n+dt
   * @param time_n the time corresponding to the field values at iteration n
   * @param dt the simulation timestep
   * @param var_np1 the field values at time_n + dt
   * @param var_n the field values at time_n
   * @param varAtreceivers the array holding the trace values, where the output is written
   */
  virtual void computeAllSeismoTraces( real64 const time_n,
                                       real64 const dt,
                                       arrayView1d< real32 const > const var_np1,
                                       arrayView1d< real32 const > const var_n,
                                       arrayView2d< real32 > varAtReceivers );


  /**
   * @brief Overridden from ExecutableGroup. Used to write last seismogram if needed.
   */
  virtual void cleanup( real64 const time_n, integer const cycleNumber, integer const eventCounter, real64 const eventProgress, DomainPartition & domain ) override;

  struct viewKeyStruct : SolverBase::viewKeyStruct
  {
    static constexpr char const * sourceNodeIdsString() { return "sourceNodeIds"; }
    static constexpr char const * sourceConstantsString() { return "sourceConstants"; }
    static constexpr char const * sourceIsAccessibleString() { return "sourceIsAccessible"; }

    static constexpr char const * receiverNodeIdsString() { return "receiverNodeIds"; }
    static constexpr char const * receiverConstantsString() {return "receiverConstants"; }
    static constexpr char const * receiverIsLocalString() { return "receiverIsLocal"; }

    static constexpr char const * displacementXNp1AtReceiversString() { return "displacementXNp1AtReceivers"; }
    static constexpr char const * displacementYNp1AtReceiversString() { return "displacementYNp1AtReceivers"; }
    static constexpr char const * displacementZNp1AtReceiversString() { return "displacementZNp1AtReceivers"; }

  } waveEquationViewKeys;


protected:

  virtual void postProcessInput() override final;

  virtual void initializePostInitialConditionsPreSubGroups() override final;

private:

  /**
   * @brief Locate sources and receivers position in the mesh elements, evaluate the basis functions at each point and save them to the
   * corresponding elements nodes.
   * @param mesh mesh of the computational domain
   * @param regionNames the names of the region you loop on
   */
  virtual void precomputeSourceAndReceiverTerm( MeshLevel & mesh, arrayView1d< string const > const & regionNames ) override;

  /**
   * @brief Apply free surface condition to the face define in the geometry box from the xml
   * @param time the time to apply the BC
   * @param domain the partition domain
   */
  virtual void applyFreeSurfaceBC( real64 const time, DomainPartition & domain ) override;

  /**
   * @brief Initialize Perfectly Matched Layer (PML) information
   */
  virtual void initializePML() override;

  /**
   * @brief Apply Perfectly Matched Layer (PML) to the regions defined in the geometry box from the xml
   * @param time the time to apply the BC
   * @param domain the partition domain
   */
  virtual void applyPML( real64 const time, DomainPartition & domain ) override;


  /// save the sismo trace in file
  void saveSeismo( localIndex iseismo, real32 val, string const & filename ) override;

  /// Indices of the nodes (in the right order) for each source point
  array2d< localIndex > m_sourceNodeIds;


  /// Constant part of the source for the nodes listed in m_sourceNodeIds in x-direction
  array2d< real64 > m_sourceConstantsx;

  /// Constant part of the source for the nodes listed in m_sourceNodeIds in x-direction
  array2d< real64 > m_sourceConstantsy;

  /// Constant part of the source for the nodes listed in m_sourceNodeIds in x-direction
  array2d< real64 > m_sourceConstantsz;

  /// Flag that indicates whether the source is accessible or not to the MPI rank
  array1d< localIndex > m_sourceIsAccessible;

  /// Indices of the element nodes (in the right order) for each receiver point
  array2d< localIndex > m_receiverNodeIds;

  /// Basis function evaluated at the receiver for the nodes listed in m_receiverNodeIds
  array2d< real64 > m_receiverConstants;

  /// Flag that indicates whether the receiver is local or not to the MPI rank
  array1d< localIndex > m_receiverIsLocal;

  /// Displacement_np1 at the receiver location for each time step for each receiver (x-component)
  array2d< real32 > m_displacementXNp1AtReceivers;

  /// Displacement_np1 at the receiver location for each time step for each receiver (y-component)
  array2d< real32 > m_displacementYNp1AtReceivers;

  /// Displacement_np1 at the receiver location for each time step for each receiver (z-component)
  array2d< real32 > m_displacementZNp1AtReceivers;

};


namespace extrinsicMeshData
{

EXTRINSIC_MESH_DATA_TRAIT( Displacementx_nm1,
                           "displacementx_nm1",
                           array1d< real32 >,
                           0,
                           NOPLOT,
                           WRITE_AND_READ,
                           "x-component of displacement at time n-1." );

EXTRINSIC_MESH_DATA_TRAIT( Displacementy_nm1,
                           "displacementy_nm1",
                           array1d< real32 >,
                           0,
                           NOPLOT,
                           WRITE_AND_READ,
                           "y-component of displacement at time n-1." );

EXTRINSIC_MESH_DATA_TRAIT( Displacementz_nm1,
                           "displacementz_nm1",
                           array1d< real32 >,
                           0,
                           NOPLOT,
                           WRITE_AND_READ,
                           "z-component of displacement at time n-1." );


EXTRINSIC_MESH_DATA_TRAIT( Displacementx_n,
                           "displacementx_n",
                           array1d< real32 >,
                           0,
                           NOPLOT,
                           WRITE_AND_READ,
                           "x-component of displacement at time n." );

EXTRINSIC_MESH_DATA_TRAIT( Displacementy_n,
                           "displacementy_n",
                           array1d< real32 >,
                           0,
                           NOPLOT,
                           WRITE_AND_READ,
                           "y-component of displacement at time n." );

EXTRINSIC_MESH_DATA_TRAIT( Displacementz_n,
                           "displacementz_n",
                           array1d< real32 >,
                           0,
                           NOPLOT,
                           WRITE_AND_READ,
                           "z-component of displacement at time n." );
EXTRINSIC_MESH_DATA_TRAIT( Displacementx_np1,
                           "displacementx_np1",
                           array1d< real32 >,
                           0,
                           LEVEL_0,
                           WRITE_AND_READ,
                           "x-component of displacement at time n+1." );

EXTRINSIC_MESH_DATA_TRAIT( Displacementy_np1,
                           "displacementy_np1",
                           array1d< real32 >,
                           0,
                           LEVEL_0,
                           WRITE_AND_READ,
                           "y-component of displacement at time n+1." );

EXTRINSIC_MESH_DATA_TRAIT( Displacementz_np1,
                           "displacementz_np1",
                           array1d< real32 >,
                           0,
                           LEVEL_0,
                           WRITE_AND_READ,
                           "z-component of displacement at time n+1." );

EXTRINSIC_MESH_DATA_TRAIT( ForcingRHSx,
                           "rhsx",
                           array1d< real32 >,
                           0,
                           NOPLOT,
                           WRITE_AND_READ,
                           "RHS for x-direction" );

EXTRINSIC_MESH_DATA_TRAIT( ForcingRHSy,
                           "rhsy",
                           array1d< real32 >,
                           0,
                           NOPLOT,
                           WRITE_AND_READ,
                           "RHS for y-direction" );

EXTRINSIC_MESH_DATA_TRAIT( ForcingRHSz,
                           "rhsz",
                           array1d< real32 >,
                           0,
                           NOPLOT,
                           WRITE_AND_READ,
                           "RHS for z-direction" );

EXTRINSIC_MESH_DATA_TRAIT( MassVector,
                           "massVector",
                           array1d< real32 >,
                           0,
                           NOPLOT,
                           WRITE_AND_READ,
                           "Diagonal Mass Matrix." );

EXTRINSIC_MESH_DATA_TRAIT( DampingVectorx,
                           "dampingVectorx",
                           array1d< real32 >,
                           0,
                           NOPLOT,
                           WRITE_AND_READ,
                           "Diagonal Damping Matrix in x-direction." );

EXTRINSIC_MESH_DATA_TRAIT( DampingVectory,
                           "dampingVectory",
                           array1d< real32 >,
                           0,
                           NOPLOT,
                           WRITE_AND_READ,
                           "Diagonal Damping Matrix in y-direction." );

EXTRINSIC_MESH_DATA_TRAIT( DampingVectorz,
                           "dampingVectorz",
                           array1d< real32 >,
                           0,
                           NOPLOT,
                           WRITE_AND_READ,
                           "Diagonal Damping Matrix in z-direction." );

EXTRINSIC_MESH_DATA_TRAIT( StiffnessVectorx,
                           "stiffnessVectorx",
                           array1d< real32 >,
                           0,
                           NOPLOT,
                           WRITE_AND_READ,
                           "x-component of stiffness vector." );

EXTRINSIC_MESH_DATA_TRAIT( StiffnessVectory,
                           "stiffnessVectory",
                           array1d< real32 >,
                           0,
                           NOPLOT,
                           WRITE_AND_READ,
                           "y-component of stiffness vector." );

EXTRINSIC_MESH_DATA_TRAIT( StiffnessVectorz,
                           "stiffnessVectorz",
                           array1d< real32 >,
                           0,
                           NOPLOT,
                           WRITE_AND_READ,
                           "z-component of stiffness vector." );

EXTRINSIC_MESH_DATA_TRAIT( MediumVelocityVp,
                           "mediumVelocityVp",
                           array1d< real32 >,
                           0,
                           NOPLOT,
                           WRITE_AND_READ,
                           "P-waves speed in the cell" );

EXTRINSIC_MESH_DATA_TRAIT( MediumVelocityVs,
                           "mediumVelocityVs",
                           array1d< real32 >,
                           0,
                           NOPLOT,
                           WRITE_AND_READ,
                           "S-waves speed in the cell" );

EXTRINSIC_MESH_DATA_TRAIT( MediumDensity,
                           "mediumDensity",
                           array1d< real32 >,
                           0,
                           NOPLOT,
                           WRITE_AND_READ,
                           "Medium density of the cell" );

EXTRINSIC_MESH_DATA_TRAIT( FreeSurfaceFaceIndicator,
                           "freeSurfaceFaceIndicator",
                           array1d< localIndex >,
                           0,
                           NOPLOT,
                           WRITE_AND_READ,
                           "Free surface indicator, 1 if a face is on free surface 0 otherwise." );

EXTRINSIC_MESH_DATA_TRAIT( FreeSurfaceNodeIndicator,
                           "freeSurfaceNodeIndicator",
                           array1d< localIndex >,
                           0,
                           NOPLOT,
                           WRITE_AND_READ,
                           "Free surface indicator, 1 if a node is on free surface 0 otherwise." );


}


} /* namespace geosx */

#endif /* SRC_CORECOMPONENTS_PHYSICSSOLVERS_WAVEPROPAGATION_ELASSTICWAVEEQUATIONSEM_HPP_ */
