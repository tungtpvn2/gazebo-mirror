/*
 *  Gazebo - Outdoor Multi-Robot Simulator
 *  Copyright (C) 2003
 *     Nate Koenig & Andrew Howard
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */
/* Desc: The Simulator; Top level managing object
 * Author: Jordi Polo
 * Date: 3 Jan 2008
 */

#include <assert.h>
#include <iostream>
#include <fstream>
#include <sys/time.h>
#include <boost/bind.hpp>
#include <boost/thread/recursive_mutex.hpp>

#include "RenderState.hh"
#include "gazebo_config.h"
#include "Plugin.hh"
#include "Timer.hh"
#include "Body.hh"
#include "Geom.hh"
#include "Model.hh"
#include "Entity.hh"
#include "OgreVisual.hh"
#include "World.hh"
#include "XMLConfig.hh"
#include "SimulationApp.hh"
#include "GazeboConfig.hh"
#include "gz.h"
#include "PhysicsEngine.hh"
#include "OgreAdaptor.hh"
#include "GazeboMessage.hh"
#include "GazeboError.hh"
#include "Global.hh"

#include "Simulator.hh"

using namespace gazebo;

std::string Simulator::defaultWorld = 
"<?xml version='1.0'?> <gazebo:world xmlns:xi='http://www.w3.org/2001/XInclude' xmlns:gazebo='http://playerstage.sourceforge.net/gazebo/xmlschema/#gz' xmlns:model='http://playerstage.sourceforge.net/gazebo/xmlschema/#model' xmlns:sensor='http://playerstage.sourceforge.net/gazebo/xmlschema/#sensor' xmlns:body='http://playerstage.sourceforge.net/gazebo/xmlschema/#body' xmlns:geom='http://playerstage.sourceforge.net/gazebo/xmlschema/#geom' xmlns:joint='http://playerstage.sourceforge.net/gazebo/xmlschema/#joint' xmlns:interface='http://playerstage.sourceforge.net/gazebo/xmlschema/#interface' xmlns:rendering='http://playerstage.sourceforge.net/gazebo/xmlschema/#rendering' xmlns:renderable='http://playerstage.sourceforge.net/gazebo/xmlschema/#renderable' xmlns:controller='http://playerstage.sourceforge.net/gazebo/xmlschema/#controller' xmlns:physics='http://playerstage.sourceforge.net/gazebo/xmlschema/#physics' >\
  <physics:ode>\
    <stepTime>0.001</stepTime>\
    <gravity>0 0 -9.8</gravity>\
    <cfm>0.0000000001</cfm>\
    <erp>0.2</erp>\
    <stepType>quick</stepType>\
    <stepIters>10</stepIters>\
    <stepW>1.3</stepW>\
    <contactMaxCorrectingVel>100.0</contactMaxCorrectingVel>\
    <contactSurfaceLayer>0.001</contactSurfaceLayer>\
  </physics:ode>\
  <rendering:gui>\
    <type>fltk</type>\
    <size>800 600</size>\
    <pos>0 0</pos>\
  </rendering:gui>\
  <rendering:ogre>\
    <ambient>.1 .1 .1 1</ambient>\
    <shadows>true</shadows>\
    <grid>false</grid>\
  </rendering:ogre>\
   <model:physical name=\"plane1_model\">\
    <xyz>0 0 0</xyz>\
    <rpy>0 0 0</rpy>\
    <static>true</static>\
    <body:plane name=\"plane1_body\">\
      <geom:plane name=\"plane1_geom\">\
        <normal>0 0 1</normal>\
        <size>100 100</size>\
        <segments>1 1</segments>\
        <uvTile>100 100</uvTile>\
        <material>Gazebo/GrayGrid</material>\
        <mu1>109999.0</mu1>\
        <mu2>1000.0</mu2>\
      </geom:plane>\
    </body:plane>\
  </model:physical>\
  <model:renderable name='directional_light'>\
    <xyz>0.0 0 10</xyz>\
    <static>true</static>\
    <light>\
      <type>directional</type>\
      <diffuseColor>0.6 0.6 0.6 1.0</diffuseColor>\
      <specularColor>.1 .1 .1 1.0</specularColor>\
      <attenuation>.2 0.1 0.0</attenuation>\
      <range>100</range>\
      <direction>-.4 0 -0.6</direction>\
      <castShadows>true</castShadows>\
    </light>\
  </model:renderable>\
</gazebo:world>";

