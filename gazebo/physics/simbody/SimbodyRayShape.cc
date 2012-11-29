/*
 * Copyright 2011 Nate Koenig
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
/* Desc: A ray
 * Author: Nate Koenig
 * Date: 24 May 2009
 */

#include "physics/World.hh"
#include "physics/simbody/SimbodyLink.hh"
#include "physics/simbody/SimbodyPhysics.hh"
#include "physics/simbody/SimbodyTypes.hh"
#include "physics/simbody/SimbodyCollision.hh"
#include "physics/simbody/SimbodyRayShape.hh"

using namespace gazebo;
using namespace physics;

//////////////////////////////////////////////////
SimbodyRayShape::SimbodyRayShape(PhysicsEnginePtr _physicsEngine)
  : RayShape(_physicsEngine),
    rayCallback(btVector3(0, 0, 0), btVector3(0, 0, 0))
{
  this->SetName("ODE Ray Shape");

  this->physicsEngine =
    boost::shared_static_cast<SimbodyPhysics>(_physicsEngine);
}

//////////////////////////////////////////////////
SimbodyRayShape::SimbodyRayShape(CollisionPtr _parent)
    : RayShape(_parent),
    rayCallback(btVector3(0, 0, 0), btVector3(0, 0, 0))
{
  this->SetName("Simbody Ray Shape");
  this->physicsEngine = boost::shared_static_cast<SimbodyPhysics>(
      this->collisionParent->GetWorld()->GetPhysicsEngine());
}

//////////////////////////////////////////////////
SimbodyRayShape::~SimbodyRayShape()
{
}

//////////////////////////////////////////////////
void SimbodyRayShape::Update()
{
  this->physicsEngine->GetDynamicsWorld()->rayTest(
      this->rayCallback.m_rayFromWorld,
      this->rayCallback.m_rayToWorld,
      this->rayCallback);

    if (this->rayCallback.hasHit())
    {
      math::Vector3 result(this->rayCallback.m_hitPointWorld.getX(),
                           this->rayCallback.m_hitPointWorld.getY(),
                           this->rayCallback.m_hitPointWorld.getZ());
      this->SetLength(this->globalStartPos.Distance(result));
    }
}

//////////////////////////////////////////////////
void SimbodyRayShape::GetIntersection(double &_dist, std::string &_entity)
{
  _dist = 0;
  _entity = "";

  if (this->physicsEngine && this->collisionParent)
  {
    this->physicsEngine->GetDynamicsWorld()->rayTest(
        this->rayCallback.m_rayFromWorld,
        this->rayCallback.m_rayToWorld,
        this->rayCallback);

    if (this->rayCallback.hasHit())
    {
      math::Vector3 result(this->rayCallback.m_hitPointWorld.getX(),
                           this->rayCallback.m_hitPointWorld.getY(),
                           this->rayCallback.m_hitPointWorld.getZ());
      _dist = this->globalStartPos.Distance(result);
      _entity = static_cast<SimbodyLink*>(
          this->rayCallback.m_collisionObject->getUserPointer())->GetName();
    }
  }
}

//////////////////////////////////////////////////
void SimbodyRayShape::SetPoints(const math::Vector3 &_posStart,
                                   const math::Vector3 &_posEnd)
{
  this->globalStartPos = _posStart;
  this->globalEndPos = _posEnd;

  this->rayCallback.m_rayFromWorld.setX(_posStart.x);
  this->rayCallback.m_rayFromWorld.setY(_posStart.y);
  this->rayCallback.m_rayFromWorld.setZ(_posStart.z);

  this->rayCallback.m_rayToWorld.setX(_posEnd.x);
  this->rayCallback.m_rayToWorld.setY(_posEnd.y);
  this->rayCallback.m_rayToWorld.setZ(_posEnd.z);
}
