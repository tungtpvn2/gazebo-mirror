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

#ifndef SIMULATOR_HH
#define SIMULATOR_HH

#include <string>

#include "SingletonT.hh"

namespace gazebo
{
/// \addtogroup gazebo_server
/// \brief The World
/// \{

// Forward declarations
  class Gui;
  class Server;
  class SimulationIface;
  class XMLConfig;
  class XMLConfigNode;

/// \brief The World
/*
 * Top level class: Takes care of World Gui SimulatorIface and Server
 * 
 */
  class Simulator : public SingletonT<Simulator>
  {
    /// \brief Private constructor
    private: Simulator();

    /// \brief Private destructor
    private: ~Simulator();

    /// \brief Load the world configuration file 
    public: void Load(const std::string &worldFileName, int serverId );

    /// \brief Save the world configuration file
    public: void Save(const std::string& filename);

    /// \brief Initialize the simulation
    public: int Init( );

    /// \brief Finalize the simulation
    public: int Fini( );

    /// \brief Update the simulation
    public: void Update();

    /// \brief Main simulation loop, when this loop ends the simulation finish
    public: void MainLoop();

    /// \brief Gets our current GUI interface
    public: Gui *GetUI();

    /// \brief Returns the state of the simulation true if paused
    public: bool IsPaused() const;

    /// \brief Get the number of iterations
    public: unsigned long GetIterations();
/*
    /// \brief Set the number of iterations
    public: static void SetIterations(unsigned long count);

    /// \brief Increment the number of iterations
    public: static void IncIterations();
*/
    /// Get the simulation time
    /// \return The simulation time
    public: double GetSimTime() const;

    /// Get the pause time
    /// \return The pause time
    public: double GetPauseTime() const;

    /// Get the start time
    /// \return The start time
    public: double GetStartTime() const;

    /// Get the real time (elapsed time)
    /// \return The real time
    public: double GetRealTime() const;

    /// \brief Get the wall clock time
    /// \return The wall clock time
    public: double GetWallTime() const;

    public: void LoadGui(XMLConfigNode *rootNode);
    public: void SaveGui(XMLConfigNode *node);


    //User Iteractions
    /// \brief Simulator finished by the user
    public: void SetUserQuit();

    /// \brief Return true if the user has pased
    public: bool GetUserPause();

    /// \brief Set whether the user has paused
    public: void SetUserPause(bool pause);

    /// \brief Return true if the user has stepped the simulation
    public: bool GetUserStep();

    /// \brief Set whether the user has stepped the simulation
    public: void SetUserStep( bool step );

    /// \brief Return true if the step has incremented
    public: bool GetUserStepInc();

    /// \brief Set whether the step has incremented
    public: void SetUserStepInc(bool step);


  
    ///pointer to the XML Data
    private:gazebo::XMLConfig *xmlFile;

    /// Pointer to the selected Gui 
    private: Gui *gui;


    /// Flag set if simulation is paused
    private: bool pause;

    ///  Count of the number of iterations
    private: unsigned long iterations;

    /// Current simulation time
    private: double simTime, pauseTime, startTime;

    // UserIteractions 
    /// \brief Set to true to pause the simulation
    private: bool userPause;

    /// Set to true to step through the simulation
    private: bool userStep;

    /// Set to true to increment the simulation once. This is only
    ///  valid when userStep is true.
    private: bool userStepInc;

    private: bool userQuit;


    private: friend class DestroyerT<Simulator>;
    private: friend class SingletonT<Simulator>;

};

/// \}
}

#endif
