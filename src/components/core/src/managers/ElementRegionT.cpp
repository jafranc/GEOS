//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright (c) 2015, Lawrence Livermore National Security, LLC.
//  Produced at the Lawrence Livermore National Laboratory
//
//  GEOS Computational Framework - Core Package, Version 3.0.0
//
//  Written by:
//  Randolph Settgast (settgast1@llnl.gov)
//  Stuart Walsh(walsh24@llnl.gov)
//  Pengcheng Fu (fu4@llnl.gov)
//  Joshua White (white230@llnl.gov)
//  Chandrasekhar Annavarapu Srinivas
//  Eric Herbold
//  Michael Homel
//
//
//  All rights reserved.
//
//  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
//  THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL LAWRENCE LIVERMORE NATIONAL SECURITY,
//  LLC, THE U.S. DEPARTMENT OF ENERGY OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES 
//  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED 
//  AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
//  IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
//  1. This notice is required to be provided under our contract with the U.S. Department of Energy (DOE). This work was produced at Lawrence Livermore 
//     National Laboratory under Contract No. DE-AC52-07NA27344 with the DOE.
//  2. Neither the United States Government nor Lawrence Livermore National Security, LLC nor any of their employees, makes any warranty, express or 
//     implied, or assumes any liability or responsibility for the accuracy, completeness, or usefulness of any information, apparatus, product, or 
//     process disclosed, or represents that its use would not infringe privately-owned rights.
//  3. Also, reference herein to any specific commercial products, process, or services by trade name, trademark, manufacturer or otherwise does not 
//     necessarily constitute or imply its endorsement, recommendation, or favoring by the United States Government or Lawrence Livermore National Security, 
//     LLC. The views and opinions of authors expressed herein do not necessarily state or reflect those of the United States Government or Lawrence 
//     Livermore National Security, LLC, and shall not be used for advertising or product endorsement purposes.
//
//  This Software derives from a BSD open source release LLNL-CODE-656616. The BSD  License statment is included in this distribution in src/bsd_notice.txt.
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/*
 * ElementManagerT.cpp
 *
 *  Created on: Sep 14, 2010
 *      Author: settgast1
 */

#include "ElementRegionT.hpp"

#include <stdlib.h>
//#include "Utilities/Kinematics.h"
#include "legacy/Utilities/Utilities.h"
#include "legacy/IO/BinStream.h"

#include "legacy/ElementLibrary/LagrangeBasis.h"
#include "legacy/ElementLibrary/GaussQuadrature.h"
#include "legacy/ElementLibrary/FiniteElement.h"
#include "legacy/ElementLibrary/SpecializedFormulations/UniformStrainHexahedron.h"
#include "legacy/ElementLibrary/SpecializedFormulations/SimpleTetrahedron.h"
#include "legacy/ElementLibrary/SpecializedFormulations/UniformStrainQuadrilateral.h"
#include "legacy/ElementLibrary/SpecializedFormulations/LinearTriangle.h"
#include "legacy/ElementLibrary/SpecializedFormulations/Line.h"
#include "legacy/ElementLibrary/SpecializedFormulations/QuadrilateralShell.h"
#include "legacy/ElementLibrary/SpecializedFormulations/TriangleShell.h"

#include "legacy/PhysicsSolvers/Lagrange/LagrangeHelperFunctions.h"

#include "FaceManager.hpp"
//#include "legacy/ObjectManagers/PhysicalDomainT.h"

//#include "ElementLibrary/IntegrationRuleT.h"
#include "legacy/DataStructures/Tables/Table.h"

//#include "legacy/Constitutive/Material/MaterialFactory.h"
#include "NodeManager.hpp"

namespace geosx
{

void AddElementResidual( const R2SymTensor& cauchyStress,
                         const Array1dT<R1Tensor>& dNdX,
                         const realT& detJ,
                         const realT& detF,
                         const R2Tensor& Finv,
                         Array1dT<R1Tensor>& force );


void AddElementResidual( const R2SymTensor& cauchyStress,
                         const R1Tensor* const dNdX,
                         const realT& detJ,
                         const realT& detF,
                         const R2Tensor& Finv,
                         Array1dT<R1Tensor>& force );

