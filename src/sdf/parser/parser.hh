/*
 * Copyright 2011 Nate Koenig & Andrew Howard
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
*/
#ifndef SDF_PARSER_HH
#define SDF_PARSER_HH

#include <string>
#include <tinyxml.h>

#include "sdf/interface/SDF.hh"

namespace sdf
{
  /// \brief Init based on the installed sdf_format.xml file
  bool init( SDFPtr _sdf );

  // \brief Initialize the SDF interface using a file
  bool initFile(const std::string &_filename, SDFPtr _sdf);

  // \brief Initialize and SDFElement interface using a file
  bool initFile(const std::string &_filename, ElementPtr _sdf);

  // \brief Initialize the SDF interface using a string
  bool initString(const std::string &_xmlString, SDFPtr &_sdf);

  // \brief Initialize the SDF interface using a TinyXML document
  bool initDoc(TiXmlDocument *_xmlDoc, SDFPtr &_sdf);

  // \brief Initialize and SDF Element using a TinyXML document
  bool initDoc(TiXmlDocument *_xmlDoc, ElementPtr &_sdf);

  // \brief For internal use only. Do not use this function.
  bool initXml(TiXmlElement *_xml, ElementPtr &_sdf);

  /// \brief Populate the SDF values from a file
  bool readFile(const std::string &_filename, SDFPtr &_sdf);

  /// \brief Populate the SDF values from a string
  bool readString(const std::string &_xmlString, SDFPtr &_sdf);

  bool readString(const std::string &_xmlString, ElementPtr &_sdf);

  /// \brief Populate the SDF values from a TinyXML document
  bool readDoc(TiXmlDocument *_xmlDoc, SDFPtr &_sdf);

  bool readDoc(TiXmlDocument *_xmlDoc, ElementPtr &_sdf);

  // \brief For internal use only. Do not use this function.
  bool readXml(TiXmlElement *_xml, ElementPtr &_sdf);

  void copyChildren( ElementPtr &_sdf, TiXmlElement *_xml);
}

#endif
