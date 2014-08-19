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

#include <algorithm>
#include <string>
#include <vector>
#include <sdf/sdf.hh>
#include "gazebo/common/Assert.hh"
#include "gazebo/common/Console.hh"
#include "gazebo/math/Kmeans.hh"
#include "gazebo/math/Rand.hh"
#include "gazebo/physics/Model.hh"
#include "gazebo/physics/Population.hh"
#include "gazebo/physics/World.hh"


using namespace gazebo;
using namespace common;
using namespace physics;

//////////////////////////////////////////////////
Population::Population(sdf::ElementPtr _sdf, World *_world)
  : dataPtr(new PopulationPrivate)
{
  this->dataPtr->world = _world;
  if (_sdf->HasElement("population"))
    this->dataPtr->populationElem = _sdf->GetElement("population");
}

//////////////////////////////////////////////////
Population::~Population()
{
}

//////////////////////////////////////////////////
bool Population::PopulateAll()
{
  GZ_ASSERT(this->dataPtr->populationElem, "<population> SDF element is NULL");

  sdf::ElementPtr popElem = this->dataPtr->populationElem;
  bool result = true;

  // Iterate through all the population elements in the sdf.
  while (popElem)
  {
    if (!this->PopulateOne(popElem))
      result = false;
    popElem = popElem->GetNextElement("population");
  }

  return result;
}

//////////////////////////////////////////////////
bool Population::PopulateOne(const sdf::ElementPtr _population)
{
  std::vector<math::Vector3> objects;
  PopulationParams params;

  GZ_ASSERT(_population, "'_population' parameter is NULL");

  if (!this->ParseSdf(_population, params))
    return false;

  if (params.modelCount <= 0)
  {
    gzwarn << "Trying to populate a non positive number of models ["
           << params.modelCount << "]. Population ignored." << std::endl;
    return false;
  }

  // Generate the population of poses based on the region and distribution.
  if (params.region == "box" && params.distribution == "random")
  {
    this->CreatePosesBoxRandom(params.modelCount, params.min,
      params.max, objects);
  }
  else if (params.region == "box" && params.distribution == "uniform")
  {
    this->CreatePosesBoxUniform(params.modelCount, params.min,
      params.max, objects);
  }
  else if (params.region == "box" && params.distribution == "grid")
  {
    this->CreatePosesBoxGrid(params.min, params.rows, params.cols, params.step,
      objects);
  }
  else if (params.region == "box" && params.distribution == "linear-x")
  {
    this->CreatePosesBoxLinearX(params.modelCount, params.min, params.max,
      objects);
  }
  else if (params.region == "box" && params.distribution == "linear-y")
  {
    this->CreatePosesBoxLinearY(params.modelCount, params.min, params.max,
      objects);
  }
  else if (params.region == "box" && params.distribution == "linear-z")
  {
    this->CreatePosesBoxLinearZ(params.modelCount, params.min, params.max,
      objects);
  }
  else if (params.region == "cylinder" && params.distribution == "random")
  {
    this->CreatePosesCylinderRandom(params.modelCount, params.center,
      params.radius, params.height, objects);
  }
  else if (params.region == "cylinder" && params.distribution == "uniform")
  {
    this->CreatePosesCylinderUniform(params.modelCount, params.center,
      params.radius, params.height, objects);
  }
  else
  {
    gzerr << "Unrecognized combination of region [" << params.region << "] and "
          << "distribution [" << params.distribution << "]" << std::endl;
    return false;
  }

  // Check that we have generated the appropriate number of poses.
  GZ_ASSERT(params.modelCount == static_cast<int>(objects.size()),
    "Unexpected number of objects while generating a population");

  // Create an sdf containing the model description.
  sdf::SDF sdf;
  sdf.SetFromString("<sdf version ='1.5'>" + params.modelSdf + "</sdf>");

  for (size_t i = 0; i < objects.size(); ++i)
  {
    math::Vector3 p(objects[i].x, objects[i].y, objects[i].z);

    // Create a unique model for each clone.
    std::string cloneSdf = sdf.ToString();
    std::string delim = "model name='";
    size_t first = cloneSdf.find(delim) + delim.size();
    size_t last = cloneSdf.find("'", first);
    std::string newName = params.modelName + std::string("_clone_") +
      boost::lexical_cast<std::string>(i);
    cloneSdf.replace(first, last - first, newName);

    // Insert the <pose> element.
    std::string endDelim = "'>";
    first = cloneSdf.find(delim) + delim.size();
    last = cloneSdf.find(endDelim, first);
    std::string pose = "\n    <pose>" +
      boost::lexical_cast<std::string>(p.x) + " " +
      boost::lexical_cast<std::string>(p.y) + " " +
      boost::lexical_cast<std::string>(p.z) + " 0 0 0</pose>";
    cloneSdf.insert(last + endDelim.size(), pose);

    this->dataPtr->world->InsertModelString(cloneSdf);
  }

  return true;
}

