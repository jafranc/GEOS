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
 * @file VTKMeshGenerator.cpp
 */

#include "VTKMeshGenerator.hpp"

#include "common/DataTypes.hpp"
#include "common/DataLayouts.hpp"
#include "common/MpiWrapper.hpp"
#include "common/TypeDispatch.hpp"
#include "mesh/DomainPartition.hpp"
#include "mesh/MeshBody.hpp"
#include "mesh/generators/CellBlockManager.hpp"
#include "mesh/generators/VTKMeshGeneratorTools.hpp"
#include "mesh/generators/ParMETISInterface.hpp"
#ifdef GEOSX_USE_SCOTCH
#include "mesh/generators/PTScotchInterface.hpp"
#endif
#include "mesh/mpiCommunications/CommunicationTools.hpp"

#include <vtkArrayDispatch.h>
#include <vtkBoundingBox.h>
#include <vtkCellData.h>
#include <vtkExtractCells.h>
#include <vtkGenerateGlobalIds.h>
#include <vtkPartitionedDataSet.h>
#include <vtkPointData.h>
#include <vtkRedistributeDataSetFilter.h>
#include <vtkSmartPointer.h>
#include <vtkUnstructuredGrid.h>
#include <vtkUnstructuredGridReader.h>
#include <vtkXMLPUnstructuredGridReader.h>
#include <vtkXMLUnstructuredGridReader.h>
// for vts import
#include <vtkNew.h>
#include <vtkStructuredGrid.h>
#include <vtkXMLStructuredGridReader.h>
#include <vtkXMLPStructuredGridReader.h>
// for vti import
#include <vtkImageData.h>
#include <vtkXMLImageDataReader.h>
// #include <vtkXMLPImageDataReader.h>
#include <vtkImageDataToPointSet.h>

#ifdef GEOSX_USE_MPI
#include <vtkMPIController.h>
#include <vtkMPI.h>
#else
#include <vtkDummyController.h>
#endif

#include <numeric>
#include <unordered_set>

