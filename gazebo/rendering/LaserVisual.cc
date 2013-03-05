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
/* Desc: Laser Visualization Class
 * Author: Nate Koenig
 * Date: 14 Dec 2007
 */

#include "gazebo/common/MeshManager.hh"
#include "gazebo/transport/transport.hh"

#include "gazebo/rendering/Conversions.hh"
#include "gazebo/rendering/Scene.hh"
#include "gazebo/rendering/DynamicLines.hh"
#include "gazebo/rendering/LaserVisual.hh"

using namespace gazebo;
using namespace rendering;

/////////////////////////////////////////////////
LaserVisual::LaserVisual(const std::string &_name, VisualPtr _vis,
                         const std::string &_topicName)
: Visual(_name, _vis)
{
  this->node = transport::NodePtr(new transport::Node());
  this->node->Init(this->scene->GetName());

  this->laserScanSub = this->node->Subscribe(_topicName,
      &LaserVisual::OnScan, this);

  this->rayFan = this->CreateDynamicLine(rendering::RENDERING_TRIANGLE_FAN);

  this->rayFan->setMaterial("Gazebo/BlueLaser");
  this->rayFan->AddPoint(math::Vector3(0, 0, 0));
  this->SetVisibilityFlags(GZ_VISIBILITY_GUI);

  common::MeshManager::Instance()->CreateSphere("laser_contact_sphere",
      0.01, 5, 5);

  // Add the mesh into OGRE
  if (!this->sceneNode->getCreator()->hasEntity("laser_contact_sphere") &&
      common::MeshManager::Instance()->HasMesh("laser_contact_sphere"))
  {
    const common::Mesh *mesh =
      common::MeshManager::Instance()->GetMesh("laser_contact_sphere");
    this->InsertMesh(mesh);
  }
}

/////////////////////////////////////////////////
LaserVisual::~LaserVisual()
{
  delete this->rayFan;
  this->rayFan = NULL;
}

/////////////////////////////////////////////////
void LaserVisual::OnScan(ConstLaserScanStampedPtr &_msg)
{
  // Skip the update if the user is moving the laser.
  if (this->GetScene()->GetSelectedVisual() &&
      this->GetRootVisual()->GetName() ==
      this->GetScene()->GetSelectedVisual()->GetName())
  {
    return;
  }

  double angle = _msg->scan().angle_min();
  double r;
  math::Vector3 pt;
  math::Pose offset = msgs::Convert(_msg->scan().world_pose()) -
                      this->GetWorldPose();

  this->rayFan->SetPoint(0, offset.pos);
  for (size_t i = 0; static_cast<int>(i) < _msg->scan().ranges_size(); i++)
  {
    r = _msg->scan().ranges(i) + _msg->scan().range_min();
    pt.x = 0 + r * cos(angle);
    pt.y = 0 + r * sin(angle);
    pt.z = 0;
    pt += offset.pos;

    if (i+1 >= this->rayFan->GetPointCount())
      this->rayFan->AddPoint(pt);
    else
      this->rayFan->SetPoint(i+1, pt);

    if (i >= this->points.size())
    {
      std::string objName = this->GetName() + "_lasercontactpoint_" +
        boost::lexical_cast<std::string>(this->points.size());

      Ogre::Entity *obj = this->scene->GetManager()->createEntity(
          objName, "laser_contact_sphere");
      obj->setMaterialName("Gazebo/RedTransparent");

      LaserVisual::ContactPoint *cp = new LaserVisual::ContactPoint();
      cp->sceneNode = this->sceneNode->createChildSceneNode(objName + "_node");
      cp->sceneNode->attachObject(obj);
      cp->sceneNode->setVisible(true);
      cp->sceneNode->setPosition(Conversions::Convert(pt));
      this->points.push_back(cp); 
    }
    else
      this->points[i]->sceneNode->setPosition(Conversions::Convert(pt));

    angle += _msg->scan().angle_step();
  }
}

/////////////////////////////////////////////////
void LaserVisual::SetEmissive(const common::Color &/*_color*/)
{
}