  const char ElementRegionT::ElementObjectToElementManager[] = "ElementObjectToElementManager";
  const char ElementRegionT::ElementToNode[] = "ElementToNode";
  const char ElementRegionT::ElementToFace[] = "ElementToFace";
  const char ElementRegionT::ElementToEdge[] = "ElementToEdge";
//  const char ElementRegionT::ElementToCrackSurface[] = "ElementToFractureSurface";
//  const char ElementRegionT::ElementToCrackSurfaceVertex[] = "ElementToFractureSurfaceVertex";
//  const char ElementRegionT::ElementToLocalVolume[] = "ElementToLocalVolume";
//  const char ElementRegionT::ElementToPhysicalNodes[] = "ElementToPhysicalNodes";
//  const char ElementRegionT::ElementToCrackToVertexNodes[] = "ElementToCrackToVertexNodes";
//  const char ElementRegionT::ElementToCracks[] = "ElementToCracks";
//
//  region.AddMap<OrderedVariableOneToManyRelation>("ElementToPhysicalNodes");
//
//  region.AddMap< OrderedVariableOneToManyToManyRelation >("ElementToCrackToVertexNodes");
//  region.AddMap< OrderedVariableOneToManyRelation >("ElementToCracks");
//  region.AddKeylessDataField<int>("isPhysical");



ElementRegionT::ElementRegionT( ObjectManagerBase * const parent ):
    ObjectManagerBase( "ElementRegion", parent ),
m_regionName(),
m_regionNumber(0),
m_numElems(this->m_DataLengths),
m_numNodesPerElem(0),
m_numIntegrationPointsPerElem(0),
m_elementType(),
m_elementGeometryID(),
m_ElementDimension(0),
m_dNdX(),
m_detJ(),
m_detJ_n(),
m_detJ_np1(),
m_dUdX(),
m_Finv(),
m_Dadt(),
m_Rot(),
m_Ke(),
m_matrixB(),
m_matrixE(),
m_basis(),
m_quadrature(),
m_finiteElement(NULL),
m_elementQuadrature(NULL),
m_elementBasis(NULL),
//m_materialComputations(NULL),
m_numFacesPerElement(0),
m_numNodesPerFace(0),
m_energy(),
m_hgDamp(0.0),
m_hgStiff(0.0),
m_failStress(std::numeric_limits<realT>::max())
{
  this->AddKeyedDataField<FieldInfo::volume>();
  this->AddKeyedDataField<FieldInfo::mass>();
  this->AddKeyedDataField<FieldInfo::density>();

  this->AddKeyedDataField<FieldInfo::pressure>();
  this->AddKeyedDataField<FieldInfo::deviatorStress>();
  this->AddKeylessDataField<realT>("sigma_x", true, true);
  this->AddKeylessDataField<realT>("sigma_y", true, true);
  this->AddKeylessDataField<realT>("sigma_z", true, true);
  this->AddKeylessDataField<realT>("sigma_xy", false, true);
  this->AddKeylessDataField<realT>("sigma_yz", false, true);
  this->AddKeylessDataField<realT>("sigma_xz", false, true);

  this->AddKeylessDataField<realT>("volume_n", true, true);
}

ElementRegionT::ElementRegionT(const ElementRegionT& init):
    ObjectManagerBase(init),
m_regionName(init.m_regionName),
m_regionNumber(init.m_regionNumber),
m_numElems(this->m_DataLengths),
m_numNodesPerElem(init.m_numNodesPerElem),
m_numIntegrationPointsPerElem(init.m_numIntegrationPointsPerElem),
m_elementType(init.m_elementType),
m_elementGeometryID(init.m_elementGeometryID),
m_ElementDimension(init.m_ElementDimension),
//m_ElementObjectToElementManagerMap(m_OneToOneMaps[ElementObjectToElementManager]),
//m_toNodesRelation(m_FixedOneToManyMaps[ElementToNode]),
//m_toFacesRelation(m_FixedOneToManyMaps[ElementToFace]),
//m_toCrackSurfacesRelation(m_UnorderedVariableOneToManyMaps[ElementToCrackSurface]),
//m_toCrackSurfaceVerticesRelation(m_UnorderedVariableOneToManyMaps[ElementToCrackSurfaceVertex]),
//m_toLocalVolumeRelation(m_UnorderedVariableOneToManyMaps[ElementToLocalVolume]),
//m_toPhysicalNodes(m_VariableOneToManyMaps[ElementToPhysicalNodes]),
//m_toCrackToVertexNodes(m_VariableOneToManyToManyMaps[ElementToCrackToVertexNodes]),
//m_toCracks(m_VariableOneToManyMaps[ElementToCracks]),
//m_toEdgesRelation(m_FixedOneToManyMaps[ElementToEdge]),
m_dNdX(init.m_dNdX),
m_detJ(init.m_detJ),
m_detJ_n(init.m_detJ_n),
m_detJ_np1(init.m_detJ_np1),
m_dUdX(init.m_dUdX),
m_Finv(init.m_Finv),
m_Dadt(init.m_Dadt),
m_Rot(init.m_Rot),
m_Ke(init.m_Ke),
m_basis(),
m_quadrature(),
m_finiteElement(NULL),
m_elementQuadrature(NULL),
m_elementBasis(NULL),
//m_materialComputations(NULL),
m_numFacesPerElement(init.m_numFacesPerElement),
m_numNodesPerFace(init.m_numNodesPerFace),
m_energy(init.m_energy),
m_hgDamp(init.m_hgDamp),
m_hgStiff(init.m_hgStiff)
{
  if (init.m_finiteElement != NULL)
  {
    AllocateElementLibrary(init.m_elementBasis->size(), init.m_elementQuadrature->size());
  }
}

ElementRegionT::~ElementRegionT()
{
#if USECPP11!=1
  if (m_mat)
    delete m_mat;
#endif
  delete m_elementQuadrature;
  delete m_elementBasis;
  delete m_finiteElement;
}

void ElementRegionT::DeserializeObjectField(const std::string& name, const rArray1d& field)
{
  if(m_DataLengths == 0)
    return;
  m_mat->SetValues(name, field);
}

void ElementRegionT::DeserializeObjectFields(const sArray1d& names, const Array1dT<rArray1d>& fields)
{
  if (m_DataLengths == 0)
    return;
  m_mat->SetValues(names, fields);
}

void ElementRegionT::SetGeometryBasedVariables()
{
  if (!m_elementGeometryID.compare(0, 2, "CP"))
  {
    m_ElementDimension = 2;
  }
  else if (!m_elementGeometryID.compare(0, 4, "STRI"))
  {
    m_ElementDimension = 2;
  }
  else if (!m_elementGeometryID.compare(0, 3, "S4R"))
  {
    m_ElementDimension = 3;
  }
  else if (!m_elementGeometryID.compare(0, 4, "TRSH"))
  {
    m_ElementDimension = 3;
  }
  else if (!m_elementGeometryID.compare(0, 2, "C3"))
  {
    m_ElementDimension = 3;
  }
  else
  {
    throw GPException("ElementRegionT::AllocateElementLibrary(): invalid abaqusID");
  }

  if (m_ElementDimension == 2)
  {

    if( !m_elementGeometryID.compare(0, 4, "CPE2") )
    {
      m_numNodesPerElem = 2;
      m_numFacesPerElement = 1;
      m_numNodesPerFace = 2;
    }
    else if( !m_elementGeometryID.compare(0, 4, "CPE3") )
    {
      m_numNodesPerElem = 3;
      m_numFacesPerElement = 3;
      m_numNodesPerFace = 2;
      //      throw GPException("ElementRegionT::AllocateElementLibrary(): CPE3 unimplemented");
    }
    else if (!m_elementGeometryID.compare(0, 4, "CPE4"))
    {
      m_numNodesPerElem = 4;
      m_numFacesPerElement = 4;
      m_numNodesPerFace = 2;
    }
    else if (!m_elementGeometryID.compare(0, 4, "STRI"))
    {
      m_numNodesPerElem = 3;
      m_numFacesPerElement = 3;
      m_numNodesPerFace = 2;
    }
  }
  else if (m_ElementDimension == 3)
  {

    if (!m_elementGeometryID.compare(0, 4, "C3D4"))
    {
      m_numNodesPerElem = 4;
      m_numFacesPerElement = 4;
      m_numNodesPerFace = 3;
    }
    else if (!m_elementGeometryID.compare(0, 4, "C3D8"))
    {
      m_numNodesPerElem = 8;
      m_numFacesPerElement = 6;
      m_numNodesPerFace = 4;
    }
    else if (!m_elementGeometryID.compare(0, 4, "C3D6"))
    {
      m_numNodesPerElem = 8;
      m_numFacesPerElement = 5;
      m_numNodesPerFace = 4;  // We have to do special treatment for the triangular faces of prisms.
    }
    else if (!m_elementGeometryID.compare(0, 3, "S4R"))
    {
      m_numNodesPerElem = 4;
      m_numFacesPerElement = 1;
      m_numNodesPerFace = 4;
    }
    else if (!m_elementGeometryID.compare(0, 4, "TRSH"))
    {
      m_numNodesPerElem = 3;
      m_numFacesPerElement = 1;
      m_numNodesPerFace = 3;
    }

  }
}

void ElementRegionT::AllocateElementLibrary(const int basis, const int quadrature)
{
  if (m_ElementDimension == 2)
  {
    if (!m_elementGeometryID.compare(0, 4, "CPE3"))
    {
      throw GPException("ElementRegionT::AllocateElementLibrary(): CPE3 unimplemented");
    }
    else if (!m_elementGeometryID.compare(0, 4, "CPE4"))
    {
      if (!m_elementType.compare("uniformstrain"))
      {
        m_finiteElement = new UniformStrainQuadrilateral();
        m_numIntegrationPointsPerElem = 1;
        m_finiteElement->m_type = m_elementType;
      }
      else if (!m_elementType.compare(0, 4, "poly"))
      {
        m_elementQuadrature = new GaussQuadrature<2>(quadrature);
        m_elementBasis = new LagrangeBasis<2>(basis);
        m_finiteElement = new FiniteElement<2>(*m_elementBasis, *m_elementQuadrature);
        m_numIntegrationPointsPerElem = quadrature * quadrature;
        m_finiteElement->m_type = m_elementType;
      }
      else
      {
        throw GPException("ElementRegionT::AllocateElementLibrary(): invalid integration for CPE4");
      }
    }
    else if (!m_elementGeometryID.compare(0, 4, "STRI"))
    {
      if (!m_elementType.compare("linear"))
      {
        m_elementQuadrature = new GaussQuadrature<nsdof>(quadrature);
        m_elementBasis = new LagrangeBasis<nsdof>(basis);
        m_finiteElement = new LinearTriangle();
        m_numIntegrationPointsPerElem = quadrature * quadrature;
        m_finiteElement->m_type = m_elementType;
      }
      else
      {
        throw GPException("ElementRegionT::AllocateElementLibrary(): invalid integration for STRI");
      }
    }
    else if( !m_elementGeometryID.compare(0, 4, "CPE2") )
    {
      if ( !m_elementType.compare("linear") )
      {
        m_elementQuadrature = new GaussQuadrature<nsdof>(quadrature);
        m_elementBasis = new LagrangeBasis<nsdof>(basis);
        m_finiteElement = new Line();
        m_numIntegrationPointsPerElem = quadrature;
        m_finiteElement->m_type = m_elementType;

      }
      else
      {
        throw GPException("ElementRegionT::AllocateElementLibrary(): invalid integration for CPE2");
      }
    }
  }
  else if (m_ElementDimension == 3)
  {
    if (!m_elementGeometryID.compare(0, 4, "C3D4"))
    {
      if (!m_elementType.compare("linear"))
      {
        m_elementQuadrature = new GaussQuadrature<nsdof>(quadrature);
        m_elementBasis = new LagrangeBasis<nsdof>(basis);
        m_finiteElement = new SimpleTetrahedron();
        m_numIntegrationPointsPerElem = quadrature * quadrature * quadrature;
        m_finiteElement->m_type = m_elementType;
      }
      else
      {
        throw GPException(
            "ElementRegionT::AllocateElementLibrary(): invalid integration rule for C3D4");
      }
    }
    else if ( (!m_elementGeometryID.compare(0, 4, "C3D8")) || (!m_elementGeometryID.compare(0, 4, "C3D6")) )
    {
      if (!m_elementType.compare("poly"))
      {
        m_elementQuadrature = new GaussQuadrature<nsdof>(quadrature);
        m_elementBasis = new LagrangeBasis<nsdof>(basis);
        m_finiteElement = new FiniteElement<nsdof>(*m_elementBasis, *m_elementQuadrature);
        m_numIntegrationPointsPerElem = quadrature * quadrature * quadrature;
        m_finiteElement->m_type = m_elementType;
      }
      else if (!m_elementType.compare("uniformstrain"))
      {
        m_finiteElement = new UniformStrainHexahedron();
        m_numIntegrationPointsPerElem = 1;
        m_finiteElement->m_type = m_elementType;
      }
      else
      {
        throw GPException("ElementRegionT::AllocateElementLibrary(): invalid integration rule for C3D8 or C3D6");
      }

    }
    else if (!m_elementGeometryID.compare(0, 3, "S4R"))
    {
      if (!m_elementType.compare("flow_only"))
      {
        m_finiteElement = new QuadrilateralShell();
        m_numIntegrationPointsPerElem = 1;
        m_finiteElement->m_type = m_elementType;
      }
      else
      {
        throw GPException(
            "ElementRegionT::AllocateElementLibrary(): invalid integration rule for S4R");
      }
    }
    else if (!m_elementGeometryID.compare(0, 4, "TRSH"))
    {
      if ( !m_elementType.compare("flow_only") )
      {
          m_finiteElement = new TriangleShell();
          m_numIntegrationPointsPerElem = 1;
          m_finiteElement->m_type = m_elementType;
      }
      else
      {
        throw GPException(
            "ElementRegionT::AllocateElementLibrary(): invalid integration rule for TRSH");
      }
    }
  }

  if (m_finiteElement->zero_energy_modes() >= 1)
  {
    this->AddKeylessDataField<R1Tensor>("Qhg1", true);
  }
  if (m_finiteElement->zero_energy_modes() >= 2)
  {
    this->AddKeylessDataField<R1Tensor>("Qhg2", true);
  }
  if (m_finiteElement->zero_energy_modes() >= 3)
  {
    this->AddKeylessDataField<R1Tensor>("Qhg3", true);
  }
  if (m_finiteElement->zero_energy_modes() >= 4)
  {
    this->AddKeylessDataField<R1Tensor>("Qhg4", true);
  }

//  m_material.SetNumberOfIntegrationPointsPerState(m_numIntegrationPointsPerElem);
  m_mat->resize(0, m_numIntegrationPointsPerElem);
}


globalIndex ElementRegionT::resize( const localIndex size, const bool assignGlobals )
{
  const localIndex oldSize = m_DataLengths;

  const globalIndex firstNewGlobalIndex = ObjectDataStructureBaseT::resize(size, assignGlobals);

  m_toNodesRelation.resize2(m_DataLengths, m_numNodesPerElem);
  m_toFacesRelation.resize2(m_DataLengths, m_numFacesPerElement);

  m_dNdX.resize(m_numElems);
  m_dUdX.resize(m_numElems);
  m_Finv.resize(m_numElems);
  m_detJ.resize(m_numElems);
  m_detJ_n.resize(m_numElems);
  m_detJ_np1.resize(m_numElems);
  m_Dadt.resize(m_numElems);
  m_Rot.resize(m_numElems);

//  m_material.resize(m_numElems);
  if (m_mat)
    m_mat->resize(m_numElems, m_numIntegrationPointsPerElem);

  //TODO: change this to be determined based on solver type.  We only use these arrays for small def solvers.
  if (false) //(m_numNodesPerElem==4)
  {
    m_Ke.resize(m_numElems);
    m_matrixB.resize(m_numElems);
    m_matrixE.resize(m_numElems);
  }

  m_dUdX.resize2(m_numElems, m_numIntegrationPointsPerElem);
  m_Finv.resize2(m_numElems, m_numIntegrationPointsPerElem);
  m_detJ.resize2(m_numElems, m_numIntegrationPointsPerElem);
  m_detJ_n.resize2(m_numElems, m_numIntegrationPointsPerElem);
  m_detJ_np1.resize2(m_numElems, m_numIntegrationPointsPerElem);

  R2Tensor Identity;
  Identity.PlusIdentity(1.0);
  m_Finv = Identity;

  for (localIndex k = oldSize; k < m_numElems; ++k)
  {

    m_dNdX(k).resize2(m_numIntegrationPointsPerElem, m_numNodesPerElem);
    m_Dadt(k).resize(m_numIntegrationPointsPerElem);
    m_Rot(k).resize(m_numIntegrationPointsPerElem);

    //TODO: change this to be determined based on solver type.  We only use these arrays for small def solvers.
    if (false) //(m_numNodesPerElem==4)
    {
      m_Ke(k).resize2(m_numNodesPerElem * 3, m_numNodesPerElem * 3);
      m_matrixB(k).resize2(3 * (nsdof - 1), m_numNodesPerElem * 3);
      m_matrixE(k).resize2(3 * (nsdof - 1), 3 * (nsdof - 1));
    }
  }

  return firstNewGlobalIndex;

}

void ElementRegionT::Initialize()
{
  rArray1d& density = this->GetFieldData<FieldInfo::density>();
  rArray1d& mass = this->GetFieldData<FieldInfo::mass>();
  rArray1d& volume = this->GetFieldData<FieldInfo::volume>();

  // For a cracked element, physical volume represents the part of the element with material
  // Initially, physical volume is equal to the volume of the element
  const auto physicalVolume = this->GetFieldDataPointer<realT>("physicalVolume");
//  auto& physicalVolume = this->GetFieldData<realT>("physicalVolume");

  density = 1.0;
  // Fu: I don't know why we do this here.  The element mass field will be updated with real density values when we calcualte nodal mass.

  volume = 0.0;
  for (localIndex k = 0; k < m_numElems; ++k)
  {
    for (unsigned int a = 0; a < m_numIntegrationPointsPerElem; ++a)
    {
      volume[k] += m_detJ[k][a];
    }
    if( physicalVolume != NULL )
      (*physicalVolume)[k] = volume[k];
  }

  for (localIndex k = 0; k < m_numElems; ++k)
  {
    mass[k] = density[k] * volume[k];
//    if (m_mat)
//    {
//      m_mat->InitializeStates(k);
//    }
  }
}

void ElementRegionT::ReadXML(TICPP::HierarchicalDataNode* const erNode, const bool isRestart)
{
//
//  //const std::string erName = erNode->GetAttributeString("name");
//  const std::string erType = erNode->GetAttributeString("elementtype");
//
//  m_basis = erNode->GetAttributeOrDefault<int>("basis", 1);
//  m_quadrature = erNode->GetAttributeOrDefault("quadrature", 2);
//
//    if (!m_elementGeometryID.compare(0, 4, "C3D4") || !m_elementGeometryID.compare(0, 4, "STRI") || !m_elementGeometryID.compare(0, 4, "CPE2") )
//      m_quadrature = 1;
//
//  m_elementType = erType;
//
//  m_parentFaceSetNames = erNode->GetStringVector("parentFaceSetNames");
//
//  {
//    TICPP::HierarchicalDataNode* matNode = erNode->Next(true);
//    if(!matNode)
//      throw GPException("Need to have one (and only one) material defined for the element region");
//    const std::string matName(matNode->Heading());
//#if USECPP11!=1
//    if (m_mat)
//      delete m_mat;
//#endif
//    m_mat = MaterialFactory::NewMaterial(matName,matNode);
//    m_mat->resize(m_DataLengths, 1);
//    m_mat->ReadXML(*matNode);
//
//    m_plotMat = matNode->GetAttributeOrDefault("write2Plot", false);
//  }
//
//  if (!isRestart)
//  {
//    AllocateElementLibrary(m_basis, m_quadrature);
//  }
//  m_hgDamp = erNode->GetAttributeOrDefault<realT>("hgDamp", 0.0);
//  m_hgStiff = erNode->GetAttributeOrDefault<realT>("hgStiff", 0.01);

}

void ElementRegionT::SetDomainBoundaryObjects(const ObjectDataStructureBaseT* const referenceObject)
{
  referenceObject->CheckObjectType(ObjectDataStructureBaseT::FaceManager);
  const FaceManagerT& faceManager = static_cast<const FaceManagerT&>(*referenceObject);

  const iArray1d& isFaceOnDomainBoundary = faceManager.GetFieldData<FieldInfo::isDomainBoundary>();

  iArray1d& isElemOnDomainBoundary = this->GetFieldData<FieldInfo::isDomainBoundary>();
  isElemOnDomainBoundary = 0;

  for (localIndex k = 0; k < m_numElems; ++k)
  {
    const localIndex* const faceIndicies = m_toFacesRelation[k];
    for (int lf = 0; lf < this->m_numFacesPerElement; ++lf)
    {
      const localIndex faceIndex = faceIndicies[lf];
      if (isFaceOnDomainBoundary(faceIndex) == 1)
      {
        isElemOnDomainBoundary(k) = 1;
      }
    }
  }

}

int ElementRegionT::CalculateShapeFunctionDerivatives(const NodeManager& nodeManager)
{

  Array1dT<R1Tensor> X(this->m_numNodesPerElem);

  const Array1dT<R1Tensor>& referencePosition = nodeManager.GetFieldData<
      FieldInfo::referencePosition>();

  if (m_finiteElement != NULL)
  {
    for (localIndex k = 0; k < m_numElems; ++k)
    {

      const localIndex* const elemToNodeMap = m_toNodesRelation[k];

//      nodeManager.CopyGlobalFieldToLocalField<FieldInfo::referencePosition>( elemToNodeMap, X );
      CopyGlobalToLocal(elemToNodeMap, referencePosition, X);

      m_finiteElement->reinit(X);

      for (unsigned int a = 0; a < m_numIntegrationPointsPerElem; ++a)
      {

        m_detJ(k, a) = m_finiteElement->JxW(a);
        for (unsigned int b = 0; b < m_numNodesPerElem; ++b)
        {
          m_dNdX(k)(a, b) = m_finiteElement->gradient(b, a);
        }
        //std::cout<<"Element, ip, dNdX :"<<k<<", "<<a<<", "<<m_dNdX(k)(a)[0]<<std::endl;

      }
    }
  }

  m_detJ_n = m_detJ;
  m_detJ_np1 = m_detJ;

  return 0;
}

//int ElementRegionT::CalculateShapeFunctionDerivativesCutElements(const NodeManagerT& nodeManager)
//{
//
//  Array1dT<R1Tensor> X(this->m_numNodesPerElem);
//
//  const Array1dT<R1Tensor>& referencePosition = nodeManager.GetFieldData<
//      FieldInfo::referencePosition>();
//
//  rArray1d& physicalVolume = this->GetFieldData<realT>("physicalVolume");
//  rArray1d& volume = this->GetFieldData<FieldInfo::volume>();
//  realT volFrac;
//
//  if (m_finiteElement != NULL)
//  {
//    for (localIndex k = 0; k < m_numElems; ++k)
//    {
//
//      volFrac = physicalVolume[k]/volume[k];
//      const localIndex* const elemToNodeMap = m_toNodesRelation[k];
//
//      CopyGlobalToLocal(elemToNodeMap, referencePosition, X);
//
//      m_finiteElement->reinit(X);
//
//      for (unsigned int a = 0; a < m_numIntegrationPointsPerElem; ++a)
//      {
//
//        m_detJ(k, a) = volFrac*(m_finiteElement->JxW(a));
//
//        for (unsigned int b = 0; b < m_numNodesPerElem; ++b)
//        {
//          m_dNdX(k)(a, b) = m_finiteElement->gradient(b, a);
//        }
//      }
//    }
//  }
//
//  m_detJ_n = m_detJ;
//  m_detJ_np1 = m_detJ;
//
//  return 0;
//}

int ElementRegionT::CalculateVelocityGradients(const NodeManager& nodeManager, const int calcGroup)
{
  R2SymTensor tempStress;

  R2Tensor A;
  R2Tensor F;
  R2Tensor dUhatdX;

  Array1dT<R1Tensor> u_local(this->m_numNodesPerElem);
  Array1dT<R1Tensor> uhat_local(this->m_numNodesPerElem);

  const Array1dT<R1Tensor>& incrementalDisplacement = nodeManager.GetFieldData<
      FieldInfo::incrementalDisplacement>();
  const Array1dT<R1Tensor>& totalDisplacement = nodeManager.GetFieldData<FieldInfo::displacement>();

//  const iArray1d& ghostRank = this->GetFieldData<FieldInfo::ghostRank>();
//  const iArray1d& attachedToSendingGhostNode = GetFieldData<int>("attachedToSendingGhostNode");

  rArray1d& volume = GetFieldData<FieldInfo::volume>();
  rArray1d& volume_n = GetFieldData<realT>("volume_n");

  volume_n = volume;
  m_detJ_n = m_detJ_np1;

  for (localIndex k = 0; k < m_numElems; ++k)
  {
//    if( ghostRank[k] < 0 && attachedToSendingGhostNode[k]==calcGroup )
    {

      const localIndex* const elemToNodeMap = m_toNodesRelation[k];

      CopyGlobalToLocal(elemToNodeMap, incrementalDisplacement, totalDisplacement, uhat_local,
                        u_local);

      volume[k] = 0.0;
      for (unsigned int a = 0; a < m_numIntegrationPointsPerElem; ++a)
      {

        // calculate dUhat/dX
        CalculateGradient(dUhatdX, uhat_local, m_dNdX[k][a]);

        // calculate gradient
        m_dUdX[k][a] += dUhatdX;
        F = m_dUdX[k][a];
        F.PlusIdentity(1.0);

        // calculate element volume
        m_detJ_np1[k][a] = m_detJ[k][a] * F.Det();
        volume[k] += m_detJ_np1[k][a];

        m_Finv[k][a].Inverse(F);

        // Calculate Rate of deformation tensor and rotationAxis tensor
        A.AijBjk(dUhatdX, m_Finv[k][a]);
        IncrementalKinematics(A, m_Dadt[k](a), m_Rot[k](a));

      }
    }

  }

  return 0;
}


int ElementRegionT::MaterialUpdate(const realT dt)
{
//  const iArray1d& ghostRankAll = this->GetFieldData<FieldInfo::ghostRank>();
//  m_energy.Zero();
//
//  for (localIndex k = 0; k < m_numElems; ++k)
//  {
////    MaterialBaseParameterDataT& parameter = m_material.MaterialParameter(k);
//
//    const int ghostRank = ghostRankAll[k];
//    //if( ghostRank < 0 )
//    {
//      for (unsigned int a = 0; a < m_numIntegrationPointsPerElem; ++a)
//      {
//        MaterialBaseStateData& state = *(m_mat->StateData(k, a));
//        R2Tensor L; //just stub in since this whole function is being removed
//        m_mat->StrainDrivenUpdateMember(k, a, m_Dadt[k][a], L, m_Rot[k][a], m_detJ_n[k][a],
//                                        m_detJ_np1[k][a], dt);
//        if (ghostRank < 0)
//        {
//          m_energy.IncrementStressPower(state.StressPower);
//          m_energy.IncrementStrainEnergy(state.ElasticStrainEnergy);
//          m_energy.IncrementDissipatedEnergy(state.DissipatedEnergy);
//        }
//      }
//    }
//  }

  return 0;
}

int ElementRegionT::CalculateNodalForces(NodeManager& nodeManager, StableTimeStep& timeStep,
                                         const realT dt)
{
  if (nodeManager.DataLengths() == 0)
    return 1;

  R2SymTensor totalStress;
  R2Tensor F;
  Array1dT<R1Tensor> f_local(m_numNodesPerElem);

  Array1dT<R1Tensor> x(m_numNodesPerElem);
  Array1dT<R1Tensor> u(m_numNodesPerElem);
  Array1dT<R1Tensor> v(m_numNodesPerElem);
  Array1dT<R1Tensor> dNdx(m_numNodesPerElem);
  Array1dT<R1Tensor> f_zemc(m_numNodesPerElem);

  const Array1dT<R1Tensor>& referencePosition = nodeManager.GetFieldData<
      FieldInfo::referencePosition>();
  const Array1dT<R1Tensor>& totalDisplacement = nodeManager.GetFieldData<FieldInfo::displacement>();
  const Array1dT<R1Tensor>& velocity = nodeManager.GetFieldData<FieldInfo::velocity>();
  Array1dT<R1Tensor>& force = nodeManager.GetFieldData<FieldInfo::force>();
  Array1dT<R1Tensor>& hgforce = nodeManager.GetFieldData<FieldInfo::hgforce>();

//  hgforce = 0.0;
  Array1dT<Array1dT<R1Tensor>*> Qstiffness(m_finiteElement->zero_energy_modes(), NULL);
  if (m_finiteElement->zero_energy_modes() >= 1)
  {
    Qstiffness[0] = &(this->GetFieldData<R1Tensor>("Qhg1"));
  }
  if (m_finiteElement->zero_energy_modes() >= 2)
  {
    Qstiffness[1] = &(this->GetFieldData<R1Tensor>("Qhg2"));
  }
  if (m_finiteElement->zero_energy_modes() >= 3)
  {
    Qstiffness[2] = &(this->GetFieldData<R1Tensor>("Qhg3"));
  }
  if (m_finiteElement->zero_energy_modes() >= 4)
  {
    Qstiffness[3] = &(this->GetFieldData<R1Tensor>("Qhg4"));
  }

//  const iArray1d& ghostRank = this->GetFieldData<FieldInfo::ghostRank>();

  Array1dT<R1Tensor> Q(m_finiteElement->zero_energy_modes());

  for (localIndex k = 0; k < m_numElems; ++k)
  {

    const localIndex paramIndex = m_mat->NumParameterIndex0() > 1 ? k : 0;
    const MaterialBaseParameterData& parameter = *(m_mat->ParameterData(paramIndex));
//    if( ghostRank[k] < 0 )
    {

      const localIndex* const elemToNodeMap = m_toNodesRelation[k];

      for (unsigned int a = 0; a < m_numIntegrationPointsPerElem; ++a)
      {
        const MaterialBaseStateData& state = *(m_mat->StateData(k, a));

        state.TotalStress(totalStress);

        f_local = 0.0;
        F = m_dUdX[k][a];
        F.PlusIdentity(1.0);
        realT detF = F.Det();

        AddElementResidual(totalStress, m_dNdX[k][a], m_detJ[k][a], detF, m_Finv[k][a], f_local);

        realT BB = 0.0;
        for (unsigned int b = 0; b < this->m_numNodesPerElem; ++b)
        {
          dNdx(b).AijBi(m_Finv[k][a], m_dNdX[k][a][b]);
          BB += Dot(dNdx(b), dNdx(b));
        }

        realT thisdt = LagrangeHelperFunctions::CalculateMaxStableExplicitTimestep(
            parameter.init_density / fabs(detF), parameter.Lame + 2 * parameter.init_shearModulus,
            BB);

        //      realT thisdt = sqrt( m_materialParameters->init_density / ( fabs(detF) * 2 *( m_materialParameters->Lame + 2*m_materialParameters->ShearModulus )*BB ) );
        if (this->m_ElementDimension == 3)
        {
          thisdt /= sqrt(2.0);
        }

        if (thisdt < timeStep.m_maxdt)
        {
//        timeStep.m_region = this->m_regionName;
//        timeStep.m_index = k;
          timeStep.m_maxdt = thisdt;
        }

        if (m_finiteElement->zero_energy_modes())
        {

          CopyGlobalToLocal(elemToNodeMap, referencePosition, totalDisplacement, velocity, x, u, v);

          x += u;

          for (int m = 0; m < m_finiteElement->zero_energy_modes(); ++m)
          {
            Q[m] = (*(Qstiffness[m]))[k];
          }
          m_finiteElement->zero_energy_mode_control(
              dNdx, m_detJ[k][a], x, v, m_hgDamp, m_hgStiff * dt, parameter.init_density,
              parameter.Lame + 2 * parameter.init_shearModulus, dt, Q, f_zemc);

          for (int m = 0; m < m_finiteElement->zero_energy_modes(); ++m)
          {
            (*(Qstiffness[m]))[k] = Q[m];
          }

          AddLocalToGlobal(elemToNodeMap, f_zemc, f_zemc, force, hgforce);

        }

        /*
         realT elementMass = m_materialParameters->init_density * m_detJ(k)(a);
         R1Tensor bodyforce;
         bodyforce[2] = - 0.125 * elementMass * 9.81;
         f_local += bodyforce;
         */

        AddLocalToGlobal(elemToNodeMap, f_local, force);

      }
    }
  }
  return 0;
}

int ElementRegionT::CalculateNodalForcesFromOneElement(const localIndex nodeID,
                                                       const localIndex elemID,
                                                       NodeManager& nodeManager, R1Tensor& fNode)
{
  // The force is weighted by the Young's modulus.  This was merely for the convenience of calculating SIF.

  if (nodeManager.DataLengths() == 0)
    return 1;

  fNode = 0.0;
  // Array1dT<R1Tensor> fOnNode_Local(m_numNodesPerElem);

  R2SymTensor totalStress;
  R2Tensor F;
  Array1dT<R1Tensor> f_local(m_numNodesPerElem);

  Array1dT<R1Tensor> x(m_numNodesPerElem);
  Array1dT<R1Tensor> u(m_numNodesPerElem);
  Array1dT<R1Tensor> v(m_numNodesPerElem);
  Array1dT<R1Tensor> dNdx(m_numNodesPerElem);
  Array1dT<R1Tensor> f_zemc(m_numNodesPerElem);

//  const Array1dT<R1Tensor>& referencePosition = nodeManager.GetFieldData<FieldInfo::referencePosition>();
//  const Array1dT<R1Tensor>& totalDisplacement = nodeManager.GetFieldData<FieldInfo::displacement>();
//  const Array1dT<R1Tensor>& velocity = nodeManager.GetFieldData<FieldInfo::velocity>();
//  Array1dT<R1Tensor>& force = nodeManager.GetFieldData<FieldInfo::force>();
  //Array1dT<R1Tensor>& hgforce = nodeManager.GetFieldData<FieldInfo::hgforce>();

  const localIndex paramIndex = m_mat->NumParameterIndex0() > 1 ? elemID : 0;
  const MaterialBaseParameterData& parameter = *(m_mat->ParameterData(paramIndex));

  const localIndex* const elemToNodeMap = m_toNodesRelation[elemID];

  for (unsigned int a = 0; a < m_numIntegrationPointsPerElem; ++a)
  {
    const MaterialBaseStateData& state = *(m_mat->StateData(elemID, a));

    state.TotalStress(totalStress);

    f_local = 0.0;
    F = m_dUdX[elemID][a];
    F.PlusIdentity(1.0);
    realT detF = F.Det();

    AddElementResidual(totalStress, m_dNdX[elemID][a], m_detJ[elemID][a], detF, m_Finv[elemID][a],
                       f_local);

    for (localIndex i = 0; i < m_numNodesPerElem; ++i)
    {
      if (nodeID == elemToNodeMap[i])
      {
        fNode += f_local[i] * parameter.E;
      }
    }
  }

  return 0;
}

// Need this value for opening-based SIF calculation
realT ElementRegionT::ElementGDivBeta(const localIndex elemID)
{
  const localIndex paramIndex = m_mat->NumParameterIndex0() > 1 ? elemID : 0;
  const MaterialBaseParameterData& parameter = *(m_mat->ParameterData(paramIndex));
  return (parameter.init_shearModulus/2/(1-parameter.Nu));
}

void ElementRegionT::CalculateNodalForceFromStress(const localIndex elemID,
                                   const NodeManager& nodeManager,
                                   R2SymTensor& stress,
                                   Array1dT<R1Tensor>& fNode)
{

  fNode = 0.0;
  Array1dT<R1Tensor> f_local(m_numNodesPerElem);

  R2Tensor F;

  Array1dT<R1Tensor> dNdx(m_numNodesPerElem);

//  const localIndex paramIndex = m_mat->NumParameterIndex0() > 1 ? elemID : 0;
//  const MaterialBaseParameterData& parameter = *(m_mat->ParameterData(paramIndex));

//  const localIndex* const elemToNodeMap = m_toNodesRelation[elemID];

  for (unsigned int a = 0; a < m_numIntegrationPointsPerElem; ++a)
  {

    f_local = 0.0;
    F = m_dUdX[elemID][a];
    F.PlusIdentity(1.0);
    realT detF = F.Det();

    AddElementResidual(stress, m_dNdX[elemID][a], m_detJ[elemID][a], detF, m_Finv[elemID][a],
                       f_local);
    fNode += f_local;

  }

}

int ElementRegionT::CalculateSmallDeformationNodalForces(NodeManager& nodeManager, StableTimeStep&,
                                                         const realT)
{
  Array1dT<R1Tensor> f_local(m_numNodesPerElem);
//  realT f_vec;
//  realT u_vec[m_numNodesPerElem*3];

  Array1dT<R1Tensor> u(m_numNodesPerElem);
  const Array1dT<R1Tensor>& totalDisplacement = nodeManager.GetFieldData<FieldInfo::displacement>();
  Array1dT<R1Tensor>& force = nodeManager.GetFieldData<FieldInfo::force>();

  for (localIndex k = 0; k < m_numElems; ++k)
  {
    const localIndex* const elemToNodeMap = m_toNodesRelation[k];

    CopyGlobalToLocal(elemToNodeMap, totalDisplacement, u);

    realT* const u_vec = &(u[0][0]);

    /*
     for (localIndex i = 0; i < m_numNodesPerElem; i++)
     {
     for (localIndex j = 0; j < 3; j++)
     u_vec[i * 3 + j] = u[i][j];
     }

     for (localIndex a = 0; a < m_numNodesPerElem; a++)
     {
     for (localIndex iDOF = 0; iDOF < nsdof; iDOF++)
     {
     int iVec = a*3 + iDOF;
     f_vec=0.0;

     for (localIndex j = 0; j < m_numNodesPerElem * nsdof; j++)
     {
     f_vec -= m_Ke[k](iVec, j) * u_vec[j];
     }

     f_local[a][iDOF] = f_vec;
     }
     }
     */
    const realT* const Ke = m_Ke[k].data();
    realT* const ptr_f_local = &(f_local[0][0]);
    for (localIndex a = 0; a < m_numNodesPerElem * nsdof; a++)
    {
      ptr_f_local[a] = 0.0;
      for (rArray2d::size_type b = 0; b < m_numNodesPerElem * nsdof; ++b)
      {
        int count = a * m_numNodesPerElem * nsdof + b;
        ptr_f_local[a] -= Ke[count] * u_vec[b];
      }
    }

    AddLocalToGlobal(elemToNodeMap, f_local, force);

  }
  return 0;
}

int ElementRegionT::CalculateNodalMasses(NodeManager& nodeManager)
{

  rArray1d& mass = nodeManager.GetFieldData<FieldInfo::mass>();
  rArray1d& massEle = this->GetFieldData<FieldInfo::mass>();
  rArray1d& density = this->GetFieldData<FieldInfo::density>();
  rArray1d * const volume = nodeManager.GetFieldDataPointer<FieldInfo::volume>();


  for (localIndex k = 0; k < m_numElems; ++k)
  {
    realT elemMass = 0.0;
    realT elemVolume = 0.0;

    const localIndex paramIndex = m_mat->NumParameterIndex0() > 1 ? k : 0;

    const MaterialBaseParameterData& parameter = *(m_mat->ParameterData(paramIndex));
    for (unsigned int a = 0; a < m_numIntegrationPointsPerElem; ++a)
    {
      elemMass += parameter.init_density * this->m_detJ[k][a];
      elemVolume += this->m_detJ[k][a];;
    }
    massEle[k] = elemMass;
    density[k] = parameter.init_density;
    rArray1d nodalMass(m_numNodesPerElem);
    rArray1d nodalVolume(m_numNodesPerElem);

    nodalVolume = elemVolume / m_numNodesPerElem;
    nodalMass = elemMass / m_numNodesPerElem;
    const localIndex* const elemToNodeMap = m_toNodesRelation[k];

    if( volume == nullptr )
    {
      AddLocalToGlobal(elemToNodeMap,
                       nodalMass,
                       mass );
    }
    else
    {
      AddLocalToGlobal(elemToNodeMap,
                       nodalMass, nodalVolume,
                       mass, *volume );
    }
  }
  return 0;
}

void ElementRegionT::SetIsAttachedToSendingGhostNode(const NodeManager& nodeManager)
{
  const iArray1d& nodeGhostRank = nodeManager.GetFieldData<FieldInfo::ghostRank>();
  Array1dT<NodeManager::nodeToElemType> nodeToElements = nodeManager.m_toElementsRelation;

  iArray1d& attachedToSendingGhostNode = GetFieldData<int>("attachedToSendingGhostNode");
  attachedToSendingGhostNode = 0;

  for (localIndex k = 0; k < m_numElems; ++k)
  {
    const localIndex* const elemToNodeMap = m_toNodesRelation[k];

    for (localIndex a = 0; a < nodeManager.DataLengths(); ++a)
    {

      if (nodeGhostRank[elemToNodeMap[a]] == -1)
      {
        attachedToSendingGhostNode[k] = 1;
      }
    }
  }

}

void AddElementResidual(const R2SymTensor& cauchyStress, const Array1dT<R1Tensor>& dNdX,
                        const realT& detJ, const realT& detF, const R2Tensor& Finv,
                        Array1dT<R1Tensor>& force)
{
  R2Tensor P;

  realT integration_factor;

  integration_factor = detJ * detF;

  P.AijBkj(cauchyStress, Finv);
  P *= integration_factor;

  for (Array1dT<R1Tensor>::size_type a = 0; a < force.size(); ++a) // loop through all shape functions in element
  {
    force(a).minusAijBj(P, dNdX(a));
  }

}

void AddElementResidual(const R2SymTensor& cauchyStress, const R1Tensor* const dNdX,
                        const realT& detJ, const realT& detF, const R2Tensor& Finv,
                        Array1dT<R1Tensor>& force)
{
  R2Tensor P;

  realT integration_factor;

  integration_factor = detJ * detF;

  P.AijBkj(cauchyStress, Finv);
  P *= integration_factor;

  for (Array1dT<R1Tensor>::size_type a = 0; a < force.size(); ++a) // loop through all shape functions in element
  {
    force(a).minusAijBj(P, dNdX[a]);
  }

}

/// Get element neighbors within the element region
void ElementRegionT::GetElementNeighbors(localIndex el, const FaceManagerT& faceManager,
                                         std::set<localIndex>& neighbors) const
{

  localIndex* const facelist = m_toFacesRelation[el];
  for (unsigned int kf = 0; kf < this->m_toFacesRelation.Dimension(1); ++kf)
  {
    localIndex fc = facelist[kf];

    const Array1dT<std::pair<ElementRegionT*, localIndex> >& nbrs = faceManager.m_toElementsRelation[fc];
    if (nbrs.size() > 1)
    {
      ElementIdPair nbr = nbrs[0];
      if (nbr.second == el && nbr.first == this)
      {
        nbr = nbrs[1];
      }
      if (nbr.first == this)
      {
        neighbors.insert(nbr.second);
      }
    }
  }
}

void ElementRegionT::GetFaceNodes(const localIndex elementIndex, const localIndex localFaceIndex,
                                  lArray1d& nodeIndicies) const
{
  // get nodelist for this element
  const localIndex* const elemToNodeMap = m_toNodesRelation[elementIndex];

  // resize the nodeIndicies based on element type (this is wrong for some types of elements)
  nodeIndicies.resize(m_numNodesPerFace);

  if (!m_elementGeometryID.compare(0, 4, "C3D8"))
  {
    if (localFaceIndex == 0)
    {
      nodeIndicies[0] = elemToNodeMap[0];
      nodeIndicies[1] = elemToNodeMap[1];
      nodeIndicies[2] = elemToNodeMap[5];
      nodeIndicies[3] = elemToNodeMap[4];
    }
    else if (localFaceIndex == 1)
    {
      nodeIndicies[0] = elemToNodeMap[0];
      nodeIndicies[1] = elemToNodeMap[2];
      nodeIndicies[2] = elemToNodeMap[3];
      nodeIndicies[3] = elemToNodeMap[1];
    }
    else if (localFaceIndex == 2)
    {
      nodeIndicies[0] = elemToNodeMap[0];
      nodeIndicies[1] = elemToNodeMap[4];
      nodeIndicies[2] = elemToNodeMap[6];
      nodeIndicies[3] = elemToNodeMap[2];
    }
    else if (localFaceIndex == 3)
    {
      nodeIndicies[0] = elemToNodeMap[1];
      nodeIndicies[1] = elemToNodeMap[3];
      nodeIndicies[2] = elemToNodeMap[7];
      nodeIndicies[3] = elemToNodeMap[5];
    }
    else if (localFaceIndex == 4)
    {
      nodeIndicies[0] = elemToNodeMap[3];
      nodeIndicies[1] = elemToNodeMap[2];
      nodeIndicies[2] = elemToNodeMap[6];
      nodeIndicies[3] = elemToNodeMap[7];
    }
    else if (localFaceIndex == 5)
    {
      nodeIndicies[0] = elemToNodeMap[4];
      nodeIndicies[1] = elemToNodeMap[5];
      nodeIndicies[2] = elemToNodeMap[7];
      nodeIndicies[3] = elemToNodeMap[6];
    }

  }
  else if (!m_elementGeometryID.compare(0, 4, "C3D6"))
  {
    if (localFaceIndex == 0)
    {
      nodeIndicies[0] = elemToNodeMap[0];
      nodeIndicies[1] = elemToNodeMap[1];
      nodeIndicies[2] = elemToNodeMap[5];
      nodeIndicies[3] = elemToNodeMap[4];
    }
    else if (localFaceIndex == 1)
    {
      nodeIndicies[0] = elemToNodeMap[0];
      nodeIndicies[1] = elemToNodeMap[2];
      nodeIndicies[2] = elemToNodeMap[3];
      nodeIndicies[3] = elemToNodeMap[1];
    }
    else if (localFaceIndex == 2)
    {
      nodeIndicies[0] = elemToNodeMap[0];
      nodeIndicies[1] = elemToNodeMap[2];
      nodeIndicies[2] = elemToNodeMap[4];
      nodeIndicies[3] = std::numeric_limits<localIndex>::max();
    }
    else if (localFaceIndex == 3)
    {
      nodeIndicies[0] = elemToNodeMap[1];
      nodeIndicies[1] = elemToNodeMap[3];
      nodeIndicies[2] = elemToNodeMap[5];
      nodeIndicies[3] = std::numeric_limits<localIndex>::max();
    }
    else if (localFaceIndex == 4)
    {
      nodeIndicies[0] = elemToNodeMap[2];
      nodeIndicies[1] = elemToNodeMap[3];
      nodeIndicies[2] = elemToNodeMap[5];
      nodeIndicies[3] = elemToNodeMap[4];
    }
  }

  else if (!m_elementGeometryID.compare(0, 4, "C3D4"))
  {
    if (localFaceIndex == 0)
    {
      nodeIndicies[0] = elemToNodeMap[0];
      nodeIndicies[1] = elemToNodeMap[2];
      nodeIndicies[2] = elemToNodeMap[1];
    }
    else if (localFaceIndex == 1)
    {
      nodeIndicies[0] = elemToNodeMap[0];
      nodeIndicies[1] = elemToNodeMap[1];
      nodeIndicies[2] = elemToNodeMap[3];
    }
    else if (localFaceIndex == 2)
    {
      nodeIndicies[0] = elemToNodeMap[0];
      nodeIndicies[1] = elemToNodeMap[3];
      nodeIndicies[2] = elemToNodeMap[2];
    }
    else if (localFaceIndex == 3)
    {
      nodeIndicies[0] = elemToNodeMap[1];
      nodeIndicies[1] = elemToNodeMap[2];
      nodeIndicies[2] = elemToNodeMap[3];
    }
  }

  else if ( !m_elementGeometryID.compare(0,4,"CPE2") )
  {
    if( localFaceIndex == 0 )
    {
      nodeIndicies[0] = elemToNodeMap[0];
      nodeIndicies[1] = elemToNodeMap[1];
    }
  }

  else if ( !m_elementGeometryID.compare(0,4,"CPE3") )
  {
    if( localFaceIndex == 0 )
    {
      nodeIndicies[0] = elemToNodeMap[0];
      nodeIndicies[1] = elemToNodeMap[1];
    }
    else if( localFaceIndex == 1 )
    {
      nodeIndicies[0] = elemToNodeMap[1];
      nodeIndicies[1] = elemToNodeMap[2];
    }
    else if( localFaceIndex == 2 )
    {
      nodeIndicies[0] = elemToNodeMap[2];
      nodeIndicies[1] = elemToNodeMap[0];
    }
  }

  else if (!m_elementGeometryID.compare(0, 4, "CPE4"))
  {
    if (localFaceIndex == 0)
    {
      nodeIndicies[0] = elemToNodeMap[0];
      nodeIndicies[1] = elemToNodeMap[1];
    }
    else if (localFaceIndex == 1)
    {
      nodeIndicies[0] = elemToNodeMap[1];
      nodeIndicies[1] = elemToNodeMap[3];
    }
    else if (localFaceIndex == 2)
    {
      nodeIndicies[0] = elemToNodeMap[3];
      nodeIndicies[1] = elemToNodeMap[2];
    }
    else if (localFaceIndex == 3)
    {
      nodeIndicies[0] = elemToNodeMap[2];
      nodeIndicies[1] = elemToNodeMap[0];
    }
  }

  else if (!m_elementGeometryID.compare(0, 4, "STRI"))
  {
    if (localFaceIndex == 0)
    {
      nodeIndicies[0] = elemToNodeMap[0];
      nodeIndicies[1] = elemToNodeMap[1];
    }
    else if (localFaceIndex == 1)
    {
      nodeIndicies[0] = elemToNodeMap[1];
      nodeIndicies[1] = elemToNodeMap[2];
    }
    else if (localFaceIndex == 2)
    {
      nodeIndicies[0] = elemToNodeMap[2];
      nodeIndicies[1] = elemToNodeMap[0];
    }
  }

  else if (!m_elementGeometryID.compare(0, 3, "S4R"))
  {
    if (localFaceIndex == 0)
    {
      nodeIndicies[0] = elemToNodeMap[0];
      nodeIndicies[1] = elemToNodeMap[1];
      nodeIndicies[2] = elemToNodeMap[2];
      nodeIndicies[3] = elemToNodeMap[3];
    }
  }

  else if (!m_elementGeometryID.compare(0, 4, "TRSH"))
  {
    if (localFaceIndex == 0)
    {
      nodeIndicies[0] = elemToNodeMap[0];
      nodeIndicies[1] = elemToNodeMap[1];
      nodeIndicies[2] = elemToNodeMap[2];
    }
  }

  else
  {
    throw GPException("Error.  Don't know what kind of element this is and cannot build faces.");
  }

}

R1Tensor ElementRegionT::GetElementCenter(localIndex k, const NodeManager& nodeManager, const bool useReferencePos) const
{
  const Array1dT< R1Tensor >& refPosition = nodeManager.GetFieldData<FieldInfo::referencePosition>();
  const Array1dT< R1Tensor >& displacement = nodeManager.GetFieldData<FieldInfo::displacement>();

  const localIndex* const nodelist = m_toNodesRelation[k];
  R1Tensor elementCenter(0.0);
  for (unsigned int a = 0; a < m_numNodesPerElem; ++a)
  {
    const localIndex b = nodelist[a];
    elementCenter += refPosition[b];
    if(!useReferencePos)
      elementCenter += displacement[b];
  }
  elementCenter /= realT(m_numNodesPerElem);

  return elementCenter;
}

bool ElementRegionT::ContainsElement(const ElementIdPair& ep)
{
  return (ep.first->m_regionNumber == m_regionNumber) && (ep.second < m_numElems);
}

/**
 *
 * @param buffer
 * @param elementList
 * @param nodeManager
 * @return
 */
template< typename T_indices >
unsigned int ElementRegionT::PackElements( bufvector& buffer,
                                           lSet& sendnodes,
                                           lSet& sendfaces,
                                           const T_indices& elementList,
                                           const NodeManager& nodeManager,
                                           const FaceManagerT& faceManager,
                                           const bool packConnectivityToGlobal,
                                           const bool packFields,
                                           const bool packMaps,
                                           const bool packSets  ) const
{
  unsigned int sizeOfPacked = 0;

  sizeOfPacked += ObjectDataStructureBaseT::PackBaseObjectData(buffer, elementList, packFields,
                                                               packMaps, packSets,
                                                               packConnectivityToGlobal);

  for (typename T_indices::const_iterator elementIndex = elementList.begin();
      elementIndex != elementList.end(); ++elementIndex)
  {
    const localIndex* const nodelist = m_toNodesRelation[*elementIndex];
    for (unsigned int a = 0; a < this->m_toNodesRelation.Dimension(1); ++a)
    {
      sendnodes.insert(nodelist[a]);

      globalIndex gnode = LOCALINDEX_MAX;
      if (packConnectivityToGlobal)
      {
        gnode = nodeManager.m_localToGlobalMap(nodelist[a]);
      }
      else
      {
        gnode = nodelist[a];
      }

      sizeOfPacked += buffer.Pack(gnode);
    }

    const localIndex* const facelist = m_toFacesRelation[*elementIndex];
    for (unsigned int kf = 0; kf < this->m_toFacesRelation.Dimension(1); ++kf)
    {
      sendfaces.insert(facelist[kf]);

      globalIndex gface = LOCALINDEX_MAX;
      if (packConnectivityToGlobal)
      {
        gface = faceManager.m_localToGlobalMap(facelist[kf]);
      }
      else
      {
        gface = facelist[kf];
      }
      sizeOfPacked += buffer.Pack(gface);

    }
  }

  if (packFields)
  {
    this->PackAllFieldsIntoBuffer(buffer, elementList);
  }

  return sizeOfPacked;
}

template unsigned int ElementRegionT::PackElements( bufvector& buffer,
                                                    lSet& sendnodes,
                                                    lSet& sendfaces,
                                                    const lArray1d& elementList,
                                                    const NodeManager& nodeManager,
                                                    const FaceManagerT& faceManager,
                                                    const bool packConnectivityToGlobal,
                                                    const bool packFields,
                                                    const bool packMaps,
                                                    const bool packSets ) const;

template unsigned int ElementRegionT::PackElements( bufvector& buffer,
                                                    lSet& sendnodes,
                                                    lSet& sendfaces,
                                                    const lSet& elementList,
                                                    const NodeManager& nodeManager,
                                                    const FaceManagerT& faceManager,
                                                    const bool packConnectivityToGlobal,
                                                    const bool packFields,
                                                    const bool packMaps,
                                                    const bool packSets ) const;

/**
 *
 * @param buffer
 * @param nodeManager
 * @return
 *
 * ASSUMES that ALL nodes are present on current domain!
 */
unsigned int ElementRegionT::UnpackElements( const char*& buffer,
                                             const NodeManager& nodeManager,
                                             const FaceManagerT& faceManager,
                                             lArray1d& elementRegionReceiveLocalIndices,
                                             const bool unpackConnectivityToLocal,
                                             const bool unpackFields,
                                             const bool unpackMaps,
                                             const bool unpackSets )
{

  unsigned int sizeOfUnpacked = 0;

  lArray1d junk;
  sizeOfUnpacked += ObjectDataStructureBaseT::UnpackBaseObjectData(buffer,
                                                                   elementRegionReceiveLocalIndices,
                                                                   junk,
                                                                   unpackFields, unpackMaps,
                                                                   unpackSets,
                                                                   unpackConnectivityToLocal);

  const lArray1d::size_type numUnpackedElems = elementRegionReceiveLocalIndices.size();

  // TODO need to check to see if the elements already exist on the partition, as they can be created from another
  // neighbor

  for (lArray1d::size_type k = 0; k < numUnpackedElems; ++k)
  {
    const lArray1d::size_type lElemIndex = elementRegionReceiveLocalIndices[k];

    for (unsigned int a = 0; a < m_toNodesRelation.Dimension(1); ++a)
    {
      globalIndex gnode;
      sizeOfUnpacked += bufvector::Unpack(buffer, gnode);

      if (unpackConnectivityToLocal)
      {
        const localIndex lnode = stlMapLookup(nodeManager.m_globalToLocalMap, gnode);
        m_toNodesRelation(lElemIndex, a) = lnode;
      }
      else
      {
        m_toNodesRelation(lElemIndex, a) = gnode;
      }
    }

    localIndex* const facelist = m_toFacesRelation[lElemIndex];
    for (unsigned int kf = 0; kf < this->m_toFacesRelation.Dimension(1); ++kf)
    {
      globalIndex gface;
      sizeOfUnpacked += bufvector::Unpack(buffer, gface);
      if (unpackConnectivityToLocal)
      {
        const localIndex lface = stlMapLookup(faceManager.m_globalToLocalMap, gface);
        facelist[kf] = lface;
      }
      else
      {
        facelist[kf] = gface;
      }
    }
  }

  if (unpackFields)
  {
    sizeOfUnpacked += UnpackAllFieldsFromBuffer(buffer, elementRegionReceiveLocalIndices);
    CalculateShapeFunctionDerivatives(nodeManager);
  }

  return sizeOfUnpacked;
}



void ElementRegionT::ConnectivityFromGlobalToLocal( const lSet& list,
                                                    const std::map<globalIndex,localIndex>& nodeGlobalToLocal,
                                                    const std::map<globalIndex,localIndex>& faceGlobalToLocal )
{
  for (lSet::const_iterator k = list.begin(); k != list.end(); ++k)
  {
    for (unsigned int a = 0; a < m_toNodesRelation.Dimension(1); ++a)
    {
      const globalIndex gnode = m_toNodesRelation(*k, a);
      const localIndex lnode = stlMapLookup(nodeGlobalToLocal, gnode);
      m_toNodesRelation(*k, a) = lnode;
    }

    for (unsigned int a = 0; a < m_toFacesRelation.Dimension(1); ++a)
    {
      const globalIndex gface = m_toFacesRelation(*k, a);
      const localIndex lface = stlMapLookup(faceGlobalToLocal, gface);
      m_toFacesRelation(*k, a) = lface;
    }
  }
}



template< typename T_indices >
unsigned int ElementRegionT::PackFieldsIntoBuffer( bufvector& buffer,
                                                   const sArray1d& fieldNames,
                                                   const T_indices& localIndices,
                                                   const bool doBufferPacking ) const
{
  unsigned int packedSize = 0;

  packedSize += ObjectDataStructureBaseT::PackFieldsIntoBuffer( buffer, fieldNames, localIndices, doBufferPacking );
  if(m_mat) packedSize += m_mat->Pack(localIndices, buffer, doBufferPacking);
  return packedSize;
}


template unsigned int ElementRegionT::PackFieldsIntoBuffer( bufvector& buffer, const sArray1d& fieldNames, const lSet& localIndices, const bool doBufferPacking ) const;
template unsigned int ElementRegionT::PackFieldsIntoBuffer( bufvector& buffer, const sArray1d& fieldNames, const lArray1d& localIndices, const bool doBufferPacking ) const;


template< typename T_indices >
unsigned int ElementRegionT::PackFieldsIntoBuffer( char*& buffer,
                                                   const sArray1d& fieldNames,
                                                   const T_indices& localIndices,
                                                   const bool doBufferPacking ) const
{
  unsigned int packedSize = 0;
  packedSize += ObjectDataStructureBaseT::PackFieldsIntoBuffer(buffer, fieldNames, localIndices,
                                                               doBufferPacking);
  if (m_mat)
    packedSize += m_mat->Pack(localIndices, buffer, doBufferPacking);
  return packedSize;
}
template unsigned int ElementRegionT::PackFieldsIntoBuffer( char*& buffer, const sArray1d& fieldNames, const lSet& localIndices, const bool doBufferPacking ) const;
template unsigned int ElementRegionT::PackFieldsIntoBuffer( char*& buffer, const sArray1d& fieldNames, const lArray1d& localIndices, const bool doBufferPacking ) const;


unsigned int ElementRegionT::UnpackFieldsFromBuffer( const char*& buffer,
                                                     const sArray1d& fieldNames,
                                                     const lArray1d& localIndices )
{
  unsigned int sizeOfUnpackedChars = 0;
  sizeOfUnpackedChars += ObjectDataStructureBaseT::UnpackFieldsFromBuffer(buffer, fieldNames,
                                                                          localIndices);
  if (m_mat)
    sizeOfUnpackedChars += m_mat->Unpack(localIndices, buffer);
  return sizeOfUnpackedChars;
}

template< typename T_indices >
unsigned int ElementRegionT::PackAllFieldsIntoBuffer( bufvector& buffer,
                                                      const T_indices& localIndices ) const
{
  unsigned int packedSize = 0;
  packedSize += ObjectDataStructureBaseT::PackAllFieldsIntoBuffer(buffer, localIndices);
  if (m_mat)
    packedSize += m_mat->Pack(localIndices, buffer);
  return packedSize;
}
template unsigned int ElementRegionT::PackAllFieldsIntoBuffer( bufvector& buffer, const lSet& localIndices ) const;
template unsigned int ElementRegionT::PackAllFieldsIntoBuffer( bufvector& buffer, const lArray1d& localIndices ) const;


unsigned int ElementRegionT::UnpackAllFieldsFromBuffer( const char*& buffer,
                                                        const lArray1d& localIndices )
{
  unsigned int sizeOfUnpackedChars = 0;
  sizeOfUnpackedChars += ObjectDataStructureBaseT::UnpackAllFieldsFromBuffer(buffer, localIndices);
  if (m_mat)
    sizeOfUnpackedChars += m_mat->Unpack(localIndices, buffer);
  return sizeOfUnpackedChars;
}

void ElementRegionT::UpdateElementFieldsWithGaussPointData(){
	sArray1d intVarNames;
	sArray1d realVarNames;
	sArray1d R1TensorVarNames;
	sArray1d R2TensorVarNames;
	sArray1d R2SymTensorVarNames;

	Array1dT<iArray1d*> intVars;
	Array1dT<rArray1d*> realVars;
	Array1dT<Array1dT<R1Tensor>*> R1Vars;
	Array1dT<Array1dT<R2Tensor>*> R2Vars;
	Array1dT<Array1dT<R2SymTensor>*> R2SymVars;

	if (m_mat)
	{

		m_mat->GetVariableNames(intVarNames, realVarNames, R1TensorVarNames, R2TensorVarNames,
				R2SymTensorVarNames);

		AllocateDummyFields(intVarNames, intVars, m_plotMat);
		AllocateDummyFields(realVarNames, realVars, m_plotMat);
		AllocateDummyFields(R1TensorVarNames, R1Vars, m_plotMat);
		AllocateDummyFields(R2TensorVarNames, R2Vars, m_plotMat);
		AllocateDummyFields(R2SymTensorVarNames, R2SymVars, m_plotMat);

		m_mat->Serialize(intVars, realVars, R1Vars, R2Vars, R2SymVars);

		rArray1d& sigma_x = GetFieldData<realT>("sigma_x");
		rArray1d& sigma_y = GetFieldData<realT>("sigma_y");
		rArray1d& sigma_z = GetFieldData<realT>("sigma_z");
		rArray1d& sigma_xy = GetFieldData<realT>("sigma_xy");
		rArray1d& sigma_yz = GetFieldData<realT>("sigma_yz");
		rArray1d& sigma_xz = GetFieldData<realT>("sigma_xz");

		rArray1d& pressure = GetFieldData<FieldInfo::pressure>();
		Array1dT<R2SymTensor>& s = GetFieldData<FieldInfo::deviatorStress>();
		rArray1d& density = GetFieldData<FieldInfo::density>();

		for (localIndex k = 0; k < m_numElems; ++k)
		{
			//    m_material.MaterialState(k).MeanPressureDevStress(pressure[k], s[k]);

			s[k] = 0.0;
			pressure[k] = 0.0;
			if( m_mat->NeedsDensity() ) density[k] = 0.0;
			for (localIndex a = 0; a < m_numIntegrationPointsPerElem; ++a)
			{
				const MaterialBaseStateData& state = *(m_mat->StateData(k, a));
				s[k] += state.devStress;
				pressure[k] += state.pressure;
				if( m_mat->NeedsDensity() ) density[k] += state.GetDensity();
			}
			s[k] /= m_numIntegrationPointsPerElem;
			pressure[k] /= m_numIntegrationPointsPerElem;
			if( m_mat->NeedsDensity() ) density[k] /= m_numIntegrationPointsPerElem;
			sigma_x[k] = s[k](0, 0) + pressure[k];
			sigma_y[k] = s[k](1, 1) + pressure[k];
			sigma_z[k] = s[k](2, 2) + pressure[k];
			sigma_xy[k] = s[k](0, 1);
			sigma_yz[k] = s[k](1, 2);
			sigma_xz[k] = s[k](0, 2);
		}

		DeallocateDummyFields<int>(intVarNames);
		DeallocateDummyFields<realT>(realVarNames);
		DeallocateDummyFields<R1Tensor>(R1TensorVarNames);
		DeallocateDummyFields<R2Tensor>(R2TensorVarNames);
		DeallocateDummyFields<R2SymTensor>(R2SymTensorVarNames);
	}
}

/*
template< typename T >
void ElementRegionT::AllocateDummyFields( const sArray1d& names, Array1dT<Array1dT<T>* >& vars )
{
  vars.resize( names.size() );
  for( sArray1d::size_type i=0 ; i<names.size() ; ++i )
  {
    this->AddKeylessDataField<T>(names[i],true,true);
    vars[i] = &this->GetFieldData<T>(names[i]);
  }
}



template< typename T >
void ElementRegionT::DeallocateDummyFields( const sArray1d& names )
{
  for( sArray1d::size_type i=0 ; i<names.size() ; ++i )
    this->RemoveDataField<T>(names[i]);
}
*/
void ElementRegionT::WriteSiloRegionMesh( SiloFile& siloFile,
                                          const std::string& meshname,
                                          const int cycleNum,
                                          const realT problemTime,
                                          const bool isRestart,
                                          const std::string& regionName )
{
  int rank = 0;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);

