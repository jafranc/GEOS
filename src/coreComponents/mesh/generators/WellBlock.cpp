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
 * @file DomainPartition.cpp
 */

#include "mesh/Perforation.hpp"
#include "mesh/generators/WellBlock.hpp"
#include "mesh/generators/InternalWellGenerator.hpp"




namespace geosx
{

WellBlock::WellBlock( string const & name, Group * const parent ):
  WellBlockABC( name, parent )
{}

void WellBlock::importFieldsFromInternalWellGenerator( InternalWellGenerator const & internalWellGenerator ) {
  m_numElemsPerSegment = internalWellGenerator.getNumElementsPerSegment();
  m_minSegmentLength = internalWellGenerator.getMinSegmentLength();
  m_minElemLength = internalWellGenerator.getMinElemLength();
  m_radius = internalWellGenerator.getElementRadius();
  m_numElems = internalWellGenerator.getNumElements();
  m_elemCenterCoords = internalWellGenerator.getElemCoords();
  m_nextElemId = internalWellGenerator.getNextElemIndex();
  int size = internalWellGenerator.getPrevElemIndices().size();
  m_prevElemId.resize( size );
  for ( int i = 0; i < size; i++ )
  {
    m_prevElemId[i] = internalWellGenerator.getPrevElemIndices()[i];
  }
  m_elemToNodesMap = internalWellGenerator.getElemToNodesMap();
  m_elemVolume = internalWellGenerator.getElemVolume();
  m_numNodesPerElem = internalWellGenerator.getNumNodesPerElement();
  m_numNodes = internalWellGenerator.getNumNodes();
  m_nodeCoords = internalWellGenerator.getNodeCoords();
  m_numPerforations = internalWellGenerator.getNumPerforations();
  m_perfCoords = internalWellGenerator.getPerfCoords();
  m_perfTransmissibility = internalWellGenerator.getPerfTransmissibility();
  m_perfElemId = internalWellGenerator.getPerfElemIndex();
  m_nDims = internalWellGenerator.getPhysicalDimensionsNumber();
  m_perforationList = internalWellGenerator.getPerforationList();
}
}