/////////////////////////////////////////////////
bool Population::ElementFromSdf(const sdf::ElementPtr &_sdfElement,
  const std::string &_element, sdf::ElementPtr &_value)
{
  if (_sdfElement->HasElement(_element))
  {
    _value = _sdfElement->GetElement(_element);
    return true;
  }
  gzerr << "Unable to find <" << _element << "> inside the population tag"
        << std::endl;
  return false;
}

/////////////////////////////////////////////////
bool Population::ParseSdf(sdf::ElementPtr _population,
  PopulationParams &_params)
{
  GZ_ASSERT(_population, "'_population' parameter is NULL");

  // Read the model element.
  sdf::ElementPtr model;
  if (!this->ElementFromSdf(_population, "model", model))
    return false;

  _params.modelSdf = model->ToString("");
  _params.modelName = model->Get<std::string>("name");

  // Read the model_count element.
  if (!this->ValueFromSdf<int>(_population, "model_count", _params.modelCount))
    return false;

  // Read the distribution element.
  sdf::ElementPtr distribution;
  if (!this->ElementFromSdf(_population, "distribution", distribution))
    return false;

  // Read the distribution type.
  if (!this->ValueFromSdf<std::string>(distribution, "type",
    _params.distribution))
  {
    return false;
  }

  if ((_params.distribution != "random")   &&
      (_params.distribution != "uniform")  &&
      (_params.distribution != "grid")     &&
      (_params.distribution != "linear-x") &&
      (_params.distribution != "linear-y") &&
      (_params.distribution != "linear-z"))
  {
    gzerr << "Unknown distribution type [" << _params.distribution << "]"
          << std::endl;
    return false;
  }

  // Models evenly distributed in a 2D grid pattern.
  if (_params.distribution == "grid")
  {
    // Read the number of rows.
    if (!this->ValueFromSdf<int>(distribution, "rows", _params.rows))
      return false;

    // Read the number of columns.
    if (!this->ValueFromSdf<int>(distribution, "cols", _params.cols))
      return false;

    // Read the <step> value used to separate each model in the grid.
    if (!this->ValueFromSdf<math::Vector3>(distribution, "step", _params.step))
      return false;
  }

  // Read the region element.
  sdf::ElementPtr region;
  if (!this->ElementFromSdf(_population, "region", region))
    return false;

  if (region->HasElement("box"))
  {
    sdf::ElementPtr box = region->GetElement("box");
    _params.region = "box";

    // Read the minimim corner of the bounding box.
    if (!this->ValueFromSdf<math::Vector3>(box, "min", _params.min))
      return false;

    if ((_params.distribution == "random")   ||
        (_params.distribution == "uniform")  ||
        (_params.distribution == "linear-x") ||
        (_params.distribution == "linear-y") ||
        (_params.distribution == "linear-z"))
    {
      // Read the maximum corner of the bounding box.
      if (!this->ValueFromSdf<math::Vector3>(box, "max", _params.max))
        return false;
    }
  }
  else if (region->HasElement("cylinder"))
  {
    sdf::ElementPtr cylinder = region->GetElement("cylinder");
    _params.region = "cylinder";

    // Read the center of the cylinder's base.
    if (!this->ValueFromSdf<math::Vector3>(cylinder, "center", _params.center))
      return false;

    // Read the radius of the cylinder's base.
    if (!this->ValueFromSdf<double>(cylinder, "radius", _params.radius))
      return false;

    // Read the cylinder's height.
    if (!this->ValueFromSdf<double>(cylinder, "height", _params.height))
      return false;
  }
  else
  {
    gzerr << "I have not found a valid region. 'box' or 'cylinder' are"
          << " the valid region types" << std::endl;
    return false;
  }

  return true;
}