  std::string rootDirectory = "/" + regionName;
  if (rank == 0)
  {
    DBMkDir(siloFile.m_dbFilePtr, rootDirectory.c_str());
  }
  DBMkDir(siloFile.m_dbFilePtr, regionName.c_str());

  DBSetDir(siloFile.m_dbFilePtr, regionName.c_str());

  sArray1d intVarNames;
  sArray1d realVarNames;
  sArray1d R1TensorVarNames;
  sArray1d R2TensorVarNames;
  sArray1d R2SymTensorVarNames;

  Array1dT<iArray1d*> intVars;
  Array1dT<rArray1d*> realVars;
  Array1dT<Array1dT<R1Tensor>*> R1Vars;
  Array1dT<Array1dT<R2Tensor>*> R2Vars;
  Array1dT<Array1dT<R2SymTensor>*> R2SymVars;

  if (m_mat)
  {

    m_mat->GetVariableNames(intVarNames, realVarNames, R1TensorVarNames, R2TensorVarNames,
                            R2SymTensorVarNames);

    AllocateDummyFields(intVarNames, intVars, m_plotMat);
    AllocateDummyFields(realVarNames, realVars, m_plotMat);
    AllocateDummyFields(R1TensorVarNames, R1Vars, m_plotMat);
    AllocateDummyFields(R2TensorVarNames, R2Vars, m_plotMat);
    AllocateDummyFields(R2SymTensorVarNames, R2SymVars, m_plotMat);

    m_mat->Serialize(intVars, realVars, R1Vars, R2Vars, R2SymVars);

    rArray1d& sigma_x = GetFieldData<realT>("sigma_x");
    rArray1d& sigma_y = GetFieldData<realT>("sigma_y");
    rArray1d& sigma_z = GetFieldData<realT>("sigma_z");
    rArray1d& sigma_xy = GetFieldData<realT>("sigma_xy");
    rArray1d& sigma_yz = GetFieldData<realT>("sigma_yz");
    rArray1d& sigma_xz = GetFieldData<realT>("sigma_xz");

    rArray1d& pressure = GetFieldData<FieldInfo::pressure>();
    Array1dT<R2SymTensor>& s = GetFieldData<FieldInfo::deviatorStress>();

    for (localIndex k = 0; k < m_numElems; ++k)
    {
      //    m_material.MaterialState(k).MeanPressureDevStress(pressure[k], s[k]);

      s[k] = 0.0;
      pressure[k] = 0.0;
      for (localIndex a = 0; a < m_numIntegrationPointsPerElem; ++a)
      {
        const MaterialBaseStateData& state = *(m_mat->StateData(k, a));
        s[k] += state.devStress;
        pressure[k] += state.pressure;
      }
      s[k] /= m_numIntegrationPointsPerElem;
      pressure[k] /= m_numIntegrationPointsPerElem;
      sigma_x[k] = s[k](0, 0) + pressure[k];
      sigma_y[k] = s[k](1, 1) + pressure[k];
      sigma_z[k] = s[k](2, 2) + pressure[k];
      sigma_xy[k] = s[k](0, 1);
      sigma_yz[k] = s[k](1, 2);
      sigma_xz[k] = s[k](0, 2);
    }

    rArray1d* antiThermalStress = this->GetFieldDataPointer<realT>("antiThermalStress");

    if (antiThermalStress != NULL) // We need to correct for the anti thermal stress
    {
      for (localIndex k = 0; k < m_numElems; ++k)
      {
        sigma_x[k] -= (*antiThermalStress)[k];
        sigma_y[k] -= (*antiThermalStress)[k];
        sigma_z[k] -= (*antiThermalStress)[k];
      }

    }

  }

