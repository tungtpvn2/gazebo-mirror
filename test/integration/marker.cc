/*
 * Copyright (C) 2016 Open Source Robotics Foundation
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
#include <ignition/msgs.hh>
#include <ignition/transport.hh>
#include "gazebo/gui/GuiIface.hh"
#include "gazebo/gui/MainWindow.hh"
#include "marker.hh"

#include "test_config.h"

/////////////////////////////////////////////////
int Marker_TEST::WhiteCount(const int _threshold)
{
  gazebo::rendering::UserCameraPtr cam = gazebo::gui::get_active_camera();

  // Get the image data
  const unsigned char *data = cam->ImageData();
  unsigned int width = cam->ImageWidth();
  unsigned int height = cam->ImageHeight();
  unsigned int depth = cam->ImageDepth();

  int result = 0;

  // Count all white pixels
  for (unsigned int y = 0; y < height; y++)
  {
    for (unsigned int x = 0; x < width*depth; ++x)
    {
      if (data[y * width * depth + x] >= _threshold)
        result++;
    }
  }

  return result;
}

/////////////////////////////////////////////////
int Marker_TEST::MidWhiteWidth(const int _threshold)
{
  gazebo::rendering::UserCameraPtr cam = gazebo::gui::get_active_camera();

  // Get the image data
  const unsigned char *data = cam->ImageData();
  unsigned int width = cam->ImageWidth();
  unsigned int height = cam->ImageHeight();
  unsigned int depth = cam->ImageDepth();

  int result = 0;

  // Count the white pixels over the 4 middle rows
  for (unsigned int y = height/2 - 2; y < height/2 + 2; y++)
  {
    for (unsigned int x = 0; x < width*depth; ++x)
    {
      if (data[y * width * depth + x] >= _threshold)
        result++;
    }
  }

  // Return the average over the four rows.
  return result/4;
}

/////////////////////////////////////////////////
void Marker_TEST::AddRemove()
{
  this->resMaxPercentChange = 5.0;
  this->shareMaxPercentChange = 2.0;

  this->Load("worlds/empty_bright.world", false, false, false);

  gazebo::gui::MainWindow *mainWindow = new gazebo::gui::MainWindow();
  QVERIFY(mainWindow != nullptr);

  // Create the main window.
  mainWindow->Load();
  mainWindow->Init();
  mainWindow->show();

  this->ProcessEventsAndDraw(mainWindow);

  gzmsg << "Main window size: " << mainWindow->width() << "x" <<
      mainWindow->height() << std::endl;

  // Get the user camera and scene
  gazebo::rendering::UserCameraPtr cam = gazebo::gui::get_active_camera();
  QVERIFY(cam != nullptr);

  cam->SetCaptureData(true);

  // Create our node for communication
  ignition::transport::Node node;

  std::string topicName = "/marker";

  std::vector<std::string> serviceList;
  node.ServiceList(serviceList);

  QVERIFY(std::find(serviceList.begin(), serviceList.end(), topicName)
          != serviceList.end());

  // Publish to a Gazebo topic
  node.Advertise<ignition::msgs::Marker>(topicName);

  // Create the marker message
  ignition::msgs::Marker markerMsg;
  markerMsg.set_ns("default");
  markerMsg.set_id(0);
  markerMsg.set_action(ignition::msgs::Marker::ADD_MODIFY);
  markerMsg.set_type(ignition::msgs::Marker::SPHERE);

  gzmsg << "Add sphere" << std::endl;
  QVERIFY(node.Request(topicName, markerMsg));

  this->ProcessEventsAndDraw(mainWindow);

  gazebo::rendering::ScenePtr scene = gazebo::rendering::get_scene();
  QVERIFY(scene != nullptr);

  gazebo::rendering::VisualPtr vis =
    scene->GetVisual("__GZ_MARKER_VISUAL_default_0");
  QVERIFY(vis != nullptr);

#ifndef __APPLE__
  // Check that a white object is rendered
  int shapeWidth = this->MidWhiteWidth();
  QVERIFY2(shapeWidth > 750, "Measured: " + shapeWidth);
  QVERIFY2(shapeWidth < 830, "Measured: " + shapeWidth);
#endif

  // Remove the shape
  gzmsg << "Remove sphere" << std::endl;
  markerMsg.set_action(ignition::msgs::Marker::DELETE_MARKER);
  QVERIFY(node.Request(topicName, markerMsg));
  this->ProcessEventsAndDraw(mainWindow);

  QVERIFY(scene->GetVisual("__GZ_MARKER_VISUAL_default_0") == nullptr);

#ifndef __APPLE__
  shapeWidth = this->MidWhiteWidth();
  QCOMPARE(shapeWidth, 0);
#endif

  // Add a box
  gzmsg << "Add box" << std::endl;
  markerMsg.set_action(ignition::msgs::Marker::ADD_MODIFY);
  markerMsg.set_type(ignition::msgs::Marker::BOX);
  QVERIFY(node.Request(topicName, markerMsg));
  this->ProcessEventsAndDraw(mainWindow);

  QVERIFY(scene->GetVisual("__GZ_MARKER_VISUAL_default_0") != nullptr);

#ifndef __APPLE__
  shapeWidth = this->MidWhiteWidth();
  QVERIFY2(shapeWidth > 1125, "Measured: " + shapeWidth);
  QVERIFY2(shapeWidth < 1140, "Measured: " + shapeWidth);
#endif

  // Add a cylinder
  gzmsg << "Add cylinder" << std::endl;
  markerMsg.set_ns("default");
  markerMsg.set_id(1);
  ignition::msgs::Set(markerMsg.mutable_pose(),
                    ignition::math::Pose3d(2, 0, .5, 0, 0, 0));
  markerMsg.set_action(ignition::msgs::Marker::ADD_MODIFY);
  markerMsg.set_type(ignition::msgs::Marker::CYLINDER);
  QVERIFY(node.Request(topicName, markerMsg));
  this->ProcessEventsAndDraw(mainWindow);

  QVERIFY(scene->GetVisual("__GZ_MARKER_VISUAL_default_0") != nullptr);
  QVERIFY(scene->GetVisual("__GZ_MARKER_VISUAL_default_1") != nullptr);

#ifndef __APPLE__
  shapeWidth = this->MidWhiteWidth();
  QVERIFY2(shapeWidth > 2165, "Measured: " + shapeWidth);
  QVERIFY2(shapeWidth < 2180, "Measured: " + shapeWidth);
#endif

  // Delete everything
  gzmsg << "Delete everything" << std::endl;
  markerMsg.set_action(ignition::msgs::Marker::DELETE_ALL);
  QVERIFY(node.Request(topicName, markerMsg));
  this->ProcessEventsAndDraw(mainWindow);

  QVERIFY(scene->GetVisual("__GZ_MARKER_VISUAL_default_0") == nullptr);
  QVERIFY(scene->GetVisual("__GZ_MARKER_VISUAL_default_1") == nullptr);

#ifndef __APPLE__
  shapeWidth = this->MidWhiteWidth();
  QCOMPARE(shapeWidth, 0);
#endif

  // Draw a vertical line using LINE_LIST
  gzmsg << "Draw line list" << std::endl;
  markerMsg.set_id(2);
  ignition::msgs::Set(markerMsg.mutable_pose(),
                    ignition::math::Pose3d(0, 0, 0, 0, 0, 0));
  markerMsg.set_action(ignition::msgs::Marker::ADD_MODIFY);
  markerMsg.set_type(ignition::msgs::Marker::LINE_LIST);
  ignition::msgs::Set(markerMsg.add_point(),
      ignition::math::Vector3d(0, 0, -10));
  ignition::msgs::Set(markerMsg.add_point(),
      ignition::math::Vector3d(0, 0, 10));
  QVERIFY(node.Request(topicName, markerMsg));
  this->ProcessEventsAndDraw(mainWindow);

  QVERIFY(scene->GetVisual("__GZ_MARKER_VISUAL_default_0") == nullptr);
  QVERIFY(scene->GetVisual("__GZ_MARKER_VISUAL_default_1") == nullptr);
  QVERIFY(scene->GetVisual("__GZ_MARKER_VISUAL_default_2") != nullptr);

#ifndef __APPLE__
  shapeWidth = this->MidWhiteWidth(180);
  QVERIFY2(shapeWidth > 0, "Measured: " + shapeWidth);
  QVERIFY2(shapeWidth < 10, "Measured: " + shapeWidth);
#endif

  // Draw another vertical line using LINE_STRIP
  gzmsg << "Draw line strip" << std::endl;
  markerMsg.set_id(3);
  ignition::msgs::Set(markerMsg.mutable_pose(),
                    ignition::math::Pose3d(0, 0, 0, 0, 0, 0));
  markerMsg.set_action(ignition::msgs::Marker::ADD_MODIFY);
  markerMsg.set_type(ignition::msgs::Marker::LINE_STRIP);
  ignition::msgs::Set(markerMsg.add_point(),
      ignition::math::Vector3d(1, 0, -10));
  ignition::msgs::Set(markerMsg.add_point(),
      ignition::math::Vector3d(1, 0, 10));
  ignition::msgs::Set(markerMsg.add_point(),
      ignition::math::Vector3d(2, 0, 10));
  ignition::msgs::Set(markerMsg.add_point(),
      ignition::math::Vector3d(2, 0, -10));
  QVERIFY(node.Request(topicName, markerMsg));
  this->ProcessEventsAndDraw(mainWindow);

  QVERIFY(scene->GetVisual("__GZ_MARKER_VISUAL_default_0") == nullptr);
  QVERIFY(scene->GetVisual("__GZ_MARKER_VISUAL_default_1") == nullptr);
  QVERIFY(scene->GetVisual("__GZ_MARKER_VISUAL_default_2") != nullptr);
  QVERIFY(scene->GetVisual("__GZ_MARKER_VISUAL_default_3") != nullptr);

#ifndef __APPLE__
  shapeWidth = this->MidWhiteWidth(180);
  QVERIFY2(shapeWidth > 10, "Measured: " + shapeWidth);
  QVERIFY2(shapeWidth < 20, "Measured: " + shapeWidth);
#endif

  // Delete everything
  gzmsg << "Delete everything" << std::endl;
  markerMsg.set_action(ignition::msgs::Marker::DELETE_ALL);
  QVERIFY(node.Request(topicName, markerMsg));
  this->ProcessEventsAndDraw(mainWindow);

  QVERIFY(scene->GetVisual("__GZ_MARKER_VISUAL_default_0") == nullptr);
  QVERIFY(scene->GetVisual("__GZ_MARKER_VISUAL_default_1") == nullptr);
  QVERIFY(scene->GetVisual("__GZ_MARKER_VISUAL_default_2") == nullptr);
  QVERIFY(scene->GetVisual("__GZ_MARKER_VISUAL_default_3") == nullptr);

#ifndef __APPLE__
  int count = this->WhiteCount(100);
  QCOMPARE(count, 0);
#endif

  // Draw a bunch of points
  gzmsg << "Draw points" << std::endl;
  markerMsg.set_id(4);
  markerMsg.set_action(ignition::msgs::Marker::ADD_MODIFY);
  markerMsg.set_type(ignition::msgs::Marker::POINTS);
  markerMsg.clear_point();
  for (int i = 0; i < 100; ++i)
  {
    ignition::msgs::Set(markerMsg.add_point(),
        ignition::math::Vector3d(
          ignition::math::Rand::DblUniform(-1, 1),
          ignition::math::Rand::DblUniform(-1, 1),
          ignition::math::Rand::DblUniform(-1, 1)));
  }
  QVERIFY(node.Request(topicName, markerMsg));
  this->ProcessEventsAndDraw(mainWindow);

  QVERIFY(scene->GetVisual("__GZ_MARKER_VISUAL_default_0") == nullptr);
  QVERIFY(scene->GetVisual("__GZ_MARKER_VISUAL_default_1") == nullptr);
  QVERIFY(scene->GetVisual("__GZ_MARKER_VISUAL_default_2") == nullptr);
  QVERIFY(scene->GetVisual("__GZ_MARKER_VISUAL_default_3") == nullptr);
  QVERIFY(scene->GetVisual("__GZ_MARKER_VISUAL_default_4") != nullptr);

#ifndef __APPLE__
  count = this->WhiteCount(180);
  QVERIFY2(count > 480, "Counted: " + count);
  QVERIFY2(count < 570, "Counted: " + count);
#endif

  // Delete everything
  gzmsg << "Delete everything" << std::endl;
  markerMsg.set_action(ignition::msgs::Marker::DELETE_ALL);
  QVERIFY(node.Request(topicName, markerMsg));
  this->ProcessEventsAndDraw(mainWindow);

  QVERIFY(scene->GetVisual("__GZ_MARKER_VISUAL_default_0") == nullptr);
  QVERIFY(scene->GetVisual("__GZ_MARKER_VISUAL_default_1") == nullptr);
  QVERIFY(scene->GetVisual("__GZ_MARKER_VISUAL_default_2") == nullptr);
  QVERIFY(scene->GetVisual("__GZ_MARKER_VISUAL_default_3") == nullptr);
  QVERIFY(scene->GetVisual("__GZ_MARKER_VISUAL_default_4") == nullptr);

#ifndef __APPLE__
  QVERIFY(this->WhiteCount(100) == 0);
#endif

  // Draw text
  gzmsg << "Draw text" << std::endl;
  markerMsg.set_id(5);
  markerMsg.set_action(ignition::msgs::Marker::ADD_MODIFY);
  markerMsg.set_type(ignition::msgs::Marker::TEXT);
  markerMsg.set_text("HELLO");
  ignition::msgs::Set(markerMsg.mutable_pose(),
                      ignition::math::Pose3d(0, 0, 0.5, 0, 0, 0));
  QVERIFY(node.Request(topicName, markerMsg));
  this->ProcessEventsAndDraw(mainWindow);

  QVERIFY(scene->GetVisual("__GZ_MARKER_VISUAL_default_0") == nullptr);
  QVERIFY(scene->GetVisual("__GZ_MARKER_VISUAL_default_1") == nullptr);
  QVERIFY(scene->GetVisual("__GZ_MARKER_VISUAL_default_2") == nullptr);
  QVERIFY(scene->GetVisual("__GZ_MARKER_VISUAL_default_3") == nullptr);
  QVERIFY(scene->GetVisual("__GZ_MARKER_VISUAL_default_4") == nullptr);
  QVERIFY(scene->GetVisual("__GZ_MARKER_VISUAL_default_5") != nullptr);

#ifndef __APPLE__
  shapeWidth = this->MidWhiteWidth(250);
  QVERIFY2(shapeWidth > 100, "Measured: " + shapeWidth);
  QVERIFY2(shapeWidth < 130, "Measured: " + shapeWidth);
#endif

  // Remove the text
  gzmsg << "Remove text" << std::endl;
  markerMsg.set_action(ignition::msgs::Marker::DELETE_MARKER);
  QVERIFY(node.Request(topicName, markerMsg));
  this->ProcessEventsAndDraw(mainWindow);

  QVERIFY(scene->GetVisual("__GZ_MARKER_VISUAL_default_0") == nullptr);
  QVERIFY(scene->GetVisual("__GZ_MARKER_VISUAL_default_1") == nullptr);
  QVERIFY(scene->GetVisual("__GZ_MARKER_VISUAL_default_2") == nullptr);
  QVERIFY(scene->GetVisual("__GZ_MARKER_VISUAL_default_3") == nullptr);
  QVERIFY(scene->GetVisual("__GZ_MARKER_VISUAL_default_4") == nullptr);
  QVERIFY(scene->GetVisual("__GZ_MARKER_VISUAL_default_5") == nullptr);

#ifndef __APPLE__
  shapeWidth = this->MidWhiteWidth();
  QCOMPARE(shapeWidth, 0);
#endif

  // Draw a triangle fan
  gzmsg << "Draw triangle fan" << std::endl;
  markerMsg.set_id(5);
  markerMsg.set_action(ignition::msgs::Marker::ADD_MODIFY);
  markerMsg.set_type(ignition::msgs::Marker::TRIANGLE_FAN);
  markerMsg.clear_point();
  ignition::msgs::Set(markerMsg.mutable_pose(),
                    ignition::math::Pose3d(0, 0, 0, 0, 0, 0));
  ignition::msgs::Set(markerMsg.add_point(),
        ignition::math::Vector3d(0, 0, 0.05));
  double radius = 2;
  for (double t = 0; t <= M_PI; t+= 0.01)
  {
    ignition::msgs::Set(markerMsg.add_point(),
        ignition::math::Vector3d(radius * cos(t), radius * sin(t), 0.05));
  }
  QVERIFY(node.Request(topicName, markerMsg));
  this->ProcessEventsAndDraw(mainWindow);

  QVERIFY(scene->GetVisual("__GZ_MARKER_VISUAL_default_0") == nullptr);
  QVERIFY(scene->GetVisual("__GZ_MARKER_VISUAL_default_1") == nullptr);
  QVERIFY(scene->GetVisual("__GZ_MARKER_VISUAL_default_2") == nullptr);
  QVERIFY(scene->GetVisual("__GZ_MARKER_VISUAL_default_3") == nullptr);
  QVERIFY(scene->GetVisual("__GZ_MARKER_VISUAL_default_4") == nullptr);
  QVERIFY(scene->GetVisual("__GZ_MARKER_VISUAL_default_5") != nullptr);

#ifndef __APPLE__
  shapeWidth = this->MidWhiteWidth();
  QVERIFY2(shapeWidth > 1480, "Measured: " + shapeWidth);
  QVERIFY2(shapeWidth < 1500, "Measured: " + shapeWidth);
#endif

  // Remove the triangle fan
  gzmsg << "Remove triangle fan" << std::endl;
  markerMsg.set_action(ignition::msgs::Marker::DELETE_MARKER);
  QVERIFY(node.Request(topicName, markerMsg));
  this->ProcessEventsAndDraw(mainWindow);

  QVERIFY(scene->GetVisual("__GZ_MARKER_VISUAL_default_0") == nullptr);
  QVERIFY(scene->GetVisual("__GZ_MARKER_VISUAL_default_1") == nullptr);
  QVERIFY(scene->GetVisual("__GZ_MARKER_VISUAL_default_2") == nullptr);
  QVERIFY(scene->GetVisual("__GZ_MARKER_VISUAL_default_3") == nullptr);
  QVERIFY(scene->GetVisual("__GZ_MARKER_VISUAL_default_4") == nullptr);
  QVERIFY(scene->GetVisual("__GZ_MARKER_VISUAL_default_5") == nullptr);

#ifndef __APPLE__
  shapeWidth = this->MidWhiteWidth();
  QCOMPARE(shapeWidth, 0);
#endif

  // Draw a triangle list
  gzmsg << "Draw triangle list" << std::endl;
  markerMsg.set_id(6);
  markerMsg.set_action(ignition::msgs::Marker::ADD_MODIFY);
  markerMsg.set_type(ignition::msgs::Marker::TRIANGLE_LIST);
  markerMsg.clear_point();
  ignition::msgs::Set(markerMsg.mutable_pose(),
                    ignition::math::Pose3d(0, -1.5, 0, 0, 0, 0));
  ignition::msgs::Set(markerMsg.add_point(),
        ignition::math::Vector3d(0, 0, 0.5));
  ignition::msgs::Set(markerMsg.add_point(),
      ignition::math::Vector3d(1, 0, 0.5));
  ignition::msgs::Set(markerMsg.add_point(),
      ignition::math::Vector3d(1, 1, 0.5));

  ignition::msgs::Set(markerMsg.add_point(),
      ignition::math::Vector3d(1, 1, 0.5));
  ignition::msgs::Set(markerMsg.add_point(),
      ignition::math::Vector3d(2, 1, 0.5));
  ignition::msgs::Set(markerMsg.add_point(),
      ignition::math::Vector3d(2, 2, 0.5));

  QVERIFY(node.Request(topicName, markerMsg));
  this->ProcessEventsAndDraw(mainWindow);

  QVERIFY(scene->GetVisual("__GZ_MARKER_VISUAL_default_0") == nullptr);
  QVERIFY(scene->GetVisual("__GZ_MARKER_VISUAL_default_1") == nullptr);
  QVERIFY(scene->GetVisual("__GZ_MARKER_VISUAL_default_2") == nullptr);
  QVERIFY(scene->GetVisual("__GZ_MARKER_VISUAL_default_3") == nullptr);
  QVERIFY(scene->GetVisual("__GZ_MARKER_VISUAL_default_4") == nullptr);
  QVERIFY(scene->GetVisual("__GZ_MARKER_VISUAL_default_5") == nullptr);
  QVERIFY(scene->GetVisual("__GZ_MARKER_VISUAL_default_6") != nullptr);

#ifndef __APPLE__
  shapeWidth = this->MidWhiteWidth();
  QVERIFY2(shapeWidth > 5, "Measured: " + shapeWidth);
  QVERIFY2(shapeWidth < 30, "Measured: " + shapeWidth);
#endif

  // Remove the triangle list
  gzmsg << "Remove triangle list" << std::endl;
  markerMsg.set_action(ignition::msgs::Marker::DELETE_MARKER);
  QVERIFY(node.Request(topicName, markerMsg));
  this->ProcessEventsAndDraw(mainWindow);

  QVERIFY(scene->GetVisual("__GZ_MARKER_VISUAL_default_0") == nullptr);
  QVERIFY(scene->GetVisual("__GZ_MARKER_VISUAL_default_1") == nullptr);
  QVERIFY(scene->GetVisual("__GZ_MARKER_VISUAL_default_2") == nullptr);
  QVERIFY(scene->GetVisual("__GZ_MARKER_VISUAL_default_3") == nullptr);
  QVERIFY(scene->GetVisual("__GZ_MARKER_VISUAL_default_4") == nullptr);
  QVERIFY(scene->GetVisual("__GZ_MARKER_VISUAL_default_5") == nullptr);
  QVERIFY(scene->GetVisual("__GZ_MARKER_VISUAL_default_6") == nullptr);

#ifndef __APPLE__
  shapeWidth = this->MidWhiteWidth();
  QCOMPARE(shapeWidth, 0);
#endif

  // Draw a triangle strip
  gzmsg << "Draw triangle strip" << std::endl;
  markerMsg.set_id(7);
  markerMsg.set_action(ignition::msgs::Marker::ADD_MODIFY);
  markerMsg.set_type(ignition::msgs::Marker::TRIANGLE_STRIP);
  markerMsg.clear_point();
  ignition::msgs::Set(markerMsg.add_point(),
        ignition::math::Vector3d(0, 0, 0.3));
  ignition::msgs::Set(markerMsg.add_point(),
      ignition::math::Vector3d(1, 0, 0.3));
  ignition::msgs::Set(markerMsg.add_point(),
      ignition::math::Vector3d(0, 1, 0.3));

  ignition::msgs::Set(markerMsg.add_point(),
      ignition::math::Vector3d(1, 1, 0.3));
  ignition::msgs::Set(markerMsg.add_point(),
      ignition::math::Vector3d(0, 2, 0.3));
  ignition::msgs::Set(markerMsg.add_point(),
      ignition::math::Vector3d(1, 2, 0.3));
  QVERIFY(node.Request(topicName, markerMsg));
  this->ProcessEventsAndDraw(mainWindow);

  QVERIFY(scene->GetVisual("__GZ_MARKER_VISUAL_default_0") == nullptr);
  QVERIFY(scene->GetVisual("__GZ_MARKER_VISUAL_default_1") == nullptr);
  QVERIFY(scene->GetVisual("__GZ_MARKER_VISUAL_default_2") == nullptr);
  QVERIFY(scene->GetVisual("__GZ_MARKER_VISUAL_default_3") == nullptr);
  QVERIFY(scene->GetVisual("__GZ_MARKER_VISUAL_default_4") == nullptr);
  QVERIFY(scene->GetVisual("__GZ_MARKER_VISUAL_default_5") == nullptr);
  QVERIFY(scene->GetVisual("__GZ_MARKER_VISUAL_default_6") == nullptr);
  QVERIFY(scene->GetVisual("__GZ_MARKER_VISUAL_default_7") != nullptr);

#ifndef __APPLE__
  shapeWidth = this->MidWhiteWidth();
  QVERIFY2(shapeWidth > 1300, "Measured: " + shapeWidth);
  QVERIFY2(shapeWidth < 1330, "Measured: " + shapeWidth);
#endif

  mainWindow->close();
  delete mainWindow;
}

// Generate a main function for the test
QTEST_MAIN(Marker_TEST)
