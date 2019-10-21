/*
 * ------------------------------------------------------------------------------------------------------------
 * SPDX-License-Identifier: LGPL-2.1-only
 *
 * Copyright (c) 2018-2019 Lawrence Livermore National Security LLC
 * Copyright (c) 2018-2019 The Board of Trustees of the Leland Stanford Junior University
 * Copyright (c) 2018-2019 Total, S.A
 * Copyright (c) 2019-     GEOSX Contributors
 * All right reserved
 *
 * See top level LICENSE, COPYRIGHT, CONTRIBUTORS, NOTICE, and ACKNOWLEDGEMENTS files for details.
 * ------------------------------------------------------------------------------------------------------------
 */

#ifndef BASE_HPP
#define BASE_HPP
#define OBJECTCATALOGVERBOSE 2
#include "dataRepository/ObjectCatalog.hpp"
#include <string>

class Parameter
{
public:
  Parameter(){}
  ~Parameter(){}
  Parameter( Parameter const & source ):
    member( source.member )
  {
    GEOS_LOG( "called copy constructor for Parameter" );
  }

#if ( __cplusplus >= 201103L )
  Parameter( Parameter && source ):
    member( std::move( source.member ))
  {
    GEOS_LOG( "called move constructor for Parameter" );
  }
#endif

  double member;


};

class Base
{
public:
  Base( int junk, double const & junk2, Parameter& pbv )
  {
    GEOS_LOG( "calling Base constructor with arguments (" << junk << " " << junk2 << ")" );
  }

  ~Base()
  {
    GEOS_LOG( "calling Base destructor" );
  }

  using CatalogInterface = dataRepository::CatalogInterface< Base, int, double const &, Parameter& >;
  static CatalogInterface::CatalogType& GetCatalog()
  {
    static CatalogInterface::CatalogType catalog;
    return catalog;
  }

  virtual std::string const getName() const = 0;
};

#endif
