/*
 * Copyright (C) 2015 Open Source Robotics Foundation
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

#ifndef _GAZEBO_LINK_ORIGIN_VISUAL_HH_
#define _GAZEBO_LINK_ORIGIN_VISUAL_HH_

#include <string>

#include "gazebo/math/Vector3.hh"
#include "gazebo/rendering/Visual.hh"
#include "gazebo/util/system.hh"

namespace gazebo
{
  namespace rendering
  {
    /// \addtogroup gazebo_rendering Rendering
    /// \{

    /// \class LinkOriginVisual LinkOriginVisual.hh rendering/rendering.hh
    /// \brief Visualization for link origins.
    class GZ_RENDERING_VISIBLE LinkOriginVisual : public AxisVisual
    {
      /// \brief Constructor
      /// \param[in] _name Name of the LinkOriginVisual
      /// \param[in] _parent Parent visual
      public: LinkOriginVisual(const std::string &_name, VisualPtr _parent);

      /// \brief Destructor
      public: virtual ~LinkOriginVisual();

      // Documentation inherited
      public: virtual void Load(ConstLinkPtr &_msg);

    };
    /// \}
  }
}
#endif
