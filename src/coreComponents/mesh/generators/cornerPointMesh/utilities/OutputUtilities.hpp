/*
 * ------------------------------------------------------------------------------------------------------------
 * SPDX-License-Identifier: LGPL-2.1-only
 *
 * Copyright (c) 2018-2020 Lawrence Livermore National Security LLC
 * Copyright (c) 2018-2020 The Board of Trustees of the Leland Stanford Junior University
 * Copyright (c) 2018-2020 Total, S.A
 * Copyright (c) 2019-     GEOSX Contributors
 * All rights reserved
 *
 * See top level LICENSE, COPYRIGHT, CONTRIBUTORS, NOTICE, and ACKNOWLEDGEMENTS files for details.
 * ------------------------------------------------------------------------------------------------------------
 */

/**
 * @file OutputUtilities.hpp
 */

#ifndef GEOSX_MESHUTILITIES_CORNERPOINTMESH_UTILITIES_OUTPUTUTILITIES_HPP_
#define GEOSX_MESHUTILITIES_CORNERPOINTMESH_UTILITIES_OUTPUTUTILITIES_HPP_

#include "mesh/generators/cornerPointMesh/CornerPointMeshData.hpp"

namespace geosx
{

namespace cornerPointMesh
{

namespace outputUtilities
{

/**
 * @brief Debug function that outputs a vtk file representing the new mesh
 * @param[in] vertices the struct holding vertex information
 * @param[in] faces the struct holding face information
 * @param[in] cells the struct holding cell information
 */
void outputDebugVTKFile( CornerPointMeshVertices const & vertices,
                         CornerPointMeshFaces const & faces,
                         CornerPointMeshCells const & cells )
{
  GEOSX_UNUSED_VAR( faces ); // not needed for now

  std::ofstream myfile;
  myfile.open ( "debug.vtk" );
  myfile << "# vtk DataFile Version 3.0\n";
  myfile << "debug mesh\n";
  myfile << "ASCII\n";
  myfile << "DATASET UNSTRUCTURED_GRID\n";

  array2d< real64 > const & vertexPositions = vertices.m_vertexPositions;
  //myfile << "POINTS " << vertexPositions.size() << " float\n";
  // TODO: dimension bug (the original code yields the size of 1D array. Here we need the first dimension, the number of points)
  myfile << "POINTS " << vertexPositions.size(0) << " float\n";

  for( localIndex iVertex = 0; iVertex < vertexPositions.size( 0 ); ++iVertex )
  {
    myfile << vertexPositions( iVertex, 0 ) << " " << vertexPositions( iVertex, 1 ) << " " << vertexPositions( iVertex, 2 ) << "\n";
  }

  array1d< localIndex > const & activeCellToCell = cells.m_activeCellToCell;
  array1d< localIndex > const & cellToCPVertices = cells.m_cellToCPVertices;
  array1d< localIndex > const & cpVertexToVertex = vertices.m_cpVertexToVertex;
  localIndex const nActiveCells = activeCellToCell.size();

  myfile << "CELLS " << nActiveCells << " " << 9*nActiveCells << "\n";
  for( localIndex iActiveCell = 0; iActiveCell < nActiveCells; ++iActiveCell )
  {
    localIndex const iLocalCell = activeCellToCell( iActiveCell );
    localIndex const iFirstCPVertex = cellToCPVertices( iLocalCell );
    myfile << "8 ";

    localIndex const order[8] = { 0, 1, 3, 2, 4, 5, 7, 6 };
    for( localIndex pos = 0; pos < 8; ++pos )
    {
      myfile << cpVertexToVertex( iFirstCPVertex + order[pos] ) << " ";
    }
    myfile << "\n";
  }

  myfile << "CELL_TYPES " << nActiveCells << std::endl;
  for( localIndex iActiveCell = 0; iActiveCell < nActiveCells; ++iActiveCell )
  {
    myfile << "12\n";
  }
  myfile.close();
}

void printFile(std::ostream& fStream, std::ostream& cStream, std::string& str )
{
  GEOSX_UNUSED_VAR( cStream );
  fStream << str;
  //cStream << str;
}
// Change the original function to accomodate data with cell type of 42
// this only works with conforming case
void outputDebugVTKFileWithFaces( CornerPointMeshVertices const & vertices,
                         CornerPointMeshFaces const & faces,
                         CornerPointMeshCells const & cells )
{

  std::ofstream myfile;
  myfile.open ( "debug_conforming_face.vtk" );
  myfile << "# vtk DataFile Version 5.1\n";
  myfile << "vtk output\n";
  myfile << "ASCII\n";
  myfile << "DATASET UNSTRUCTURED_GRID\n";

  array2d< real64 > const & vertexPositions = vertices.m_vertexPositions;
  //myfile << "POINTS " << vertexPositions.size() << " float\n";
  // TODO: dimension bug (the original code yields the size of 1D array. Here we need the first dimension, the number of points)
  std::string currLine = "";
  //myfile << "POINTS " << vertexPositions.size(0) << " float\n";
  currLine = "POINTS " + std::to_string(  vertexPositions.size(0) ) + " float\n";
  printFile( myfile, std::cout, currLine);

  for( localIndex iVertex = 0; iVertex < vertexPositions.size( 0 ); ++iVertex )
  {
    //myfile << vertexPositions( iVertex, 0 ) << " " << vertexPositions( iVertex, 1 ) << " " << vertexPositions( iVertex, 2 ) << "\n";
    currLine = std::to_string(  vertexPositions( iVertex, 0 ) ) + " " + std::to_string(  vertexPositions( iVertex, 1 ) ) + " " +
                std::to_string(  vertexPositions( iVertex, 2 ) ) + "\n";
    printFile( myfile, std::cout, currLine);
  }

  array1d< localIndex > const & activeCellToCell = cells.m_activeCellToCell;
  array1d< localIndex > const & cellToCPVertices = cells.m_cellToCPVertices;
  array1d< localIndex > const & cpVertexToVertex = vertices.m_cpVertexToVertex;
  localIndex const nActiveCells = activeCellToCell.size();

  //myfile << "CELLS " << nActiveCells << " " << 9*nActiveCells << "\n";
  myfile << "CELLS " << nActiveCells + 1 << " " << 31*( nActiveCells ) << "\n";
  myfile << "OFFSETS vtktypeint64" << "\n";

  // print offset of each cell.
  // for conforming cases, each cell is known to have 6 faces

  //myfile << "0 ";
  currLine = std::to_string( 0 ) + " ";
  printFile( myfile, std::cout, currLine);

  localIndex tmpOffset = 31;
  for( localIndex iActiveCell = 0; iActiveCell < nActiveCells; ++iActiveCell )
  {
    //myfile << tmpOffset << " ";
    currLine = std::to_string( tmpOffset ) + " ";
    printFile( myfile, std::cout, currLine);
    tmpOffset += 31;
  }

  // face-related data
  ArrayOfArrays< localIndex > const & ownedActiveCellToFaces = cells.m_ownedActiveCellToFaces;
  ArrayOfArrays< localIndex > const & faceToVertices = faces.m_faceToVertices;

  // myfile << "\nCONNECTIVITY vtktypeint64" << "\n";
  currLine = "\nCONNECTIVITY vtktypeint64\n";
  printFile( myfile, std::cout, currLine);

  for( localIndex iActiveCell = 0; iActiveCell < nActiveCells; ++iActiveCell )
  {
    //localIndex const iLocalCell = activeCellToCell( iActiveCell );
    //myfile << "6" << "\n";
    currLine = "6\n";
    printFile( myfile, std::cout, currLine);
    for ( auto iter = ownedActiveCellToFaces[iActiveCell].begin(); iter != ownedActiveCellToFaces[iActiveCell].end(); ++iter  )
    {
      //myfile << "4 ";
      currLine = "4 ";
      printFile( myfile, std::cout, currLine);
      localIndex const faceIndex = *iter;
      for (auto iterVer = faceToVertices[faceIndex].begin(); iterVer != faceToVertices[faceIndex].end(); ++iterVer)
      {
        //myfile << *iterVer << " ";
        currLine =  std::to_string( *iterVer ) + " ";
        printFile( myfile, std::cout, currLine);
      }
      //myfile << "\n";
      currLine =  "\n";
      printFile( myfile, std::cout, currLine);
    }
  }

  myfile << "CELL_TYPES " << nActiveCells << std::endl;
  for( localIndex iActiveCell = 0; iActiveCell < nActiveCells; ++iActiveCell )
  {
    myfile << "42\n";
  }
  myfile.close();
}

} // namespace outputUtilities

} // namespace cornerPointMesh

} // namespace geosx

#endif //GEOSX_MESHUTILITIES_CORNERPOINTMESH_OUTPUTUTILITIES_HPP_
