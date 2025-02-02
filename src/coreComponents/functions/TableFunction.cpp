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
 * @file TableFunction.cpp
 */

#include "TableFunction.hpp"
#include "codingUtilities/Parsing.hpp"
#include "common/DataTypes.hpp"

#include <algorithm>

namespace geosx
{

using namespace dataRepository;

TableFunction::TableFunction( const string & name,
                              Group * const parent ):
  FunctionBase( name, parent ),
  m_interpolationMethod( InterpolationType::Linear ),
  m_kernelWrapper( createKernelWrapper() )
{
  registerWrapper( viewKeyStruct::coordinatesString(), &m_tableCoordinates1D ).
    setInputFlag( InputFlags::OPTIONAL ).
    setDescription( "Coordinates inputs for 1D tables" );

  registerWrapper( viewKeyStruct::valuesString(), &m_values ).
    setInputFlag( InputFlags::OPTIONAL ).
    setDescription( "Values for 1D tables" );

  registerWrapper( viewKeyStruct::coordinateFilesString(), &m_coordinateFiles ).
    setInputFlag( InputFlags::OPTIONAL ).
    setRestartFlags( RestartFlags::NO_WRITE ).
    setDescription( "List of coordinate file names for ND Table" );

  registerWrapper( viewKeyStruct::voxelFileString(), &m_voxelFile ).
    setInputFlag( InputFlags::OPTIONAL ).
    setRestartFlags( RestartFlags::NO_WRITE ).
    setDescription( "Voxel file name for ND Table" );

  registerWrapper( viewKeyStruct::interpolationString(), &m_interpolationMethod ).
    setInputFlag( InputFlags::OPTIONAL ).
    setDescription( "Interpolation method. Valid options:\n* " + EnumStrings< InterpolationType >::concat( "\n* " ) ).
    setApplyDefaultValue( m_interpolationMethod );
}

void TableFunction::readFile( string const & filename, array1d< real64 > & target )
{
  auto const skipped = []( char const c ){ return std::isspace( c ) || c == ','; };
  try
  {
    parseFile( filename, target, skipped );
  }
  catch( std::runtime_error const & e )
  {
    GEOSX_THROW( GEOSX_FMT( "{} {}: {}", catalogName(), getName(), e.what() ), InputError );
  }
}

void TableFunction::setInterpolationMethod( InterpolationType const method )
{
  m_interpolationMethod = method;
  reInitializeFunction();
}

void TableFunction::setTableCoordinates( array1d< real64_array > const & coordinates )
{
  m_coordinates.resize( 0 );
  for( localIndex i = 0; i < coordinates.size(); ++i )
  {
    for( localIndex j = 1; j < coordinates[i].size(); ++j )
    {
      GEOSX_THROW_IF( coordinates[i][j] - coordinates[i][j-1] <= 0,
                      GEOSX_FMT( "{} {}: coordinates must be strictly increasing, but axis {} is not",
                                 catalogName(), getName(), i ),
                      InputError );
    }
    m_coordinates.appendArray( coordinates[i].begin(), coordinates[i].end() );
  }
  reInitializeFunction();
}

void TableFunction::setTableValues( real64_array values )
{
  m_values = std::move( values );
  reInitializeFunction();
}

void TableFunction::initializeFunction()
{
  // Read in data
  if( m_coordinates.size() > 0 )
  {
    // This function appears to be already initialized
    // Apparently, this can be called multiple times during unit tests?
  }
  else if( m_coordinateFiles.empty() )
  {
    // 1D Table
    m_coordinates.appendArray( m_tableCoordinates1D.begin(), m_tableCoordinates1D.end() );
    GEOSX_THROW_IF_NE_MSG( m_tableCoordinates1D.size(), m_values.size(),
                           GEOSX_FMT( "{} {}: 1D table function coordinates and values must have the same length",
                                      catalogName(), getName() ),
                           InputError );
  }
  else
  {
    array1d< real64 > tmp;
    localIndex numValues = 1;
    for( localIndex ii = 0; ii < m_coordinateFiles.size(); ++ii )
    {
      tmp.clear();
      readFile( m_coordinateFiles[ii], tmp );
      m_coordinates.appendArray( tmp.begin(), tmp.end() );
      numValues *= tmp.size();
    }
    // ND Table
    m_values.reserve( numValues );
    readFile( m_voxelFile, m_values );
  }

  reInitializeFunction();
}

void TableFunction::reInitializeFunction()
{
  // Setup index increment (assume data is in Fortran array order)
  localIndex increment = 1;
  for( localIndex ii = 0; ii < m_coordinates.size(); ++ii )
  {
    increment *= m_coordinates.sizeOfArray( ii );
  }
  if( m_coordinates.size() > 0 && !m_values.empty() ) // coordinates and values have been set
  {
    GEOSX_THROW_IF_NE_MSG( increment, m_values.size(),
                           GEOSX_FMT( "{} {}: number of values does not match total number of table coordinates",
                                      catalogName(), getName() ),
                           InputError );
  }

  // Create the kernel wrapper
  m_kernelWrapper = createKernelWrapper();
}

TableFunction::KernelWrapper TableFunction::createKernelWrapper() const
{
  return { m_interpolationMethod,
           m_coordinates.toViewConst(),
           m_values.toViewConst() };
}

real64 TableFunction::evaluate( real64 const * const input ) const
{
  return m_kernelWrapper.compute( input );
}

TableFunction::KernelWrapper::KernelWrapper( InterpolationType const interpolationMethod,
                                             ArrayOfArraysView< real64 const > const & coordinates,
                                             arrayView1d< real64 const > const & values )
  :
  m_interpolationMethod( interpolationMethod ),
  m_coordinates( coordinates ),
  m_values( values )
{}

REGISTER_CATALOG_ENTRY( FunctionBase, TableFunction, string const &, Group * const )

} // end of namespace geosx
