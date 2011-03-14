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
/* Desc: Bullet motion state class.
 * Author: Nate Koenig 
 * Date: 25 May 2009
 */

#ifndef BULLETMOTIONSTATE
#define BULLETMOTIONSTATE

#include <btBulletDynamicsCommon.h>

#include "common/Pose3d.hh"

namespace gazebo
{
	namespace physics
{
  class Visual;
  class Body;

  class BulletMotionState : public btMotionState
  {
    /// \brief Constructor
    public: BulletMotionState(Body *body);

    /// \brief Constructor
    //public: BulletMotionState(const Pose3d &initPose);

    /// \brief Destructor
    public: virtual ~BulletMotionState();

    /// \brief Set the visual
    public: void SetVisual(Visual *vis);

    /// \brief Get the pose
    public: Pose3d GetWorldPose() const;

    /// \brief Set the position of the body
    /// \param pos Vector position
    public: virtual void SetWorldPosition(const Vector3 &pos);

    /// \brief Set the rotation of the body
    /// \param rot Quaternion rotation
    public: virtual void SetWorldRotation(const Quatern &rot);

    /// \brief Set the pose
    public: void SetWorldPose(const Pose3d &pose);

    /// \brief Set the center of mass offset
    public: void SetCoMOffset( const Pose3d &com );

    /// \brief Get the world transform
    public: virtual void getWorldTransform(btTransform &worldTrans) const;

    /// \brief Set the world transform
    public: virtual void setWorldTransform(const btTransform &worldTrans);

    private: Visual *visual;
    private: Pose3d worldPose;
    private: Pose3d comOffset;
    private: Body *body;
  };
}
}
}
#endif
