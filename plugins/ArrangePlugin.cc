/*
 * Copyright (C) 2014 Open Source Robotics Foundation
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

#include <gazebo/common/Console.hh>
#include <gazebo/physics/Model.hh>
#include <gazebo/physics/World.hh>
#include "plugins/ArrangePlugin.hh"

using namespace gazebo;

GZ_REGISTER_WORLD_PLUGIN(ArrangePlugin)

/////////////////////////////////////////////////
ArrangePlugin::ArrangePlugin()
{
}

/////////////////////////////////////////////////
ArrangePlugin::~ArrangePlugin()
{
}

/////////////////////////////////////////////////
void ArrangePlugin::Load(physics::WorldPtr _world, sdf::ElementPtr _sdf)
{
  this->world = _world;
  this->sdf = _sdf;

  // Fill this->objects with initial poses of named models
  {
    const std::string elemName = "model_name";
    if (this->sdf->HasElement(elemName))
    {
      sdf::ElementPtr elem = this->sdf->GetElement(elemName);
      while (elem)
      {
        // Add names to map
        std::string modelName = elem->Get<std::string>();
        ObjectPtr object(new Object);
        object->model = world->GetModel(modelName);
        object->pose = object->model->GetWorldPose();
        this->objects[modelName] = object;

        elem = elem->GetNextElement(elemName);
      }
    }
  }

  // Get initial arrangement name
  {
    const std::string elemName = "initial_arrangement";
    if (this->sdf->HasElement(elemName))
    {
      this->initialArrangementName = this->sdf->Get<std::string>(elemName);
    }
  }

  // Get arrangement information
  {
    const std::string elemName = "arrangement";
    if (this->sdf->HasElement(elemName))
    {
      sdf::ElementPtr elem = this->sdf->GetElement(elemName);
      while (elem)
      {
        // Read arrangement name attribute
        if (!elem->HasAttribute("name"))
        {
          gzerr << "arrangement element missing name attribute" << std::endl;
          continue;
        }
        std::string arrangementName = elem->Get<std::string>("name");
        if (this->initialArrangementName.empty())
        {
          this->initialArrangementName = arrangementName;
        }

        // Read pose elements into Pose_M
        Pose_M poses;
        if (elem->HasElement("pose"))
        {
          sdf::ElementPtr poseElem = elem->GetElement("pose");
          while (poseElem)
          {
            // Read pose model attribute
            if (!poseElem->HasAttribute("model"))
            {
              gzerr << "In arrangement ["
                    << arrangementName
                    << "], a pose element is missing the model attribute"
                    << std::endl;
              continue;
            }
            std::string poseName = poseElem->Get<std::string>("model");
            poses[poseName] = poseElem->Get<math::Pose>();

            poseElem = poseElem->GetNextElement("pose");
          }
        }
        this->arrangements[arrangementName] = poses;

        elem = elem->GetNextElement(elemName);
      }
    }
  }
}

/////////////////////////////////////////////////
void ArrangePlugin::Init()
{
  // Set initial arrangement
  this->SetArrangement(this->initialArrangementName);
}

/////////////////////////////////////////////////
void ArrangePlugin::Reset()
{
  this->SetArrangement(this->currentArrangementName);
}

/////////////////////////////////////////////////
bool ArrangePlugin::SetArrangement(const std::string &_arrangement)
{
  if (this->arrangements.find(_arrangement) == this->arrangements.end())
  {
    gzerr << "Cannot SetArrangement ["
          << _arrangement
          << "], unrecognized arrangement name"
          << std::endl;
    return false;
  }

  this->currentArrangementName = _arrangement;
  Pose_M arrangement = this->arrangements[_arrangement];

  for (Object_M::iterator iter = this->objects.begin();
        iter != this->objects.end(); ++iter)
  {
    physics::ModelPtr model = iter->second->model;
    math::Pose pose;
    Pose_M::iterator poseIter = arrangement.find(iter->first);
    if (poseIter != arrangement.end())
    {
      // object name found in arrangement
      // use arrangement pose
      pose = poseIter->second;
    }
    else
    {
      // object name not found in arrangement
      // use initial pose
      pose = iter->second->pose;
    }
    model->SetWorldPose(pose);
    model->ResetPhysicsStates();
  }
  return true;
}

