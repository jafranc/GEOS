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
 * @file HexCellBlockManager.hpp
 */

#ifndef GEOSX_MESH_HEXCELLBLOCKMANAGER_H_
#define GEOSX_MESH_HEXCELLBLOCKMANAGER_H_

#include "mesh/generators/CellBlockManagerBase.hpp"


namespace geosx
{
  
class MeshConnectivityBuilder;

/**
 * @class HexCellBlockManager
 * @brief The HexCellBlockManager specializes CellBlockManagerBase 
 * with a lazy computation strategy and NO overallocation.
 * 
 * Only implemented for hexahedral meshes.
 * The hexahedral mesh may be structured or unstructured.
 * 
 * TODO Implement for other type of cells the MeshConnectivityBuilder
 * This class should not be modified 
 * 
 * POTENTIAL ISSUE Where are the Element indices valid in the maps NodeToElements
 * and FaceToElements? In the CellBlock? 
 * 
 */
class HexCellBlockManager : public CellBlockManagerBase
{
public:
  /**
   * @brief Constructor for HexCellBlockManager object.
   * @param name name of this instantiation of CellBlockManagerBase
   * @param parent pointer to the parent Group of this instantiation of CellBlockManagerBase
   */
  HexCellBlockManager( string const & name, Group * const parent );
  HexCellBlockManager( const HexCellBlockManager & ) = delete;
  HexCellBlockManager & operator=( const HexCellBlockManager & ) = delete;
  ~HexCellBlockManager() override;

  localIndex numEdges() const override
  { return m_numEdges; }

  localIndex numFaces() const override
  { return m_numFaces; }

  localIndex numElements() const
  { return m_numElements; }

  /**
   * @brief Initialize the mapping computations 
   * @details Does not build the maps.
   * Computations are done lazily when calling getters.
   * 
   * MUST be called before any call to the getters
   */
  void buildMaps() override;

  array2d<geosx::localIndex> getEdgeToNodes() override;
  ArrayOfSets<geosx::localIndex> getEdgeToFaces() override;
  ArrayOfArrays<localIndex> getFaceToNodes() override;
  ArrayOfArrays<geosx::localIndex> getFaceToEdges() override;
  array2d<localIndex> getFaceToElements() override;
  ArrayOfSets<localIndex> getNodeToEdges() override;
  ArrayOfSets<localIndex> getNodeToFaces() override;
  ArrayOfArrays<localIndex> getNodeToElements() override;

private:
  /// Instance of the class that build the mappings
  MeshConnectivityBuilder * m_theOneWhoDoesTheJob;

  /// Number of edges (no duplicates)
  localIndex m_numEdges = 0;
  /// Number of faces (no duplicates)
  localIndex m_numFaces = 0;
  /// Total number of cells in all managed CellBlocks (no duplicates)
  localIndex m_numElements = 0;
};

}
#endif