////////////////////////////////////////////////////////////////////////////////
// Constructor
Simulator::Simulator()
: gui(NULL),
  renderEngine(NULL),
  gazeboConfig(NULL),
  loaded(false),
  pause(false),
  simTime(0.0),
  pauseTime(0.0),
  startTime(0.0),
  physicsUpdates(0),
  checkpoint(0.0),
  renderUpdates(0),
  stepInc(false),
  userQuit(false),
  physicsQuit(false),
  guiEnabled(true),
  renderEngineEnabled(true),
  physicsEnabled(true),
  timeout(-1)
{
  this->render_mutex = new boost::recursive_mutex();
  this->model_delete_mutex = new boost::recursive_mutex();
  this->startTime = this->GetWallTime();
  this->gazeboConfig=new gazebo::GazeboConfig();
  this->pause = false;
}

////////////////////////////////////////////////////////////////////////////////
// Destructor
Simulator::~Simulator()
{
  if (this->gazeboConfig)
  {
    delete this->gazeboConfig;
    this->gazeboConfig = NULL;
  }

  if (this->render_mutex)
  {
    delete this->render_mutex;
    this->render_mutex = NULL;
  }

  if (this->model_delete_mutex)
  {
    delete this->model_delete_mutex;
    this->model_delete_mutex = NULL;
  }

  if (this->physicsThread)
  {
    delete this->physicsThread;
    this->physicsThread = NULL;
  }

}

////////////////////////////////////////////////////////////////////////////////
/// Closes the Simulator and frees everything
void Simulator::Close()
{
  if (!this->loaded)
    return;

  gazebo::World::Instance()->Close();

  if (this->renderEngineEnabled)
    gazebo::OgreAdaptor::Instance()->Close();
}

////////////////////////////////////////////////////////////////////////////////
/// Load the world configuration file
/// Any error that reach this level must make the simulator exit
void Simulator::Load(const std::string &worldFileName, unsigned int serverId )
{
  this->state = LOAD;

  if (loaded)
  {
    this->Close();
    loaded=false;
  }

  // Load the world file
  XMLConfig *xmlFile=new gazebo::XMLConfig();

  try
  {
    if (worldFileName.size())
      xmlFile->Load(worldFileName);
    else
      xmlFile->LoadString(defaultWorld);
  }
  catch (GazeboError e)
  {
    gzthrow("The XML config file can not be loaded, please make sure is a correct file\n" << e); 
  }

  XMLConfigNode *rootNode(xmlFile->GetRootNode());

  // Load the messaging system
  gazebo::GazeboMessage::Instance()->Load(rootNode);

  // load the configuration options 
  try
  {
    this->gazeboConfig->Load();
  }
  catch (GazeboError e)
  {
    gzthrow("Error loading the Gazebo configuration file, check the .gazeborc file on your HOME directory \n" << e); 
  }

  // Load the Ogre rendering system
  if (this->renderEngineEnabled)
    OgreAdaptor::Instance()->Load(rootNode);

  // Create and initialize the Gui
  if (this->renderEngineEnabled && this->guiEnabled)
  {
    try
    {
      XMLConfigNode *childNode = NULL;
      if (rootNode)
       childNode = rootNode->GetChild("gui");

      int width=0;
      int height=0;
      int x = 0;
      int y = 0;

      if (childNode)
      {
        width = childNode->GetTupleInt("size", 0, 800);
        height = childNode->GetTupleInt("size", 1, 600);
        x = childNode->GetTupleInt("pos",0,0);
        y = childNode->GetTupleInt("pos",1,0);
      }

        // Create the GUI
      if (!this->gui && (childNode || !rootNode))
      {
        this->gui = new SimulationApp();
        this->gui->Load();
      }
    }
    catch (GazeboError e)
    {
      gzthrow( "Error loading the GUI\n" << e);
    }
  }
  else
  {
    this->gui = NULL;
  }

  //Initialize RenderEngine
  if (this->renderEngineEnabled)
  {
    try
    {
      OgreAdaptor::Instance()->Init(rootNode);
      this->renderEngine = OgreAdaptor::Instance();
    }
    catch (gazebo::GazeboError e)
    {
      gzthrow("Failed to Initialize the Rendering engine subsystem\n" << e );
    }
  }

  // Initialize the GUI
  if (this->gui)
  {
    this->gui->Init();
  }

  //Create the world
  try
  {
    gazebo::World::Instance()->Load(rootNode, serverId);
  }
  catch (GazeboError e)
  {
    gzthrow("Failed to load the World\n"  << e);
  }

  XMLConfigNode *pluginNode = rootNode->GetChild("plugin");
  while (pluginNode != NULL)
  {
    this->AddPlugin( pluginNode->GetString("filename","",1), 
                     pluginNode->GetString("handle","",1) );
    pluginNode = pluginNode->GetNext("plugin");
  }

  this->loaded=true;
}