  ObjectDataStructureBaseT::WriteSilo(siloFile, meshname, DB_ZONECENT, cycleNum, problemTime,
                                      isRestart, rootDirectory, regionName);

  DeallocateDummyFields<int>(intVarNames);
  DeallocateDummyFields<realT>(realVarNames);
  DeallocateDummyFields<R1Tensor>(R1TensorVarNames);
  DeallocateDummyFields<R2Tensor>(R2TensorVarNames);
  DeallocateDummyFields<R2SymTensor>(R2SymTensorVarNames);

  if (isRestart)
  {
    const int varParams = m_mat ? (m_mat->VariableParameters() ? 1 : 0) : 0;
    siloFile.DBWriteWrapper("m_mat_hasVariableParameters", varParams);

    siloFile.DBWriteWrapper("m_regionName", m_regionName);
    siloFile.DBWriteWrapper("m_regionNumber", m_regionNumber);
    siloFile.DBWriteWrapper("m_numNodesPerElem", static_cast<int>(m_numNodesPerElem));
    siloFile.DBWriteWrapper("m_numIntegrationPointsPerElem", (int) m_numIntegrationPointsPerElem);

    siloFile.DBWriteWrapper("m_elementType", m_elementType);
    siloFile.DBWriteWrapper("m_elementGeometryID", m_elementGeometryID);

    siloFile.DBWriteWrapper("m_ElementDimension", m_ElementDimension);

    siloFile.DBWriteWrapper("m_dNdX", m_dNdX);
    siloFile.DBWriteWrapper("m_dUdX", m_dUdX);
    siloFile.DBWriteWrapper("m_detJ", m_detJ);
    siloFile.DBWriteWrapper("m_detJ_n", m_detJ_n);
    siloFile.DBWriteWrapper("m_detJ_np1", m_detJ_np1);

    siloFile.DBWriteWrapper("m_basis", m_basis);
    siloFile.DBWriteWrapper("m_quadrature", m_quadrature);

    siloFile.DBWriteWrapper("m_numFacesPerElement", m_numFacesPerElement);
    siloFile.DBWriteWrapper("m_numNodesPerFace", m_numNodesPerFace);

    rArray1d energy(EnergyT::numVars);
    m_energy.Serialize(energy.data());
    siloFile.DBWriteWrapper("m_energy", energy);
  }

