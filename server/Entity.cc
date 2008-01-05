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

/* Desc: External interfaces for Gazebo
 * Author: Nate Koenig
 * Date: 03 Apr 2007
 * SVN: $Id$
 */
#include "GazeboError.hh"
#include "Global.hh"
#include "OgreVisual.hh"
#include "World.hh"
#include "PhysicsEngine.hh"
#include "Entity.hh"

using namespace gazebo;
unsigned int Entity::idCounter = 0;


Entity::Entity(Entity *parent)
{
  // Set the parent and the id
  this->parent = parent;
  this->id = idCounter++;
  this->isStatic = false;
  this->visualNode=0;
  
  if (this->parent)
  {
    this->parent->AddChild(this);
    this->visualNode=new OgreVisual(this->parent->GetVisualNode());
    this->SetStatic(parent->IsStatic());
  }
  else
  {
    this->visualNode = new OgreVisual(NULL);
  }

  // Add this to the phyic's engine
  World::Instance()->GetPhysicsEngine()->AddEntity(this);
}

Entity::~Entity()
{
  GZ_DELETE (this->visualNode);
  World::Instance()->GetPhysicsEngine()->RemoveEntity(this);

}

int Entity::GetId() const
{
  return this->id;
}

// Return the ID of the parent
int Entity::GetParentId() const
{
  return this->parent == NULL ? 0 : this->parent->GetId();
}


// Set the parent
void Entity::SetParent(Entity *parent)
{
  this->parent = parent;
}

// Get the parent
Entity *Entity::GetParent() const
{
  return this->parent;
}

// Add a child to this entity
void Entity::AddChild(Entity *child)
{
  if (child == NULL)
    gzthrow("Cannot add a null child to an entity");

  // Add this child to our list
  this->children.push_back(child);
}

// Get all children
std::vector< Entity* > &Entity::GetChildren()
{
  return this->children;
}

// Return this entitie's sceneNode
OgreVisual *Entity::GetVisualNode() const
{
  return this->visualNode;
}

// Set the scene node
void Entity::SetVisualNode(OgreVisual *visualNode)
{
  this->visualNode = visualNode;
}

////////////////////////////////////////////////////////////////////////////////
// Set the name of the body
void Entity::SetName(const std::string &name)
{
  this->name = name;
}

////////////////////////////////////////////////////////////////////////////////
// Return the name of the body
std::string Entity::GetName() const
{
  return this->name;
}


////////////////////////////////////////////////////////////////////////////////
// Set whether this entity is static: immovable
void Entity::SetStatic(bool s)
{
  this->isStatic = s;
}

////////////////////////////////////////////////////////////////////////////////
// Return whether this entity is static
bool Entity::IsStatic() const
{
  return this->isStatic;
}

////////////////////////////////////////////////////////////////////////////////
/// Returns true if the entities are the same. Checks only the name
bool Entity::operator==(const Entity &ent)
{
  return ent.GetName() == this->GetName();
}
