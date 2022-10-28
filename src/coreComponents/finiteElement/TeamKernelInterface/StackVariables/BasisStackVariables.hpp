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
 * @file BasisStackVariables.hpp
 */

#ifndef GEOSX_FINITEELEMENT_TEAMKERNELBASE_STACKVARIABLES_BASIS_HPP_
#define GEOSX_FINITEELEMENT_TEAMKERNELBASE_STACKVARIABLES_BASIS_HPP_

#include "common/DataTypes.hpp"
#include "common/GEOS_RAJA_Interface.hpp"
#include "tensor/tensor_types.hpp"
#include "common.hpp"

namespace geosx
{

namespace stackVariables
{

template < localIndex Order >
class LagrangeBasis;

template <>
class LagrangeBasis<1> : public finiteElement::LagrangeBasis1 { };


template < localIndex num_dofs_1d, localIndex num_quads_1d >
struct StackBasis
{
  GEOSX_HOST_DEVICE
  StackBasis( LaunchContext & ctx )
  {
    // basis: computation of the shape functions at quadrature points
    for ( int dof = 0; dof < num_dofs_1d; dof++ )
    {
      for ( int quad = 0; quad < num_quads_1d; quad++ )
      {
        basis[ dof ][ quad ] =
          LagrangeBasis<num_dofs_1d-1>::value(
          dof, LagrangeBasis<num_quads_1d-1>::parentSupportCoord( quad ) );
      }
    }

    // basis gradient: gradient of the shape functions at quadrature points
    for ( int dof = 0; dof < num_dofs_1d; dof++ )
    {
      for ( int quad = 0; quad < num_quads_1d; quad++ )
      {
        basis_gradient[ dof ][ quad ] =
          LagrangeBasis<num_dofs_1d-1>::gradient(
          dof, LagrangeBasis<num_quads_1d-1>::parentSupportCoord( quad ) );
      }
    }
  }

  // basis accessor
  real64 basis[num_dofs_1d][num_quads_1d];
  GEOSX_HOST_DEVICE
  real64 const ( & getValuesAtQuadPts() const )[num_dofs_1d][num_quads_1d]
  {
    return basis;
  }

  GEOSX_HOST_DEVICE
  real64 ( & getValuesAtQuadPts() )[num_dofs_1d][num_quads_1d]
  {
    return basis;
  }

  // basis gradient accessor
  real64 basis_gradient[num_dofs_1d][num_quads_1d];
  GEOSX_HOST_DEVICE
  real64 const ( & getGradientValuesAtQuadPts() const )[num_dofs_1d][num_quads_1d]
  {
    return basis_gradient;
  }

  GEOSX_HOST_DEVICE
  real64 ( & getGradientValuesAtQuadPts() )[num_dofs_1d][num_quads_1d]
  {
    return basis_gradient;
  }
};

template < localIndex num_dofs_1d, localIndex num_quads_1d >
struct SharedBasis
{
  GEOSX_HOST_DEVICE
  SharedBasis( LaunchContext & ctx )
  {
    using RAJA::RangeSegment;

    // basis: shape functions at quadrature points
    GEOSX_STATIC_SHARED real64 s_basis[num_dofs_1d][num_quads_1d];
    basis = &s_basis;
    loop<thread_z> (ctx, RangeSegment(0, 1), [&] (const int tidz)
    {
      loop<thread_y> (ctx, RangeSegment(0, num_dofs_1d), [&] (localIndex d)
      {
        loop<thread_x> (ctx, RangeSegment(0, num_quads_1d), [&] (localIndex q)
        {
          GEOSX_UNUSED_VAR( tidz );
          s_basis[ d ][ q ] =
            LagrangeBasis<num_dofs_1d-1>::value(
            d, LagrangeBasis<num_quads_1d-1>::parentSupportCoord( q ) );
        } );
      } );
    } );

    // basis gradient: gradient of the shape functions at quadrature points
    GEOSX_STATIC_SHARED real64 s_basis_gradient[num_dofs_1d][num_quads_1d];
    basis_gradient = &s_basis_gradient;
    loop<thread_z> (ctx, RangeSegment(0, 1), [&] (const int tidz)
    {
      loop<thread_y> (ctx, RangeSegment(0, num_dofs_1d), [&] (localIndex d)
      {
        loop<thread_x> (ctx, RangeSegment(0, num_quads_1d), [&] (localIndex q)
        {
          GEOSX_UNUSED_VAR( tidz );
          s_basis_gradient[ d ][ q ] =
            LagrangeBasis<num_dofs_1d-1>::gradient(
            d, LagrangeBasis<num_quads_1d-1>::parentSupportCoord( q ) );
        } );
      } );
    } );
  }

  // basis accessor
  real64 ( * basis )[num_dofs_1d][num_quads_1d];
  GEOSX_HOST_DEVICE
  real64 const ( & getValuesAtQuadPts() const )[num_dofs_1d][num_quads_1d]
  {
    return *basis;
  }

  GEOSX_HOST_DEVICE
  real64 ( & getValuesAtQuadPts() )[num_dofs_1d][num_quads_1d]
  {
    return *basis;
  }

  // basis gradient accessor
  real64 ( * basis_gradient )[num_dofs_1d][num_quads_1d];
  GEOSX_HOST_DEVICE
  real64 const ( & getGradientValuesAtQuadPts() const )[num_dofs_1d][num_quads_1d]
  {
    return *basis_gradient;
  }

  GEOSX_HOST_DEVICE
  real64 ( & getGradientValuesAtQuadPts() )[num_dofs_1d][num_quads_1d]
  {
    return *basis_gradient;
  }
};

// Generic type alias
template < Location location, localIndex num_dofs_1d, localIndex num_quads_1d >
struct Basis_t;

template < localIndex num_dofs_1d, localIndex num_quads_1d >
struct Basis_t< Location::Stack, num_dofs_1d, num_quads_1d >
{
  using type = StackBasis< num_dofs_1d, num_quads_1d >;
};

template < localIndex num_dofs_1d, localIndex num_quads_1d >
struct Basis_t< Location::Shared, num_dofs_1d, num_quads_1d >
{
  using type = SharedBasis< num_dofs_1d, num_quads_1d >;
};


template < Location location, localIndex num_dofs_1d, localIndex num_quads_1d >
using Basis = typename Basis_t< location, num_dofs_1d, num_quads_1d >::type;

} // namespace stackVariables

} // namespace geosx

#endif // GEOSX_FINITEELEMENT_TEAMKERNELBASE_STACKVARIABLES_BASIS_HPP_
