//
// Created by klokik on 02.07.15.
//

#ifndef _GAZEBO_RENDERING_WIDEANGLECAMERA_HH_
#define _GAZEBO_RENDERING_WIDEANGLECAMERA_HH_

#include "Camera.hh"


namespace gazebo
{
  namespace rendering
  {
    class GAZEBO_VISIBLE CameraProjection
    {
      private: enum ProjFunction {SIN=0,TAN,ID};

      public: void Init(float c1,float c2,std::string fun,float f=1.0f,float c3=0.0f);
      public: void Init(std::string name);

      public: void Load(sdf::ElementPtr sdf);
      public: void Load();

      public: float GetC1() const;
      public: float GetC2() const;
      public: float GetC3() const;
      public: float GetF() const;
      public: std::string GetFun() const;
      public: float GetCutOffAngle() const;

      public: void SetC1(float c);
      public: void SetC2(float c);
      public: void SetC3(float c);
      public: void SetF(float f);
      public: void SetFun(std::string fun);
      public: void SetCutOffAngle(float _angle);
      public: void SetFullFrame(bool _full_frame);

      private: void ConvertToCustom();

      public: std::string GetType() const;

      public: void SetType(std::string type);

      public: bool IsCustom() const;

      public: bool IsFullFrame() const;

      public: void SetCompositorMaterial(Ogre::MaterialPtr material);

      public: void SetMaterialVariables(float _ratio);

      // r = c1*f*fun(theta/c2+c3)

      private: float c1;
      private: float c2;
      private: float c3;
      private: float f;
      private: float cutOffAngle;

      private: ProjFunction fun;

      private: sdf::ElementPtr sdf;

      private: Ogre::MaterialPtr compositorMaterial;
    };

    class GAZEBO_VISIBLE WideAngleCamera : public Camera
    {
      public: WideAngleCamera(const std::string &_namePrefix, ScenePtr _scene,
                              bool _autoRender = true, int textureSize = 256);

      public: ~WideAngleCamera();

      public: virtual void SetRenderTarget(Ogre::RenderTarget *_target) override;

      public: void CreateEnvRenderTexture(const std::string &_textureName);

      public: int GetEnvTextureSize();

      public: void SetEnvTextureSize(int size);

      private: void CreateEnvCameras();

      public: virtual void SetClipDist();

      protected: virtual void RenderImpl();

      public: virtual void Init();

      public: virtual void Load();

      public: virtual void Fini();

      public: const CameraProjection *GetProjection();

      protected: Ogre::CompositorInstance *wamapInstance;

      private: Ogre::Camera *envCameras[6];

      private: Ogre::RenderTarget *envRenderTargets[6];

      private: Ogre::Viewport *envViewports[6];

      private: Ogre::Texture *envCubeMapTexture;

      private: int envTextureSize;

      private: Ogre::MaterialPtr compMat;

      private: CameraProjection projection;
    };
  }
}


#endif
