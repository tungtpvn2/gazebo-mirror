/*
 * Copyright (C) 2017 Open Source Robotics Foundation
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
#ifndef _GAZEBO_RENDERING_GPULASERDATAITERATOR_HH_
#define _GAZEBO_RENDERING_GPULASERDATAITERATOR_HH_

#include <memory>

namespace gazebo
{
  namespace rendering
  {
    // \brief struct containing info about a single ray measurement
    struct GpuLaserData
    {
      // The distance of the reading in meters
      double range;
      // The intensity reading
      double intensity;
      // Which plane or cone this reading belongs to [0, verticalResolution)
      unsigned int beam;
      // Which reading in a plane or cone is this [0, horizontalResolution)
      unsigned int reading;
    };

    // \brief const Bidirectional iterator for laser data
    //
    // This class contains the information needed to access Laser Data
    // It implements a Bidirectional Input iterator
    // http://www.cplusplus.com/reference/iterator/BidirectionalIterator/
    // The compiler should optimize out calls to this class
    template <typename F>
    class GpuLaserDataIterator
    {
      public:

        friend F;

        ~GpuLaserDataIterator();

        // \brief return true if the iterators point to the same element
        bool operator==(const GpuLaserDataIterator &rvalue) const;

        // \brief return true if the iterators point to different elements
        bool operator!=(const GpuLaserDataIterator &rvalue) const;

        // \brief returns laser data at the index pointed to by the iterator
        const GpuLaserData operator*() const;

        // \brief returns a shared pointer object at the iterator's index
        const std::shared_ptr<const GpuLaserData> operator->() const;

        // \brief Advance iterator to next reading (++it)
        GpuLaserDataIterator<F>& operator++();

        // \brief Advance this iterator, but return a copy of this one (it++)
        GpuLaserDataIterator<F> operator++(int _dummy);

        // \brief Move itereator to previous (--it)
        GpuLaserDataIterator<F>& operator--();

        // \breif Go to previous, but return a copy of this one (it--)
        GpuLaserDataIterator<F> operator--(int _dummy);

      protected:
        // \brief contstruct an iterator to a specified index
        GpuLaserDataIterator(const unsigned int _index, const float *_data,
            const unsigned int _skip, unsigned int _rangeOffset,
            const unsigned int _intensityOffset,
            const unsigned int _horizontalResolution);

      private:
        // Not using PIMPL because it has no benefit on templated classes

        // \brief which reading is this [0, vRes * hRes)
        unsigned int index = 0;
        // \brief the data being decoded
        const float *data = nullptr;
        // \brief offset between consecutive readings
        const unsigned int skip = 0;
        // \brief offset within a reading to range data
        const unsigned int rangeOffset = 0;
        // \brief offset within a reading to intensity data
        const unsigned int intensityOffset = 0;
        // \brief Number of readings in each plane or cone
        const unsigned int horizontalResolution = 0;
    };
  }
}

#include "GpuLaserDataIterator.impl.hh"

#endif  // _GAZEBO_RENDERING_GPULASERDATAITERATOR_HH_