////////////////////////////////////////////////////////////////////////////////
/// Initialize the simulation
void Simulator::Init()
{
  this->state = INIT;

  RenderState::Init();

  //Initialize the world
  try
  {
    gazebo::World::Instance()->Init();
  }
  catch (GazeboError e)
  {
    gzthrow("Failed to Initialize the World\n"  << e);
  }

  // This is not a debug line. This is useful for external programs that 
  // launch Gazebo and wait till it is ready   
  std::cout << "Gazebo successfully initialized" << std::endl;
}

////////////////////////////////////////////////////////////////////////////////
/// Save the world configuration file
void Simulator::Save(const std::string& filename)
{
  std::fstream output;

  output.open(filename.c_str(), std::ios::out);

  // Write out the xml header
  output << "<?xml version=\"1.0\"?>\n";
  output << "<gazebo:world\n\
    xmlns:xi=\"http://www.w3.org/2001/XInclude\"\n\
    xmlns:gazebo=\"http://playerstage.sourceforge.net/gazebo/xmlschema/#gz\"\n\
    xmlns:model=\"http://playerstage.sourceforge.net/gazebo/xmlschema/#model\"\n\
    xmlns:sensor=\"http://playerstage.sourceforge.net/gazebo/xmlschema/#sensor\"\n\
    xmlns:window=\"http://playerstage.sourceforge.net/gazebo/xmlschema/#window\"\n\
    xmlns:param=\"http://playerstage.sourceforge.net/gazebo/xmlschema/#param\"\n\
    xmlns:body=\"http://playerstage.sourceforge.net/gazebo/xmlschema/#body\"\n\
    xmlns:geom=\"http://playerstage.sourceforge.net/gazebo/xmlschema/#geom\"\n\
    xmlns:joint=\"http://playerstage.sourceforge.net/gazebo/xmlschema/#joint\"\n\
    xmlns:interface=\"http://playerstage.sourceforge.net/gazebo/xmlschema/#interface\"\n\
    xmlns:ui=\"http://playerstage.sourceforge.net/gazebo/xmlschema/#ui\"\n\
    xmlns:rendering=\"http://playerstage.sourceforge.net/gazebo/xmlschema/#rendering\"\n\
    xmlns:controller=\"http://playerstage.sourceforge.net/gazebo/xmlschema/#controller\"\n\
    xmlns:physics=\"http://playerstage.sourceforge.net/gazebo/xmlschema/#physics\">\n\n";

  std::string prefix = "  ";

  if (output.is_open())
  {
    GazeboMessage::Instance()->Save(prefix, output);
    output << "\n";

    World::Instance()->GetPhysicsEngine()->Save(prefix, output);
    output << "\n";

    if (this->renderEngineEnabled)
    {
      this->GetRenderEngine()->Save(prefix, output);
      output << "\n";
    }

    this->gui->Save(prefix, output);
    output << "\n";

    World::Instance()->Save(prefix, output);
    output << "\n";

    output << "</gazebo:world>\n";
    output.close();
  }
  else
  {
    gzerr(0) << "Unable to save XML file to file[" << filename << "]\n";
  }
}

