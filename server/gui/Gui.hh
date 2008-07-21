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
/* Desc: FLTK Mainwindow
 * Author: Nate Koenig
 * Date: 13 Feb 2006
 * SVN: $Id$
 */

#ifndef FLTKMAINWINDOW_HH
#define FLTKMAINWINDOW_HH

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <string>
#include <iostream>

namespace gazebo
{

  class GLWindow;
  class Toolbar;
  class StatusBar;
  class GLFrameManager;
  class XMLConfigNode;

  /// \brief FLTK Main Window
  class Gui : public Fl_Window
  {
    /// \brief Constructor
    public: Gui (int x, int y, int w, int h, const std::string &t);
 
    /// \brief Destructor
    public: virtual ~Gui();

    /// \brief Load the gui
    public: virtual void Load(XMLConfigNode *node);

    /// \brief Initalize the gui
    public: virtual void Init();

    public: virtual void Update();

    /// \brief Get the width of the gui's rendering window
    public: unsigned int GetWidth() const;

    /// \brief Get the height of the gui's rendering window
    public: unsigned int GetHeight() const;

    /// \brief Handle an event
    public: int handle(int event);

    /// \brief Get the average FPS
    public: float GetAvgFPS() const;

    private: Toolbar *toolbar;
    private: StatusBar *statusbar;
    private: GLFrameManager *frameMgr;

    private: bool hasFocus;
  };
}

#endif

