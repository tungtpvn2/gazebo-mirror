/*
 * Copyright 2012 Open Source Robotics Foundation
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

#ifndef _DARTMODEL_HH_
#define _DARTMODEL_HH_

#include "gazebo/physics/dart/dart_inc.h"
#include "gazebo/physics/dart/DARTTypes.hh"
#include "gazebo/physics/Model.hh"

namespace gazebo
{
  namespace physics
  {
    /// \ingroup gazebo_physics
    /// \addtogroup gazebo_physics_dart DART Physics
    /// \brief dart physics engine wrapper
    /// \{

    /// \class DARTModel
    /// \brief DART model class
    class DARTModel : public Model
    {
      /// \brief Constructor.
      /// \param[in] _parent Parent object.
      public: explicit DARTModel(BasePtr _parent);

      /// \brief Destructor.
      public: virtual ~DARTModel();

      /// \brief Load the model.
      /// \param[in] _sdf SDF parameters to load from.
      public: virtual void Load(sdf::ElementPtr _sdf);

      /// \brief Initialize the model.
      public: virtual void Init();

      /// \brief Update the model.
      public: virtual void Update();

      /// \brief Finalize the model.
      public: virtual void Fini();

      // Documentation inherited.
      // public: virtual void Reset();

      /// \brief
      public: dart::dynamics::Skeleton* GetDARTSkeleton();

      /// \brief
      public: DARTPhysicsPtr GetDARTPhysics(void) const;

      /// \brief
      public: dart::simulation::World* GetDARTWorld(void) const;

      /// \brief
      protected: dart::dynamics::Skeleton* dtSkeleton;

    };
    /// \}
  }
}
#endif
