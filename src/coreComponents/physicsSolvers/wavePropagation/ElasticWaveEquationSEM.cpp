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
 * @file ElasticWaveEquationSEM.cpp
 */

#include "ElasticWaveEquationSEM.hpp"
#include "ElasticWaveEquationSEMKernel.hpp"

#include "dataRepository/KeyNames.hpp"
#include "finiteElement/FiniteElementDiscretization.hpp"
#include "fieldSpecification/FieldSpecificationManager.hpp"
#include "mainInterface/ProblemManager.hpp"
#include "mesh/ElementType.hpp"
#include "mesh/mpiCommunications/CommunicationTools.hpp"

namespace geosx
{

using namespace dataRepository;

ElasticWaveEquationSEM::ElasticWaveEquationSEM( const std::string & name,
                                                Group * const parent ):

  WaveSolverBase( name,
                  parent )
{

  registerWrapper( viewKeyStruct::sourceNodeIdsString(), &m_sourceNodeIds ).
    setInputFlag( InputFlags::FALSE ).
    setSizedFromParent( 0 ).
    setDescription( "Indices of the nodes (in the right order) for each source point" );

  registerWrapper( viewKeyStruct::sourceConstantsString(), &m_sourceConstantsx ).
    setInputFlag( InputFlags::FALSE ).
    setSizedFromParent( 0 ).
    setDescription( "Constant part of the source for the nodes listed in m_sourceNodeIds in x-direction" );

  registerWrapper( viewKeyStruct::sourceConstantsString(), &m_sourceConstantsy ).
    setInputFlag( InputFlags::FALSE ).
    setSizedFromParent( 0 ).
    setDescription( "Constant part of the source for the nodes listed in m_sourceNodeIds in y-direction" );

  registerWrapper( viewKeyStruct::sourceConstantsString(), &m_sourceConstantsz ).
    setInputFlag( InputFlags::FALSE ).
    setSizedFromParent( 0 ).
    setDescription( "Constant part of the source for the nodes listed in m_sourceNodeIds in z-direction" );

  registerWrapper( viewKeyStruct::sourceIsAccessibleString(), &m_sourceIsAccessible ).
    setInputFlag( InputFlags::FALSE ).
    setSizedFromParent( 0 ).
    setDescription( "Flag that indicates whether the source is accessible to this MPI rank" );

  registerWrapper( viewKeyStruct::receiverNodeIdsString(), &m_receiverNodeIds ).
    setInputFlag( InputFlags::FALSE ).
    setSizedFromParent( 0 ).
    setDescription( "Indices of the nodes (in the right order) for each receiver point" );

  registerWrapper( viewKeyStruct::sourceConstantsString(), &m_receiverConstants ).
    setInputFlag( InputFlags::FALSE ).
    setSizedFromParent( 0 ).
    setDescription( "Constant part of the receiver for the nodes listed in m_receiverNodeIds" );

  registerWrapper( viewKeyStruct::receiverIsLocalString(), &m_receiverIsLocal ).
    setInputFlag( InputFlags::FALSE ).
    setSizedFromParent( 0 ).
    setDescription( "Flag that indicates whether the receiver is local to this MPI rank" );

  registerWrapper( viewKeyStruct::displacementXNp1AtReceiversString(), &m_displacementXNp1AtReceivers ).
    setInputFlag( InputFlags::FALSE ).
    setSizedFromParent( 0 ).
    setDescription( "Displacement value at each receiver for each timestep (x-component)" );

  registerWrapper( viewKeyStruct::displacementYNp1AtReceiversString(), &m_displacementYNp1AtReceivers ).
    setInputFlag( InputFlags::FALSE ).
    setSizedFromParent( 0 ).
    setDescription( "Displacement value at each receiver for each timestep (y-component)" );

  registerWrapper( viewKeyStruct::displacementZNp1AtReceiversString(), &m_displacementZNp1AtReceivers ).
    setInputFlag( InputFlags::FALSE ).
    setSizedFromParent( 0 ).
    setDescription( "Displacement value at each receiver for each timestep (z-component)" );

}

ElasticWaveEquationSEM::~ElasticWaveEquationSEM()
{
  // TODO Auto-generated destructor stub
}

void ElasticWaveEquationSEM::initializePreSubGroups()
{

  WaveSolverBase::initializePreSubGroups();

  DomainPartition & domain = this->getGroupByPath< DomainPartition >( "/Problem/domain" );

  NumericalMethodsManager const & numericalMethodManager = domain.getNumericalMethodManager();

  FiniteElementDiscretizationManager const &
  feDiscretizationManager = numericalMethodManager.getFiniteElementDiscretizationManager();

  FiniteElementDiscretization const * const
  feDiscretization = feDiscretizationManager.getGroupPointer< FiniteElementDiscretization >( m_discretizationName );
  GEOSX_THROW_IF( feDiscretization == nullptr,
                  getName() << ": FE discretization not found: " << m_discretizationName,
                  InputError );


}


void ElasticWaveEquationSEM::registerDataOnMesh( Group & meshBodies )
{

  forDiscretizationOnMeshTargets( meshBodies, [&] ( string const &,
                                                    MeshLevel & mesh,
                                                    arrayView1d< string const > const & )
  {
    NodeManager & nodeManager = mesh.getNodeManager();

    nodeManager.registerExtrinsicData< extrinsicMeshData::Displacementx_nm1,
                                       extrinsicMeshData::Displacementy_nm1,
                                       extrinsicMeshData::Displacementz_nm1,
                                       extrinsicMeshData::Displacementx_n,
                                       extrinsicMeshData::Displacementy_n,
                                       extrinsicMeshData::Displacementz_n,
                                       extrinsicMeshData::Displacementx_np1,
                                       extrinsicMeshData::Displacementy_np1,
                                       extrinsicMeshData::Displacementz_np1,
                                       extrinsicMeshData::ForcingRHSx,
                                       extrinsicMeshData::ForcingRHSy,
                                       extrinsicMeshData::ForcingRHSz,
                                       extrinsicMeshData::MassVector,
                                       extrinsicMeshData::DampingVectorx,
                                       extrinsicMeshData::DampingVectory,
                                       extrinsicMeshData::DampingVectorz,
                                       extrinsicMeshData::StiffnessVectorx,
                                       extrinsicMeshData::StiffnessVectory,
                                       extrinsicMeshData::StiffnessVectorz,
                                       extrinsicMeshData::FreeSurfaceNodeIndicator >( this->getName() );

    FaceManager & faceManager = mesh.getFaceManager();
    faceManager.registerExtrinsicData< extrinsicMeshData::FreeSurfaceFaceIndicator >( this->getName() );

    ElementRegionManager & elemManager = mesh.getElemManager();

    elemManager.forElementSubRegions< CellElementSubRegion >( [&]( CellElementSubRegion & subRegion )
    {
      subRegion.registerExtrinsicData< extrinsicMeshData::MediumVelocityVp >( this->getName() );
      subRegion.registerExtrinsicData< extrinsicMeshData::MediumVelocityVs >( this->getName() );
      subRegion.registerExtrinsicData< extrinsicMeshData::MediumDensity >( this->getName() );
    } );

  } );
}



void ElasticWaveEquationSEM::postProcessInput()
{

  GEOSX_ERROR_IF( m_sourceCoordinates.size( 1 ) != 3,
                  "Invalid number of physical coordinates for the sources" );

  GEOSX_ERROR_IF( m_receiverCoordinates.size( 1 ) != 3,
                  "Invalid number of physical coordinates for the receivers" );

  EventManager const & event = this->getGroupByPath< EventManager >( "/Problem/Events" );
  real64 const & maxTime = event.getReference< real64 >( EventManager::viewKeyStruct::maxTimeString() );
  real64 dt = 0;
  for( localIndex numSubEvent = 0; numSubEvent < event.numSubGroups(); ++numSubEvent )
  {
    EventBase const * subEvent = static_cast< EventBase const * >( event.getSubGroups()[numSubEvent] );
    if( subEvent->getEventName() == "/Solvers/" + this->getName() )
    {
      dt = subEvent->getReference< real64 >( EventBase::viewKeyStruct::forceDtString() );
    }
  }

  GEOSX_THROW_IF( dt < epsilonLoc*maxTime, "Value for dt: " << dt <<" is smaller than local threshold: " << epsilonLoc, std::runtime_error );

  if( m_dtSeismoTrace > 0 )
  {
    m_nsamplesSeismoTrace = int( maxTime / m_dtSeismoTrace) + 1;
  }
  else
  {
    m_nsamplesSeismoTrace = 0;
  }
  localIndex const nsamples = int(maxTime/dt) + 1;

  localIndex numNodesPerElem = 8;

  localIndex const numSourcesGlobal = m_sourceCoordinates.size( 0 );
  m_sourceNodeIds.resize( numSourcesGlobal, numNodesPerElem );
  m_sourceConstantsx.resize( numSourcesGlobal, numNodesPerElem );
  m_sourceConstantsy.resize( numSourcesGlobal, numNodesPerElem );
  m_sourceConstantsz.resize( numSourcesGlobal, numNodesPerElem );
  m_sourceIsAccessible.resize( numSourcesGlobal );


  localIndex const numReceiversGlobal = m_receiverCoordinates.size( 0 );
  m_receiverNodeIds.resize( numReceiversGlobal, numNodesPerElem );
  m_receiverConstants.resize( numReceiversGlobal, numNodesPerElem );
  m_receiverIsLocal.resize( numReceiversGlobal );

  m_displacementXNp1AtReceivers.resize( m_nsamplesSeismoTrace, numReceiversGlobal );
  m_displacementYNp1AtReceivers.resize( m_nsamplesSeismoTrace, numReceiversGlobal );
  m_displacementZNp1AtReceivers.resize( m_nsamplesSeismoTrace, numReceiversGlobal );
  m_sourceValue.resize( nsamples, numSourcesGlobal );


}


void ElasticWaveEquationSEM::precomputeSourceAndReceiverTerm( MeshLevel & mesh, arrayView1d< string const > const & regionNames )
{
  NodeManager const & nodeManager = mesh.getNodeManager();
  FaceManager const & faceManager = mesh.getFaceManager();

  arrayView2d< real64 const, nodes::REFERENCE_POSITION_USD > const X =
    nodeManager.referencePosition().toViewConst();
  arrayView2d< real64 const > const faceNormal  = faceManager.faceNormal();
  arrayView2d< real64 const > const faceCenter  = faceManager.faceCenter();


  arrayView2d< real64 const > const sourceCoordinates = m_sourceCoordinates.toViewConst();
  arrayView2d< localIndex > const sourceNodeIds = m_sourceNodeIds.toView();
  arrayView2d< real64 > const sourceConstantsx = m_sourceConstantsx.toView();
  arrayView2d< real64 > const sourceConstantsy = m_sourceConstantsy.toView();
  arrayView2d< real64 > const sourceConstantsz = m_sourceConstantsz.toView();
  arrayView1d< localIndex > const sourceIsAccessible = m_sourceIsAccessible.toView();
  sourceNodeIds.setValues< EXEC_POLICY >( -1 );
  sourceConstantsx.setValues< EXEC_POLICY >( -1 );
  sourceConstantsy.setValues< EXEC_POLICY >( -1 );
  sourceConstantsz.setValues< EXEC_POLICY >( -1 );
  sourceIsAccessible.zero();

  arrayView2d< real64 const > const receiverCoordinates = m_receiverCoordinates.toViewConst();
  arrayView2d< localIndex > const receiverNodeIds = m_receiverNodeIds.toView();
  arrayView2d< real64 > const receiverConstants = m_receiverConstants.toView();
  arrayView1d< localIndex > const receiverIsLocal = m_receiverIsLocal.toView();
  receiverNodeIds.setValues< EXEC_POLICY >( -1 );
  receiverConstants.setValues< EXEC_POLICY >( -1 );
  receiverIsLocal.zero();

  real32 const timeSourceFrequency = this->m_timeSourceFrequency;
  localIndex const rickerOrder = this->m_rickerOrder;
  arrayView2d< real32 > const sourceValue = m_sourceValue.toView();
  real64 dt = 0;
  EventManager const & event = this->getGroupByPath< EventManager >( "/Problem/Events" );
  for( localIndex numSubEvent = 0; numSubEvent < event.numSubGroups(); ++numSubEvent )
  {
    EventBase const * subEvent = static_cast< EventBase const * >( event.getSubGroups()[numSubEvent] );
    if( subEvent->getEventName() == "/Solvers/" + this->getName() )
    {
      dt = subEvent->getReference< real64 >( EventBase::viewKeyStruct::forceDtString() );
    }
  }

  mesh.getElemManager().forElementSubRegions< CellElementSubRegion >( regionNames, [&]( localIndex const,
                                                                                        CellElementSubRegion & elementSubRegion )
  {

    GEOSX_THROW_IF( elementSubRegion.getElementType() != ElementType::Hexahedron,
                    "Invalid type of element, the elastic solver is designed for hexahedral meshes only (C3D8) ",
                    InputError );

    arrayView2d< localIndex const > const elemsToFaces = elementSubRegion.faceList();
    arrayView2d< localIndex const, cells::NODE_MAP_USD > const & elemsToNodes = elementSubRegion.nodeList();
    arrayView2d< real64 const > const elemCenter = elementSubRegion.getElementCenter();
    arrayView1d< integer const > const elemGhostRank = elementSubRegion.ghostRank();

    finiteElement::FiniteElementBase const &
    fe = elementSubRegion.getReference< finiteElement::FiniteElementBase >( getDiscretizationName() );
    finiteElement::dispatch3D( fe,
                               [&]
                                 ( auto const finiteElement )
    {
      using FE_TYPE = TYPEOFREF( finiteElement );

      localIndex const numFacesPerElem = elementSubRegion.numFacesPerElement();

      elasticWaveEquationSEMKernels::
        PrecomputeSourceAndReceiverKernel::
        launch< EXEC_POLICY, FE_TYPE >
        ( elementSubRegion.size(),
        numFacesPerElem,
        X,
        elemGhostRank,
        elemsToNodes,
        elemsToFaces,
        elemCenter,
        faceNormal,
        faceCenter,
        sourceCoordinates,
        sourceIsAccessible,
        sourceNodeIds,
        sourceConstantsx,
        sourceConstantsy,
        sourceConstantsz,
        receiverCoordinates,
        receiverIsLocal,
        receiverNodeIds,
        receiverConstants,
        sourceValue,
        dt,
        timeSourceFrequency,
        rickerOrder );
    } );
  } );

}


void ElasticWaveEquationSEM::addSourceToRightHandSide( integer const & cycleNumber, arrayView1d< real32 > const rhsx, arrayView1d< real32 > const rhsy, arrayView1d< real32 > const rhsz )
{
  arrayView2d< localIndex const > const sourceNodeIds = m_sourceNodeIds.toViewConst();
  arrayView2d< real64 const > const sourceConstantsx   = m_sourceConstantsx.toViewConst();
  arrayView2d< real64 const > const sourceConstantsy   = m_sourceConstantsy.toViewConst();
  arrayView2d< real64 const > const sourceConstantsz   = m_sourceConstantsz.toViewConst();
  arrayView1d< localIndex const > const sourceIsAccessible = m_sourceIsAccessible.toViewConst();
  arrayView2d< real32 const > const sourceValue   = m_sourceValue.toViewConst();

  GEOSX_THROW_IF( cycleNumber > sourceValue.size( 0 ), "Too many steps compared to array size", std::runtime_error );
  forAll< EXEC_POLICY >( m_sourceConstantsx.size( 0 ), [=] GEOSX_HOST_DEVICE ( localIndex const isrc )
  {
    if( sourceIsAccessible[isrc] == 1 )
    {
      for( localIndex inode = 0; inode < sourceConstantsx.size( 1 ); ++inode )
      {
        real32 const localIncrementx = sourceConstantsx[isrc][inode] * sourceValue[cycleNumber][isrc];
        RAJA::atomicAdd< ATOMIC_POLICY >( &rhsx[sourceNodeIds[isrc][inode]], localIncrementx );
        real32 const localIncrementy = sourceConstantsy[isrc][inode] * sourceValue[cycleNumber][isrc];
        RAJA::atomicAdd< ATOMIC_POLICY >( &rhsy[sourceNodeIds[isrc][inode]], localIncrementy );
        real32 const localIncrementz = sourceConstantsz[isrc][inode] * sourceValue[cycleNumber][isrc];
        RAJA::atomicAdd< ATOMIC_POLICY >( &rhsz[sourceNodeIds[isrc][inode]], localIncrementz );
      }
    }
  } );
}

void ElasticWaveEquationSEM::computeSeismoTrace( real64 const time_n,
                                                 real64 const dt,
                                                 real64 const timeSeismo,
                                                 localIndex iSeismo,
                                                 arrayView1d< real32 const > const var_np1,
                                                 arrayView1d< real32 const > const var_n,
                                                 arrayView2d< real32 > varAtReceivers )
{
  real64 const time_np1 = time_n+dt;
  arrayView2d< localIndex const > const receiverNodeIds = m_receiverNodeIds.toViewConst();
  arrayView2d< real64 const > const receiverConstants   = m_receiverConstants.toViewConst();
  arrayView1d< localIndex const > const receiverIsLocal = m_receiverIsLocal.toViewConst();

  real32 const a1 = (dt < epsilonLoc) ? 1.0 : (time_np1 - timeSeismo)/dt;
  real32 const a2 = 1.0 - a1;

  if( m_nsamplesSeismoTrace > 0 )
  {
    forAll< EXEC_POLICY >( receiverConstants.size( 0 ), [=] GEOSX_HOST_DEVICE ( localIndex const ircv )
    {
      if( receiverIsLocal[ircv] == 1 )
      {
        varAtReceivers[iSeismo][ircv] = 0.0;
        real32 vtmp_np1 = 0.0;
        real32 vtmp_n = 0.0;
        for( localIndex inode = 0; inode < receiverConstants.size( 1 ); ++inode )
        {
          vtmp_np1 += var_np1[receiverNodeIds[ircv][inode]] * receiverConstants[ircv][inode];
          vtmp_n += var_n[receiverNodeIds[ircv][inode]] * receiverConstants[ircv][inode];
        }
        // linear interpolation between the pressure value at time_n and time_(n+1)
        varAtReceivers[iSeismo][ircv] = a1*vtmp_n + a2*vtmp_np1;
      }
    } );
  }

  // TODO DEBUG: the following output is only temporary until our wave propagation kernels are finalized.
  // Output will then only be done via the previous code.
  if( iSeismo == m_nsamplesSeismoTrace - 1 )
  {
    forAll< serialPolicy >( receiverConstants.size( 0 ), [=] ( localIndex const ircv )
    {
      if( this->m_outputSeismoTrace == 1 )
      {
        if( receiverIsLocal[ircv] == 1 )
        {
          for( localIndex iSample = 0; iSample < m_nsamplesSeismoTrace; ++iSample )
          {
            this->saveSeismo( iSample, varAtReceivers[iSample][ircv], GEOSX_FMT( "seismoTraceReceiver{:03}.txt", ircv ) );
          }
        }
      }
    } );
  }

}

/// Use for now until we get the same functionality in TimeHistory
/// TODO: move implementation into WaveSolverBase
void ElasticWaveEquationSEM::saveSeismo( localIndex const iSeismo, real32 const val, string const & filename )
{
  std::ofstream f( filename, std::ios::app );
  f<< iSeismo << " " << val << std::endl;
  f.close();
}



void ElasticWaveEquationSEM::initializePostInitialConditionsPreSubGroups()
{

  WaveSolverBase::initializePostInitialConditionsPreSubGroups();

  DomainPartition & domain = this->getGroupByPath< DomainPartition >( "/Problem/domain" );

  real64 const time = 0.0;
  applyFreeSurfaceBC( time, domain );

  forDiscretizationOnMeshTargets( domain.getMeshBodies(), [&] ( string const &,
                                                                MeshLevel & mesh,
                                                                arrayView1d< string const > const & regionNames )
  {
    precomputeSourceAndReceiverTerm( mesh, regionNames );

    NodeManager & nodeManager = mesh.getNodeManager();
    FaceManager & faceManager = mesh.getFaceManager();

    arrayView2d< real64 const, nodes::REFERENCE_POSITION_USD > const X = nodeManager.referencePosition().toViewConst();

    /// Get table containing all the face normals
    arrayView2d< real64 const > const faceNormal  = faceManager.faceNormal();
    ArrayOfArraysView< localIndex const > const facesToNodes = faceManager.nodeList().toViewConst();

    arrayView1d< integer > const & facesDomainBoundaryIndicator = faceManager.getDomainBoundaryIndicator();
    arrayView1d< localIndex const > const freeSurfaceFaceIndicator = faceManager.getExtrinsicData< extrinsicMeshData::FreeSurfaceFaceIndicator >();

    arrayView1d< real32 > const mass = nodeManager.getExtrinsicData< extrinsicMeshData::MassVector >();

    arrayView1d< real32 > const dampingx = nodeManager.getExtrinsicData< extrinsicMeshData::DampingVectorx >();
    arrayView1d< real32 > const dampingy = nodeManager.getExtrinsicData< extrinsicMeshData::DampingVectory >();
    arrayView1d< real32 > const dampingz = nodeManager.getExtrinsicData< extrinsicMeshData::DampingVectorz >();

    mass.zero();

    dampingx.zero();
    dampingy.zero();
    dampingz.zero();

    mesh.getElemManager().forElementSubRegions< CellElementSubRegion >( regionNames, [&]( localIndex const,
                                                                                          CellElementSubRegion & elementSubRegion )
    {

      arrayView2d< localIndex const, cells::NODE_MAP_USD > const & elemsToNodes = elementSubRegion.nodeList();
      arrayView2d< localIndex const > const elemsToFaces = elementSubRegion.faceList();

      arrayView1d< real32 > const density = elementSubRegion.getExtrinsicData< extrinsicMeshData::MediumDensity >();
      arrayView1d< real32 > const velocityVp = elementSubRegion.getExtrinsicData< extrinsicMeshData::MediumVelocityVp >();
      arrayView1d< real32 > const velocityVs = elementSubRegion.getExtrinsicData< extrinsicMeshData::MediumVelocityVs >();

      finiteElement::FiniteElementBase const &
      fe = elementSubRegion.getReference< finiteElement::FiniteElementBase >( getDiscretizationName() );
      finiteElement::dispatch3D( fe,
                                 [&]
                                   ( auto const finiteElement )
      {
        using FE_TYPE = TYPEOFREF( finiteElement );

        localIndex const numFacesPerElem = elementSubRegion.numFacesPerElement();
        localIndex const numNodesPerFace = facesToNodes.sizeOfArray( 0 );

        elasticWaveEquationSEMKernels::MassAndDampingMatrixKernel< FE_TYPE > kernel( finiteElement );
        kernel.template launch< EXEC_POLICY, ATOMIC_POLICY >( elementSubRegion.size(),
                                                              numFacesPerElem,
                                                              numNodesPerFace,
                                                              X,
                                                              elemsToNodes,
                                                              elemsToFaces,
                                                              facesToNodes,
                                                              facesDomainBoundaryIndicator,
                                                              freeSurfaceFaceIndicator,
                                                              faceNormal,
                                                              density,
                                                              velocityVp,
                                                              velocityVs,
                                                              dampingx,
                                                              dampingy,
                                                              dampingz,
                                                              mass );
      } );
    } );
  } );
}


void ElasticWaveEquationSEM::applyFreeSurfaceBC( real64 const time, DomainPartition & domain )
{
  FieldSpecificationManager & fsManager = FieldSpecificationManager::getInstance();
  FunctionManager const & functionManager = FunctionManager::getInstance();

  FaceManager & faceManager = domain.getMeshBody( 0 ).getMeshLevel( m_discretizationName ).getFaceManager();
  NodeManager & nodeManager = domain.getMeshBody( 0 ).getMeshLevel( m_discretizationName ).getNodeManager();

  arrayView1d< real32 > const ux_np1 = nodeManager.getExtrinsicData< extrinsicMeshData::Displacementx_np1 >();
  arrayView1d< real32 > const uy_np1 = nodeManager.getExtrinsicData< extrinsicMeshData::Displacementy_np1 >();
  arrayView1d< real32 > const uz_np1 = nodeManager.getExtrinsicData< extrinsicMeshData::Displacementz_np1 >();
  arrayView1d< real32 > const ux_n = nodeManager.getExtrinsicData< extrinsicMeshData::Displacementx_n >();
  arrayView1d< real32 > const uy_n = nodeManager.getExtrinsicData< extrinsicMeshData::Displacementy_n >();
  arrayView1d< real32 > const uz_n = nodeManager.getExtrinsicData< extrinsicMeshData::Displacementz_n >();
  arrayView1d< real32 > const ux_nm1 = nodeManager.getExtrinsicData< extrinsicMeshData::Displacementx_nm1 >();
  arrayView1d< real32 > const uy_nm1 = nodeManager.getExtrinsicData< extrinsicMeshData::Displacementy_nm1 >();
  arrayView1d< real32 > const uz_nm1 = nodeManager.getExtrinsicData< extrinsicMeshData::Displacementz_nm1 >();



  ArrayOfArraysView< localIndex const > const faceToNodeMap = faceManager.nodeList().toViewConst();

  /// set array of indicators: 1 if a face is on on free surface; 0 otherwise
  arrayView1d< localIndex > const freeSurfaceFaceIndicator = faceManager.getExtrinsicData< extrinsicMeshData::FreeSurfaceFaceIndicator >();

  /// set array of indicators: 1 if a node is on on free surface; 0 otherwise
  arrayView1d< localIndex > const freeSurfaceNodeIndicator = nodeManager.getExtrinsicData< extrinsicMeshData::FreeSurfaceNodeIndicator >();


  freeSurfaceFaceIndicator.zero();
  freeSurfaceNodeIndicator.zero();

  fsManager.apply( time,
                   domain.getMeshBody( 0 ).getMeshLevel( m_discretizationName ),
                   string( "FreeSurface" ),
                   [&]( FieldSpecificationBase const & bc,
                        string const &,
                        SortedArrayView< localIndex const > const & targetSet,
                        Group &,
                        string const & )
  {
    string const & functionName = bc.getFunctionName();

    if( functionName.empty() || functionManager.getGroup< FunctionBase >( functionName ).isFunctionOfTime() == 2 )
    {
      real64 const value = bc.getScale();

      for( localIndex i = 0; i < targetSet.size(); ++i )
      {
        localIndex const kf = targetSet[ i ];
        freeSurfaceFaceIndicator[kf] = 1;

        localIndex const numNodes = faceToNodeMap.sizeOfArray( kf );
        for( localIndex a=0; a < numNodes; ++a )
        {
          localIndex const dof = faceToNodeMap( kf, a );
          freeSurfaceNodeIndicator[dof] = 1;

          ux_np1[dof] = value;
          uy_np1[dof] = value;
          uz_np1[dof] = value;
          ux_n[dof] = value;
          uy_n[dof] = value;
          uz_n[dof] = value;
          ux_nm1[dof] = value;
          uy_nm1[dof] = value;
          uz_nm1[dof] = value;
        }
      }
    }
    else
    {
      GEOSX_ERROR( "This option is not supported yet" );
    }
  } );
}

real64 ElasticWaveEquationSEM::solverStep( real64 const & time_n,
                                           real64 const & dt,
                                           integer const cycleNumber,
                                           DomainPartition & domain )
{
  return explicitStep( time_n, dt, cycleNumber, domain );
}

real64 ElasticWaveEquationSEM::explicitStep( real64 const & time_n,
                                             real64 const & dt,
                                             integer const cycleNumber,
                                             DomainPartition & domain )
{
  GEOSX_MARK_FUNCTION;

  GEOSX_UNUSED_VAR( time_n, dt, cycleNumber );

  GEOSX_LOG_RANK_0_IF( dt < epsilonLoc, "Warning! Value for dt: " << dt << "s is smaller than local threshold: " << epsilonLoc );

  forDiscretizationOnMeshTargets( domain.getMeshBodies(), [&] ( string const &,
                                                                MeshLevel & mesh,
                                                                arrayView1d< string const > const & regionNames )
  {
    NodeManager & nodeManager = mesh.getNodeManager();

    arrayView1d< real32 const > const mass = nodeManager.getExtrinsicData< extrinsicMeshData::MassVector >();
    arrayView1d< real32 const > const dampingx = nodeManager.getExtrinsicData< extrinsicMeshData::DampingVectorx >();
    arrayView1d< real32 const > const dampingy = nodeManager.getExtrinsicData< extrinsicMeshData::DampingVectory >();
    arrayView1d< real32 const > const dampingz = nodeManager.getExtrinsicData< extrinsicMeshData::DampingVectorz >();
    arrayView1d< real32 > const stiffnessVectorx = nodeManager.getExtrinsicData< extrinsicMeshData::StiffnessVectorx >();
    arrayView1d< real32 > const stiffnessVectory = nodeManager.getExtrinsicData< extrinsicMeshData::StiffnessVectory >();
    arrayView1d< real32 > const stiffnessVectorz = nodeManager.getExtrinsicData< extrinsicMeshData::StiffnessVectorz >();


    arrayView1d< real32 > const ux_nm1 = nodeManager.getExtrinsicData< extrinsicMeshData::Displacementx_nm1 >();
    arrayView1d< real32 > const uy_nm1 = nodeManager.getExtrinsicData< extrinsicMeshData::Displacementy_nm1 >();
    arrayView1d< real32 > const uz_nm1 = nodeManager.getExtrinsicData< extrinsicMeshData::Displacementz_nm1 >();
    arrayView1d< real32 > const ux_n = nodeManager.getExtrinsicData< extrinsicMeshData::Displacementx_n >();
    arrayView1d< real32 > const uy_n = nodeManager.getExtrinsicData< extrinsicMeshData::Displacementy_n >();
    arrayView1d< real32 > const uz_n = nodeManager.getExtrinsicData< extrinsicMeshData::Displacementz_n >();
    arrayView1d< real32 > const ux_np1 = nodeManager.getExtrinsicData< extrinsicMeshData::Displacementx_np1 >();
    arrayView1d< real32 > const uy_np1 = nodeManager.getExtrinsicData< extrinsicMeshData::Displacementy_np1 >();
    arrayView1d< real32 > const uz_np1 = nodeManager.getExtrinsicData< extrinsicMeshData::Displacementz_np1 >();


    /// get array of indicators: 1 if node on free surface; 0 otherwise
    arrayView1d< localIndex const > const freeSurfaceNodeIndicator = nodeManager.getExtrinsicData< extrinsicMeshData::FreeSurfaceNodeIndicator >();

    arrayView1d< real32 > const rhsx = nodeManager.getExtrinsicData< extrinsicMeshData::ForcingRHSx >();
    arrayView1d< real32 > const rhsy = nodeManager.getExtrinsicData< extrinsicMeshData::ForcingRHSy >();
    arrayView1d< real32 > const rhsz = nodeManager.getExtrinsicData< extrinsicMeshData::ForcingRHSz >();

    auto kernelFactory = elasticWaveEquationSEMKernels::ExplicitElasticSEMFactory( dt );

    finiteElement::
      regionBasedKernelApplication< EXEC_POLICY,
                                    constitutive::NullModel,
                                    CellElementSubRegion >( mesh,
                                                            regionNames,
                                                            getDiscretizationName(),
                                                            "",
                                                            kernelFactory );


    addSourceToRightHandSide( cycleNumber, rhsx, rhsy, rhsz );



    real64 dt2 = dt*dt;
    forAll< EXEC_POLICY >( nodeManager.size(), [=] GEOSX_HOST_DEVICE ( localIndex const a )
    {
      if( freeSurfaceNodeIndicator[a]!=1 )
      {
        ux_np1[a] = ux_n[a];
        ux_np1[a] *= 2.0*mass[a];
        ux_np1[a] -= (mass[a]-0.5*dt*dampingx[a])*ux_nm1[a];
        ux_np1[a] += dt2*(rhsx[a]-stiffnessVectorx[a]);
        ux_np1[a] /= mass[a]+0.5*dt*dampingx[a];
        uy_np1[a] = uy_n[a];
        uy_np1[a] *= 2.0*mass[a];
        uy_np1[a] -= (mass[a]-0.5*dt*dampingy[a])*uy_nm1[a];
        uy_np1[a] += dt2*(rhsy[a]-stiffnessVectory[a]);
        uy_np1[a] /= mass[a]+0.5*dt*dampingy[a];
        uz_np1[a] = uz_n[a];
        uz_np1[a] *= 2.0*mass[a];
        uz_np1[a] -= (mass[a]-0.5*dt*dampingz[a])*uz_nm1[a];
        uz_np1[a] += dt2*(rhsz[a]-stiffnessVectorz[a]);
        uz_np1[a] /= mass[a]+0.5*dt*dampingz[a];
      }
    } );

    /// synchronize pressure fields
    FieldIdentifiers fieldsToBeSync;
    fieldsToBeSync.addFields( FieldLocation::Node, { extrinsicMeshData::Displacementx_np1::key(), extrinsicMeshData::Displacementy_np1::key(), extrinsicMeshData::Displacementz_np1::key() } );

    CommunicationTools & syncFields = CommunicationTools::getInstance();
    syncFields.synchronizeFields( fieldsToBeSync,
                                  domain.getMeshBody( 0 ).getMeshLevel( m_discretizationName ),
                                  domain.getNeighbors(),
                                  true );

    // compute the seismic traces since last step.
    arrayView2d< real32 > const uXReceivers   = m_displacementXNp1AtReceivers.toView();
    arrayView2d< real32 > const uYReceivers   = m_displacementYNp1AtReceivers.toView();
    arrayView2d< real32 > const uZReceivers   = m_displacementZNp1AtReceivers.toView();

    computeAllSeismoTraces( time_n, dt, ux_np1, ux_n, uXReceivers );
    computeAllSeismoTraces( time_n, dt, uy_np1, uy_n, uYReceivers );
    computeAllSeismoTraces( time_n, dt, uz_np1, uz_n, uZReceivers );

    forAll< EXEC_POLICY >( nodeManager.size(), [=] GEOSX_HOST_DEVICE ( localIndex const a )
    {
      ux_nm1[a] = ux_n[a];
      uy_nm1[a] = uy_n[a];
      uz_nm1[a] = uz_n[a];
      ux_n[a] = ux_np1[a];
      uy_n[a] = uy_np1[a];
      uz_n[a] = uz_np1[a];

      stiffnessVectorx[a] = 0.0;
      stiffnessVectory[a] = 0.0;
      stiffnessVectorz[a] = 0.0;
      rhsx[a] = 0.0;
      rhsy[a] = 0.0;
      rhsz[a] = 0.0;
    } );

    // increment m_indexSeismoTrace
    while( (m_dtSeismoTrace*m_indexSeismoTrace) <= (time_n + epsilonLoc) && m_indexSeismoTrace < m_nsamplesSeismoTrace )
    {
      m_indexSeismoTrace++;
    }

  } );
  return dt;

}

void ElasticWaveEquationSEM::cleanup( real64 const time_n,
                                      integer const cycleNumber,
                                      integer const eventCounter,
                                      real64 const eventProgress,
                                      DomainPartition & domain )
{
  // call the base class cleanup (for reporting purposes)
  SolverBase::cleanup( time_n, cycleNumber, eventCounter, eventProgress, domain );

  // compute the remaining seismic traces, if needed
  forDiscretizationOnMeshTargets( domain.getMeshBodies(), [&] ( string const &,
                                                                MeshLevel & mesh,
                                                                arrayView1d< string const > const & )
  {
    NodeManager & nodeManager = mesh.getNodeManager();
    arrayView1d< real32 const > const ux_n = nodeManager.getExtrinsicData< extrinsicMeshData::Displacementx_n >();
    arrayView1d< real32 const > const ux_np1 = nodeManager.getExtrinsicData< extrinsicMeshData::Displacementx_np1 >();
    arrayView1d< real32 const > const uy_n = nodeManager.getExtrinsicData< extrinsicMeshData::Displacementy_n >();
    arrayView1d< real32 const > const uy_np1 = nodeManager.getExtrinsicData< extrinsicMeshData::Displacementy_np1 >();
    arrayView1d< real32 const > const uz_n = nodeManager.getExtrinsicData< extrinsicMeshData::Displacementz_n >();
    arrayView1d< real32 const > const uz_np1 = nodeManager.getExtrinsicData< extrinsicMeshData::Displacementz_np1 >();
    arrayView2d< real32 > const uXReceivers   = m_displacementXNp1AtReceivers.toView();
    arrayView2d< real32 > const uYReceivers   = m_displacementYNp1AtReceivers.toView();
    arrayView2d< real32 > const uZReceivers   = m_displacementZNp1AtReceivers.toView();

    computeAllSeismoTraces( time_n, 0, ux_np1, ux_n, uXReceivers );
    computeAllSeismoTraces( time_n, 0, uy_np1, uy_n, uYReceivers );
    computeAllSeismoTraces( time_n, 0, uz_np1, uz_n, uZReceivers );
  } );

  // increment m_indexSeismoTrace
  while( (m_dtSeismoTrace*m_indexSeismoTrace) <= (time_n + epsilonLoc) && m_indexSeismoTrace < m_nsamplesSeismoTrace )
  {
    m_indexSeismoTrace++;
  }
}

void ElasticWaveEquationSEM::computeAllSeismoTraces( real64 const time_n,
                                                     real64 const dt,
                                                     arrayView1d< real32 const > const var_np1,
                                                     arrayView1d< real32 const > const var_n,
                                                     arrayView2d< real32 > varAtReceivers )
{
  localIndex indexSeismoTrace = m_indexSeismoTrace;

  for( real64 timeSeismo;
       (timeSeismo = m_dtSeismoTrace*indexSeismoTrace) <= (time_n + epsilonLoc) && indexSeismoTrace < m_nsamplesSeismoTrace;
       indexSeismoTrace++ )
  {
    computeSeismoTrace( time_n, dt, timeSeismo, indexSeismoTrace, var_np1, var_n, varAtReceivers );
  }
}

void ElasticWaveEquationSEM::initializePML()
{
  GEOSX_ERROR( "PML for the elastic wave propagator not yet implemented" );
}

void ElasticWaveEquationSEM::applyPML( real64 const, DomainPartition & )
{
  GEOSX_ERROR( "PML for the elastic wave propagator not yet implemented" );
}

REGISTER_CATALOG_ENTRY( SolverBase, ElasticWaveEquationSEM, string const &, dataRepository::Group * const )

} /* namespace geosx */
