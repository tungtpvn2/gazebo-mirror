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
/* Desc: Dynamic line generator
 * Author: Nate Koenig
 * Date: 28 June 2007
 * CVS: $Id: OgreAdaptor.hh,v 1.1.2.1 2006/12/16 22:41:17 natepak Exp $
 */

#ifndef OGREDYNAMICLINES_HH
#define OGREDYNAMICLINES_HH

#include "Vector3.hh"
#include "OgreDynamicRenderable.hh"

#include <vector>

namespace gazebo
{

class OgreDynamicLines : public OgreDynamicRenderable
{
  /// Constructor
  public: OgreDynamicLines(Ogre::RenderOperation::OperationType opType=Ogre::RenderOperation::OT_LINE_STRIP);

  /// Destructor
  public: virtual ~OgreDynamicLines();

  /// Add a point to the point list
  /// \param pt Vector3 point
  public: void AddPoint(const Vector3 &pt);

  /// Change the location of an existing point in the point list
  /// \param index Index of the point to set
  /// \param value Vector3 value to set the point to
  public: void SetPoint(unsigned int index, const Vector3 &value);

  /// Return the location of an existing point in the point list
  /// \param index Number of the point to return
  /// \return Vector3 value of the point
  public: const Vector3& GetPoint(unsigned int index) const;

  /// Return the total number of points in the point list
  /// \return Number of points
  public: unsigned int GetNumPoints() const;

  /// Remove all points from the point list
  public: void Clear();

  /// Call this to update the hardware buffer after making changes.  
  public: void Update();

  /// Set the type of operation to draw with.
  /// @param opType Can be one of 
  ///    - RenderOperation::OT_LINE_STRIP
  ///    - RenderOperation::OT_LINE_LIST
  ///    - RenderOperation::OT_POINT_LIST
  ///    - RenderOperation::OT_TRIANGLE_LIST
  ///    - RenderOperation::OT_TRIANGLE_STRIP
  ///   - RenderOperation::OT_TRIANGLE_FAN
  ///   The default is OT_LINE_STRIP.
  public: void SetOperationType(Ogre::RenderOperation::OperationType opType);

  /// Get the operation type
  /// \return The operation type
  public: Ogre::RenderOperation::OperationType GetOperationType() const;

  /// \brief Implementation DynamicRenderable, creates a simple vertex-only decl
  protected: virtual void  CreateVertexDeclaration();

  /// \brief Implementation DynamicRenderable, pushes point list out to hardware memory
  protected: virtual void FillHardwareBuffers();

  private: std::vector<Vector3> points;
  private: bool dirty;
};

}
#endif
