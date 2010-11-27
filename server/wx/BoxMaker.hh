#ifndef BOXMAKER_HH
#define BOXMAKER_HH

#include "Vector2.hh"
#include "EntityMaker.hh"

namespace gazebo
{
  class Visual;

  class BoxMaker : public EntityMaker
  {
    public: BoxMaker();
    public: virtual ~BoxMaker();
  
    public: virtual void Start(Scene *scene);
    public: virtual void Stop();
    public: virtual bool IsActive() const;

    public: virtual void MousePushCB(const MouseEvent &event);
    public: virtual void MouseReleaseCB(const MouseEvent &event);
    public: virtual void MouseDragCB(const MouseEvent &event);
  
    private: virtual void CreateTheEntity();
    private: int state;
    private: bool leftMousePressed;
    private: Vector2<int> mousePushPos;
    private: Visual *visual;

    private: static unsigned int counter;
  };
}

#endif