/////////////////////////////////////////////////
void Population::CreatePosesBoxRandom(int _modelCount,
  const math::Vector3 &_min, const math::Vector3 &_max,
  std::vector<math::Vector3> &_poses)
{
  // _poses should be empty.
  GZ_ASSERT(_poses.empty(), "Output parameter '_poses' is not empty");

  // _modelCount should be positive.
  GZ_ASSERT(_modelCount > 0, "Argument _modelCount is nonpositive");

  double dx = std::abs(_max.x - _min.x);
  double dy = std::abs(_max.y - _min.y);
  double dz = std::abs(_max.z - _min.z);

  _poses.clear();
  for (int i = 0; i < _modelCount; ++i)
  {
    math::Vector3 p;
    p.x = std::min(_min.x, _max.x) + math::Rand::GetDblUniform(0, dx);
    p.y = std::min(_min.y, _max.y) + math::Rand::GetDblUniform(0, dy);
    p.z = std::min(_min.z, _max.z) + math::Rand::GetDblUniform(0, dz);

  _poses.push_back(p);
  }
}

/////////////////////////////////////////////////
void Population::CreatePosesBoxUniform(int _modelCount,
  const math::Vector3 &_min, const math::Vector3 &_max,
  std::vector<math::Vector3> &_poses)
{
  // _poses should be empty.
  GZ_ASSERT(_poses.empty(), "Output parameter '_poses' is not empty");

  // _modelCount should be positive.
  GZ_ASSERT(_modelCount > 0, "Argument _modelCount is nonpositive");

  std::vector<math::Vector3> obs;

  double dx = std::abs(_max.x - _min.x);
  double dy = std::abs(_max.y - _min.y);
  double dz = std::abs(_max.z - _min.z);

  // Step1: Sample points in a box.
  double x = 0.0;
  double y = 0.0;
  while (y < dy)
  {
    while (x < dx)
    {
      math::Vector3 p;
      p.x = x;
      p.y = y;
      p.z = 0;
      obs.push_back(p);
      x += .1;
    }
    x = 0.0;
    y += .1;
  }

  // Step2: Cluster the sampled points in 'modelCount' clusters.
  std::vector<math::Vector3> centroids;
  std::vector<unsigned int> labels;
  math::Kmeans kmeans(obs);
  kmeans.Cluster(_modelCount, centroids, labels);

  // Step3: Create the list of object positions.
  _poses.clear();
  for (int i = 0; i < _modelCount; ++i)
  {
    math::Vector3 p;
    p.x = std::min(_min.x, _max.x) + centroids[i].x;
    p.y = std::min(_min.y, _max.y) + centroids[i].y;
    p.z = std::min(_min.z, _max.z) + math::Rand::GetDblUniform(0, dz);
    _poses.push_back(p);
  }
}

/////////////////////////////////////////////////
void Population::CreatePosesBoxGrid(const math::Vector3 &_min, int _rows,
  int _cols, const math::Vector3 &_step, std::vector<math::Vector3> &_poses)
{
  // _poses should be empty.
  GZ_ASSERT(_poses.empty(), "Output parameter '_poses' is not empty");

  // _rows should be positive.
  GZ_ASSERT(_rows > 0, "Argument _rows is nonpositive");

  // _cols should be positive.
  GZ_ASSERT(_cols > 0, "Argument _cols is nonpositive");

  _poses.clear();
  math::Vector3 p = _min;
  for (int i = 0; i < _rows; ++i)
  {
    for (int j = 0; j < _cols; ++j)
    {
      _poses.push_back(p);
      p.x += _step.x;
    }
    p.x = _min.x;
    p.y += _step.y;
  }
}

/////////////////////////////////////////////////
void Population::CreatePosesBoxLinearX(int _modelCount,
  const math::Vector3 &_min, const math::Vector3 &_max,
  std::vector<math::Vector3> &_poses)
{
  // _poses should be empty.
  GZ_ASSERT(_poses.empty(), "Output parameter '_poses' is not empty");

  // _modelCount should be positive.
  GZ_ASSERT(_modelCount > 0, "Argument _modelCount is nonpositive");

  double dx = std::abs(_max.x - _min.x);

  // Evenly placed in a row along the global x-axis.
  _poses.clear();
  for (int i = 0; i < _modelCount; ++i)
  {
    math::Vector3 p;
    p.x = std::min(_min.x, _max.x) + i * dx / static_cast<double>(_modelCount);
    p.y = (_min.y + _max.y) / 2.0;
    p.z = (_min.z + _max.z) / 2.0;
    _poses.push_back(p);
  }
}