////////////////////////////////////////////////////////////////////////////////
/// Finalize the simulation
void Simulator::Fini( )
{
  gazebo::World::Instance()->Fini();

  if (this->renderEngineEnabled)
    gazebo::OgreAdaptor::Instance()->Fini();

  this->Close();
}

////////////////////////////////////////////////////////////////////////////////
// Stop the physics engine
void Simulator::StopPhysics()
{
  this->physicsQuit = true;
  if (this->physicsThread)
  {
    this->physicsThread->join();
    delete this->physicsThread;
    this->physicsThread = NULL;
  }
}

////////////////////////////////////////////////////////////////////////////////
// Start the physics engine
void Simulator::StartPhysics()
{
  if (this->physicsThread)
    this->StopPhysics();

  this->physicsQuit = false;
  this->physicsThread = new boost::thread( 
      boost::bind(&Simulator::PhysicsLoop, this));
}

////////////////////////////////////////////////////////////////////////////////
/// Main simulation loop, when this loop ends the simulation finish
void Simulator::Run()
{
  this->state = RUN;

  //DiagnosticTimer timer("--------------------------- START Simulator::MainLoop() --------------------------");
  Time currTime = 0;
  Time lastTime = 0;
  struct timespec timeSpec;
  double freq = 80.0;

  this->StartPhysics();

  if (this->gui)
    this->gui->Run();
  else
  {
    while (!this->userQuit)
    {
      currTime = this->GetWallTime();
      if ( currTime - lastTime > 1.0/freq)
      {
        lastTime = this->GetWallTime();

        this->GraphicsUpdate();
        currTime = this->GetWallTime();
        if (currTime - lastTime < 1/freq)
        {
          Time sleepTime = ( Time(1.0/freq) - (currTime - lastTime));
          timeSpec.tv_sec = sleepTime.sec;
          timeSpec.tv_nsec = sleepTime.nsec;

          nanosleep(&timeSpec, NULL);
        }
      }
      else
      {
        Time sleepTime = ( Time(1.0/freq) - (currTime - lastTime));
        timeSpec.tv_sec = sleepTime.sec;
        timeSpec.tv_nsec = sleepTime.nsec;
        nanosleep(&timeSpec, NULL);
      }
    }
  }

  this->StopPhysics();
}

void Simulator::GraphicsUpdate()
{
  // Update the gui
  //while (!this->userQuit)
  //{
    //currTime = this->GetWallTime();
    //if ( currTime - lastTime > 1.0/freq)
    //{
      //lastTime = this->GetWallTime();

      if (this->gui)
        this->gui->Update();

      if (this->renderEngineEnabled)
      {
        OgreAdaptor::Instance()->UpdateScenes();
        World::Instance()->GraphicsUpdate();
      }

      //currTime = this->GetWallTime();

      World::Instance()->ProcessEntitiesToLoad();
      World::Instance()->ProcessEntitiesToDelete();

      /*if (currTime - lastTime < 1/freq)
      {
        Time sleepTime = ( Time(1.0/freq) - (currTime - lastTime));
        timeSpec.tv_sec = sleepTime.sec;
        timeSpec.tv_nsec = sleepTime.nsec;

        nanosleep(&timeSpec, NULL);
      }
    }
    else
    {
      Time sleepTime = ( Time(1.0/freq) - (currTime - lastTime));
      timeSpec.tv_sec = sleepTime.sec;
      timeSpec.tv_nsec = sleepTime.nsec;
      nanosleep(&timeSpec, NULL);
    }
    */
  //}

}

////////////////////////////////////////////////////////////////////////////////
/// Gets local configuration for this computer
GazeboConfig *Simulator::GetGazeboConfig() const
{
  return this->gazeboConfig;
}

OgreAdaptor *Simulator::GetRenderEngine() const
{
  if (this->renderEngineEnabled)
    return this->renderEngine;
  else
    return NULL;
}

