#ifndef BOX_HH
#define BOX_HH

#include "Vector3.hh"

namespace gazebo
{
  class Box
  {
    /// \brief Constructor
    public: Box (const Vector3 min, const Vector3 max);

    /// \brief Destructor
    public: virtual ~Box();

    /// \brief Get the length along the x dimension
    public: double GetXLength();

    /// \brief Get the length along the y dimension
    public: double GetYLength();

    /// \brief Get the length along the z dimension
    public: double GetZLength();

    public: Vector3 min, max;
  };
}

#endif
