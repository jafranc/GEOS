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

#include "MeshGeneratorBase.hpp"
#include "InternalWellGenerator.hpp"
#include "mesh/MeshBody.hpp"

namespace geosx
{
using namespace dataRepository;

MeshGeneratorBase::MeshGeneratorBase( string const & name, Group * const parent ):
  Group( name, parent )
{
  setInputFlags( InputFlags::OPTIONAL_NONUNIQUE );
}

Group * MeshGeneratorBase::createChild( string const & childKey, string const & childName )
{
  GEOSX_LOG_RANK_0( "Adding Mesh attribute: " << childKey << ", " << childName );
  std::unique_ptr< InternalWellGenerator > wellGen = InternalWellGenerator::CatalogInterface::factory( childKey, childName, this );
  return &this->registerGroup< InternalWellGenerator >( childName, std::move( wellGen ) );
}

MeshGeneratorBase::CatalogInterface::CatalogType & MeshGeneratorBase::getCatalog()
{
  static MeshGeneratorBase::CatalogInterface::CatalogType catalog;
  return catalog;
}

MeshGeneratorHelper MeshGeneratorBase::generateMesh( MeshBody & meshBody ) {
  meshBody.createMeshLevel( 0 );
  MeshLevel & meshLevel = meshBody.getBaseDiscretization();
  CellBlockManager & cellBlockManager = meshBody.registerGroup< CellBlockManager >( keys::cellManager );

  MeshGeneratorHelper meshGeneratorHelper = generateCellBlockManager( cellBlockManager );

  this->generateWells( meshLevel ); // Accroche les informations des puits à l'instance du CellBlockManager crée à la ligne du dessus.
  meshBody.setGlobalLengthScale( meshGeneratorHelper.getGlobalLength() );
  return meshGeneratorHelper;
}

void MeshGeneratorBase::generateWells( MeshLevel & meshLevel ) {
  forSubGroups<InternalWellGenerator>( [&]( InternalWellGenerator & wellGen ) {
      wellGen.generateWellGeometry();
      ElementRegionManager & elemManager = meshLevel.getElemManager();
      WellElementRegion &
        wellRegion = elemManager.getGroup( ElementRegionManager::groupKeyStruct::elementRegionsGroup() ).
        getGroup< WellElementRegion >( wellGen.getWellRegionName() );
      wellRegion.setWellGeneratorName( wellGen.getName() );
      wellRegion.setWellControlsName( wellGen.getWellControlsName() );
    } );
}
}