////////////////////////////////////////////////////////////////////////////////
// Return when this simulator is paused
bool Simulator::IsPaused() const
{
  return this->pause;
}

////////////////////////////////////////////////////////////////////////////////
/// Set whether the simulation is paused
void Simulator::SetPaused(bool p)
{
  boost::recursive_mutex::scoped_lock model_render_lock(*this->GetMRMutex());

  if (this->pause == p)
    return;

  this->pauseSignal(p);
  this->pause = p;
}

////////////////////////////////////////////////////////////////////////////////
// Get the simulation time
gazebo::Time Simulator::GetSimTime() const
{
  return this->simTime;
}

////////////////////////////////////////////////////////////////////////////////
/// Set the sim time
void Simulator::SetSimTime(Time t)
{
  this->simTime = t;
}

////////////////////////////////////////////////////////////////////////////////
// Get the pause time
gazebo::Time Simulator::GetPauseTime() const
{
  return this->pauseTime;
}

////////////////////////////////////////////////////////////////////////////////
/// Get the start time
gazebo::Time Simulator::GetStartTime() const
{
  return this->startTime;
}

////////////////////////////////////////////////////////////////////////////////
/// Get the real time (elapsed time)
gazebo::Time Simulator::GetRealTime() const
{
  return this->GetWallTime() - this->startTime;
}

////////////////////////////////////////////////////////////////////////////////
/// Get the wall clock time
gazebo::Time Simulator::GetWallTime() const
{
  Time t;
  t.SetToWallTime();
  return t;
}

////////////////////////////////////////////////////////////////////////////////
// Set the user quit flag
void Simulator::SetUserQuit()
{
  //  this->Save("test.xml");
  this->userQuit = true;
}

////////////////////////////////////////////////////////////////////////////////
bool Simulator::GetStepInc() const
{
  return this->stepInc;
}

////////////////////////////////////////////////////////////////////////////////
void Simulator::SetStepInc(bool step)
{
  boost::recursive_mutex::scoped_lock model_render_lock(*this->GetMRMutex());
  this->stepInc = step;
  this->stepSignal(step);

  this->SetPaused(!step);
}

////////////////////////////////////////////////////////////////////////////////
// True if the gui is to be used
void Simulator::SetGuiEnabled( bool enabled )
{
  this->guiEnabled = enabled;
}

////////////////////////////////////////////////////////////////////////////////
/// Return true if the gui is enabled
bool Simulator::GetGuiEnabled() const
{
  return this->guiEnabled;
}

////////////////////////////////////////////////////////////////////////////////
// True if the gui is to be used
void Simulator::SetRenderEngineEnabled( bool enabled )
{
  this->renderEngineEnabled = enabled;
}

////////////////////////////////////////////////////////////////////////////////
/// Set the length of time the simulation should run.
void Simulator::SetTimeout(double time)
{
  this->timeout = time;
}

////////////////////////////////////////////////////////////////////////////////
// Set the physics enabled/disabled
void Simulator::SetPhysicsEnabled( bool enabled )
{
  this->physicsEnabled = enabled;
}

////////////////////////////////////////////////////////////////////////////////
/// Get the model that contains the entity
Model *Simulator::GetParentModel( Entity *entity ) const
{
  Model *model = NULL;

  if (entity == NULL)
    return NULL;

  do 
  {
    if (entity && entity->HasType(MODEL))
      model = (Model*)entity;

    entity = dynamic_cast<Entity*>(entity->GetParent());
  } while (model == NULL);

  return model;
}

////////////////////////////////////////////////////////////////////////////////
/// Get the body that contains the entity
Body *Simulator::GetParentBody( Entity *entity ) const
{
  Body *body = NULL;

  if (entity == NULL)
    return NULL;

  do 
  {
    if (entity && entity->HasType(BODY))
      body = (Body*)(entity);
    entity = dynamic_cast<Entity*>(entity->GetParent());
  } while (body == NULL);

  return body;
}

