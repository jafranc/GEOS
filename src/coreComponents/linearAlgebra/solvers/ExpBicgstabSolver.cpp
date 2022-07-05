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
 * @file ExpBicgstabSolver.cpp
 *
 */

#include "ExpBicgstabSolver.hpp"

#include "common/Stopwatch.hpp"
#include "linearAlgebra/interfaces/InterfaceTypes.hpp"
#include "common/LinearOperator.hpp"
#include "linearAlgebra/solvers/KrylovUtils.hpp"

namespace geosx
{


template< typename VECTOR >
ExpBicgstabSolver< VECTOR >::ExpBicgstabSolver( LinearSolverParameters params,
                                                LinearOperator< Vector > const & A,
                                                LinearOperator< Vector > const & M )
  : KrylovSolver< VECTOR >( std::move( params ), A, M )
{}

template< typename VECTOR >
void ExpBicgstabSolver< VECTOR >::solve( Vector const & b,
                                         Vector & x ) const
{
  Stopwatch watch;

  // Define vectors
  VectorTemp r( x );

  // Compute initial rk
  m_operator.residual( x, b, r );

  // Compute the target absolute tolerance
  real64 const rnorm0 = r.norm2();
  real64 const absTol = rnorm0 * m_params.krylov.relTolerance;

  // Set restart iteration
  integer iRestart = 30;//m_params.krylov.maxRestart;

  // Define temporary vectors
  VectorTemp r0( r );
  VectorTemp p( r );
  VectorTemp Mp    = createTempVector( r );
  VectorTemp AMp   = createTempVector( r );
  VectorTemp Mr    = createTempVector( r );
  VectorTemp AMr   = createTempVector( r );
  VectorTemp Mq    = createTempVector( r );
  VectorTemp AMq   = createTempVector( r );
  VectorTemp MAMp  = createTempVector( r );
  VectorTemp AMAMp = createTempVector( r );
  VectorTemp MAMq  = createTempVector( r );
  VectorTemp AMAMq = createTempVector( r );

  m_precond.apply( p, Mp );
  m_operator.apply( Mp, AMp );

  real64 alpha = 1.0;

  // Initialize iteration state
  m_result.status = LinearSolverResult::Status::NotConverged;
  m_residualNorms.clear();

  integer & k = m_result.numIterations;
  for( k = 0; k <= m_params.krylov.maxIterations; ++k )
  {
    real64 const rnorm = r.norm2();
    m_residualNorms.emplace_back( rnorm );
    logProgress();

    // Convergence check on ||rk||/||b||
    if( rnorm <= absTol )
    {
      m_result.status = LinearSolverResult::Status::Success;
      break;
    }

    std::future< std::array< real64, 2 > > request = r0.iDotMultiple( r, AMp );

    m_precond.apply( r, Mr );
    m_operator.apply( Mr, AMr );

    m_precond.apply( AMp, MAMp );
    m_operator.apply( MAMp, AMAMp );

    std::array< real64, 2 > const dp = request.get();
    real64 const inner_r0_r   = dp[0];
    real64 const inner_r0_AMp = dp[1];


    // Error correction
    //
    // ... TBD

    //  Scalar operations
    alpha = inner_r0_r / inner_r0_AMp;
    // Mq  =  Mr - alpha * MAMp;
    Mq.copy( Mr );
    Mq.axpy( -alpha, MAMp );

    // AMq = AMr - alpha * AMAMp;
    AMq.copy( AMr );
    AMq.axpy( -alpha, AMAMp );

    std::future< std::array< real64, 4 > > request2 = AMq.iDotMultiple( r0, AMq, r, AMp );

    m_precond.apply( AMq, MAMq );
    m_operator.apply( MAMq, AMAMq );

    std::array< real64, 4 > const dp2 = request2.get();
    real64 const inner_r0_AMq  = dp2[0];
    real64 const inner_AMq_AMq = dp2[1];
    real64 const inner_r_AMq   = dp2[2];
    real64 const inner_AMp_AMq = dp2[3];

    real64 const inner_q_AMq = inner_r_AMq - alpha * inner_AMp_AMq;
    real64 const omega = inner_q_AMq / inner_AMq_AMq;
    real64 const inner_r0_rp1 = inner_r0_r - alpha * inner_r0_AMp - omega * inner_r0_AMq;
    real64 const beta = ( alpha / omega) * ( inner_r0_rp1 / inner_r0_r );

    // x = x + alpha *  Mp + omega*Mq;
    x.axpbypcz( alpha, Mp, omega, Mq, 1.0 );

    // r = r - alpha * AMp - omega*AMq;
    r.axpbypcz( -alpha, AMp, -omega, AMq, 1.0 );

    // Mp  =  Mq - omega* MAMq + beta*(  Mp - omega*MAMp ) ;
    Mp.axpy( -omega, MAMp );
    Mp.axpbypcz( 1.0, Mq, -omega, MAMq, beta );

    // AMp = AMq - omega*AMAMq + beta*( AMp - omega*AMAMp );
    AMp.axpy( -omega, AMAMp );
    AMp.axpbypcz( 1.0, AMq, -omega, AMAMq, beta );

    // Restart
    if( k == iRestart )
    {
      VectorTemp realResidual = createTempVector( r );
      m_operator.residual( x, b, realResidual );
      VectorTemp residualError( realResidual );
      residualError.axpy( -1.0, r );

      real64 const residualRelativeError = residualError.norm2() / realResidual.norm2();

      if( residualRelativeError > 0.1 )
      {
        r0.copy( realResidual );
        r.copy( r0 );
        p.copy( r0 );
        m_precond.apply( p, Mp );
        m_operator.apply( Mp, AMp );
      }

      iRestart = iRestart + m_params.krylov.maxRestart;
    }
  }


  m_result.residualReduction = rnorm0 > 0.0 ? m_residualNorms.back() / rnorm0 : 0.0;
  m_result.solveTime = watch.elapsedTime();
  logResult();
}

// -----------------------
// Explicit Instantiations
// -----------------------
#ifdef GEOSX_USE_TRILINOS
template class ExpBicgstabSolver< TrilinosInterface::ParallelVector >;
template class ExpBicgstabSolver< BlockVectorView< TrilinosInterface::ParallelVector > >;
#endif

#ifdef GEOSX_USE_HYPRE
template class ExpBicgstabSolver< HypreInterface::ParallelVector >;
template class ExpBicgstabSolver< BlockVectorView< HypreInterface::ParallelVector > >;
#endif

#ifdef GEOSX_USE_PETSC
template class ExpBicgstabSolver< PetscInterface::ParallelVector >;
template class ExpBicgstabSolver< BlockVectorView< PetscInterface::ParallelVector > >;
#endif

} //namespace geosx