  DBSetDir(siloFile.m_dbFilePtr, "..");

}



void ElementRegionT::ReadSiloRegionMesh( const SiloFile& siloFile,
                                         const std::string& meshname,
                                         const int cycleNum,
                                         const realT problemTime,
                                         const bool isRestart,
                                         const std::string& regionName )
{
  DBSetDir(siloFile.m_dbFilePtr, regionName.c_str());

  if (isRestart)
  {
    int varParams;
    siloFile.DBReadWrapper("m_mat_hasVariableParameters", varParams);
    if(m_mat)
      m_mat->SetVariableParameters(varParams == 1, m_DataLengths);

    siloFile.DBReadWrapper("m_regionName", m_regionName);
    siloFile.DBReadWrapper("m_regionNumber", m_regionNumber);
    siloFile.DBReadWrapper("m_numNodesPerElem", m_numNodesPerElem);
    siloFile.DBReadWrapper("m_numIntegrationPointsPerElem", m_numIntegrationPointsPerElem);

    siloFile.DBReadWrapper("m_elementType", m_elementType);
    siloFile.DBReadWrapper("m_elementGeometryID", m_elementGeometryID);

    siloFile.DBReadWrapper("m_ElementDimension", m_ElementDimension);

    siloFile.DBReadWrapper("m_basis", m_basis);
    siloFile.DBReadWrapper("m_quadrature", m_quadrature);

    AllocateElementLibrary(m_basis, m_quadrature);

    siloFile.DBReadWrapper("m_numFacesPerElement", m_numFacesPerElement);
    siloFile.DBReadWrapper("m_numNodesPerFace", m_numNodesPerFace);

    rArray1d energy(EnergyT::numVars);
    siloFile.DBReadWrapper("m_energy", energy);
    m_energy.Deserialize(energy.data());

//    m_material.ReadSilo(siloFile, meshname, cycleNum, problemTime, isRestart, regionName);
//    m_material.SetNumberOfIntegrationPointsPerState(m_numIntegrationPointsPerElem);
  }

  sArray1d intVarNames;
  sArray1d realVarNames;
  sArray1d R1TensorVarNames;
  sArray1d R2TensorVarNames;
  sArray1d R2SymTensorVarNames;

  Array1dT<iArray1d*> intVars;
  Array1dT<rArray1d*> realVars;
  Array1dT<Array1dT<R1Tensor>*> R1Vars;
  Array1dT<Array1dT<R2Tensor>*> R2Vars;
  Array1dT<Array1dT<R2SymTensor>*> R2SymVars;

  m_mat->GetVariableNames(intVarNames, realVarNames, R1TensorVarNames, R2TensorVarNames,
                          R2SymTensorVarNames);

  AllocateDummyFields(intVarNames, intVars, false);
  AllocateDummyFields(realVarNames, realVars, false);
  AllocateDummyFields(R1TensorVarNames, R1Vars, false);
  AllocateDummyFields(R2TensorVarNames, R2Vars, false);
  AllocateDummyFields(R2SymTensorVarNames, R2SymVars, false);

  ObjectDataStructureBaseT::ReadSilo(siloFile, meshname, DB_ZONECENT, cycleNum, problemTime,
                                     isRestart, regionName);

  m_mat->Deserialize(intVars, realVars, R1Vars, R2Vars, R2SymVars);

  DeallocateDummyFields<int>(intVarNames);
  DeallocateDummyFields<realT>(realVarNames);
  DeallocateDummyFields<R1Tensor>(R1TensorVarNames);
  DeallocateDummyFields<R2Tensor>(R2TensorVarNames);
  DeallocateDummyFields<R2SymTensor>(R2SymTensorVarNames);

  if (isRestart)
  {
    siloFile.DBReadWrapper("m_dNdX", m_dNdX);
    siloFile.DBReadWrapper("m_dUdX", m_dUdX);
    siloFile.DBReadWrapper("m_detJ", m_detJ);
    siloFile.DBReadWrapper("m_detJ_n", m_detJ_n);
    siloFile.DBReadWrapper("m_detJ_np1", m_detJ_np1);

  }

  DBSetDir(siloFile.m_dbFilePtr, "..");
}