namespace geosx
{
using namespace dataRepository;


VTKMeshGenerator::VTKMeshGenerator( string const & name,
                                    Group * const parent )
  : ExternalMeshGeneratorBase( name, parent )
{
  registerWrapper( viewKeyStruct::regionAttributeString(), &m_attributeName ).
    setInputFlag( InputFlags::OPTIONAL ).
    setApplyDefaultValue( "attribute" ).
    setDescription( "Name of the VTK cell attribute to use as region marker" );

  registerWrapper( viewKeyStruct::nodesetNamesString(), &m_nodesetNames ).
    setInputFlag( InputFlags::OPTIONAL ).
    setDescription( "Names of the VTK nodesets to import" );

  registerWrapper( viewKeyStruct::partitionRefinementString(), &m_partitionRefinement ).
    setInputFlag( InputFlags::OPTIONAL ).
    setApplyDefaultValue( 1 ).
    setDescription( "Number of partitioning refinement iterations (defaults to 1, recommended value)."
                    "A value of 0 disables graph partitioning and keeps simple kd-tree partitions (not recommended). "
                    "Values higher than 1 may lead to slightly improved partitioning, but yield diminishing returns." );

  registerWrapper( viewKeyStruct::partitionMethodString(), &m_partitionMethod ).
    setInputFlag( InputFlags::OPTIONAL ).
    setDescription( "Method (library) used to partition the mesh" );

  registerWrapper( viewKeyStruct::useGlobalIdsString(), &m_useGlobalIds ).
    setInputFlag( InputFlags::OPTIONAL ).
    setApplyDefaultValue( 0 ).
    setDescription( "Controls the use of global IDs in the input file for cells and points."
                    " If set to 0 (default value), the GlobalId arrays in the input mesh are used if available, and generated otherwise."
                    " If set to a negative value, the GlobalId arrays in the input mesh are not used, and generated global Ids are automatically generated."
                    " If set to a positive value, the GlobalId arrays in the input mesh are used and required, and the simulation aborts if they are not available" );
}

namespace vtk
{

/**
 * @brief Return a VTK controller for multiprocessing.
 */
vtkSmartPointer< vtkMultiProcessController > getController()
{
#ifdef GEOSX_USE_MPI
  vtkNew< vtkMPIController > controller;
  vtkMPICommunicatorOpaqueComm vtkGeosxComm( &MPI_COMM_GEOSX );
  vtkNew< vtkMPICommunicator > communicator;
  communicator->InitializeExternal( &vtkGeosxComm );
  controller->SetCommunicator( communicator );
#else
  vtkNew< vtkDummyController > controller;
#endif
  return controller;
}

/**
 * @brief Load the VTK file into the VTK data structure
 * @param[in] filePath the Path of the file to load
 */
vtkSmartPointer< vtkDataSet >
loadMesh( Path const & filePath )
{
  string const extension = filePath.extension();
  vtkSmartPointer< vtkDataSet > loadedMesh;

  if( extension == "pvtu" )
  {
    auto const vtkUgReader = vtkSmartPointer< vtkXMLPUnstructuredGridReader >::New();
    vtkUgReader->SetFileName( filePath.c_str() );
    vtkUgReader->UpdateInformation();
    vtkUgReader->UpdatePiece( MpiWrapper::commRank(), MpiWrapper::commSize(), 0 );
    loadedMesh = vtkUgReader->GetOutput();

    // TODO: Apply vtkStaticCleanUnstructuredGrid, once it is included in the next VTK release.
    //       https://gitlab.kitware.com/vtk/vtk/-/blob/master/Filters/Core/vtkStaticCleanUnstructuredGrid.h
    //       This removes duplicate points, either present in the dataset, or resulting from merging pieces.
  }
  else if( extension == "pvts" )
  {
    auto const vtkSgReader = vtkSmartPointer< vtkXMLPStructuredGridReader >::New();
    vtkSgReader->SetFileName( filePath.c_str() );
    vtkSgReader->UpdateInformation();
    vtkSgReader->UpdatePiece( MpiWrapper::commRank(), MpiWrapper::commSize(), 0 );
    loadedMesh = vtkSgReader->GetOutput();
  }
  else
  {
    if( MpiWrapper::commRank() == 0 )
    {
      auto const read = [&]( auto const vtkGridReader )
      {
        vtkGridReader->SetFileName( filePath.c_str() );
        vtkGridReader->Update();
        return vtkGridReader->GetOutput();
      };

      if( extension == "vtk" )
      {
        loadedMesh = read( vtkSmartPointer< vtkUnstructuredGridReader >::New() );
      }
      else if( extension == "vtu" )
      {
        loadedMesh = read( vtkSmartPointer< vtkXMLUnstructuredGridReader >::New() );
      }
      else if( extension == "vts" )
      {
        loadedMesh = read( vtkSmartPointer< vtkXMLStructuredGridReader >::New() );
      }
      else if( extension == "vti" )
      {
        vtkSmartPointer< vtkImageData > image = read( vtkSmartPointer< vtkXMLImageDataReader >::New() );
        vtkNew< vtkImageDataToPointSet > imageDataToSg;
        imageDataToSg->SetInputData( image );
        imageDataToSg->Update();
        vtkStructuredGrid * sGrid = imageDataToSg->GetOutput();
        loadedMesh = sGrid;
      }
      else
      {
        GEOSX_ERROR( extension << " is not a recognized extension for VTKMesh. Please use .vtk, .vtu, .vts, .vti, .pvtu or .ptvs." );
      }
    }
    else
    {
      loadedMesh = vtkSmartPointer< vtkUnstructuredGrid >::New();
    }
  }

  return loadedMesh;
}

/**
 * @brief Get the cell array object
 *
 * @param mesh vtk mesh
 * @return vtk cell array
 *
 * replaces GetCells() that exist only in vtkUnstructuredGrid
 */
vtkSmartPointer< vtkCellArray > GetCellArray( vtkDataSet & mesh )
{
  vtkSmartPointer< vtkCellArray > cells = vtkSmartPointer< vtkCellArray >::New();
  if( mesh.IsA( "vtkUnstructuredGrid" ))
  {
    cells = vtkUnstructuredGrid::SafeDownCast( &mesh )->GetCells();
  }
  else
  {
    int numCell = mesh.GetNumberOfCells();
    for( int c = 0; c < numCell; c++ )
    {
      cells->InsertNextCell( mesh.GetCell( c ));
    }
  }
  return cells;
}

template< typename INDEX_TYPE, typename POLICY >
ArrayOfArrays< INDEX_TYPE, INDEX_TYPE >
buildElemToNodesImpl( vtkDataSet & mesh,
                      vtkSmartPointer< vtkCellArray > const & cells )
{
  localIndex const numCells = LvArray::integerConversion< localIndex >( mesh.GetNumberOfCells() );
  array1d< INDEX_TYPE > nodeCounts( numCells );

  // GetCellSize() is always thread-safe, can run in parallel
  forAll< parallelHostPolicy >( numCells, [nodeCounts = nodeCounts.toView(), &cells] ( localIndex const cellIdx )
  {
    nodeCounts[cellIdx] = LvArray::integerConversion< INDEX_TYPE >( cells->GetCellSize( cellIdx ) );
  } );

  ArrayOfArrays< INDEX_TYPE, INDEX_TYPE > elemToNodes;
  elemToNodes.template resizeFromCapacities< parallelHostPolicy >( numCells, nodeCounts.data() );

  vtkIdTypeArray const & globalPointId = *vtkIdTypeArray::FastDownCast( mesh.GetPointData()->GetGlobalIds() );

  // GetCellAtId() is conditionally thread-safe, use POLICY argument
  forAll< POLICY >( numCells, [&cells, &globalPointId, elemToNodes = elemToNodes.toView()] ( localIndex const cellIdx )
  {
    vtkIdType numPts;
    vtkIdType const * points;
    cells->GetCellAtId( cellIdx, numPts, points );
    for( int a = 0; a < numPts; ++a )
    {
      vtkIdType const pointIdx = globalPointId.GetValue( points[a] );
      elemToNodes.emplaceBack( cellIdx, LvArray::integerConversion< INDEX_TYPE >( pointIdx ) );
    }
  } );

  return elemToNodes;
}

template< typename INDEX_TYPE >
ArrayOfArrays< INDEX_TYPE, INDEX_TYPE >
buildElemToNodes( vtkDataSet & mesh )
{
  vtkSmartPointer< vtkCellArray > const & cells = GetCellArray( mesh );
  // According to VTK docs, IsStorageShareable() indicates whether pointers extracted via
  // vtkCellArray::GetCellAtId() are pointers into internal storage rather than temp buffer
  // and thus results can be used in a thread-safe way.
  return cells->IsStorageShareable()
       ? buildElemToNodesImpl< INDEX_TYPE, parallelHostPolicy >( mesh, cells )
       : buildElemToNodesImpl< INDEX_TYPE, serialPolicy >( mesh, cells );
}

template< typename PART_INDEX >
vtkSmartPointer< vtkPartitionedDataSet >
splitMeshByPartition( vtkDataSet & mesh,
                      PART_INDEX const numParts,
                      arrayView1d< PART_INDEX const > const & part )
{
  array1d< localIndex > cellCounts( numParts );
  forAll< parallelHostPolicy >( part.size(), [part, cellCounts = cellCounts.toView()] ( localIndex const cellIdx )
  {
    RAJA::atomicInc< parallelHostAtomic >( &cellCounts[part[cellIdx]] );
  } );

  ArrayOfArrays< vtkIdType > cellsLists;
  cellsLists.resizeFromCapacities< serialPolicy >( numParts, cellCounts.data() );

  forAll< parallelHostPolicy >( part.size(), [part, cellsLists = cellsLists.toView()] ( localIndex const cellIdx )
  {
    cellsLists.emplaceBackAtomic< parallelHostAtomic >( LvArray::integerConversion< localIndex >( part[cellIdx] ),
                                                        LvArray::integerConversion< vtkIdType >( cellIdx ) );
  } );

  vtkNew< vtkPartitionedDataSet > result;
  result->SetNumberOfPartitions( LvArray::integerConversion< unsigned int >( numParts ) );

  vtkNew< vtkExtractCells > extractor;
  extractor->SetInputDataObject( &mesh );

  for( localIndex p = 0; p < numParts; ++p )
  {
    arraySlice1d< vtkIdType const > const cells = cellsLists[p];
    if( cells.size() > 0 )
    {
      extractor->SetCellIds( cells.dataIfContiguous(), LvArray::integerConversion< vtkIdType >( cells.size() ) );
      extractor->Update();

      vtkNew< vtkUnstructuredGrid > ug;
      ug->ShallowCopy( extractor->GetOutputDataObject( 0 ) );
      result->SetPartition( LvArray::integerConversion< unsigned int >( p ), ug );
    }
  }
  return result;
}

vtkSmartPointer< vtkDataSet >
generateGlobalIDs( vtkDataSet & mesh )
{
  GEOSX_MARK_FUNCTION;

  vtkNew< vtkGenerateGlobalIds > generator;
  generator->SetInputDataObject( &mesh );
  generator->Update();
  return vtkDataSet::SafeDownCast( generator->GetOutputDataObject( 0 ) );
}

vtkSmartPointer< vtkDataSet >
redistributeByCellGraph( vtkDataSet & mesh,
                         VTKMeshGenerator::PartitionMethod const method,
                         MPI_Comm const comm,
                         int const numRefinements )
{
  GEOSX_MARK_FUNCTION;

  int64_t const numElems = mesh.GetNumberOfCells();
  int64_t const numProcs = MpiWrapper::commSize( comm );

  // Compute `elemdist` parameter (element range owned by each rank)
  array1d< int64_t > const elemDist( numProcs + 1 );
  {
    array1d< int64_t > elemCounts;
    MpiWrapper::allGather( numElems, elemCounts, comm );
    std::partial_sum( elemCounts.begin(), elemCounts.end(), elemDist.begin() + 1 );
  }

  // Use int64_t here to match ParMETIS' idx_t
  ArrayOfArrays< int64_t, int64_t > const elemToNodes = buildElemToNodes< int64_t >( mesh );
  ArrayOfArrays< int64_t, int64_t > const graph = parmetis::meshToDual( elemToNodes.toViewConst(), elemDist, comm, 3 );

  array1d< int64_t > const newParts = [&]()
  {
    switch( method )
    {
      case VTKMeshGenerator::PartitionMethod::parmetis:
      {
        return parmetis::partition( graph.toViewConst(), elemDist, numProcs, comm, numRefinements );
      }
      case VTKMeshGenerator::PartitionMethod::ptscotch:
      {
#ifdef GEOSX_USE_SCOTCH
        GEOSX_WARNING_IF( numRefinements > 0, "Partition refinement is not supported by 'ptscotch' partitioning method" );
        return ptscotch::partition( graph.toViewConst(), numProcs, comm );
#else
        GEOSX_THROW( "GEOSX must be built with Scotch support (ENABLE_SCOTCH=ON) to use 'ptscotch' partitioning method" );
#endif
      }
      default:
      {
        GEOSX_THROW( "Unknown partition method", InputError );
      }
    }
  }();
  vtkSmartPointer< vtkPartitionedDataSet > const splitMesh = splitMeshByPartition( mesh, numProcs, newParts.toViewConst() );
  return vtk::redistribute( *splitMesh, MPI_COMM_GEOSX );
}

vtkSmartPointer< vtkDataSet >
redistributeByKdTree( vtkDataSet & mesh )
{
  GEOSX_MARK_FUNCTION;

  // Use a VTK filter which employs a kd-tree partition internally
  vtkNew< vtkRedistributeDataSetFilter > rdsf;
  rdsf->SetInputDataObject( &mesh );
  rdsf->SetNumberOfPartitions( MpiWrapper::commSize() );
  rdsf->Update();
  return vtkDataSet::SafeDownCast( rdsf->GetOutputDataObject( 0 ) );
}

/**
 * @brief Compute the rank neighbor candidate list.
 * @param[in] boundingBoxes the bounding boxes used by the VTK partitioner for all ranks
 * @return the list of neighboring MPI ranks, will be updated
 */
std::vector< int >
findNeighborRanks( std::vector< vtkBoundingBox > boundingBoxes )
{
  int const numParts = LvArray::integerConversion< int >( boundingBoxes.size() );
  int const thisRank = MpiWrapper::commRank();

  // Inflate boxes to detect intersections more reliably
  double constexpr inflateFactor = 1.01;
  for( vtkBoundingBox & box : boundingBoxes )
  {
    box.ScaleAboutCenter( inflateFactor );
  }

  std::vector< int > neighbors;
  for( int i = 0; i < numParts; ++i )
  {
    if( i != thisRank && boundingBoxes[thisRank].Intersects( boundingBoxes[i] ) )
    {
      neighbors.push_back( i );
    }
  }

  return neighbors;
}

/**
 * @brief Generate global point/cell IDs and redistribute the mesh among MPI ranks.
 * @param[in] loadedMesh the mesh that was loaded on one or several MPI ranks
 * @param[in] comm the MPI communicator
 * @param[in] partitionRefinement number of graph partitioning refinement cycles
 * @param[in] useGlobalIds controls whether global id arrays from the vtk input should be used
 */
vtkSmartPointer< vtkDataSet >
redistributeMesh( vtkDataSet & loadedMesh,
                  MPI_Comm const comm,
                  VTKMeshGenerator::PartitionMethod const method,
                  int const partitionRefinement,
                  int const useGlobalIds )
{
  GEOSX_MARK_FUNCTION;

  // Generate global IDs for vertices and cells, if needed
  vtkSmartPointer< vtkDataSet > mesh;
  bool globalIdsAvailable = loadedMesh.GetPointData()->GetGlobalIds() != nullptr
                            && loadedMesh.GetCellData()->GetGlobalIds() != nullptr;
  if( useGlobalIds > 0 && !globalIdsAvailable )
  {
    GEOSX_ERROR( "Global IDs strictly required (useGlobalId > 0) but unavailable. Set useGlobalIds to 0 to build them automatically." );
  }
  else if( useGlobalIds >= 0 && globalIdsAvailable )
  {
    mesh = &loadedMesh;
    vtkIdTypeArray const * const globalCellId = vtkIdTypeArray::FastDownCast( mesh->GetCellData()->GetGlobalIds() );
    vtkIdTypeArray const * const globalPointId = vtkIdTypeArray::FastDownCast( mesh->GetPointData()->GetGlobalIds() );
    GEOSX_ERROR_IF( globalCellId->GetNumberOfComponents() != 1 && globalCellId->GetNumberOfTuples() != mesh->GetNumberOfCells(),
                    "Global cell IDs are invalid. Check the array or enable automatic generation (useGlobalId < 0)" );
    GEOSX_ERROR_IF( globalPointId->GetNumberOfComponents() != 1 && globalPointId->GetNumberOfTuples() != mesh->GetNumberOfPoints(),
                    "Global cell IDs are invalid. Check the array or enable automatic generation (useGlobalId < 0)" );

    GEOSX_LOG_RANK_0( "Using global Ids defined in VTK mesh" );
  }
  else
  {
    GEOSX_LOG_RANK_0( "Generating global Ids from VTK mesh" );
    vtkNew< vtkGenerateGlobalIds > generator;
    generator->SetInputDataObject( &loadedMesh );
    generator->Update();
    mesh = generateGlobalIDs( loadedMesh );
  }


  // Determine if redistribution is required
  vtkIdType const minCellsOnAnyRank = MpiWrapper::min( mesh->GetNumberOfCells(), comm );
  if( minCellsOnAnyRank == 0 )
  {
    // Redistribute the mesh over all ranks using simple octree partitions
    mesh = redistributeByKdTree( *mesh );
  }

  // Redistribute the mesh again using higher-quality graph partitioner
  if( partitionRefinement > 0 )
  {
    mesh = redistributeByCellGraph( *mesh, method, comm, partitionRefinement - 1 );
  }

  return mesh;
}

/**
 * @brief Gathers all the data from all ranks, merge them, sort them, and remove duplicates.
 * @tparam T Type of the exchanged data.
 * @param data The data to be exchanged.
 * @return The merged data.
 * @note This function makes MPI calls.
 */
template< typename T >
std::vector< T > collectUniqueValues( std::vector< T > const & data )
{
  // Exchange the sizes of the data across all ranks.
  array1d< int > dataSizes( MpiWrapper::commSize() );
  MpiWrapper::allGather( LvArray::integerConversion< int >( data.size() ), dataSizes, MPI_COMM_GEOSX );
  // `totalDataSize` contains the total data size across all the MPI ranks.
  int const totalDataSize = std::accumulate( dataSizes.begin(), dataSizes.end(), 0 );

  // Once the MPI exchange is done, `allData` will contain all the data of all the MPI ranks.
  // We want all ranks to get all the data. But each rank may have a different size of information.
  // Therefore, we use `allgatherv` that does not impose the same size across ranks like `allgather` does.
  std::vector< T > allData( totalDataSize );
  // `displacements` is the offset (relative to the receive buffer) to store the data for each rank.
  std::vector< int > displacements( MpiWrapper::commSize(), 0 );
  std::partial_sum( dataSizes.begin(), dataSizes.end() - 1, displacements.begin() + 1 );
  MpiWrapper::allgatherv( data.data(), data.size(), allData.data(), dataSizes.data(), displacements.data(), MPI_COMM_GEOSX );

  // Finalizing by sorting, removing duplicates and trimming the result vector at the proper size.
  std::sort( allData.begin(), allData.end() );
  auto newEnd = std::unique( allData.begin(), allData.end() );
  allData.erase( newEnd, allData.end() );

  return allData;
}

/**
 * @brief Identify the GEOSX type of the polyhedron
 *
 * @param cell The vtk cell VTK_POLYHEDRON
 * @return The geosx element type associated to VTK_POLYHEDRON
 */
geosx::ElementType buildGeosxPolyhedronType( vtkCell * const cell )
{
  GEOSX_ERROR_IF_NE_MSG( cell->GetCellType(), VTK_POLYHEDRON, "Input for polyhedronType() must be a VTK_POLYHEDRON." );

  localIndex const numFaces = cell->GetNumberOfFaces();
  localIndex numTriangles = 0;
  localIndex numQuads = 0;

  // Compute the number of triangles and quads
  for( localIndex iFace = 0; iFace < numFaces; ++iFace )
  {
    localIndex numFaceNodes = cell->GetFace( iFace )->GetNumberOfPoints();
    if( numFaceNodes == 3 )
    {
      numTriangles++;
    }
    if( numFaceNodes == 4 )
    {
      numQuads++;
    }
  }

  if( numTriangles == 4 && numFaces == 4 )
  {
    return geosx::ElementType::Tetrahedron;
  }

  if( numQuads == 6 && numFaces == 6 )
  {
    return geosx::ElementType::Hexahedron;
  }

  if( numTriangles == 2 && numQuads == 3 && numFaces == 5 )
  {
    return geosx::ElementType::Wedge;
  }

  if( numTriangles == 4 && numQuads == 1 && numFaces == 5 )
  {
    return geosx::ElementType::Pyramid;
  }

  if( numFaces - numQuads != 2 )
  {
    return geosx::ElementType::Polyhedron;
  }

  // Check if the polyhedron is a prism
  // quadsPoints contains points defining all the quads
  // noQuadsPoints contains points defining all the faces which are not quad
  set< localIndex > quadsPoints;
  set< localIndex > noQuadsPoints;
  for( localIndex iFace = 0; iFace < numFaces; ++iFace )
  {
    vtkCell *cellFace = cell->GetFace( iFace );
    if( cellFace->GetNumberOfPoints() == 4 )
    {
      for( localIndex iPoint = 0; iPoint < 4; ++iPoint )
      {
        quadsPoints.insert( cellFace->GetPointId( iPoint ) );
      }
    }
    else
    {
      for( localIndex iPoint = 0; iPoint < cellFace->GetNumberOfPoints(); ++iPoint )
      {
        noQuadsPoints.insert( cellFace->GetPointId( iPoint ) );
      }
    }
  }

  if( quadsPoints != noQuadsPoints )
  {
    return geosx::ElementType::Polyhedron;
  }

  // The polyhedron is a prism
  switch( numQuads )
  {
    case 5:  return geosx::ElementType::Prism5;
    case 6:  return geosx::ElementType::Prism6;
    case 7:  return geosx::ElementType::Prism7;
    case 8:  return geosx::ElementType::Prism8;
    case 9:  return geosx::ElementType::Prism9;
    case 10: return geosx::ElementType::Prism10;
    case 11: return geosx::ElementType::Prism11;
    default:
    {
      GEOSX_ERROR( "Prism with " << numQuads << " sides is not supported." );
      return{};
    }
  }
}

/**
 * @brief Get the geosx element type
 * @param[in] cellType The vtk cell type
 * @return The geosx element type
 */
ElementType convertVtkToGeosxElementType( vtkCell *cell )
{
  switch( cell->GetCellType() )
  {
    case VTK_VERTEX:           return ElementType::Vertex;
    case VTK_LINE:             return ElementType::Line;
    case VTK_TRIANGLE:         return ElementType::Triangle;
    case VTK_QUAD:             return ElementType::Quadrilateral;
    case VTK_POLYGON:          return ElementType::Polygon;
    case VTK_TETRA:            return ElementType::Tetrahedron;
    case VTK_PYRAMID:          return ElementType::Pyramid;
    case VTK_WEDGE:            return ElementType::Wedge;
    case VTK_HEXAHEDRON:       return ElementType::Hexahedron;
    case VTK_PENTAGONAL_PRISM: return ElementType::Prism5;
    case VTK_HEXAGONAL_PRISM:  return ElementType::Prism6;
    case VTK_POLYHEDRON:       return buildGeosxPolyhedronType( cell );
    default:
    {
      GEOSX_ERROR( cell->GetCellType() << " is not a recognized cell type to be used with the VTKMeshGenerator" );
      return {};
    }
  }
}

std::map< ElementType, std::vector< vtkIdType > >
splitCellsByType( vtkDataSet & mesh )
{
  std::map< ElementType, std::vector< vtkIdType > > typeToCells;
  vtkIdType const numCells = mesh.GetNumberOfCells();

  // Count the number of each cell type
  std::array< size_t, numElementTypes() > cellTypeCounts{};
  for( vtkIdType c = 0; c < numCells; c++ )
  {
    ElementType const elemType = convertVtkToGeosxElementType( mesh.GetCell( c ) );
    ++cellTypeCounts[static_cast< integer >( elemType )];
  }

  // Allocate space to hold cell id lists by type
  std::array< std::vector< vtkIdType >, numElementTypes() > cellListsByType;
  for( integer t = 0; t < numElementTypes(); ++t )
  {
    cellListsByType[t].reserve( cellTypeCounts[t] );
  }

  // Collect cell lists for each type (using array for speed in a hot loop)
  for( vtkIdType c = 0; c < numCells; c++ )
  {
    ElementType const elemType = convertVtkToGeosxElementType( mesh.GetCell( c ) );
    cellListsByType[static_cast< integer >( elemType )].push_back( c );
  }

  // Convert from array to map while also performing some checks
  for( integer t = 0; t < numElementTypes(); ++t )
  {
    // Avoid creating unneeded map entries that will show up in statistics
    if( cellListsByType[t].empty() )
    {
      continue;
    }

    ElementType const type = static_cast< ElementType >( t );
    switch( getElementDim( type ) )
    {
      case 0:
      case 1:
      {
        // Ignore vertex/line elements for now; maybe later we can import well polylines here
        break;
      }
      case 2:
      {
        // Merge all 2D elements together as polygons (we don't track their shapes).
        std::vector< vtkIdType > & surfaceCells = typeToCells[ ElementType::Polygon ];
        surfaceCells.insert( surfaceCells.end(), cellListsByType[t].begin(), cellListsByType[t].end() );
        break;
      }
      case 3:
      {
        // Collect 3D elements as is
        typeToCells.emplace( type, std::move( cellListsByType[t] ) );
        break;
      }
      default:
      {
        GEOSX_ERROR( "Invalid element dimension: " << getElementDim( type ) );
      }
    }
  }

  return typeToCells;
}

VTKMeshGenerator::CellMapType
splitCellsByTypeAndAttribute( std::map< ElementType, std::vector< vtkIdType > > & typeToCells,
                              vtkDataArray * const attributeDataArray )
{
  VTKMeshGenerator::CellMapType typeToAttributeToCells;
  for( auto & t2c : typeToCells )
  {
    ElementType const elemType = t2c.first;
    std::vector< vtkIdType > & cells = t2c.second;
    std::unordered_map< int, std::vector< vtkIdType > > & attributeToCells = typeToAttributeToCells[elemType];

    if( attributeDataArray == nullptr )
    {
      attributeToCells.emplace( -1, std::move( cells ) );
    }
    else
    {
      GEOSX_ERROR_IF_NE_MSG( attributeDataArray->GetNumberOfComponents(), 1,
                             "Invalid number of components in attribute array" );
      vtkArrayDispatch::Dispatch::Execute( attributeDataArray, [&]( auto const * attributeArray )
      {
        using ArrayType = TYPEOFPTR( attributeArray );
        vtkDataArrayAccessor< ArrayType > attribute( attributeArray );
        std::unordered_map< int, size_t > cellCounts;
        for( vtkIdType c: cells )
        {
          int const region = static_cast< int >( attribute.Get( c, 0 ) );
          ++cellCounts[region];
        }
        for( auto const & count : cellCounts )
        {
          attributeToCells[count.first].reserve( count.second );
        }
        for( vtkIdType c: cells )
        {
          int const region = static_cast< int >( attribute.Get( c, 0 ) );
          attributeToCells[region].push_back( c );
        }
      } );
    }
  }
  return typeToAttributeToCells;
}

void extendCellMapWithRemoteKeys( VTKMeshGenerator::CellMapType & cellMap )
{
  // Gather all element types encountered on any rank and enrich the local collection
  std::vector< ElementType > allElementTypes = collectUniqueValues( mapKeys( cellMap ) );
  std::vector< int > allCellAttributes;
  for( auto const & typeRegions : cellMap )
  {
    if( getElementDim( typeRegions.first ) == 3 )
    {
      std::vector< int > const attrs = mapKeys( typeRegions.second );
      allCellAttributes.insert( allCellAttributes.end(), attrs.begin(), attrs.end() );
    }
  }
  allCellAttributes = collectUniqueValues( allCellAttributes );

  for( ElementType const elemType : allElementTypes )
  {
    if( getElementDim( elemType ) == 3 )
    {
      for( int attrValue: allCellAttributes )
      {
        // This code inserts an empty element list if one was not present
        cellMap[elemType][attrValue];
      }
    }
  }

  // Treat surfaces separately - and avoid inadvertently creating a map entry for polygons
  std::vector< int > const surfaceAttributes = cellMap.count( ElementType::Polygon ) > 0
                                             ? mapKeys( cellMap.at( ElementType::Polygon ) )
                                             : std::vector< int >();
  std::vector< int > allSurfaceAttributes = collectUniqueValues( surfaceAttributes );
  for( int attrValue: allSurfaceAttributes )
  {
    cellMap[ElementType::Polygon][attrValue];
  }
}

using CellMapType = std::map< ElementType, std::unordered_map< int, std::vector< vtkIdType > > >;
/**
 * @brief Get the tetrahedron node ordering from a VTK_POLYHEDRON
 *
 * @param cell The vtk cell, type VTK_POLYHEDRON
 * @return The node ordering
 */
std::vector< localIndex > getTetrahedronNodeOrderingFromPolyhedron( vtkCell * const cell )
{
  GEOSX_ERROR_IF_NE_MSG( cell->GetCellType(), VTK_POLYHEDRON, "Input must be a VTK_POLYHEDRON." );

  real64 vectorA[3];
  real64 vectorB[3];
  real64 vectorC[3];
  real64 vectorN[3];
  vtkPoints *cellPoint = cell->GetPoints();

  // Check if the orientation is correct
  for( localIndex i = 0; i < 3; ++i )
  {
    vectorA[i] = cellPoint->GetPoint( 1 )[i] - cellPoint->GetPoint( 0 )[i];
    vectorB[i] = cellPoint->GetPoint( 2 )[i] - cellPoint->GetPoint( 0 )[i];
    vectorC[i] = cellPoint->GetPoint( 3 )[i] - cellPoint->GetPoint( 0 )[i];
  }

  LvArray::tensorOps::crossProduct( vectorN, vectorA, vectorB );

  if( LvArray::tensorOps::AiBi< 3 >( vectorN, vectorC ) > 0 )
  {
    // The orientation is correct
    return { 0, 1, 2, 3 };
  }
  else
  {
    // The orientation is incorrect, renumber nodes
    return { 0, 2, 1, 3 };
  }
}

/**
 * @brief Get the hexahedron node ordering from a VTK_POLYHEDRON
 *
 * @param cell The vtk cell, type VTK_POLYHEDRON
 * @return The node ordering
 *
 * It could be possible to use getPrismNodeOrderingFromPolyhedron< 4 > with additional
 * permutations. But at this point computationalGeometry::prismVolume< NUM_SIDES >
 * is not ready.
 */
std::vector< localIndex > getHexahedronNodeOrderingFromPolyhedron( vtkCell * const cell )
{
  GEOSX_ERROR_IF_NE_MSG( cell->GetCellType(), VTK_POLYHEDRON, "Input must be a VTK_POLYHEDRON." );

  localIndex iFace;
  std::vector< localIndex > nodeOrder( 8 );

  // Generate global to local map
  std::unordered_map< localIndex, localIndex > G2L;
  for( localIndex iPoint = 0; iPoint < 8; ++iPoint )
  {
    G2L[cell->GetPointId( iPoint )] = iPoint;
  }

  // Assuming the input parameters are correct, take the first quad
  iFace = 0;

  // Get global pointIds for the first quad
  vtkCell *cellFace = cell->GetFace( iFace );
  nodeOrder[0] = cellFace->GetPointId( 0 );
  nodeOrder[1] = cellFace->GetPointId( 1 );
  nodeOrder[2] = cellFace->GetPointId( 3 );
  nodeOrder[3] = cellFace->GetPointId( 2 );

  // Generate the global pointIds for the opposit quad using the edges connecting the two bases
  for( localIndex iEdge = 0; iEdge < 12; ++iEdge )
  {
    auto edgeNode0 = cell->GetEdge( iEdge )->GetPointId( 0 );
    auto edgeNode1 = cell->GetEdge( iEdge )->GetPointId( 1 );
    auto it0 = std::find( &nodeOrder[0], &nodeOrder[4], edgeNode0 );
    auto it1 = std::find( &nodeOrder[0], &nodeOrder[4], edgeNode1 );

    if( it0 != &nodeOrder[4] && it1 == &nodeOrder[4] )
    {
      nodeOrder[ 4 + std::distance( &nodeOrder[0], it0 ) ] = edgeNode1;
    }

    if( it0 == &nodeOrder[4] && it1 != &nodeOrder[4] )
    {
      nodeOrder[ 4 + std::distance( &nodeOrder[0], it1 ) ] = edgeNode0;
    }
  }

  // Convert global numbering to local numbering
  for( localIndex iPoint = 0; iPoint < 8; ++iPoint )
  {
    nodeOrder[iPoint] = G2L.at( nodeOrder[iPoint] );
  }

  real64 Xlocal[8][3];
  for( localIndex iPoint = 0; iPoint < 8; ++iPoint )
  {
    std::copy_n( cell->GetPoints()->GetPoint( nodeOrder[iPoint] ), 3, Xlocal[iPoint] );
  }
  real64 const cellVolume = computationalGeometry::hexahedronVolume( Xlocal );

  // If cell volume is negative swap the quads
  if( cellVolume < 0 )
  {
    std::rotate( nodeOrder.begin(), nodeOrder.begin()+4, nodeOrder.end());
  }

  return nodeOrder;
}

/**
 * @brief Get the wedge node ordering from a VTK_POLYHEDRON
 *
 * @param cell The vtk cell, type VTK_POLYHEDRON
 * @return The node ordering
 *
 * It could be possible to use getPrismNodeOrderingFromPolyhedron< 3 > with additional
 * permutations. But at this point computationalGeometry::prismVolume< NUM_SIDES >
 * is not ready.
 */
std::vector< localIndex > getWedgeNodeOrderingFromPolyhedron( vtkCell * const cell )
{
  GEOSX_ERROR_IF_NE_MSG( cell->GetCellType(), VTK_POLYHEDRON, "Input must be a VTK_POLYHEDRON." );

  localIndex iFace;
  std::vector< localIndex > nodeTri0( 3 );
  std::vector< localIndex > nodeTri1( 3 );
  std::vector< localIndex > nodeOrder( 6 );

  // Generate global to local map
  std::unordered_map< localIndex, localIndex > G2L;
  for( localIndex iPoint = 0; iPoint < 6; ++iPoint )
  {
    G2L[cell->GetPointId( iPoint )] = iPoint;
  }

  // Assuming the input parameters are correct, identify one of the triangles
  localIndex const numFaces = cell->GetNumberOfFaces();
  for( iFace = 0; iFace < numFaces; ++iFace )
  {
    if( cell->GetFace( iFace )->GetNumberOfPoints() == 3 )
    {
      break;
    }
  }

  GEOSX_ERROR_IF( iFace == numFaces, "Invalid wedge." );

  // Get global pointIds for the first triangle
  for( localIndex i = 0; i < 3; ++i )
  {
    nodeTri0[i] = cell->GetFace( iFace )->GetPointId( i );
  }

  // Generate the global pointIds for the second triangle using the edges connecting the two triangles
  for( localIndex iEdge = 0; iEdge < 9; ++iEdge )
  {
    auto edgeNode0 = cell->GetEdge( iEdge )->GetPointId( 0 );
    auto edgeNode1 = cell->GetEdge( iEdge )->GetPointId( 1 );
    auto it0 = std::find( &nodeTri0[0], &nodeTri0[3], edgeNode0 );
    auto it1 = std::find( &nodeTri0[0], &nodeTri0[3], edgeNode1 );

    if( it0 != &nodeTri0[3] && it1 == &nodeTri0[3] )
    {
      nodeTri1[std::distance( &nodeTri0[0], it0 )] = edgeNode1;
    }

    if( it0 == &nodeTri0[3] && it1 != &nodeTri0[3] )
    {
      nodeTri1[std::distance( &nodeTri0[0], it1 )] = edgeNode0;
    }
  }

  // Convert global numbering to local numbering
  for( int iPoint = 0; iPoint < 3; ++iPoint )
  {
    nodeTri0[iPoint] = G2L.at( nodeTri0[iPoint] );
    nodeTri1[iPoint] = G2L.at( nodeTri1[iPoint] );
  }

  // Set nodes numbering
  nodeOrder[0] = nodeTri0[0];
  nodeOrder[1] = nodeTri1[0];
  nodeOrder[2] = nodeTri0[1];
  nodeOrder[3] = nodeTri1[1];
  nodeOrder[4] = nodeTri0[2];
  nodeOrder[5] = nodeTri1[2];

  // Compute cell volume
  real64 Xlocal[6][3];
  for( localIndex iPoint = 0; iPoint < 6; ++iPoint )
  {
    std::copy_n( cell->GetPoints()->GetPoint( nodeOrder[iPoint] ), 3, Xlocal[iPoint] );
  }
  real64 const cellVolume = computationalGeometry::wedgeVolume( Xlocal );

  // If cell volume is negative reorder nodes
  if( cellVolume < 0 )
  {
    nodeOrder[0] = nodeTri0[0];
    nodeOrder[1] = nodeTri1[0];
    nodeOrder[2] = nodeTri0[2];
    nodeOrder[3] = nodeTri1[2];
    nodeOrder[4] = nodeTri0[1];
    nodeOrder[5] = nodeTri1[1];
  }

  return nodeOrder;
}

/**
 * @brief Get the pyramid node ordering from a VTK_POLYHEDRON
 *
 * @param cell The vtk cell, type VTK_POLYHEDRON
 * @return The node ordering
 */
std::vector< localIndex > getPyramidNodeOrderingFromPolyhedron( vtkCell * const cell )
{
  GEOSX_ERROR_IF_NE_MSG( cell->GetCellType(), VTK_POLYHEDRON, "Input must be a VTK_POLYHEDRON." );

  localIndex iPoint;
  localIndex iFace;
  std::vector< localIndex > nodeOrder( 5 );

  // Generate global to local map
  std::unordered_map< localIndex, localIndex > G2L;
  for( iPoint = 0; iPoint < 5; ++iPoint )
  {
    G2L[cell->GetPointId( iPoint )] = iPoint;
  }

  // Assuming the input parameters are correct, identify the base
  localIndex const numFaces = cell->GetNumberOfFaces();
  for( iFace = 0; iFace < numFaces; ++iFace )
  {
    if( cell->GetFace( iFace )->GetNumberOfPoints() == 4 )
    {
      break;
    }
  }

  GEOSX_ERROR_IF( iFace == numFaces, "Invalid pyramid." );

  // Get global pointIds for the base
  vtkCell * cellFace = cell->GetFace( iFace );
  nodeOrder[0] = cellFace->GetPointId( 0 );
  nodeOrder[1] = cellFace->GetPointId( 1 );
  nodeOrder[2] = cellFace->GetPointId( 3 );
  nodeOrder[3] = cellFace->GetPointId( 2 );

  // Identify the missing node
  iPoint = 0;
  while( std::find( &nodeOrder[0], &nodeOrder[4], cell->GetPointId( iPoint ) ) != &nodeOrder[4] )
  {
    iPoint++;
  }

  // Add it to nodeOrder
  nodeOrder[4] = cell->GetPointId( iPoint );

  // Convert global numbering to local numbering
  for( iPoint = 0; iPoint < 5; ++iPoint )
  {
    nodeOrder[iPoint] = G2L.at( nodeOrder[iPoint] );
  }

  // Compute cell volume
  real64 Xlocal[5][3];
  for( iPoint = 0; iPoint < 5; ++iPoint )
  {
    std::copy_n( cell->GetPoints()->GetPoint( nodeOrder[iPoint] ), 3, Xlocal[iPoint] );
  }
  real64 const cellVolume = computationalGeometry::pyramidVolume( Xlocal );

  // If cell volume is negative swap two nodes from the base
  if( cellVolume < 0 )
  {
    std::swap( nodeOrder[1], nodeOrder[2] );
  }

  return nodeOrder;
}

/**
 * @brief Get the prism node ordering from a VTK_POLYHEDRON
 *
 * @tparam NUM_SIDES number of sides of the prism
 * @param cell The vtk cell, type VTK_POLYHEDRON
 * @return The node ordering
 */
template< integer NUM_SIDES >
std::vector< localIndex > getPrismNodeOrderingFromPolyhedron( vtkCell * const cell )
{
  GEOSX_ERROR_IF_NE_MSG( cell->GetCellType(), VTK_POLYHEDRON, "Input must be a VTK_POLYHEDRON." );

  localIndex iFace;
  std::vector< localIndex > nodeOrder( 2*NUM_SIDES );

  // Generate global to local map
  std::unordered_map< localIndex, localIndex > G2L;
  for( localIndex iPoint = 0; iPoint < cell->GetNumberOfPoints(); ++iPoint )
  {
    G2L[cell->GetPointId( iPoint )] = iPoint;
  }

  // Assuming the input parameters are correct, identify one of the bases
  localIndex const numFaces = cell->GetNumberOfFaces();
  for( iFace = 0; iFace < numFaces; ++iFace )
  {
    if( cell->GetFace( iFace )->GetNumberOfPoints() == NUM_SIDES )
    {
      break;
    }
  }

  GEOSX_ERROR_IF( iFace == numFaces, "Invalid prism." );

  // Get global pointIds for the first base
  vtkCell *cellFace = cell->GetFace( iFace );
  for( localIndex iPoint = 0; iPoint < NUM_SIDES; ++iPoint )
  {
    nodeOrder[iPoint] = cellFace->GetPointId( iPoint );
  }

  // Generate the global pointIds for the second base using the edges connecting the two bases
  localIndex numEdges = cell->GetNumberOfEdges();
  for( localIndex iEdge = 0; iEdge < numEdges; ++iEdge )
  {
    auto edgeNode0 = cell->GetEdge( iEdge )->GetPointId( 0 );
    auto edgeNode1 = cell->GetEdge( iEdge )->GetPointId( 1 );
    auto it0 = std::find( &nodeOrder[0], &nodeOrder[NUM_SIDES], edgeNode0 );
    auto it1 = std::find( &nodeOrder[0], &nodeOrder[NUM_SIDES], edgeNode1 );

    if( it0 != &nodeOrder[NUM_SIDES] && it1 == &nodeOrder[NUM_SIDES] )
    {
      nodeOrder[NUM_SIDES + std::distance( &nodeOrder[0], it0 )] = edgeNode1;
    }

    if( it0 == &nodeOrder[NUM_SIDES] && it1 != &nodeOrder[NUM_SIDES] )
    {
      nodeOrder[NUM_SIDES + std::distance( &nodeOrder[0], it1 )] = edgeNode0;
    }
  }

  // Convert global numbering to local numbering
  for( localIndex iPoint = 0; iPoint < 2*NUM_SIDES; ++iPoint )
  {
    nodeOrder[iPoint] = G2L.at( nodeOrder[iPoint] );
  }

  // Compute cell volume
  real64 Xlocal[2*NUM_SIDES][3];
  for( localIndex iPoint = 0; iPoint < 2*NUM_SIDES; ++iPoint )
  {
    std::copy_n( cell->GetPoints()->GetPoint( nodeOrder[iPoint] ), 3, Xlocal[iPoint] );
  }
  real64 const cellVolume = computationalGeometry::prismVolume< NUM_SIDES >( Xlocal );

  // If cell volume is negative swap the bases
  if( cellVolume < 0 )
  {
    std::rotate( nodeOrder.begin(), nodeOrder.begin()+NUM_SIDES, nodeOrder.end());
  }

  return nodeOrder;
}

/**
 * @brief Collect lists of VTK cell indices organized by type and attribute value.
 * @param[in] mesh the vtkUnstructuredGrid or vtkStructuredGrid that is loaded
 * @param[in] attributeName name of the VTK data array containing the attribute, if any
 * @return A map from element type to a map of attribute to the associated cell ids for the current rank.
 *         The map contains entries for all types and attribute values across all MPI ranks,
 *         even if there are no cells on current rank (then the list will be empty).
 */
CellMapType buildCellMap( vtkDataSet & mesh, string const & attributeName )
{

  // First, pass through all VTK cells and split them int sub-lists based on type.
  std::map< ElementType, std::vector< vtkIdType > > typeToCells = splitCellsByType( mesh );

  // Now, actually split into groups according to region attribute, if present
  vtkDataArray * const attributeDataArray =
    vtkDataArray::FastDownCast( mesh.GetCellData()->GetAbstractArray( attributeName.c_str() ) );

  CellMapType cellMap =
    splitCellsByTypeAndAttribute( typeToCells, attributeDataArray );

  // Gather all element types encountered on any rank and enrich the local collection
  extendCellMapWithRemoteKeys( cellMap );

  return cellMap;
}

std::vector< int > getGeosxToVtkNodeOrdering( ElementType const elemType )
{
  switch( elemType )
  {
    case ElementType::Vertex:        return { 0 };
    case ElementType::Line:          return { 0, 1 };
    case ElementType::Triangle:      return { 0, 1, 2 };
    case ElementType::Quadrilateral: return { 0, 1, 2, 3 };  // TODO check
    case ElementType::Polygon:       return { };  // TODO
    case ElementType::Tetrahedron:   return { 0, 1, 2, 3 };
    case ElementType::Pyramid:       return { 0, 1, 3, 2, 4 };
    case ElementType::Wedge:         return { 0, 3, 2, 5, 1, 4 };
    case ElementType::Hexahedron:    return { 0, 1, 3, 2, 4, 5, 7, 6 };
    case ElementType::Prism5:        return { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };
    case ElementType::Prism6:        return { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 };
    case ElementType::Polyhedron:    return { };  // TODO
    default:
    {
      return { };
    }
  }
}

std::vector< int > getGeosxToVtkNodeOrdering( ElementType const elemType, vtkCell *cell )
{
  GEOSX_ERROR_IF_NE_MSG( cell->GetCellType(), VTK_POLYHEDRON, "Input for getGeosxToVtkNodeOrdering( ElementType const elemType, vtkCell *cell ) must be a VTK_POLYHEDRON." );
  switch( elemType )
  {
    case ElementType::Tetrahedron: return getTetrahedronNodeOrderingFromPolyhedron( cell );
    case ElementType::Pyramid:     return getPyramidNodeOrderingFromPolyhedron( cell );
    case ElementType::Wedge:       return getWedgeNodeOrderingFromPolyhedron( cell );
    case ElementType::Hexahedron:  return getHexahedronNodeOrderingFromPolyhedron( cell );
    case ElementType::Prism5:      return getPrismNodeOrderingFromPolyhedron< 5 >( cell );
    case ElementType::Prism6:      return getPrismNodeOrderingFromPolyhedron< 6 >( cell );
    case ElementType::Prism7:      return getPrismNodeOrderingFromPolyhedron< 7 >( cell );
    case ElementType::Prism8:      return getPrismNodeOrderingFromPolyhedron< 8 >( cell );
    case ElementType::Prism9:      return getPrismNodeOrderingFromPolyhedron< 9 >( cell );
    case ElementType::Prism10:     return getPrismNodeOrderingFromPolyhedron< 10 >( cell );
    case ElementType::Prism11:     return getPrismNodeOrderingFromPolyhedron< 11 >( cell );
    case ElementType::Polyhedron:  return { };  // TODO
    default:
    {
      return { };
    }
  }
}

/**
 * @brief Fill @p cellBlock with the appropriate nodes and local/global mappings.
 * @param[in] elemType the vtk cell type for cells of the CellBlock being written
 * @param[in] cellIds the cell indexes of cell type \p cellType within this region
 * @param[in] mesh the vtkUnstructuredGrid or vtkStructuredGrid that is loaded
 * @param[in,out] cellBlock The cell block to be written
 */
void fillCellBlock( vtkDataSet & mesh,
                    ElementType const elemType,
                    std::vector< vtkIdType > const & cellIds,
                    CellBlock & cellBlock )
{
  localIndex const numNodesPerElement = cellBlock.numNodesPerElement();
  arrayView2d< localIndex, cells::NODE_MAP_USD > const cellToVertex = cellBlock.getElemToNode();
  arrayView1d< globalIndex > const & localToGlobal = cellBlock.localToGlobalMap();
  vtkIdTypeArray const * const globalCellId = vtkIdTypeArray::FastDownCast( mesh.GetCellData()->GetGlobalIds() );
  GEOSX_ERROR_IF( !cellIds.empty() && globalCellId == nullptr, "Global cell IDs have not been generated" );

  localIndex cellCount = 0;
  auto const writeCell = [&]( vtkIdType const c, vtkCell * const cell, auto const & nodeOrder )
  {
    for( localIndex v = 0; v < numNodesPerElement; v++ )
    {
      cellToVertex[cellCount][v] = cell->GetPointId( nodeOrder[v] );
    }
    localToGlobal[cellCount++] = globalCellId->GetValue( c );
  };

  std::vector< int > const nodeOrderFixed = getGeosxToVtkNodeOrdering( elemType );
  // Writing connectivity and Local to Global
  for( vtkIdType c: cellIds )
  {
    vtkCell * const cell = mesh.GetCell( c );
    if( cell->GetCellType() == VTK_POLYHEDRON )
    {
      writeCell( c, cell, getGeosxToVtkNodeOrdering( elemType, cell ) );
    }
    else
    {
      writeCell( c, cell, nodeOrderFixed );
    }
  }
}

/**
 * @brief Returns a string describing the element.
 * @param[in] type The element type.
 * @return The name.
 * @warning This information will be visible in the input file... Consider refactoring with great care.
 */
string getElementTypeName( ElementType const type )
{
  switch( type )
  {
    case ElementType::Hexahedron:  return "hexahedra";
    case ElementType::Tetrahedron: return "tetrahedra";
    case ElementType::Wedge:       return "wedges";
    case ElementType::Pyramid:     return "pyramids";
    case ElementType::Prism5:      return "pentagonalPrisms";
    case ElementType::Prism6:      return "hexagonalPrisms";
    case ElementType::Prism7:      return "heptagonalPrisms";
    case ElementType::Prism8:      return "octagonalPrisms";
    case ElementType::Prism9:      return "nonagonalPrisms";
    case ElementType::Prism10:     return "decagonalPrisms";
    case ElementType::Prism11:     return "hendecagonalPrisms";
    case ElementType::Polyhedron:  return "polyhedra";
    default:
    {
      GEOSX_ERROR( "Element type '" << type << "' is not supported" );
      return {};
    }
  }
}

/**
 * @brief Builds the cell block name.
 * @param[in] elementName The element name.
 * @param[in] regionId The region Id.
 * @return The name.
 * @warning This name will be visible in the input file... Consider refactoring with great care.
 */
string buildCellBlockName( ElementType const type, int const regionId )
{
  GEOSX_ERROR_IF_LT_MSG( regionId, -1, "Invalid region id" );
  string const cellTypeName = getElementTypeName( type );
  return regionId != -1 ? std::to_string( regionId ) + "_" + cellTypeName : cellTypeName;
}

/**
 * @brief Collect a set of material field names registered in a subregion.
 * @param subRegion the target subregion
 * @return a set of wrapper names
 */
std::unordered_set< string > getMaterialWrapperNames( ElementSubRegionBase const & subRegion )
{
  using namespace constitutive;
  std::unordered_set< string > materialWrapperNames;
  subRegion.getConstitutiveModels().forSubGroups< ConstitutiveBase >( [&]( ConstitutiveBase const & material )
  {
    material.forWrappers( [&]( WrapperBase const & wrapper )
    {
      if( wrapper.sizedFromParent() )
      {
        materialWrapperNames.insert( ConstitutiveBase::makeFieldName( material.getName(), wrapper.getName() ) );
      }
    } );
  } );
  return materialWrapperNames;
}

/**
 * @brief Imports 2d and 3d arrays from @p vtkArray to @p wrapper, only for @p cellIds
 * @param cellIds The cells for which we should copy the data.
 * @param vtkArray The source.
 * @param wrapper The destination.
 */
void importMaterialField( std::vector< vtkIdType > const & cellIds,
                          vtkDataArray * vtkArray,
                          WrapperBase & wrapper )
{
  // Scalar material fields are stored as 2D arrays, vector/tensor are 3D
  using ImportTypes = types::ArrayTypes< types::RealTypes, types::DimsRange< 2, 3 > >;
  types::dispatch( ImportTypes{}, wrapper.getTypeId(), true, [&]( auto array )
  {
    using ArrayType = decltype( array );
    Wrapper< ArrayType > & wrapperT = Wrapper< ArrayType >::cast( wrapper );
    auto const view = wrapperT.reference().toView();

    localIndex const numComponentsSrc = LvArray::integerConversion< localIndex >( vtkArray->GetNumberOfComponents() );
    localIndex const numComponentsDst = wrapperT.numArrayComp() / view.size( 1 );
    GEOSX_ERROR_IF_NE_MSG( numComponentsDst, numComponentsSrc,
                           "Mismatch in number of components for field " << vtkArray->GetName() );

    vtkArrayDispatch::DispatchByValueType< vtkArrayDispatch::Reals >::Execute( vtkArray, [&]( auto const * srcArray )
    {
      vtkDataArrayAccessor< TYPEOFPTR( srcArray ) > data( srcArray );
      localIndex cellCount = 0;
      for( vtkIdType cellIdx : cellIds )
      {
        for( localIndex q = 0; q < view.size( 1 ); ++q )
        {
          // The same value is copied for all quadrature points.
          LvArray::forValuesInSlice( view[cellCount][q], [&, componentIdx = 0]( auto & val ) mutable
          {
            val = data.Get( cellIdx, componentIdx++ );
          } );
        }
        ++cellCount;
      }
    } );
  } );
}

/**
 * @brief Imports 1d and 2d arrays from @p vtkArray to @p wrapper, only for @p cellIds
 * @param cellIds The cells for which we should copy the data.
 * @param vtkArray The source.
 * @param wrapper The destination.
 */
void importRegularField( std::vector< vtkIdType > const & cellIds,
                         vtkDataArray * vtkArray,
                         WrapperBase & wrapper )
{
  using ImportTypes = types::ArrayTypes< types::RealTypes, types::DimsRange< 1, 2 > >;
  types::dispatch( ImportTypes{}, wrapper.getTypeId(), true, [&]( auto dstArray )
  {
    using ArrayType = decltype( dstArray );
    Wrapper< ArrayType > & wrapperT = Wrapper< ArrayType >::cast( wrapper );
    auto const view = wrapperT.reference().toView();

    localIndex const numComponentsSrc = LvArray::integerConversion< localIndex >( vtkArray->GetNumberOfComponents() );
    localIndex const numComponentsDst = wrapperT.numArrayComp();
    GEOSX_ERROR_IF_NE_MSG( numComponentsDst, numComponentsSrc,
                           "Mismatch in number of components for field " << vtkArray->GetName() );

    vtkArrayDispatch::DispatchByValueType< vtkArrayDispatch::Reals >::Execute( vtkArray, [&]( auto const * srcArray )
    {
      vtkDataArrayAccessor< TYPEOFPTR( srcArray ) > data( srcArray );
      localIndex cellCount = 0;
      for( vtkIdType cellIdx : cellIds )
      {
        LvArray::forValuesInSlice( view[cellCount], [&, componentIdx = 0]( auto & val ) mutable
        {
          val = data.Get( cellIdx, componentIdx++ );
        } );
        ++cellCount;
      }
    } );
  } );
}

void printMeshStatistics( vtkDataSet & mesh,
                          VTKMeshGenerator::CellMapType const & cellMap,
                          MPI_Comm const comm )
{
  int const rank = MpiWrapper::commRank( comm );
  int const size = MpiWrapper::commSize( comm );

  vtkIdTypeArray const & globalPointId = *vtkIdTypeArray::FastDownCast( mesh.GetPointData()->GetGlobalIds() );
  RAJA::ReduceMax< parallelHostReduce, globalIndex > maxGlobalNode( -1 );
  forAll< parallelHostPolicy >( mesh.GetNumberOfPoints(), [&globalPointId, maxGlobalNode]( vtkIdType const k )
  {
    maxGlobalNode.max( globalPointId.GetValue( k ) );
  } );
  globalIndex const numGlobalNodes = MpiWrapper::max( maxGlobalNode.get(), comm ) + 1;

  localIndex numLocalElems = 0;
  globalIndex numGlobalElems = 0;
  std::map< ElementType, globalIndex > elemCounts;

  for( auto const & typeToCells : cellMap )
  {
    localIndex const localElemsOfType =
      std::accumulate( typeToCells.second.begin(), typeToCells.second.end(), localIndex{},
                       []( auto const s, auto const & region ) { return s + region.second.size(); } );
    numLocalElems += localElemsOfType;

    globalIndex const globalElemsOfType = MpiWrapper::sum( globalIndex{ localElemsOfType }, comm );
    numGlobalElems += globalElemsOfType;
    elemCounts[typeToCells.first] = globalElemsOfType;
  }

  localIndex const minLocalElems = MpiWrapper::min( numLocalElems );
  localIndex const maxLocalElems = MpiWrapper::max( numLocalElems );
  localIndex const avgLocalElems = LvArray::integerConversion< localIndex >( numGlobalElems / size );

  if( rank == 0 )
  {
    int const widthGlobal = static_cast< int >( std::log10( std::max( numGlobalElems, numGlobalNodes ) ) + 1 );
    GEOSX_LOG( GEOSX_FMT( "Number of nodes: {:>{}}", numGlobalNodes, widthGlobal ) );
    GEOSX_LOG( GEOSX_FMT( "Number of elems: {:>{}}", numGlobalElems, widthGlobal ) );
    for( auto const & typeCount: elemCounts )
    {
      GEOSX_LOG( GEOSX_FMT( "{:>15}: {:>{}}", toString( typeCount.first ), typeCount.second, widthGlobal ) );
    }

    int const widthLocal = static_cast< int >( std::log10( maxLocalElems ) + 1 );
    GEOSX_LOG( GEOSX_FMT( "Load balancing: {1:>{0}} {2:>{0}} {3:>{0}}\n"
                          "(element/rank): {4:>{0}} {5:>{0}} {6:>{0}}",
                          widthLocal, "min", "avg", "max",
                          minLocalElems, avgLocalElems, maxLocalElems ) );
  }
}

/**
 * @brief Collect the data to be imported.
 * @return A list of pointers to VTK data arrays.
 */
std::vector< vtkDataArray * >
findArraysForImport( vtkDataSet & mesh,
                     arrayView1d< string const > const & srcFieldNames )
{
  std::vector< vtkDataArray * > arrays;
  vtkCellData & cellData = *mesh.GetCellData();

  for( string const & sourceName : srcFieldNames )
  {
    vtkAbstractArray * const curArray = cellData.GetAbstractArray( sourceName.c_str() );
    GEOSX_THROW_IF( curArray == nullptr,
                    GEOSX_FMT( "Source field '{}' not found in dataset", sourceName ),
                    InputError );

    int const dataType = curArray->GetDataType();
    GEOSX_ERROR_IF( dataType != VTK_FLOAT && dataType != VTK_DOUBLE,
                    GEOSX_FMT( "Source field '{}' has unsupported type: {} (expected floating point type)",
                               sourceName, curArray->GetDataTypeAsString() ) );
    arrays.push_back( vtkDataArray::SafeDownCast( curArray ) );
  }

  return arrays;
}

} // namespace vtk

void VTKMeshGenerator::
  importFieldOnCellElementSubRegion( int const regionId,
                                     ElementType const elemType,
                                     std::vector< vtkIdType > const & cellIds,
                                     ElementRegionManager & elemManager,
                                     arrayView1d< string const > const & fieldNames,
                                     std::vector< vtkDataArray * > const & srcArrays,
                                     FieldIdentifiers & fieldsToBeSync ) const
{
  string const cellBlockName = vtk::buildCellBlockName( elemType, regionId );

  elemManager.forElementSubRegionsComplete< CellElementSubRegion >( [&]( localIndex,
                                                                         localIndex,
                                                                         ElementRegionBase const & region,
                                                                         CellElementSubRegion & subRegion )
  {
    // We don't know how the user mapped cell blocks to regions, so we must check all of them
    if( subRegion.getName() != cellBlockName )
    {
      return;
    }
    std::unordered_set< string > const materialWrapperNames = vtk::getMaterialWrapperNames( subRegion );

    // Writing properties
    for( localIndex i = 0; i < fieldNames.size(); ++i )
    {
      // Get source
      vtkDataArray * vtkArray = srcArrays[i];

      // Find destination
      string const wrapperName = fieldNames[i];
      if( !subRegion.hasWrapper( wrapperName ) )
      {
        // Skip - the user may have not enabled a particular physics model/solver on this dstRegion.
        GEOSX_LOG_LEVEL_RANK_0( 1, GEOSX_FMT( "Skipping import of {} -> {} on {}/{} (field not found)",
                                              vtkArray->GetName(), wrapperName, region.getName(), subRegion.getName() ) );

        continue;
      }

      // Now that we know that the subRegion has this wrapper, we can add the wrapperName to the list of fields to
      // synchronize
      fieldsToBeSync.addElementFields( {wrapperName}, {region.getName()} );

      WrapperBase & wrapper = subRegion.getWrapperBase( wrapperName );

      GEOSX_LOG_LEVEL_RANK_0( 1, GEOSX_FMT( "Importing field {} -> {} on {}/{}",
                                            vtkArray->GetName(), wrapperName, region.getName(), subRegion.getName() ) );

      if( materialWrapperNames.count( wrapperName ) > 0 && wrapper.numArrayDims() > 1 )
      {
        vtk::importMaterialField( cellIds, vtkArray, wrapper );
      }
      else
      {
        vtk::importRegularField( cellIds, vtkArray, wrapper );
      }
    }
  } );
}

void VTKMeshGenerator::importFields( DomainPartition & domain ) const
{
  GEOSX_LOG_RANK_0( GEOSX_FMT( "{} '{}': importing field data from mesh dataset", catalogName(), getName() ) );
  GEOSX_ASSERT_MSG( m_vtkMesh, "Must call generateMesh() before importFields()" );

  // TODO Having CellElementSubRegion and ConstitutiveBase... here in a pure geometric module is problematic.
  ElementRegionManager & elemManager = domain.getMeshBody( this->getName() ).getBaseDiscretization().getElemManager();

  std::vector< vtkDataArray * > const srcArrays = vtk::findArraysForImport( *m_vtkMesh, m_fieldsToImport );

  FieldIdentifiers fieldsToBeSync;

  for( auto const & typeRegions : m_cellMap )
  {
    // Restrict data import to 3D cells
    if( getElementDim( typeRegions.first ) == 3 )
    {
      for( auto const & regionCells: typeRegions.second )
      {
        importFieldOnCellElementSubRegion( regionCells.first,
                                           typeRegions.first,
                                           regionCells.second,
                                           elemManager,
                                           m_fieldNamesInGEOSX,
                                           srcArrays,
                                           fieldsToBeSync );
      }
    }
  }

  CommunicationTools::getInstance().synchronizeFields( fieldsToBeSync,
                                                       domain.getMeshBody( this->getName() ).getBaseDiscretization(),
                                                       domain.getNeighbors(),
                                                       false );
}

void VTKMeshGenerator::importNodesets( vtkDataSet & mesh, CellBlockManager & cellBlockManager ) const
{
  auto & nodeSets = cellBlockManager.getNodeSets();
  localIndex const numPoints = LvArray::integerConversion< localIndex >( m_vtkMesh->GetNumberOfPoints() );

  for( int i=0; i < m_nodesetNames.size(); ++i )
  {
    GEOSX_LOG_LEVEL_RANK_0( 2, "    " + m_nodesetNames[i] );

    vtkAbstractArray * const curArray = mesh.GetPointData()->GetAbstractArray( m_nodesetNames[i].c_str() );
    GEOSX_THROW_IF( curArray == nullptr,
                    GEOSX_FMT( "Target nodeset '{}' not found in mesh", m_nodesetNames[i] ),
                    InputError );
    vtkTypeInt64Array const & nodesetMask = *vtkTypeInt64Array::FastDownCast( curArray );

    SortedArray< localIndex > & targetNodeset = nodeSets[ m_nodesetNames[i] ];
    for( localIndex j=0; j < numPoints; ++j )
    {
      if( nodesetMask.GetValue( j ) == 1 )
      {
        targetNodeset.insert( j );
      }
    }
  }
}

real64 VTKMeshGenerator::writeNodes( CellBlockManager & cellBlockManager ) const
{
  localIndex const numPts = LvArray::integerConversion< localIndex >( m_vtkMesh->GetNumberOfPoints() );
  cellBlockManager.setNumNodes( numPts );

  // Writing the points
  arrayView1d< globalIndex > const nodeLocalToGlobal = cellBlockManager.getNodeLocalToGlobal();
  arrayView2d< real64, nodes::REFERENCE_POSITION_USD > const X = cellBlockManager.getNodePositions();

  std::unordered_set< globalIndex > nodeGlobalIds;
  nodeGlobalIds.reserve( numPts );

  vtkIdTypeArray const & globalPointId = *vtkIdTypeArray::FastDownCast( m_vtkMesh->GetPointData()->GetGlobalIds() );
  forAll< serialPolicy >( numPts, [&, X, nodeLocalToGlobal]( localIndex const k )
  {
    double point[3];
    m_vtkMesh->GetPoint( k, point );
    LvArray::tensorOps::add< 3 >( point, m_translate );
    LvArray::tensorOps::hadamardProduct< 3 >( X[k], point, m_scale );
    globalIndex const pointGlobalID = globalPointId.GetValue( k );
    nodeLocalToGlobal[k] = pointGlobalID;

    // TODO: remove this check once the input mesh is cleaned of duplicate points via a filter
    //       and make launch policy parallel again
    GEOSX_ERROR_IF( nodeGlobalIds.count( pointGlobalID ) > 0,
                    GEOSX_FMT( "Duplicate point detected: globalID = {}\n"
                               "Consider cleaning the dataset in Paraview using 'Clean to grid' filter.\n"
                               "Make sure partitionRefinement is set to 1 or higher (this may help).",
                               pointGlobalID ) );
    nodeGlobalIds.insert( pointGlobalID );
  } );

  // Generate the "all" set
  array1d< localIndex > allNodes( numPts );
  std::iota( allNodes.begin(), allNodes.end(), 0 );
  SortedArray< localIndex > & allNodeSet = cellBlockManager.getNodeSets()[ "all" ];
  allNodeSet.insert( allNodes.begin(), allNodes.end() );

  // Import remaining nodesets
  importNodesets( *m_vtkMesh, cellBlockManager );

  constexpr real64 minReal = LvArray::NumericLimits< real64 >::min;
  constexpr real64 maxReal = LvArray::NumericLimits< real64 >::max;
  real64 xMin[3] = { maxReal, maxReal, maxReal };
  real64 xMax[3] = { minReal, minReal, minReal };

  vtkBoundingBox bb( m_vtkMesh->GetBounds() );
  if( bb.IsValid() )
  {
    bb.GetMinPoint( xMin );
    bb.GetMaxPoint( xMax );
  }

  MpiWrapper::min< real64 >( xMin, xMin, MPI_COMM_GEOSX );
  MpiWrapper::max< real64 >( xMax, xMax, MPI_COMM_GEOSX );
  LvArray::tensorOps::subtract< 3 >( xMax, xMin );
  return LvArray::tensorOps::l2Norm< 3 >( xMax );
}

/**
 * @brief Build all the cell blocks.
 * @param[in] cellBlockManager The instance that stores the cell blocks.
 */
void VTKMeshGenerator::writeCells( CellBlockManager & cellBlockManager ) const
{
  // Creates a new cell block for each region and for each type of cell.
  for( auto const & typeRegions : m_cellMap )
  {
    ElementType const elemType = typeRegions.first;
    if( getElementDim( elemType ) != 3 )
    {
      continue;
    }
    std::unordered_map< int, std::vector< vtkIdType > > const & regionIdToCellIds = typeRegions.second;
    for( auto const & regionCells : regionIdToCellIds )
    {
      int const regionId = regionCells.first;
      std::vector< vtkIdType > const & cellIds = regionCells.second;

      string const cellBlockName = vtk::buildCellBlockName( elemType, regionId );
      GEOSX_LOG_LEVEL_RANK_0( 1, "Importing cell block " << cellBlockName );

      // Create and resize the cell block.
      CellBlock & cellBlock = cellBlockManager.registerCellBlock( cellBlockName );
      cellBlock.setElementType( elemType );
      cellBlock.resize( LvArray::integerConversion< localIndex >( cellIds.size() ) );

      vtk::fillCellBlock( *m_vtkMesh, elemType, cellIds, cellBlock );
    }
  }
}

/**
 * @brief Build the "surface" node sets from the surface information.
 * @param[in] mesh The vtkUnstructuredGrid or vtkStructuredGrid that is loaded
 * @param[in] surfacesIdsToCellsIds Map from the surfaces index to the list of cells in this surface in this rank.
 * @param[out] cellBlockManager The instance that stores the node sets.
 * @note @p surfacesIdsToCellsIds will contain all the surface ids across all the MPI ranks, but only its cell ids.
 * If the current MPI rank has no cell id for a given surface, then an empty set will be created.
 */
void VTKMeshGenerator::writeSurfaces( CellBlockManager & cellBlockManager ) const
{
  if( m_cellMap.count( ElementType::Polygon ) == 0 )
  {
    return;
  }
  std::map< string, SortedArray< localIndex > > & nodeSets = cellBlockManager.getNodeSets();

  for( auto const & surfaceCells: m_cellMap.at( ElementType::Polygon ) )
  {
    int const surfaceId = surfaceCells.first;
    std::vector< vtkIdType > const & cellIds = surfaceCells.second;
    string const surfaceName = std::to_string( surfaceId );
    GEOSX_LOG_LEVEL_RANK_0( 1, "Importing surface " << surfaceName );

    // Get or create all surfaces (even those which are empty in this rank)
    SortedArray< localIndex > & curNodeSet = nodeSets[ surfaceName ];

    for( vtkIdType const c : cellIds )
    {
      vtkCell * const currentCell = m_vtkMesh->GetCell( c );
      for( int v = 0; v < currentCell->GetNumberOfPoints(); ++v )
      {
        curNodeSet.insert( currentCell->GetPointId( v ) );
      }
    }
  }
}

void VTKMeshGenerator::generateMesh( DomainPartition & domain )
{
  // TODO refactor void MeshGeneratorBase::generateMesh( DomainPartition & domain )
  GEOSX_MARK_FUNCTION;

  MPI_Comm const comm = MPI_COMM_GEOSX;
  vtkSmartPointer< vtkMultiProcessController > controller = vtk::getController();
  vtkMultiProcessController::SetGlobalController( controller );

  GEOSX_LOG_RANK_0( GEOSX_FMT( "{} '{}': reading mesh from {}", catalogName(), getName(), m_filePath ) );
  {
    GEOSX_LOG_LEVEL_RANK_0( 2, "  reading the dataset..." );
    vtkSmartPointer< vtkDataSet > loadedMesh = vtk::loadMesh( m_filePath );
    GEOSX_LOG_LEVEL_RANK_0( 2, "  redistributing mesh..." );
    m_vtkMesh = vtk::redistributeMesh( *loadedMesh, comm, m_partitionMethod, m_partitionRefinement, m_useGlobalIds );
    GEOSX_LOG_LEVEL_RANK_0( 2, "  finding neighbor ranks..." );
    std::vector< vtkBoundingBox > boxes = vtk::exchangeBoundingBoxes( *m_vtkMesh, comm );
    std::vector< int > const neighbors = vtk::findNeighborRanks( std::move( boxes ) );
    domain.getMetisNeighborList().insert( neighbors.begin(), neighbors.end() );
    GEOSX_LOG_LEVEL_RANK_0( 2, "  done!" );
  }

  GEOSX_LOG_RANK_0( GEOSX_FMT( "{} '{}': generating GEOSX mesh data structure", catalogName(), getName() ) );

  MeshBody & meshBody = domain.getMeshBodies().registerGroup< MeshBody >( this->getName() );
  meshBody.createMeshLevel( 0 );

  CellBlockManager & cellBlockManager = meshBody.registerGroup< CellBlockManager >( keys::cellManager );

  GEOSX_LOG_LEVEL_RANK_0( 2, "  preprocessing..." );
  m_cellMap = vtk::buildCellMap( *m_vtkMesh, m_attributeName );

  GEOSX_LOG_LEVEL_RANK_0( 2, "  writing nodes..." );
  real64 const globalLength = writeNodes( cellBlockManager );
  meshBody.setGlobalLengthScale( globalLength );

  GEOSX_LOG_LEVEL_RANK_0( 2, "  writing cells..." );
  writeCells( cellBlockManager );

  GEOSX_LOG_LEVEL_RANK_0( 2, "  writing surfaces..." );
  writeSurfaces( cellBlockManager );

  GEOSX_LOG_LEVEL_RANK_0( 2, "  building connectivity maps..." );
  cellBlockManager.buildMaps();

  GEOSX_LOG_LEVEL_RANK_0( 2, "  done!" );
  vtk::printMeshStatistics( *m_vtkMesh, m_cellMap, comm );
}

void VTKMeshGenerator::freeResources()
{
  m_vtkMesh = nullptr;
  m_cellMap.clear();
}

REGISTER_CATALOG_ENTRY( MeshGeneratorBase, VTKMeshGenerator, string const &, Group * const )

} // namespace geosx
