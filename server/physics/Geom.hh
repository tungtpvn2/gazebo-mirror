#ifndef GEOM_HH
#define GEOM_HH

#include <ode/ode.h>

#include "Entity.hh"
#include "Pose3d.hh"
#include "Vector3.hh"

namespace Ogre
{
  class Entity;
  class MaterialPtr;
  class MovableObject;
}

namespace gazebo
{

  class Body;
  class ContactParams;


class Geom : public Entity
{

  /// \brief Constructor
  public: Geom(Body *body);

   /// \brief Destructor
  public: virtual ~Geom();

  // \brief Set the encapsulated geometry object
  public: void SetGeom(dGeomID geomId, bool placeable);

  /// \brief Return the geom id
  public: dGeomID GetGeomId() const;

  /// \brief Return whether this geom is placeable
  public: bool IsPlaceable() const;

  /// \brief Set the pose
  /// \param pose New pose
  public: void SetPose(const Pose3d &pose);

  /// \brief Return the pose of the geom
  public: Pose3d GetPose() const;

  /// \brief Set the position
  /// \param pos Vector3 position
  public: void SetPosition(const Vector3 &pos);

  /// \brief Set the rotation
  /// \param rot Quaternion rotation
  public: void SetRotation(const Quatern &rot);

  /// \brief Attach a mesh to the geom
  /// \param meshName Name of the mesh
  public: void AttachMesh(const std::string &meshName);

  /// \brief Attach a moveable object to the node
  /// \param obj The moveable object
  public: void AttachObject( Ogre::MovableObject *obj );

  /// \brief Set the scale of the mesh
  /// \param scale New scale of the mesh
  public: void ScaleMesh(const Vector3 &scale);

  /// \brief Set the mesh pose
  /// \param pose Pose of the mesh.
  public: void SetMeshPose(const Pose3d &pose);

  /// \brief Set the mesh position
  /// \param pos Position of the mesh
  public: void SetMeshPosition(const Vector3 &pos);

  /// \brief Set whether the mesh casts shadows
  /// \param enable True=cast shadows
  public: void SetCastShadows(bool enable);

  /// \brief Set the material to apply to the mesh
  public: void SetMeshMaterial(const std::string &materialName);

  /// \brief Set the category bits, used during collision detection
  /// \param bits The bits
  public: void SetCategoryBits(unsigned int bits);

  /// \brief Set the collide bits, used during collision detection
  /// \param bits The bits
  public: void SetCollideBits(unsigned int bits);

  /// \brief Contact parameters
  public: ContactParams *contact; 

  /// \brief The body this geom belongs to
  protected: Body *body;

  private: bool placeable;

  private: Ogre::MovableObject *ogreObj;

  protected: dGeomID geomId;

  private: static int geomIdCounter;

  // Mass of this geometry
  protected: dMass mass;
};

}
#endif