void ElementRegionT::ModifyToElementMapsFromSplit( const lSet& modifiedElements ,
                                                   NodeManager& nodeManager,
                                                   FaceManagerT& faceManager )
{

  // loop over all modified
  for (lSet::const_iterator elemIndex = modifiedElements.begin();
      elemIndex != modifiedElements.end(); ++elemIndex)
  {

    const std::pair<ElementRegionT*, localIndex> elemPair = std::make_pair(this, *elemIndex);

    // first handle the nodeToElement map
    const localIndex* const nodelist = this->m_toNodesRelation[*elemIndex];

    // loop over all nodes in the element
    // Fu: This part should work for both 3D and 2D, except that in 3D the parent index seems to be different.
    for (localIndex a = 0; a < m_toNodesRelation.Dimension(1); ++a)
    {
      const localIndex nodeIndex = nodelist[a];
      // so now we have a node that we know is currently attached to the element. So we should add the
      // element to the nodeToElement relation in case it isn't there already.
      nodeManager.m_toElementsRelation[nodeIndex].insert(elemPair);

      // now we have to remove the element from the nodes that are no longer connected. These nodes are either parents,
      // or children of the node that we know is attached

      // remove the parent nodes
      localIndex parentNodeIndex = nodeManager.m_parentIndex[nodeIndex];

      if (m_ElementDimension == 3)
      {
        while (parentNodeIndex != LOCALINDEX_MAX)
        {
          nodeManager.m_toElementsRelation[parentNodeIndex].erase(elemPair);

          parentNodeIndex = nodeManager.m_parentIndex[parentNodeIndex];
        }
      }
      else
      {
        if (parentNodeIndex != LOCALINDEX_MAX)
        {
          nodeManager.m_toElementsRelation[parentNodeIndex].erase(elemPair);
        }

      }

      // remove the child nodes
      // This loop should never be invoked for 2D problems because if a node is attached to an element, it should not have children.
      for (lArray1d::iterator i = nodeManager.m_childIndices[nodeIndex].begin();
          i != nodeManager.m_childIndices[nodeIndex].end(); ++i)
      {
        nodeManager.m_toElementsRelation[*i].erase(elemPair);
      }
    }

    // now do the faceToElementMap
    const localIndex* const facelist = this->m_toFacesRelation[*elemIndex];

    // loop over all faces attached to the element.
    // Fu: Here we will need different logics for 2D and 3D.
    if (m_ElementDimension == 3)
    {
      for (localIndex a = 0; a < m_toFacesRelation.Dimension(1); ++a)
      {
        const localIndex faceIndex = facelist[a];
        const localIndex parentFaceIndex = faceManager.m_parentIndex[faceIndex];
        const localIndex childFaceIndex =
            faceManager.m_childIndices[faceIndex].size() == 1 ? faceManager.m_childIndices[faceIndex][0] :
                                                                LOCALINDEX_MAX;

        const localIndex deletedFaceIndex =
            parentFaceIndex != LOCALINDEX_MAX ? parentFaceIndex : childFaceIndex;

        // remove element from the parent face
        if (deletedFaceIndex != LOCALINDEX_MAX)
        {

          // get iterators to each element attached to the face
          Array1dT<std::pair<ElementRegionT*, localIndex> >::iterator iter0 = faceManager.m_toElementsRelation[deletedFaceIndex].begin();
          Array1dT<std::pair<ElementRegionT*, localIndex> >::iterator iter1 = iter0 + 1;

          // if the first element is equal to this element pair, then erasd
          if (iter0 != faceManager.m_toElementsRelation[deletedFaceIndex].end())
          {
            if (*iter0 == elemPair)
            {
              faceManager.m_toElementsRelation[deletedFaceIndex].erase(iter0);
            }
            if (iter1 != faceManager.m_toElementsRelation[deletedFaceIndex].end())
            {
              if (*iter1 == elemPair)
              {
                faceManager.m_toElementsRelation[deletedFaceIndex].erase(iter1);
              }
            }
          }
        }

        // add the element to the face
        {
          Array1dT<std::pair<ElementRegionT*, localIndex> >::iterator iter0 = faceManager.m_toElementsRelation[faceIndex].begin();
          Array1dT<std::pair<ElementRegionT*, localIndex> >::iterator iter1 = iter0 + 1;
          {

            bool elemPresent = false;
            if (!(faceManager.m_toElementsRelation[faceIndex].empty()))
            {
              if (*iter0 == elemPair)
              {
                elemPresent = true;
              }
              else if (iter1 != faceManager.m_toElementsRelation[faceIndex].end())
              {
                if (*iter1 == elemPair)
                {
                  elemPresent = true;
                }
              }
            }

            if (!elemPresent)
            {
              faceManager.m_toElementsRelation[faceIndex].push_back(elemPair);
            }
          }
        }

        Array1dT<std::pair<ElementRegionT*, localIndex> >::size_type size0 = 1;
        if (parentFaceIndex != LOCALINDEX_MAX)
          size0 = faceManager.m_toElementsRelation[parentFaceIndex].size();

        const Array1dT<std::pair<ElementRegionT*, localIndex> >::size_type size1 = faceManager.m_toElementsRelation[faceIndex].size();
        if (size0 > 2 || size0 <= 0 || size1 > 2 || size1 <= 0)
        {
          //throw GPException("ElementRegionT::ModifyToElementMapsFromSplit(): number of faces in faceManager.m_toElementsRelation is invalid");
        }
      }
    }
    else
    {

      for (localIndex a = 0; a < m_toFacesRelation.Dimension(1); ++a)
      {
        const localIndex faceIndex = facelist[a];
        const localIndex parentFaceIndex = faceManager.m_parentIndex[faceIndex];
        // In 2D, a face attached to an element should not have children.

        // remove element from the parent face
        if (parentFaceIndex != LOCALINDEX_MAX)
        {
          faceManager.m_toElementsRelation[parentFaceIndex].resize(0);
        }

        // add the element to the face
        {
          Array1dT<std::pair<ElementRegionT*, localIndex> >::iterator iter0 = faceManager.m_toElementsRelation[faceIndex].begin();
          Array1dT<std::pair<ElementRegionT*, localIndex> >::iterator iter1 = iter0 + 1;
          {

            bool elemPresent = false;
            if (!(faceManager.m_toElementsRelation[faceIndex].empty()))
            {
              if (*iter0 == elemPair)
              {
                elemPresent = true;
              }
              else if (iter1 != faceManager.m_toElementsRelation[faceIndex].end())
              {
                if (*iter1 == elemPair)
                {
                  elemPresent = true;
                }
              }
            }

            if (!elemPresent)
            {
              faceManager.m_toElementsRelation[faceIndex].push_back(elemPair);
            }
          }
        }

        Array1dT<std::pair<ElementRegionT*, localIndex> >::size_type size0 = 1;
        if (parentFaceIndex != LOCALINDEX_MAX)
          size0 = faceManager.m_toElementsRelation[parentFaceIndex].size();

        const Array1dT<std::pair<ElementRegionT*, localIndex> >::size_type size1 = faceManager.m_toElementsRelation[faceIndex].size();
        if (size0 > 2 || size0 <= 0 || size1 > 2 || size1 <= 0)
        {
          //throw GPException("ElementRegionT::ModifyToElementMapsFromSplit(): number of faces in faceManager.m_toElementsRelation is invalid");
        }
      }

    }

  }
}


