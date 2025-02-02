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
 * @file MultiphasePoroelasticSolver.cpp
 */

#include "MultiphasePoromechanicsSolver.hpp"

#include "constitutive/fluid/MultiFluidBase.hpp"
#include "constitutive/solid/PorousSolid.hpp"
#include "physicsSolvers/fluidFlow/CompositionalMultiphaseBase.hpp"
#include "physicsSolvers/multiphysics/MultiphasePoromechanicsKernel.hpp"
#include "physicsSolvers/solidMechanics/SolidMechanicsLagrangianFEM.hpp"

namespace geosx
{

using namespace dataRepository;
using namespace constitutive;

MultiphasePoromechanicsSolver::MultiphasePoromechanicsSolver( const string & name,
                                                              Group * const parent )
  : Base( name, parent )
{
  m_linearSolverParameters.get().mgr.strategy = LinearSolverParameters::MGR::StrategyType::multiphasePoromechanics;
  m_linearSolverParameters.get().mgr.separateComponents = true;
  m_linearSolverParameters.get().mgr.displacementFieldName = keys::TotalDisplacement;
  m_linearSolverParameters.get().dofsPerNode = 3;
}

void MultiphasePoromechanicsSolver::registerDataOnMesh( Group & meshBodies )
{
  SolverBase::registerDataOnMesh( meshBodies );

  forDiscretizationOnMeshTargets( meshBodies, [&] ( string const &,
                                                    MeshLevel & mesh,
                                                    arrayView1d< string const > const & regionNames )
  {
    ElementRegionManager & elemManager = mesh.getElemManager();

    elemManager.forElementSubRegions< ElementSubRegionBase >( regionNames,
                                                              [&]( localIndex const,
                                                                   ElementSubRegionBase & subRegion )
    {
      subRegion.registerWrapper< string >( viewKeyStruct::porousMaterialNamesString() ).
        setPlotLevel( PlotLevel::NOPLOT ).
        setRestartFlags( RestartFlags::NO_WRITE ).
        setSizedFromParent( 0 );
    } );
  } );
}

void MultiphasePoromechanicsSolver::setupCoupling( DomainPartition const & GEOSX_UNUSED_PARAM( domain ),
                                                   DofManager & dofManager ) const
{
  dofManager.addCoupling( keys::TotalDisplacement,
                          CompositionalMultiphaseBase::viewKeyStruct::elemDofFieldString(),
                          DofManager::Connector::Elem );
}

void MultiphasePoromechanicsSolver::assembleSystem( real64 const GEOSX_UNUSED_PARAM( time ),
                                                    real64 const dt,
                                                    DomainPartition & domain,
                                                    DofManager const & dofManager,
                                                    CRSMatrixView< real64, globalIndex const > const & localMatrix,
                                                    arrayView1d< real64 > const & localRhs )
{
  GEOSX_MARK_FUNCTION;

  forDiscretizationOnMeshTargets( domain.getMeshBodies(), [&] ( string const &,
                                                                MeshLevel & mesh,
                                                                arrayView1d< string const > const & regionNames )
  {
    NodeManager const & nodeManager = mesh.getNodeManager();

    string const displacementDofKey = dofManager.getKey( dataRepository::keys::TotalDisplacement );
    arrayView1d< globalIndex const > const & displacementDofNumber = nodeManager.getReference< globalIndex_array >( displacementDofKey );

    string const flowDofKey = dofManager.getKey( CompositionalMultiphaseBase::viewKeyStruct::elemDofFieldString() );

    localIndex const numComponents = flowSolver()->numFluidComponents();
    localIndex const numPhases = flowSolver()->numFluidPhases();

    real64 const gravityVectorData[3] = LVARRAY_TENSOROPS_INIT_LOCAL_3( gravityVector() );

    poromechanicsKernels::MultiphaseKernelFactory kernelFactory( displacementDofNumber,
                                                                 flowDofKey,
                                                                 dofManager.rankOffset(),
                                                                 gravityVectorData,
                                                                 numComponents,
                                                                 numPhases,
                                                                 FlowSolverBase::viewKeyStruct::fluidNamesString(),
                                                                 localMatrix,
                                                                 localRhs );

    // Cell-based contributions
    solidMechanicsSolver()->getMaxForce() =
      finiteElement::
        regionBasedKernelApplication< parallelDevicePolicy< 32 >,
                                      constitutive::PorousSolidBase,
                                      CellElementSubRegion >( mesh,
                                                              regionNames,
                                                              solidMechanicsSolver()->getDiscretizationName(),
                                                              viewKeyStruct::porousMaterialNamesString(),
                                                              kernelFactory );
  } );

  // Face-based contributions
  flowSolver()->assembleFluxTerms( dt,
                                   domain,
                                   dofManager,
                                   localMatrix,
                                   localRhs );
}

real64 MultiphasePoromechanicsSolver::solverStep( real64 const & time_n,
                                                  real64 const & dt,
                                                  int const cycleNumber,
                                                  DomainPartition & domain )
{
  real64 dt_return = dt;

  // setup monolithic coupled system
  SolverBase::setupSystem( domain, m_dofManager, m_localMatrix, m_rhs, m_solution );

  implicitStepSetup( time_n, dt, domain );

  dt_return = nonlinearImplicitStep( time_n, dt, cycleNumber, domain );

  implicitStepComplete( time_n, dt_return, domain );

  return dt_return;
}

void MultiphasePoromechanicsSolver::updateState( DomainPartition & domain )
{
  forDiscretizationOnMeshTargets( domain.getMeshBodies(), [&] ( string const &,
                                                                MeshLevel & mesh,
                                                                arrayView1d< string const > const & regionNames )
  {
    ElementRegionManager & elemManager = mesh.getElemManager();
    elemManager.forElementSubRegions< CellElementSubRegion >( regionNames,
                                                              [&]( localIndex const,
                                                                   CellElementSubRegion & subRegion )
    {
      flowSolver()->updateFluidState( subRegion );
    } );
  } );
}

void MultiphasePoromechanicsSolver::initializePreSubGroups()
{
  SolverBase::initializePreSubGroups();

  DomainPartition & domain = this->getGroupByPath< DomainPartition >( "/Problem/domain" );

  forDiscretizationOnMeshTargets( domain.getMeshBodies(), [&] ( string const &,
                                                                MeshLevel & mesh,
                                                                arrayView1d< string const > const & regionNames )
  {
    ElementRegionManager & elementRegionManager = mesh.getElemManager();
    elementRegionManager.forElementSubRegions< ElementSubRegionBase >( regionNames,
                                                                       [&]( localIndex const,
                                                                            ElementSubRegionBase & subRegion )
    {
      string & porousName = subRegion.getReference< string >( viewKeyStruct::porousMaterialNamesString() );
      porousName = getConstitutiveName< CoupledSolidBase >( subRegion );
      GEOSX_ERROR_IF( porousName.empty(), GEOSX_FMT( "Solid model not found on subregion {}", subRegion.getName() ) );
    } );
  } );
}

REGISTER_CATALOG_ENTRY( SolverBase, MultiphasePoromechanicsSolver, string const &, Group * const )

} /* namespace geosx */
