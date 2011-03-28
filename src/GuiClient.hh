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
#ifndef GUICLIENT_HH
#define GUICLIENT_HH

#include <boost/shared_ptr.hpp>

#include "common/Messages.hh"

namespace gazebo
{
  namespace gui
  {
    class SimulationApp;
  }

  class GuiClient
  {
    public: GuiClient();
    public: virtual ~GuiClient();
    public: void Load(const std::string &filename);

    public: void Run();

    public: void Quit();

    private: bool renderEngineEnabled, guiEnabled;
    private: gui::SimulationApp *gui;

    private: bool quit;
  };
}

#endif