void ElementRegionT::UpdateExternalityFromSplit( const lSet& modifiedElements ,
                                                 NodeManager& nodeManager,
                                                 EdgeManagerT& edgeManager,
                                                 FaceManagerT& faceManager )
{
  for (lSet::const_iterator elemIndex = modifiedElements.begin();
      elemIndex != modifiedElements.end(); ++elemIndex)
  {
//     const std::pair< ElementRegionT*, localIndex > elemPair = std::make_pair( this, *elemIndex ) ;

    const localIndex* const nodelist = this->m_toNodesRelation[*elemIndex];

    for (localIndex a = 0; a < m_toNodesRelation.Dimension(1); ++a)
    {
      const localIndex nodeIndex = nodelist[a];

      for (lSet::const_iterator iface = nodeManager.m_nodeToFaceMap[nodeIndex].begin();
          iface != nodeManager.m_nodeToFaceMap[nodeIndex].end(); ++iface)
      {
        if (faceManager.m_isExternal[*iface] == 1)
        {
          nodeManager.m_isExternal[nodeIndex] = 1;

          //We need to handle edges here because there is not a element to edge map.
          for (lSet::const_iterator iedge = nodeManager.m_nodeToEdgeMap[nodeIndex].begin();
              iedge != nodeManager.m_nodeToEdgeMap[nodeIndex].end(); ++iedge)
          {
            edgeManager.m_isExternal[*iedge] = 1;
          }
        }
      }
    }
  }
}



