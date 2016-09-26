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
/**
 * @file SchemaUtilities.cpp
 * @author sherman
 */

#include "schema/SchemaUtilities.hpp"

namespace geosx
{

void ConvertDocumentationToSchema(std::string const & fname, cxx_utilities::DocumentationNode const & inputDocumentationHead)
{
  // Build the base of the schema
  std::string schemaBase="<?xml version=\"1.1\" encoding=\"ISO-8859-1\" ?><xsd:schema xmlns:xsd=\"http://www.w3.org/2001/XMLSchema\"><xsd:annotation><xsd:documentation xml:lang=\"en\">New schema for GEOS</xsd:documentation></xsd:annotation></xsd:schema>";
  pugi::xml_document schemaTree;
  schemaTree.load_string(schemaBase.c_str());
  pugi::xml_node schemaRoot = schemaTree.child("xsd:schema");

  // Recursively build the schema from the documentation string
  SchemaConstruction(inputDocumentationHead, schemaRoot, schemaRoot);

  // Write the schema to file
  schemaTree.save_file(fname.c_str());
}

void SchemaConstruction(cxx_utilities::DocumentationNode const & docNode, pugi::xml_node schemaNode, pugi::xml_node schemaRoot)
{
  if (docNode.m_varType.find("Node") != std::string::npos)
  {
    // Set the type of target
    pugi::xml_node targetNode = schemaNode;
    if (docNode.m_varType.find("Unique") == std::string::npos)
    {
      targetNode = targetNode.child("xsd:choice");
      if (targetNode == NULL)
      {
        targetNode = schemaNode.append_child("xsd:choice");
        targetNode.append_attribute("maxOccurs") = "unbounded";
      }
    }

    // Add the entries to the current and root nodes
    pugi::xml_node newNode = targetNode.append_child("xsd:element");
    newNode.append_attribute("name") = docNode.m_varName.c_str();
    newNode.append_attribute("type") = (docNode.m_varName+"Type").c_str();
    newNode = schemaRoot.append_child("xsd:complexType");
    newNode.append_attribute("name") = (docNode.m_varName+"Type").c_str();

    for( auto const & subDocNode : docNode.m_child )
    {
      SchemaConstruction(subDocNode.second, newNode, schemaRoot);
    }
  }
  else
  {
    pugi::xml_node newNode = schemaNode.append_child("xsd:attribute");
    newNode.append_attribute("name") = docNode.m_varName.c_str();
    newNode.append_attribute("type") = ("xsd:"+docNode.m_varType).c_str();
  }
}

}
