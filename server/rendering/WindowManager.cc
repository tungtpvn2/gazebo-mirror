#include <Ogre.h>
#include <math.h>

#include "OgreAdaptor.hh"
#include "RTShaderSystem.hh"
#include "Color.hh"
#include "GazeboMessage.hh"
#include "GazeboError.hh"
#include "Camera.hh"
#include "RenderControl.hh"
#include "WindowManager.hh"

using namespace gazebo;

unsigned int WindowManager::windowCounter = 0;

////////////////////////////////////////////////////////////////////////////////
// Constructor
WindowManager::WindowManager()
{
}

////////////////////////////////////////////////////////////////////////////////
WindowManager::~WindowManager()
{
}

////////////////////////////////////////////////////////////////////////////////
int WindowManager::CreateWindow( RenderControl *control )
{
  int result = -1;

  if (control)
    result = this->CreateWindow( control->GetOgreHandle(), 
                                 control->GetWidth(), control->GetHeight());
  else
    gzerr(0) << "Invalid RenderControl\n";

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// Attach a camera to a window
void WindowManager::SetCamera( int windowId, Camera *camera)
{
  Ogre::Viewport *viewport = NULL;
 
  viewport = this->windows[windowId]->addViewport(camera->GetCamera());

  double ratio = (double)viewport->getActualWidth() / (double)viewport->getActualHeight();
  double vfov = fabs(2.0 * atan(tan(camera->GetHFOV().GetAsRadian() / 2.0) / ratio));

  camera->SetAspectRatio( ratio );
  camera->GetCamera()->setFOVy(Ogre::Radian(vfov));

  viewport->setClearEveryFrame(true);
  viewport->setBackgroundColour( Color(0,0,0).GetOgreColor() );
  viewport->setVisibilityMask(camera->GetVisibilityMask());

  RTShaderSystem::AttachViewport(viewport, camera->GetScene());
}

////////////////////////////////////////////////////////////////////////////////
// Create a window
int WindowManager::CreateWindow( const std::string ogreHandle, 
                                                 unsigned int width, 
                                                 unsigned int height) 
{
  Ogre::StringVector paramsVector;
  Ogre::NameValuePairList params;
  Ogre::RenderWindow *window = NULL;

  params["parentWindowHandle"] = ogreHandle;

  std::ostringstream stream;
  stream << "OgreWindow(" << windowCounter++ << ")";

  int attempts = 0;
  while (window == NULL && (attempts++) < 10)
  {
    try
    {
      window = OgreAdaptor::Instance()->root->createRenderWindow( stream.str(), 
                     width, height, false, &params);
    }
    catch (...)
    {
      gzerr(0) << " Unable to create the rendering window\n";
      window = NULL;
    }
  }

  if (attempts >= 10)
  {
    gzthrow("Unable to create the rendering window\n");
  }

  window->setActive(true);
  //window->setVisible(true);
  window->setAutoUpdated(false);

  this->windows.push_back(window);

  return this->windows.size()-1;
}