////////////////////////////////////////////////////////////////////////////////
/// Function to run physics. Used by physicsThread
void Simulator::PhysicsLoop()
{
  World *world = World::Instance();

  world->GetPhysicsEngine()->InitForThread();

  Time step = world->GetPhysicsEngine()->GetStepTime();
  double physicsUpdateRate = world->GetPhysicsEngine()->GetUpdateRate();
  Time physicsUpdatePeriod = 1.0 / physicsUpdateRate;

  bool userStepped;
  Time diffTime;
  Time currTime;
  Time lastTime = this->GetRealTime();
  struct timespec req, rem;

  while (!this->physicsQuit)
  {
    {
      boost::recursive_mutex::scoped_lock model_render_lock(*this->GetMRMutex());
      boost::recursive_mutex::scoped_lock model_delete_lock(*this->GetMDMutex());

      currTime = this->GetRealTime();

      userStepped = false;
      if (this->IsPaused())
        this->pauseTime += step;
      else
        this->simTime += step;

      if (this->GetStepInc())
        userStepped = true;

      lastTime = this->GetRealTime();

      world->Update();
    }

    currTime = this->GetRealTime();

    // Set a default sleep time
    req.tv_sec  = 0;
    req.tv_nsec = 10000;

    // If the physicsUpdateRate < 0, then we should try to match the
    // update rate to real time
    if ( physicsUpdateRate < 0 &&
        (this->GetSimTime() + this->GetPauseTime()) > 
        this->GetRealTime()) 
    {
      diffTime = (this->GetSimTime() + this->GetPauseTime()) - 
                 this->GetRealTime();
      req.tv_sec  = diffTime.sec;
      req.tv_nsec = diffTime.nsec;
    }
    // Otherwise try to match the update rate to the one specified in
    // the xml file
    else if (physicsUpdateRate > 0 && 
        currTime - lastTime < physicsUpdatePeriod)
    {
      diffTime = physicsUpdatePeriod - (currTime - lastTime);

      req.tv_sec  = diffTime.sec;
      req.tv_nsec = diffTime.nsec;
    }

    //nanosleep(&req, &rem);

    {
      //DiagnosticTimer timer("PhysicsLoop UpdateSimIfaces ");

      // Process all incoming messages from simiface
      world->UpdateSimulationIface();
    }

    if (this->timeout > 0 && this->GetRealTime() > this->timeout)
    {
      this->userQuit = true;
      break;
    }

    if (userStepped)
    {
      this->SetStepInc(false);
      this->SetPaused(true);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// Get the simulator mutex
boost::recursive_mutex *Simulator::GetMRMutex()
{
  return this->render_mutex;
}

////////////////////////////////////////////////////////////////////////////////
/// Get the simulator mutex
boost::recursive_mutex *Simulator::GetMDMutex()
{
  return this->model_delete_mutex;
}

////////////////////////////////////////////////////////////////////////////////
/// Get the state of the simulation
Simulator::State Simulator::GetState() const
{
  return this->state;
}

////////////////////////////////////////////////////////////////////////////////
/// Get the number of plugins
unsigned int Simulator::GetPluginCount() const
{
  return this->plugins.size();
}

////////////////////////////////////////////////////////////////////////////////
// Get the name of a plugin
std::string Simulator::GetPluginName(unsigned int i) const
{
  std::string result;
  if (i < this->plugins.size())
    result = this->plugins[i]->GetHandle();

  return result;
}

////////////////////////////////////////////////////////////////////////////////
// Add a plugin
void Simulator::AddPlugin(const std::string &filename, const std::string &handle)
{
  Plugin *plugin = Plugin::Create(filename, handle);
  if (plugin)
  {
    plugin->Load();
    this->plugins.push_back(plugin);
  }
}

////////////////////////////////////////////////////////////////////////////////
// Add a plugin
void Simulator::RemovePlugin(const std::string &name)
{
  std::vector<Plugin*>::iterator iter;
  for (iter = this->plugins.begin(); iter != this->plugins.end(); iter++)
  {
    if ((*iter)->GetHandle() == name)
    {
      delete (*iter);
      this->plugins.erase(iter);
      break;
    }
  }
}
