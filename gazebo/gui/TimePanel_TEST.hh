/*
 * Copyright (C) 2012-2014 Open Source Robotics Foundation
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

#ifndef _TIMEPANEL_TEST_HH_
#define _TIMEPANEL_TEST_HH_

#include "gazebo/gui/QTestFixture.hh"

/// \brief A test class for the DataLogger widget.
class TimePanel_TEST : public QTestFixture
{
  Q_OBJECT

  /// \brief Test that the TimePanel gets valid times from the server.
  private slots: void ValidTimes();
};

#endif