iArray1d ElementRegionT::SiloNodeOrdering()
{

  iArray1d nodeOrdering;

  if( !m_elementGeometryID.compare(0, 4, "CPE2") )
  {
    nodeOrdering.resize(2);
    nodeOrdering[0] = 0;
    nodeOrdering[1] = 1;
  }
  else if( !m_elementGeometryID.compare(0, 4, "CPE3") )
  {
    nodeOrdering.resize(3);
    nodeOrdering[0] = 0;
    nodeOrdering[1] = 1;
    nodeOrdering[2] = 2;
    //    throw GPException("ElementRegionT::AllocateElementLibrary(): CPE3 unimplemented");
  }
  else if (!m_elementGeometryID.compare(0, 4, "CPE4"))
  {
    nodeOrdering.resize(4);
    nodeOrdering[0] = 0;
    nodeOrdering[1] = 1;
    nodeOrdering[2] = 3;
    nodeOrdering[3] = 2;
  }
  else if (!m_elementGeometryID.compare(0, 4, "C3D4"))
  {
    nodeOrdering.resize(4);
    nodeOrdering[0] = 1;
    nodeOrdering[1] = 0;
    nodeOrdering[2] = 2;
    nodeOrdering[3] = 3;
  }
  else if (!m_elementGeometryID.compare(0, 4, "C3D8") || !m_elementGeometryID.compare(0, 4, "C3D6"))
  {
    nodeOrdering.resize(8);
    nodeOrdering[0] = 0;
    nodeOrdering[1] = 1;
    nodeOrdering[2] = 3;
    nodeOrdering[3] = 2;
    nodeOrdering[4] = 4;
    nodeOrdering[5] = 5;
    nodeOrdering[6] = 7;
    nodeOrdering[7] = 6;
  }
  else if (!m_elementGeometryID.compare(0, 4, "STRI"))
  {
    nodeOrdering.resize(3);
    nodeOrdering[0] = 0;
    nodeOrdering[1] = 1;
    nodeOrdering[2] = 2;
  }
  else if (!m_elementGeometryID.compare(0, 3, "S4R"))
  {
    nodeOrdering.resize(4);
    nodeOrdering[0] = 0;
    nodeOrdering[1] = 1;
    nodeOrdering[2] = 2;
    nodeOrdering[3] = 3;
  }
  else if (!m_elementGeometryID.compare(0, 4, "TRSH"))
  {
    nodeOrdering.resize(4);
    nodeOrdering[0] = 0;
    nodeOrdering[1] = 1;
    nodeOrdering[2] = 2;
  }
  return nodeOrdering;
}


bool ElementRegionT::SplitObject( const localIndex indexToSplit,
                                  const int rank,
                                  localIndex newIndices[2],
                                  const bool forceSplit )
{
  bool didSplit = ObjectDataStructureBaseT::SplitObject( indexToSplit, rank, newIndices, true );

  // split materials
  this->m_mat->resize(this->m_DataLengths);

  // copy states and params for materials


  for (unsigned int a = 0; a < m_numIntegrationPointsPerElem; ++a)
  {
    *(m_mat->StateData(newIndices[0], a)) = *(m_mat->StateData(indexToSplit, a));
    *(m_mat->StateData(newIndices[1], a)) = *(m_mat->StateData(indexToSplit, a));
  }

  return didSplit;
}

void ElementRegionT::UpdateElementsVolume( PhysicalDomainT& domain )
{
  FaceManagerT& faceManager = domain.m_feFaceManager;
  NodeManager& nodeManager = domain.m_feNodeManager;
  rArray1d& volume = this->GetFieldData<FieldInfo::volume>();
  Array1dT< R1Tensor >& refPosition = nodeManager.GetFieldData<FieldInfo::referencePosition>();
  Array1dT< R1Tensor >& displacement = nodeManager.GetFieldData<FieldInfo::displacement>();

  R1Tensor dummy;

  // Solid solvers that use element volume calculate it themselves.  This function is mainly called by some flow solvers that need element element volume.
  for (localIndex j = 0; j < DataLengths(); j++)
  {

    R1Tensor elemCenter = GetElementCenter(j, nodeManager);
    volume[j] = 0.0;

    for (localIndex k = 0; k < this->m_toFacesRelation.Dimension(1); k++)
    {
      localIndex faceIndex = m_toFacesRelation(j, k);
      localIndex nnodes = faceManager.m_toNodesRelation[faceIndex].size();

      R1Tensor x0(refPosition[faceManager.m_toNodesRelation[faceIndex][0]]);
      x0 += displacement[faceManager.m_toNodesRelation[faceIndex][0]];

      for (localIndex l = 2; l < nnodes; l++)
      {
        R1Tensor x1(refPosition[faceManager.m_toNodesRelation[faceIndex][l - 1]]);
        R1Tensor x2(refPosition[faceManager.m_toNodesRelation[faceIndex][l]]);
        x1 += displacement[faceManager.m_toNodesRelation[faceIndex][l-1]];
        x2 += displacement[faceManager.m_toNodesRelation[faceIndex][l]];

        volume[j] += GeometryUtilities::CentroidAndVolume_3DTetrahedron(
            elemCenter, x0, x1, x2, dummy);
      }
    }
  }
}

}