/////////////////////////////////////////////////
void Population::CreatePosesBoxLinearY(int _modelCount,
  const math::Vector3 &_min, const math::Vector3 &_max,
  std::vector<math::Vector3> &_poses)
{
  // _poses should be empty.
  GZ_ASSERT(_poses.empty(), "Output parameter '_poses' is not empty");

  // _modelCount should be positive.
  GZ_ASSERT(_modelCount > 0, "Argument _modelCount is nonpositive");

  double dy = std::abs(_max.y - _min.y);

  // Evenly placed in a row along the global y-axis.
  _poses.clear();
  for (int i = 0; i < _modelCount; ++i)
  {
    math::Vector3 p;
    p.x = (_min.x + _max.x) / 2.0;
    p.y = std::min(_min.y, _max.y) + i * dy / static_cast<double>(_modelCount);
    p.z = (_min.z + _max.z) / 2.0;
    _poses.push_back(p);
  }
}

/////////////////////////////////////////////////
void Population::CreatePosesBoxLinearZ(int _modelCount,
  const math::Vector3 &_min, const math::Vector3 &_max,
  std::vector<math::Vector3> &_poses)
{
  // _poses should be empty.
  GZ_ASSERT(_poses.empty(), "Output parameter '_poses' is not empty");

  // _modelCount should be positive.
  GZ_ASSERT(_modelCount > 0, "Argument _modelCount is nonpositive");

  double dz = std::abs(_max.z - _min.z);

  // Evenly placed in a row along the global z-axis.
  _poses.clear();
  for (int i = 0; i < _modelCount; ++i)
  {
    math::Vector3 p;
    p.x = (_min.x + _max.x) / 2.0;
    p.y = (_min.y + _max.y) / 2.0;
    p.z = std::min(_min.z, _max.z) + i * dz / static_cast<double>(_modelCount);
    _poses.push_back(p);
  }
}

/////////////////////////////////////////////////
void Population::CreatePosesCylinderRandom(int _modelCount,
  const math::Vector3 &_center, double _radius, double _height,
  std::vector<math::Vector3> &_poses)
{
  // _poses should be empty.
  GZ_ASSERT(_poses.empty(), "Output parameter '_poses' is not empty");

  // _modelCount should be positive.
  GZ_ASSERT(_modelCount > 0, "Argument _modelCount is nonpositive");

  _poses.clear();
  for (int i = 0; i < _modelCount; ++i)
  {
    double ang = math::Rand::GetDblUniform(0, 2 * M_PI);
    double r = math::Rand::GetDblUniform(0, _radius);
    math::Vector3 p;
    p.x = _center.x + r * cos(ang);
    p.y = _center.y + r * sin(ang);
    p.z = _center.z + math::Rand::GetDblUniform(0, _height);
    _poses.push_back(p);
  }
}

/////////////////////////////////////////////////
void Population::CreatePosesCylinderUniform(int _modelCount,
  const math::Vector3 &_center, double _radius, double _height,
  std::vector<math::Vector3> &_poses)
{
  // _poses should be empty.
  GZ_ASSERT(_poses.empty(), "Output parameter '_poses' is not empty");

  // _modelCount should be positive.
  GZ_ASSERT(_modelCount > 0, "Argument _modelCount is nonpositive");

  std::vector<math::Vector3> obs;

  // Step1: Sample points in the cylinder.
  unsigned int points = 10000;
  for (size_t i = 0; i < points; ++i)
  {
    double ang = math::Rand::GetDblUniform(0, 2 * M_PI);
    double r = math::Rand::GetDblUniform(0, _radius);
    math::Vector3 p;
    p.x = _center.x + r * cos(ang);
    p.y = _center.y + r * sin(ang);
    p.z = _center.z + math::Rand::GetDblUniform(0, _height);
    obs.push_back(p);
  }

  // Step2: Cluster the sampled points in 'modelCount' clusters.
  std::vector<math::Vector3> centroids;
  std::vector<unsigned int> labels;
  math::Kmeans kmeans(obs);
  kmeans.Cluster(_modelCount, centroids, labels);

  // Step3: Create the list of object positions.
  _poses.clear();
  for (int i = 0; i < _modelCount; ++i)
  {
    math::Vector3 p;
    p.x = centroids[i].x;
    p.y = centroids[i].y;
    p.z = math::Rand::GetDblUniform(0, _height);
    _poses.push_back(p);
  }
}
