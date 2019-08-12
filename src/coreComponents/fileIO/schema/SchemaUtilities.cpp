/*
 *~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * Copyright (c) 2019, Lawrence Livermore National Security, LLC.
 *
 * Produced at the Lawrence Livermore National Laboratory
 *
 * LLNL-CODE-746361
 *
 * All rights reserved. See COPYRIGHT for details.
 *
 * This file is part of the GEOSX Simulation Framework.
 *
 * GEOSX is a free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License (as published by the
 * Free Software Foundation) version 2.1 dated February 1999.
 *~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */

/**
 * @file SchemaUtilities.cpp
 */

#include "SchemaUtilities.hpp"

#include "dataRepository/Group.hpp"
#include "dataRepository/Wrapper.hpp"
#include "common/DataTypes.hpp"
#include "dataRepository/InputFlags.hpp"

namespace geosx
{

using namespace dataRepository;


SchemaUtilities::SchemaUtilities()
{
  // TODO Auto-generated constructor stub

}

SchemaUtilities::~SchemaUtilities()
{
  // TODO Auto-generated destructor stub
}


void SchemaUtilities::ConvertDocumentationToSchema(std::string const & fname,
                                                   Group * const group,
                                                   integer documentationType)
{
  GEOS_LOG_RANK_0("Generating XML Schema...");

  std::string schemaBase=
    "<?xml version=\"1.1\" encoding=\"ISO-8859-1\" ?>\
  <xsd:schema xmlns:xsd=\"http://www.w3.org/2001/XMLSchema\">\
  <xsd:annotation>\
  <xsd:documentation xml:lang=\"en\">GEOSX Input Schema</xsd:documentation>\
  </xsd:annotation>\
  </xsd:schema>";

  xmlWrapper::xmlDocument schemaTree;
  schemaTree.load_string(schemaBase.c_str());
  xmlWrapper::xmlNode schemaRoot = schemaTree.child("xsd:schema");

  // Build the simple schema types
  GEOS_LOG_RANK_0("  Basic datatypes");
  BuildSimpleSchemaTypes(schemaRoot);

  // Recursively build the schema from the data structure skeleton
  GEOS_LOG_RANK_0("  Data structure layout");
  SchemaConstruction(group, schemaRoot, schemaRoot, documentationType);

  // Write the schema to file
  GEOS_LOG_RANK_0("  Saving file");
  schemaTree.save_file(fname.c_str());

  GEOS_LOG_RANK_0("  Done!");
}


void SchemaUtilities::BuildSimpleSchemaTypes(xmlWrapper::xmlNode schemaRoot)
{
  rtTypes::typeRegex typeRegex;

  for( auto regex=typeRegex.begin() ; regex!=typeRegex.end() ; ++regex )
  {
    xmlWrapper::xmlNode newNode = schemaRoot.append_child("xsd:simpleType");
    newNode.append_attribute("name") = regex->first.c_str();
    xmlWrapper::xmlNode restrictionNode = newNode.append_child("xsd:restriction");
    restrictionNode.append_attribute("base") = "xsd:string";
    xmlWrapper::xmlNode patternNode = restrictionNode.append_child("xsd:pattern");

    // Default regex to string
    if( regex->second.empty())
    {
      GEOS_WARNING("schema regex not defined for " << regex->first << "...  Defaulting to limited string");
      patternNode.append_attribute("value") = "[a-zA-Z0-9_,\\(\\)+-/\\* \\n]*";
    }
    else
    {
      patternNode.append_attribute("value") = regex->second.c_str();
    }
  }
}


void SchemaUtilities::SchemaConstruction(Group * const group,
                                         xmlWrapper::xmlNode schemaRoot,
                                         xmlWrapper::xmlNode schemaParent,
                                         integer documentationType)
{
  // Get schema details
  InputFlags schemaType = group->getInputFlags();
  
  if ((schemaType != InputFlags::INVALID) || (documentationType == 1))
  {
    string targetName = group->getName();
    string typeName = targetName + "Type";

    // Insert the schema node if not present, then iterate over children
    if( schemaParent.find_child_by_attribute("xsd:element", "name", targetName.c_str()).empty())
    {
      // Add the entries to the current and root nodes
      xmlWrapper::xmlNode targetIncludeNode = schemaParent.append_child("xsd:element");
      targetIncludeNode.append_attribute("name") = targetName.c_str();
      targetIncludeNode.append_attribute("type") = typeName.c_str();

      // Add occurence conditions
      if((schemaType == InputFlags::REQUIRED_NONUNIQUE) || (schemaType == InputFlags::REQUIRED))
      {
        targetIncludeNode.append_attribute("minOccurs") = "1";
      }
      if((schemaType == InputFlags::OPTIONAL) || (schemaType == InputFlags::REQUIRED))
      {
        targetIncludeNode.append_attribute("maxOccurs") = "1";
      }

      // Insert a new type into the root node if not present
      xmlWrapper::xmlNode targetTypeDefNode = schemaRoot.find_child_by_attribute("xsd:complexType", "name", typeName.c_str());
      if( targetTypeDefNode.empty())
      {
        targetTypeDefNode = schemaRoot.append_child("xsd:complexType");
        targetTypeDefNode.append_attribute("name") = typeName.c_str();
      }

      // Add subgroups
      if (group->numSubGroups() > 0)
      {
        // Children are defined in a choice node
        xmlWrapper::xmlNode targetChoiceNode = targetTypeDefNode.child("xsd:choice");
        if( targetChoiceNode.empty() )
        {
          targetChoiceNode = targetTypeDefNode.prepend_child("xsd:choice");
          targetChoiceNode.append_attribute("minOccurs") = "0";
          targetChoiceNode.append_attribute("maxOccurs") = "unbounded";
        }

        // Get a list of the subgroup names in alphabetic order
        // Note: this is necessary because the order that objects
        //       are registered to catalogs may vary by compiler
        std::vector<string> subGroupNames;
        for( auto & subGroupPair : group->GetSubGroups())
        {
          subGroupNames.push_back(subGroupPair.first);
        }
        std::sort(subGroupNames.begin(), subGroupNames.end());

        // Add children of the group
        for ( string subName : subGroupNames )
        {
          Group * const subGroup = group->GetGroup(subName);
          SchemaConstruction(subGroup, schemaRoot, targetChoiceNode, documentationType);
        }
      }

      // Add schema deviations
      group->SetSchemaDeviations(schemaRoot, targetTypeDefNode, documentationType);

      // Add attributes
      // Note: wrappers that were added to this group by another group
      //       may end up in different order.  To avoid this, add them
      //       into the schema in alphabetic order.
      std::vector<string> groupWrapperNames;
      for( auto & wrapperPair : group->wrappers())
      {
        groupWrapperNames.push_back(wrapperPair.first);
      }
      std::sort(groupWrapperNames.begin(), groupWrapperNames.end());

      for ( string attributeName : groupWrapperNames )
      {
        WrapperBase * const wrapper = group->getWrapperBase(attributeName);
        InputFlags flag = wrapper->getInputFlag();
        
        if (( flag > InputFlags::FALSE ) != ( documentationType == 1 ))
        {
          // Ignore duplicate copies of attributes
          if( targetTypeDefNode.find_child_by_attribute("xsd:attribute", "name", attributeName.c_str()).empty())
          {
            // Write any additional documentation that isn't expected by the .xsd format in a comment
            // Attribute description
            string description = wrapper->getDescription();
            string commentString = attributeName + " => ";
            
            if (!description.empty())
            {
              commentString += description;
            }
            else
            {
              commentString += "(no description available)";
            }

            // List of objects that registered this field
            std::vector<string> registrars = wrapper->getRegisteringObjects();
            if (registrars.size() > 0)
            {
              commentString += " => " + registrars[0];
              for (size_t ii=1; ii<registrars.size(); ++ii)
              {
                commentString += ", " + registrars[ii];
              }
            }

            xmlWrapper::xmlNode commentNode = targetTypeDefNode.append_child(xmlWrapper::xmlTypes::node_comment);
            commentNode.set_value(commentString.c_str());


            // Write the valid schema attributes
            // Basic attributes
            xmlWrapper::xmlNode attributeNode = targetTypeDefNode.append_child("xsd:attribute");
            attributeNode.append_attribute("name") = attributeName.c_str();
            attributeNode.append_attribute("type") = (rtTypes::typeNames(wrapper->get_typeid()).c_str());

            // (Optional) Default Value
            if ( (flag == InputFlags::OPTIONAL_NONUNIQUE) || (flag == InputFlags::REQUIRED_NONUNIQUE))
            {
              GEOS_LOG_RANK_0(attributeName << " has an invalid input flag");
              GEOS_ERROR("SchemaUtilities::SchemaConstruction: duplicate xml attributes are not allowed");
            }
            else if ( flag == InputFlags::OPTIONAL )
            {
              rtTypes::TypeIDs const wrapperTypeID = rtTypes::typeID(wrapper->get_typeid());
              rtTypes::ApplyIntrinsicTypeLambda2( wrapperTypeID,
                                                  [&]( auto a, auto GEOSX_UNUSED_ARG( b ) ) -> void
              {
                using COMPOSITE_TYPE = decltype(a);
                Wrapper<COMPOSITE_TYPE>& typedWrapper = Wrapper<COMPOSITE_TYPE>::cast( *wrapper );
                
                if( typedWrapper.getDefaultValueStruct().has_default_value )
                {
                  SetDefaultValueString( typedWrapper.getDefaultValueStruct(), attributeNode );
                }
              });
            }
            else if (documentationType == 0)
            {
              attributeNode.append_attribute("use") = "required";
            }
          }  
        }
      }

      // Elements that are nonunique require the use of the name attribute
      if (((schemaType == InputFlags::REQUIRED_NONUNIQUE) || (schemaType == InputFlags::OPTIONAL_NONUNIQUE)) && (documentationType == 0))
      {
        // Only add this attribute if not present
        if( targetTypeDefNode.find_child_by_attribute("xsd:attribute", "name", "name").empty())
        {
          xmlWrapper::xmlNode commentNode = targetTypeDefNode.append_child(xmlWrapper::xmlTypes::node_comment);
          commentNode.set_value("name => A name is required for any non-unique nodes");
        
          xmlWrapper::xmlNode attributeNode = targetTypeDefNode.append_child("xsd:attribute");
          attributeNode.append_attribute("name") = "name";
          attributeNode.append_attribute("type") = "string";
          attributeNode.append_attribute("use") = "required";
        }
      }
    }
  }
}

}
