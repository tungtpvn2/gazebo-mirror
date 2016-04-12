/*
 * Copyright (C) 2014-2016 Open Source Robotics Foundation
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

#ifdef _WIN32
  // Ensure that Winsock2.h is included before Windows.h, which can get
  // pulled in by anybody (e.g., Boost).
  #include <Winsock2.h>
#endif

#include <boost/filesystem.hpp>
#include <sstream>
#include <functional>
#include <string>

#include <sdf/sdf.hh>

#include "gazebo/common/Exception.hh"
#include "gazebo/common/KeyEvent.hh"
#include "gazebo/common/MouseEvent.hh"
#include "gazebo/common/SVGLoader.hh"

#include "gazebo/rendering/UserCamera.hh"
#include "gazebo/rendering/Material.hh"
#include "gazebo/rendering/Scene.hh"

#include "gazebo/transport/Publisher.hh"
#include "gazebo/transport/Node.hh"
#include "gazebo/transport/TransportIface.hh"

#include "gazebo/gui/Actions.hh"
#include "gazebo/gui/KeyEventHandler.hh"
#include "gazebo/gui/MouseEventHandler.hh"
#include "gazebo/gui/GuiEvents.hh"
#include "gazebo/gui/GuiIface.hh"
#include "gazebo/gui/ModelManipulator.hh"
#include "gazebo/gui/ModelSnap.hh"
#include "gazebo/gui/ModelAlign.hh"
#include "gazebo/gui/SaveDialog.hh"
#include "gazebo/gui/MainWindow.hh"

#include "gazebo/gui/model/ModelData.hh"
#include "gazebo/gui/model/LinkInspector.hh"
#include "gazebo/gui/model/ModelPluginInspector.hh"
#include "gazebo/gui/model/JointMaker.hh"
#include "gazebo/gui/model/ModelEditorEvents.hh"
#include "gazebo/gui/model/MEUserCmdManager.hh"
#include "gazebo/gui/model/ModelCreator.hh"

namespace gazebo
{
  namespace gui
  {
    /// \internal
    /// \class ModelCreator ModelCreator.hh
    /// \brief Private data for the ModelCreator class.
    class ModelCreatorPrivate
    {
      /// \brief Name of the model preview.
      public: static const std::string previewName;

      /// \brief Default name of the model.
      public: static const std::string modelDefaultName;

      /// \brief The model in SDF format.
      public: sdf::SDFPtr modelSDF;

      /// \brief A template SDF of a simple box model.
      public: sdf::SDFPtr modelTemplateSDF;

      /// \brief Name of the model.
      public: std::string modelName;

      /// \brief Folder name, which is the model name without spaces.
      public: std::string folderName;

      /// \brief The root visual of the model.
      public: rendering::VisualPtr previewVisual;

      /// \brief Visual currently being inserted into the model, which is
      /// attached to the mouse.
      public: rendering::VisualPtr mouseVisual;

      /// \brief The pose of the model.
      public: ignition::math::Pose3d modelPose;

      /// \brief True to create a static model.
      public: bool isStatic;

      /// \brief True to auto disable model when it is at rest.
      public: bool autoDisable;

      /// \brief A list of gui editor events connected to the model creator.
      public: std::vector<event::ConnectionPtr> connections;

      /// \brief Counter for the number of links in the model.
      public: int linkCounter;

      /// \brief Counter for generating a unique model name.
      public: int modelCounter;

      /// \brief Type of entity being added.
      public: ModelCreator::EntityType addEntityType;

      /// \brief A map of nested model names to and their visuals.
      public: std::map<std::string, NestedModelData *> allNestedModels;

      /// \brief A map of model link names to and their data.
      public: std::map<std::string, LinkData *> allLinks;

      /// \brief A map of model plugin names to and their data.
      public: std::map<std::string, ModelPluginData *> allModelPlugins;

      /// \brief Transport node
      public: transport::NodePtr node;

      /// \brief Publisher that publishes msg to the server once the model is
      /// created.
      public: transport::PublisherPtr makerPub;

      /// \brief Publisher that publishes delete entity msg to remove the
      /// editor visual.
      public: transport::PublisherPtr requestPub;

      /// \brief Joint maker.
      public: JointMaker *jointMaker;

      /// \brief origin of the model.
      public: ignition::math::Pose3d origin;

      /// \brief A list of selected link visuals.
      public: std::vector<rendering::VisualPtr> selectedLinks;

      /// \brief A list of selected nested model visuals.
      public: std::vector<rendering::VisualPtr> selectedNestedModels;

      /// \brief A list of selected model plugins.
      public: std::vector<std::string> selectedModelPlugins;

      /// \brief Names of entities copied through g_copyAct
      public: std::vector<std::string> copiedNames;

      /// \brief The last mouse event
      public: common::MouseEvent lastMouseEvent;

      /// \brief Qt action for opening the link inspector.
      public: QAction *inspectAct;

      /// \brief Name of link that is currently being inspected.
      public: std::string inspectName;

      /// \brief True if the model editor mode is active.
      public: bool active;

      /// \brief Current model manipulation mode.
      public: std::string manipMode;

      /// \brief A dialog with options to save the model.
      public: SaveDialog *saveDialog;

      /// \brief Store the current save state of the model.
      public: ModelCreator::SaveState currentSaveState;

      /// \brief Mutex to protect updates
      public: std::recursive_mutex updateMutex;

      /// \brief A list of link names whose scale has changed externally.
      public: std::map<LinkData *, ignition::math::Vector3d> linkScaleUpdate;

      /// \brief A list of link data whose pose has changed externally.
      public: std::map<LinkData *, ignition::math::Pose3d> linkPoseUpdate;

      /// \brief A list of nested model data whose pose has changed externally.
      public: std::map<NestedModelData *, ignition::math::Pose3d>
          nestedModelPoseUpdate;

      /// \brief Name of model on the server that is being edited here in the
      /// model editor.
      public: std::string serverModelName;

      /// \brief SDF element of the model on the server.
      public: sdf::ElementPtr serverModelSDF;

      /// \brief A map of all visuals of the model to be edited to their
      /// visibility.
      public: std::map<uint32_t, bool> serverModelVisible;

      /// \brief Name of the canonical model
      public: std::string canonicalModel;

      /// \brief Name of the canonical link in the model
      public: std::string canonicalLink;

      /// \brief SDF element to append in the end.
      public: sdf::ElementPtr sdfToAppend;
    };
  }
}

using namespace gazebo;
using namespace gui;

const std::string ModelCreatorPrivate::modelDefaultName = "Untitled";
const std::string ModelCreatorPrivate::previewName = "ModelPreview";

/////////////////////////////////////////////////
ModelCreator::ModelCreator(QObject *_parent)
  : QObject(_parent),
    dataPtr(new ModelCreatorPrivate)
{
  this->dataPtr->active = false;

  this->dataPtr->modelTemplateSDF.reset(new sdf::SDF);
  this->dataPtr->modelTemplateSDF->SetFromString(
      ModelData::GetTemplateSDFString());

  this->dataPtr->manipMode = "";
  this->dataPtr->linkCounter = 0;
  this->dataPtr->modelCounter = 0;

  this->dataPtr->node = transport::NodePtr(new transport::Node());
  this->dataPtr->node->Init();
  this->dataPtr->makerPub =
      this->dataPtr->node->Advertise<msgs::Factory>("~/factory");
  this->dataPtr->requestPub =
      this->dataPtr->node->Advertise<msgs::Request>("~/request");

  this->dataPtr->jointMaker = new gui::JointMaker();

  this->dataPtr->sdfToAppend.reset(new sdf::Element);
  this->dataPtr->sdfToAppend->SetName("sdf_to_append");

  connect(g_editModelAct, SIGNAL(toggled(bool)), this, SLOT(OnEdit(bool)));

  this->dataPtr->inspectAct = new QAction(tr("Open Link Inspector"), this);
  connect(this->dataPtr->inspectAct, SIGNAL(triggered()), this,
      SLOT(OnOpenInspector()));

  if (g_deleteAct)
  {
    connect(g_deleteAct, SIGNAL(DeleteSignal(const std::string &)), this,
        SLOT(OnDelete(const std::string &)));
  }

  this->dataPtr->connections.push_back(
      gui::Events::ConnectEditModel(
      std::bind(&ModelCreator::OnEditModel, this, std::placeholders::_1)));

  this->dataPtr->connections.push_back(
      gui::model::Events::ConnectSaveModelEditor(
      std::bind(&ModelCreator::OnSave, this)));

  this->dataPtr->connections.push_back(
      gui::model::Events::ConnectSaveAsModelEditor(
      std::bind(&ModelCreator::OnSaveAs, this)));

  this->dataPtr->connections.push_back(
      gui::model::Events::ConnectNewModelEditor(
      std::bind(&ModelCreator::OnNew, this)));

  this->dataPtr->connections.push_back(
      gui::model::Events::ConnectExitModelEditor(
      std::bind(&ModelCreator::OnExit, this)));

  this->dataPtr->connections.push_back(
      gui::model::Events::ConnectModelNameChanged(
      std::bind(&ModelCreator::OnNameChanged, this, std::placeholders::_1)));

  this->dataPtr->connections.push_back(
      gui::model::Events::ConnectModelChanged(
      std::bind(&ModelCreator::ModelChanged, this)));

  this->dataPtr->connections.push_back(
      gui::model::Events::ConnectOpenLinkInspector(
      std::bind(&ModelCreator::OpenInspector, this, std::placeholders::_1)));

  this->dataPtr->connections.push_back(
      gui::model::Events::ConnectOpenModelPluginInspector(
      std::bind(&ModelCreator::OpenModelPluginInspector, this,
      std::placeholders::_1)));

  this->dataPtr->connections.push_back(
      gui::Events::ConnectAlignMode(
      std::bind(&ModelCreator::OnAlignMode, this, std::placeholders::_1,
      std::placeholders::_2, std::placeholders::_3, std::placeholders::_4,
      std::placeholders::_5)));

  this->dataPtr->connections.push_back(
      gui::Events::ConnectManipMode(
      std::bind(&ModelCreator::OnManipMode, this, std::placeholders::_1)));

  this->dataPtr->connections.push_back(
      event::Events::ConnectSetSelectedEntity(
      std::bind(&ModelCreator::OnSetSelectedEntity, this,
      std::placeholders::_1, std::placeholders::_2)));

  this->dataPtr->connections.push_back(
      gui::model::Events::ConnectSetSelectedLink(
      std::bind(&ModelCreator::OnSetSelectedLink, this, std::placeholders::_1,
      std::placeholders::_2)));

  this->dataPtr->connections.push_back(
      gui::model::Events::ConnectSetSelectedModelPlugin(
      std::bind(&ModelCreator::OnSetSelectedModelPlugin, this,
      std::placeholders::_1, std::placeholders::_2)));

  this->dataPtr->connections.push_back(
      gui::Events::ConnectScaleEntity(
      std::bind(&ModelCreator::OnEntityScaleChanged, this,
      std::placeholders::_1, std::placeholders::_2)));

  this->dataPtr->connections.push_back(
      gui::Events::ConnectMoveEntity(
      std::bind(&ModelCreator::OnEntityMoved, this,
      std::placeholders::_1, std::placeholders::_2, std::placeholders::_3)));

  this->dataPtr->connections.push_back(
      gui::model::Events::ConnectShowLinkContextMenu(
      std::bind(&ModelCreator::ShowContextMenu, this, std::placeholders::_1)));

  this->dataPtr->connections.push_back(
      gui::model::Events::ConnectShowModelPluginContextMenu(
      std::bind(&ModelCreator::ShowModelPluginContextMenu, this,
      std::placeholders::_1)));

  this->dataPtr->connections.push_back(
      gui::model::Events::ConnectRequestNestedModelRemoval(
        std::bind(&ModelCreator::RemoveEntity, this, std::placeholders::_1)));

  this->dataPtr->connections.push_back(
      gui::model::Events::ConnectRequestNestedModelInsertion(
      std::bind(&ModelCreator::InsertNestedModelFromSDF, this,
      std::placeholders::_1)));

  this->dataPtr->connections.push_back(
      gui::model::Events::ConnectRequestLinkRemoval(
      std::bind(&ModelCreator::RemoveEntity, this, std::placeholders::_1)));

  this->dataPtr->connections.push_back(
      gui::model::Events::ConnectRequestLinkInsertion(
      std::bind(&ModelCreator::InsertLinkFromSDF, this,
      std::placeholders::_1)));

  this->dataPtr->connections.push_back(
      gui::model::Events::ConnectRequestModelPluginRemoval(
        std::bind(&ModelCreator::RemoveModelPlugin, this,
          std::placeholders::_1)));

  this->dataPtr->connections.push_back(
      gui::model::Events::ConnectModelPropertiesChanged(
      std::bind(&ModelCreator::OnPropertiesChanged, this, std::placeholders::_1,
      std::placeholders::_2)));

  this->dataPtr->connections.push_back(
      gui::model::Events::ConnectRequestModelPluginInsertion(
      std::bind(&ModelCreator::OnAddModelPlugin, this,
      std::placeholders::_1, std::placeholders::_2, std::placeholders::_3)));

  this->dataPtr->connections.push_back(
      gui::model::Events::ConnectRequestLinkMove(
      std::bind(&ModelCreator::OnRequestLinkMove, this, std::placeholders::_1,
      std::placeholders::_2)));

  this->dataPtr->connections.push_back(
      gui::model::Events::ConnectRequestLinkScale(
      std::bind(&ModelCreator::OnRequestLinkScale, this, std::placeholders::_1,
      std::placeholders::_2)));

  this->dataPtr->connections.push_back(
      gui::model::Events::ConnectRequestNestedModelMove(
      std::bind(&ModelCreator::OnRequestNestedModelMove, this,
      std::placeholders::_1, std::placeholders::_2)));

  if (g_copyAct)
  {
    g_copyAct->setEnabled(false);
    connect(g_copyAct, SIGNAL(triggered()), this, SLOT(OnCopy()));
  }
  if (g_pasteAct)
  {
    g_pasteAct->setEnabled(false);
    connect(g_pasteAct, SIGNAL(triggered()), this, SLOT(OnPaste()));
  }

  this->dataPtr->saveDialog = new SaveDialog(SaveDialog::MODEL);
  MEUserCmdManager::Instance()->Init();

  this->Reset();
}

/////////////////////////////////////////////////
ModelCreator::~ModelCreator()
{
  MEUserCmdManager::Instance()->Clear();


  while (!this->dataPtr->allNestedModels.empty())
    this->RemoveNestedModelImpl(this->dataPtr->allNestedModels.begin()->first);

  this->dataPtr->allNestedModels.clear();
  this->dataPtr->allLinks.clear();
  this->dataPtr->allModelPlugins.clear();
  this->dataPtr->node->Fini();
  this->dataPtr->node.reset();
  this->dataPtr->modelTemplateSDF.reset();
  this->dataPtr->requestPub.reset();
  this->dataPtr->makerPub.reset();
  this->dataPtr->connections.clear();

  delete this->dataPtr->saveDialog;

  delete this->dataPtr->jointMaker;
}

/////////////////////////////////////////////////
void ModelCreator::OnEdit(const bool _checked)
{
  if (_checked)
  {
    this->dataPtr->active = true;
    KeyEventHandler::Instance()->AddPressFilter("model_creator",
        std::bind(&ModelCreator::OnKeyPress, this, std::placeholders::_1));

    MouseEventHandler::Instance()->AddPressFilter("model_creator",
        std::bind(&ModelCreator::OnMousePress, this, std::placeholders::_1));

    MouseEventHandler::Instance()->AddReleaseFilter("model_creator",
        std::bind(&ModelCreator::OnMouseRelease, this, std::placeholders::_1));

    MouseEventHandler::Instance()->AddMoveFilter("model_creator",
        std::bind(&ModelCreator::OnMouseMove, this, std::placeholders::_1));

    MouseEventHandler::Instance()->AddDoubleClickFilter("model_creator",
        std::bind(&ModelCreator::OnMouseDoubleClick, this,
        std::placeholders::_1));

    this->dataPtr->jointMaker->EnableEventHandlers();
  }
  else
  {
    this->dataPtr->active = false;
    KeyEventHandler::Instance()->RemovePressFilter("model_creator");
    MouseEventHandler::Instance()->RemovePressFilter("model_creator");
    MouseEventHandler::Instance()->RemoveReleaseFilter("model_creator");
    MouseEventHandler::Instance()->RemoveMoveFilter("model_creator");
    MouseEventHandler::Instance()->RemoveDoubleClickFilter("model_creator");
    this->dataPtr->jointMaker->DisableEventHandlers();
    this->dataPtr->jointMaker->Stop();

    this->DeselectAll();
  }
  MEUserCmdManager::Instance()->Reset();
  MEUserCmdManager::Instance()->SetActive(this->dataPtr->active);
}

/////////////////////////////////////////////////
void ModelCreator::OnEditModel(const std::string &_modelName)
{
  if (!gui::get_active_camera() ||
      !gui::get_active_camera()->GetScene())
  {
    gzerr << "Unable to edit model. GUI camera or scene is NULL"
        << std::endl;
    return;
  }

  if (!this->dataPtr->active)
  {
    gzwarn << "Model Editor must be active before loading a model. " <<
              "Not loading model " << _modelName << std::endl;
    return;
  }

  // Get SDF model element from model name
  // TODO replace with entity_info and parse gazebo.msgs.Model msgs
  // or handle model_sdf requests in world.
  auto response = transport::request(gui::get_world(), "world_sdf");

  msgs::GzString msg;
  // Make sure the response is correct
  if (response->type() == msg.GetTypeName())
  {
    // Parse the response message
    msg.ParseFromString(response->serialized_data());

    // Parse the string into sdf
    sdf::SDF sdfParsed;
    sdfParsed.SetFromString(msg.data());

    // Check that sdf contains world
    if (sdfParsed.Root()->HasElement("world") &&
        sdfParsed.Root()->GetElement("world")->HasElement("model"))
    {
      sdf::ElementPtr world = sdfParsed.Root()->GetElement("world");
      sdf::ElementPtr model = world->GetElement("model");
      while (model)
      {
        if (model->GetAttribute("name")->GetAsString() == _modelName)
        {
          NestedModelData *modelData = this->CreateModelFromSDF(model);
          rendering::VisualPtr modelVis = modelData->modelVisual;

          // Hide the model from the scene to substitute with the preview visual
          this->SetModelVisible(_modelName, false);

          rendering::ScenePtr scene = gui::get_active_camera()->GetScene();
          rendering::VisualPtr visual = scene->GetVisual(_modelName);

          ignition::math::Pose3d pose;
          if (visual)
          {
            pose = visual->GetWorldPose().Ign();
            this->dataPtr->previewVisual->SetWorldPose(pose);
          }

          this->dataPtr->serverModelName = _modelName;
          this->dataPtr->serverModelSDF = model;
          this->dataPtr->modelPose = pose;

          std::stringstream ss;
          ss << "<sdf version='" << SDF_VERSION << "'>"
             << model->ToString("")
             << "</sdf>";

          gui::model::Events::editModel(_modelName, modelVis->GetName(),
              ss.str());
          return;
        }
        model = model->GetNextElement("model");
      }
      gzwarn << "Couldn't find SDF for " << _modelName << ". Not loading it."
          << std::endl;
    }
  }
  else
  {
    GZ_ASSERT(response->type() == msg.GetTypeName(),
        "Received incorrect response from 'world_sdf' request.");
  }
}

/////////////////////////////////////////////////
NestedModelData *ModelCreator::CreateModelFromSDF(
    const sdf::ElementPtr &_modelElem, const rendering::VisualPtr &_parentVis,
    const bool _emit)
{
  rendering::VisualPtr modelVisual;
  std::stringstream modelNameStream;
  std::string nestedModelName;
  NestedModelData *modelData = new NestedModelData();

  // If no parent vis, this is the root model
  if (!_parentVis)
  {
    // Reset preview visual in case there was something already loaded
    this->Reset();

    // Keep previewModel with previewName to avoid conflicts
    modelVisual = this->dataPtr->previewVisual;
    modelNameStream << modelVisual->GetName();

    // Model general info
    if (_modelElem->HasAttribute("name"))
      this->SetModelName(_modelElem->Get<std::string>("name"));

    if (_modelElem->HasElement("pose"))
    {
      this->dataPtr->modelPose =
          _modelElem->Get<ignition::math::Pose3d>("pose");
    }
    else
    {
      this->dataPtr->modelPose = ignition::math::Pose3d::Zero;
    }
    this->dataPtr->previewVisual->SetPose(this->dataPtr->modelPose);

    if (_modelElem->HasElement("static"))
      this->dataPtr->isStatic = _modelElem->Get<bool>("static");
    if (_modelElem->HasElement("allow_auto_disable"))
      this->dataPtr->autoDisable = _modelElem->Get<bool>("allow_auto_disable");
    gui::model::Events::modelPropertiesChanged(this->dataPtr->isStatic,
        this->dataPtr->autoDisable);
    gui::model::Events::modelNameChanged(this->ModelName());

    modelData->modelVisual = modelVisual;
  }
  // Nested models are attached to a parent visual
  else
  {
    // Internal name
    std::stringstream parentNameStream;
    parentNameStream << _parentVis->GetName();

    modelNameStream << parentNameStream.str() << "::" <<
        _modelElem->Get<std::string>("name");
    nestedModelName = modelNameStream.str();

    // Generate unique name
    auto itName = this->dataPtr->allNestedModels.find(nestedModelName);
    int nameCounter = 0;
    std::string uniqueName;
    while (itName != this->dataPtr->allNestedModels.end())
    {
      std::stringstream uniqueNameStr;
      uniqueNameStr << nestedModelName << "_" << nameCounter++;
      uniqueName = uniqueNameStr.str();
      itName = this->dataPtr->allNestedModels.find(uniqueName);
    }
    if (!uniqueName.empty())
      nestedModelName = uniqueName;

    // Model Visual
    modelVisual.reset(
        new rendering::Visual(nestedModelName, _parentVis, false));
    modelVisual->Load();
    modelVisual->SetTransparency(ModelData::GetEditTransparency());

    if (_modelElem->HasElement("pose"))
      modelVisual->SetPose(_modelElem->Get<ignition::math::Pose3d>("pose"));

    // Only keep SDF and preview visual
    std::string leafName = nestedModelName;
    leafName = leafName.substr(leafName.rfind("::")+2);

    modelData->modelSDF = _modelElem;
    modelData->modelVisual = modelVisual;
    modelData->SetName(leafName);
    modelData->SetPose(_modelElem->Get<ignition::math::Pose3d>("pose"));
  }

  // Notify nested model insertion
  if (_parentVis)
  {
    std::lock_guard<std::recursive_mutex> lock(this->dataPtr->updateMutex);
    this->dataPtr->allNestedModels[nestedModelName] = modelData;

    // fire nested inserted events only when the nested model is
    //  not attached to the mouse
    if (_emit)
      gui::model::Events::nestedModelInserted(nestedModelName);
  }

  // Recursively load models nested in this model
  // This must be done after other widgets were notified about the current
  // model but before making joints
  sdf::ElementPtr nestedModelElem;
  if (_modelElem->HasElement("model"))
     nestedModelElem = _modelElem->GetElement("model");
  while (nestedModelElem)
  {
    if (this->dataPtr->canonicalModel.empty())
      this->dataPtr->canonicalModel = nestedModelName;

    NestedModelData *nestedModelData =
        this->CreateModelFromSDF(nestedModelElem, modelVisual, _emit);
    rendering::VisualPtr nestedModelVis = nestedModelData->modelVisual;
    modelData->models[nestedModelVis->GetName()] = nestedModelVis;
    nestedModelElem = nestedModelElem->GetNextElement("model");
  }

  // Links
  sdf::ElementPtr linkElem;
  if (_modelElem->HasElement("link"))
    linkElem = _modelElem->GetElement("link");
  while (linkElem)
  {
    LinkData *linkData = this->CreateLinkFromSDF(linkElem, modelVisual);

    // if its parent is not the preview visual then the link has to be nested
    if (modelVisual != this->dataPtr->previewVisual)
      linkData->nested = true;
    rendering::VisualPtr linkVis = linkData->linkVisual;

    modelData->links[linkVis->GetName()] = linkVis;
    linkElem = linkElem->GetNextElement("link");
  }

  // Don't load joints or plugins for nested models
  if (!_parentVis)
  {
    // Joints
    sdf::ElementPtr jointElem;
    if (_modelElem->HasElement("joint"))
       jointElem = _modelElem->GetElement("joint");

    while (jointElem)
    {
      this->dataPtr->jointMaker->CreateJointFromSDF(jointElem,
          modelNameStream.str());
      jointElem = jointElem->GetNextElement("joint");
    }

    // Plugins
    sdf::ElementPtr pluginElem;
    if (_modelElem->HasElement("plugin"))
      pluginElem = _modelElem->GetElement("plugin");
    while (pluginElem)
    {
      this->AddModelPlugin(pluginElem);
      pluginElem = pluginElem->GetNextElement("plugin");
    }
  }

  return modelData;
}

/////////////////////////////////////////////////
void ModelCreator::OnNew()
{
  this->Stop();

  if (this->dataPtr->allLinks.empty() &&
      this->dataPtr->allNestedModels.empty() &&
      this->dataPtr->allModelPlugins.empty())
  {
    this->Reset();
    gui::model::Events::newModel();
    return;
  }
  QString msg;
  QMessageBox msgBox(QMessageBox::Warning, QString("New"), msg);
  QPushButton *cancelButton = msgBox.addButton("Cancel",
      QMessageBox::RejectRole);
  msgBox.setEscapeButton(cancelButton);
  QPushButton *saveButton = new QPushButton("Save");

  switch (this->dataPtr->currentSaveState)
  {
    case ALL_SAVED:
    {
      msg.append("Are you sure you want to close this model and open a new "
                 "canvas?\n\n");
      QPushButton *newButton =
          msgBox.addButton("New Canvas", QMessageBox::AcceptRole);
      msgBox.setDefaultButton(newButton);
      break;
    }
    case UNSAVED_CHANGES:
    case NEVER_SAVED:
    {
      msg.append("You have unsaved changes. Do you want to save this model "
                 "and open a new canvas?\n\n");
      msgBox.addButton("Don't Save", QMessageBox::DestructiveRole);
      msgBox.addButton(saveButton, QMessageBox::AcceptRole);
      msgBox.setDefaultButton(saveButton);
      break;
    }
    default:
      return;
  }

  msgBox.setText(msg);

  msgBox.exec();

  if (msgBox.clickedButton() != cancelButton)
  {
    if (msgBox.clickedButton() == saveButton)
    {
      if (!this->OnSave())
      {
        return;
      }
    }

    this->Reset();
    gui::model::Events::newModel();
  }
}

/////////////////////////////////////////////////
bool ModelCreator::OnSave()
{
  this->Stop();

  switch (this->dataPtr->currentSaveState)
  {
    case UNSAVED_CHANGES:
    {
      this->SaveModelFiles();
      gui::model::Events::saveModel(this->dataPtr->modelName);
      return true;
    }
    case NEVER_SAVED:
    {
      return this->OnSaveAs();
    }
    default:
      return false;
  }
}

/////////////////////////////////////////////////
bool ModelCreator::OnSaveAs()
{
  this->Stop();

  if (this->dataPtr->saveDialog->OnSaveAs())
  {
    // Prevent changing save location
    this->dataPtr->currentSaveState = ALL_SAVED;
    // Get name set by user
    this->SetModelName(this->dataPtr->saveDialog->GetModelName());
    // Update name on palette
    gui::model::Events::saveModel(this->dataPtr->modelName);
    // Generate and save files
    this->SaveModelFiles();
    return true;
  }
  return false;
}

/////////////////////////////////////////////////
void ModelCreator::OnNameChanged(const std::string &_name)
{
  if (_name.compare(this->dataPtr->modelName) == 0)
    return;

  this->SetModelName(_name);
  this->ModelChanged();
}

/////////////////////////////////////////////////
void ModelCreator::OnExit()
{
  this->Stop();

  if (this->dataPtr->allLinks.empty() &&
      this->dataPtr->allNestedModels.empty() &&
      this->dataPtr->allModelPlugins.empty())
  {
    if (!this->dataPtr->serverModelName.empty())
      this->SetModelVisible(this->dataPtr->serverModelName, true);
    this->Reset();
    gui::model::Events::newModel();
    gui::model::Events::finishModel();
    return;
  }

  switch (this->dataPtr->currentSaveState)
  {
    case ALL_SAVED:
    {
      QString msg("Are you ready to exit?\n\n");
      QMessageBox msgBox(QMessageBox::NoIcon, QString("Exit"), msg);

      QPushButton *cancelButton = msgBox.addButton("Cancel",
          QMessageBox::RejectRole);
      QPushButton *exitButton =
          msgBox.addButton("Exit", QMessageBox::AcceptRole);
      msgBox.setDefaultButton(exitButton);
      msgBox.setEscapeButton(cancelButton);

      msgBox.exec();
      if (msgBox.clickedButton() == cancelButton)
      {
        return;
      }
      this->FinishModel();
      break;
    }
    case UNSAVED_CHANGES:
    case NEVER_SAVED:
    {
      QString msg("Save Changes before exiting?\n\n");

      QMessageBox msgBox(QMessageBox::NoIcon, QString("Exit"), msg);
      QPushButton *cancelButton = msgBox.addButton("Cancel",
          QMessageBox::RejectRole);
      msgBox.addButton("Don't Save, Exit", QMessageBox::DestructiveRole);
      QPushButton *saveButton = msgBox.addButton("Save and Exit",
          QMessageBox::AcceptRole);
      msgBox.setDefaultButton(cancelButton);
      msgBox.setDefaultButton(saveButton);

      msgBox.exec();
      if (msgBox.clickedButton() == cancelButton)
        return;

      if (msgBox.clickedButton() == saveButton)
      {
        if (!this->OnSave())
        {
          return;
        }
      }
      break;
    }
    default:
      return;
  }

  // Create entity on main window up to the saved point
  if (this->dataPtr->currentSaveState != NEVER_SAVED)
    this->FinishModel();
  else
    this->SetModelVisible(this->dataPtr->serverModelName, true);

  this->Reset();

  gui::model::Events::newModel();
  gui::model::Events::finishModel();
}

/////////////////////////////////////////////////
void ModelCreator::OnPropertiesChanged(const bool _static,
    const bool _autoDisable)
{
  this->dataPtr->autoDisable = _autoDisable;
  this->dataPtr->isStatic = _static;
  this->ModelChanged();
}

/////////////////////////////////////////////////
void ModelCreator::SaveModelFiles()
{
  this->dataPtr->saveDialog->GenerateConfig();
  this->dataPtr->saveDialog->SaveToConfig();
  this->GenerateSDF();
  this->dataPtr->saveDialog->SaveToSDF(this->dataPtr->modelSDF);
  this->dataPtr->currentSaveState = ALL_SAVED;
}

/////////////////////////////////////////////////
std::string ModelCreator::CreateModel()
{
  this->Reset();
  return this->dataPtr->folderName;
}

/////////////////////////////////////////////////
void ModelCreator::AddJoint(const std::string &_type)
{
  this->Stop();
  if (this->dataPtr->jointMaker)
    this->dataPtr->jointMaker->AddJoint(_type);
}

/////////////////////////////////////////////////
void ModelCreator::AddCustomLink(const EntityType _type,
    const ignition::math::Vector3d &_size, const ignition::math::Pose3d &_pose,
    const std::string &_uri, const unsigned int _samples)
{
  this->Stop();

  this->dataPtr->addEntityType = _type;
  if (_type != ENTITY_NONE)
  {
    auto linkData = this->AddShape(_type, _size, _pose, _uri, _samples);
    if (linkData)
      this->dataPtr->mouseVisual = linkData->linkVisual;
  }
}

/////////////////////////////////////////////////
LinkData *ModelCreator::AddShape(const EntityType _type,
    const ignition::math::Vector3d &_size, const ignition::math::Pose3d &_pose,
    const std::string &_uri, const unsigned int _samples)
{
  if (!this->dataPtr->previewVisual)
  {
    this->Reset();
  }

  std::stringstream linkNameStream;
  linkNameStream << this->dataPtr->previewVisual->GetName() << "::link_" <<
      this->dataPtr->linkCounter++;
  std::string linkName = linkNameStream.str();

  rendering::VisualPtr linkVisual(new rendering::Visual(linkName,
      this->dataPtr->previewVisual, false));
  linkVisual->Load();
  linkVisual->SetTransparency(ModelData::GetEditTransparency());

  std::ostringstream visualName;
  visualName << linkName << "::visual";
  rendering::VisualPtr visVisual(new rendering::Visual(visualName.str(),
      linkVisual, false));
  sdf::ElementPtr visualElem =  this->dataPtr->modelTemplateSDF->Root()
      ->GetElement("model")->GetElement("link")->GetElement("visual");

  sdf::ElementPtr geomElem =  visualElem->GetElement("geometry");
  geomElem->ClearElements();

  if (_type == ENTITY_CYLINDER)
  {
    sdf::ElementPtr cylinderElem = geomElem->AddElement("cylinder");
    (cylinderElem->GetElement("radius"))->Set(_size.X()*0.5);
    (cylinderElem->GetElement("length"))->Set(_size.Z());
  }
  else if (_type == ENTITY_SPHERE)
  {
    ((geomElem->AddElement("sphere"))->GetElement("radius"))->
        Set(_size.X()*0.5);
  }
  else if (_type == ENTITY_MESH)
  {
    sdf::ElementPtr meshElem = geomElem->AddElement("mesh");
    meshElem->GetElement("scale")->Set(_size);
    meshElem->GetElement("uri")->Set(_uri);
  }
  else if (_type == ENTITY_POLYLINE)
  {
    QFileInfo info(QString::fromStdString(_uri));
    if (!info.isFile() || info.completeSuffix().toLower() != "svg")
    {
      gzerr << "File [" << _uri << "] not found or invalid!" << std::endl;
      return NULL;
    }

    common::SVGLoader svgLoader(_samples);
    std::vector<common::SVGPath> paths;
    svgLoader.Parse(_uri, paths);

    if (paths.empty())
    {
      gzerr << "No paths found on file [" << _uri << "]" << std::endl;
      return NULL;
    }

    // SVG paths do not map to sdf polylines, because we now allow a contour
    // to be made of multiple svg disjoint paths.
    // For this reason, we compute the closed polylines that can be extruded
    // in this step
    std::vector< std::vector<ignition::math::Vector2d> > closedPolys;
    std::vector< std::vector<ignition::math::Vector2d> > openPolys;
    svgLoader.PathsToClosedPolylines(paths, 0.05, closedPolys, openPolys);
    if (closedPolys.empty())
    {
      gzerr << "No closed polylines found on file [" << _uri << "]"
        << std::endl;
      return NULL;
    }
    if (!openPolys.empty())
    {
      gzmsg << "There are " << openPolys.size() << "open polylines. "
        << "They will be ignored." << std::endl;
    }
    // Find extreme values to center the polylines
    ignition::math::Vector2d min(paths[0].polylines[0][0]);
    ignition::math::Vector2d max(min);

    for (auto const &poly : closedPolys)
    {
      for (auto const &pt : poly)
      {
        if (pt.X() < min.X())
          min.X() = pt.X();
        if (pt.Y() < min.Y())
          min.Y() = pt.Y();
        if (pt.X() > max.X())
          max.X() = pt.X();
        if (pt.Y() > max.Y())
          max.Y() = pt.Y();
      }
    }
    for (auto const &poly : closedPolys)
    {
      sdf::ElementPtr polylineElem = geomElem->AddElement("polyline");
      polylineElem->GetElement("height")->Set(_size.Z());

      for (auto const &p : poly)
      {
        // Translate to center
        ignition::math::Vector2d pt = p - min - (max-min)*0.5;
        // Swap X and Y so Z will point up
        // (in 2D it points into the screen)
        sdf::ElementPtr pointElem = polylineElem->AddElement("point");
        pointElem->Set(
            ignition::math::Vector2d(pt.Y()*_size.Y(), pt.X()*_size.X()));
      }
    }
  }
  else
  {
    if (_type != ENTITY_BOX)
    {
      gzwarn << "Unknown link type '" << _type << "'. " <<
          "Adding a box" << std::endl;
    }

    ((geomElem->AddElement("box"))->GetElement("size"))->Set(_size);
  }

  visVisual->Load(visualElem);
  LinkData *linkData = this->CreateLink(visVisual);
  linkVisual->SetVisibilityFlags(GZ_VISIBILITY_GUI | GZ_VISIBILITY_SELECTABLE);

  linkVisual->SetPose(_pose);

  // insert over ground plane for now
  auto linkPos = linkVisual->GetWorldPose().Ign().Pos();
  if (_type == ENTITY_BOX || _type == ENTITY_CYLINDER || _type == ENTITY_SPHERE)
  {
    linkPos.Z() = _size.Z() * 0.5;
  }
  // override orientation as it's more natural to insert objects upright rather
  // than inserting it in the model frame.
  linkVisual->SetWorldPose(ignition::math::Pose3d(linkPos,
      ignition::math::Quaterniond()));

  return linkData;
}

/////////////////////////////////////////////////
NestedModelData *ModelCreator::AddModel(const sdf::ElementPtr &_sdf)
{
  // Create a top-level nested model
  return this->CreateModelFromSDF(_sdf, this->dataPtr->previewVisual, false);
}

/////////////////////////////////////////////////
LinkData *ModelCreator::CreateLink(const rendering::VisualPtr &_visual)
{
  LinkData *link = new LinkData();

  msgs::Model model;
  double mass = 1.0;

  // set reasonable inertial values based on geometry
  std::string geomType = _visual->GetGeometryType();
  if (geomType == "cylinder")
    msgs::AddCylinderLink(model, mass, 0.5, 1.0);
  else if (geomType == "sphere")
    msgs::AddSphereLink(model, mass, 0.5);
  else
    msgs::AddBoxLink(model, mass, ignition::math::Vector3d::One);
  link->Load(msgs::LinkToSDF(model.link(0)));

  MainWindow *mainWindow = gui::get_main_window();
  if (mainWindow)
  {
    connect(gui::get_main_window(), SIGNAL(Close()), link->inspector,
        SLOT(close()));
  }

  link->linkVisual = _visual->GetParent();
  link->AddVisual(_visual);

  link->inspector->SetLinkId(link->linkVisual->GetName());

  // override transparency
  _visual->SetTransparency(_visual->GetTransparency() *
      (1-ModelData::GetEditTransparency()-0.1)
      + ModelData::GetEditTransparency());

  // create collision with identical geometry
  rendering::VisualPtr collisionVis =
      _visual->Clone(link->linkVisual->GetName() + "::collision",
      link->linkVisual);

  // orange
  collisionVis->SetMaterial("Gazebo/Orange");
  collisionVis->SetTransparency(
      ignition::math::clamp(ModelData::GetEditTransparency() * 2.0, 0.0, 0.8));
  ModelData::UpdateRenderGroup(collisionVis);
  link->AddCollision(collisionVis);

  std::string linkName = link->linkVisual->GetName();

  std::string leafName = linkName;
  size_t idx = linkName.rfind("::");
  if (idx != std::string::npos)
    leafName = linkName.substr(idx+2);

  link->SetName(leafName);

  {
    std::lock_guard<std::recursive_mutex> lock(this->dataPtr->updateMutex);
    this->dataPtr->allLinks[linkName] = link;
    if (this->dataPtr->canonicalLink.empty())
      this->dataPtr->canonicalLink = linkName;
  }

  this->ModelChanged();

  return link;
}

/////////////////////////////////////////////////
void ModelCreator::InsertLinkFromSDF(sdf::ElementPtr _sdf)
{
  if (!_sdf)
    return;

  this->CreateLinkFromSDF(_sdf, this->dataPtr->previewVisual);
}

/////////////////////////////////////////////////
void ModelCreator::InsertNestedModelFromSDF(sdf::ElementPtr _sdf)
{
  if (!_sdf)
    return;

  this->CreateModelFromSDF(_sdf, this->dataPtr->previewVisual);
}

/////////////////////////////////////////////////
LinkData *ModelCreator::CloneLink(const std::string &_linkName)
{
  std::lock_guard<std::recursive_mutex> lock(this->dataPtr->updateMutex);

  auto it = this->dataPtr->allLinks.find(_linkName);
  if (it == this->dataPtr->allLinks.end())
  {
    gzerr << "No link with name: " << _linkName << " found."  << std::endl;
    return NULL;
  }

  // generate unique name.
  std::string newName = _linkName + "_clone";
  auto itName = this->dataPtr->allLinks.find(newName);
  int nameCounter = 0;
  while (itName != this->dataPtr->allLinks.end())
  {
    std::stringstream newLinkName;
    newLinkName << _linkName << "_clone_" << nameCounter++;
    newName = newLinkName.str();
    itName = this->dataPtr->allLinks.find(newName);
  }

  std::string leafName = newName;
  size_t idx = newName.rfind("::");
  if (idx != std::string::npos)
    leafName = newName.substr(idx+2);
  LinkData *link = it->second->Clone(leafName);

  this->dataPtr->allLinks[newName] = link;

  this->ModelChanged();

  return link;
}

/////////////////////////////////////////////////
NestedModelData *ModelCreator::CloneNestedModel(
    const std::string &_nestedModelName)
{
  std::lock_guard<std::recursive_mutex> lock(this->dataPtr->updateMutex);

  auto it = this->dataPtr->allNestedModels.find(_nestedModelName);
  if (it == this->dataPtr->allNestedModels.end())
  {
    gzerr << "No nested model with name: " << _nestedModelName <<
        " found."  << std::endl;
    return NULL;
  }

  // generate unique name.
  std::string newName = _nestedModelName + "_clone";
  auto itName = this->dataPtr->allNestedModels.find(newName);
  int nameCounter = 0;
  while (itName != this->dataPtr->allNestedModels.end())
  {
    std::stringstream newNestedModelName;
    newNestedModelName << _nestedModelName << "_clone_" << nameCounter++;
    newName = newNestedModelName.str();
    itName = this->dataPtr->allNestedModels.find(newName);
  }

  std::string leafName = newName;
  size_t idx = newName.rfind("::");
  if (idx != std::string::npos)
    leafName = newName.substr(idx+2);
  sdf::ElementPtr cloneSDF = it->second->modelSDF->Clone();
  cloneSDF->GetAttribute("name")->Set(leafName);

  NestedModelData *modelData = this->CreateModelFromSDF(cloneSDF,
    it->second->modelVisual->GetParent(), false);

  this->ModelChanged();

  return modelData;
}

/////////////////////////////////////////////////
LinkData *ModelCreator::CreateLinkFromSDF(const sdf::ElementPtr &_linkElem,
    const rendering::VisualPtr &_parentVis)
{
  if (_linkElem == NULL)
  {
    gzwarn << "NULL SDF pointer, not creating link." << std::endl;
    return NULL;
  }

  LinkData *link = new LinkData();
  MainWindow *mainWindow = gui::get_main_window();
  if (mainWindow)
  {
    connect(gui::get_main_window(), SIGNAL(Close()), link->inspector,
        SLOT(close()));
  }

  link->Load(_linkElem);

  // Link
  std::stringstream linkNameStream;
  std::string leafName = link->Name();
  linkNameStream << _parentVis->GetName() << "::";

  linkNameStream << leafName;
  std::string linkName = linkNameStream.str();

  if (this->dataPtr->canonicalLink.empty())
    this->dataPtr->canonicalLink = linkName;

  link->SetName(leafName);

  // if link name is scoped, it could mean that it's from an included model.
  // The joint maker needs to know about this in order to specify the correct
  // parent and child links in sdf generation step.
  if (leafName.find("::") != std::string::npos)
    this->dataPtr->jointMaker->AddScopedLinkName(leafName);

  rendering::VisualPtr linkVisual(
      new rendering::Visual(linkName, _parentVis, false));
  linkVisual->Load();
  linkVisual->SetPose(link->Pose());
  link->linkVisual = linkVisual;
  link->inspector->SetLinkId(link->linkVisual->GetName());

  // Visuals
  int visualIndex = 0;
  sdf::ElementPtr visualElem;

  if (_linkElem->HasElement("visual"))
    visualElem = _linkElem->GetElement("visual");

  linkVisual->SetTransparency(ModelData::GetEditTransparency());

  while (visualElem)
  {
    // Visual name
    std::string visualName;
    if (visualElem->HasAttribute("name"))
    {
      visualName = linkName + "::" + visualElem->Get<std::string>("name");
      visualIndex++;
    }
    else
    {
      std::stringstream visualNameStream;
      visualNameStream << linkName << "::visual_" << visualIndex++;
      visualName = visualNameStream.str();
      gzwarn << "SDF missing visual name attribute. Created name " << visualName
          << std::endl;
    }
    rendering::VisualPtr visVisual(new rendering::Visual(visualName,
        linkVisual, false));
    visVisual->Load(visualElem);

    // Visual pose
    ignition::math::Pose3d visualPose;
    if (visualElem->HasElement("pose"))
      visualPose = visualElem->Get<ignition::math::Pose3d>("pose");
    else
      visualPose.Set(0, 0, 0, 0, 0, 0);
    visVisual->SetPose(visualPose);

    // Add to link
    link->AddVisual(visVisual);

    // override transparency
    visVisual->SetTransparency(visVisual->GetTransparency() *
        (1-ModelData::GetEditTransparency()-0.1)
        + ModelData::GetEditTransparency());

    visualElem = visualElem->GetNextElement("visual");
  }

  // Collisions
  int collisionIndex = 0;
  sdf::ElementPtr collisionElem;

  if (_linkElem->HasElement("collision"))
    collisionElem = _linkElem->GetElement("collision");

  while (collisionElem)
  {
    // Collision name
    std::string collisionName;
    if (collisionElem->HasAttribute("name"))
    {
      collisionName = linkName + "::" + collisionElem->Get<std::string>("name");
      collisionIndex++;
    }
    else
    {
      std::ostringstream collisionNameStream;
      collisionNameStream << linkName << "::collision_" << collisionIndex++;
      collisionName = collisionNameStream.str();
      gzwarn << "SDF missing collision name attribute. Created name " <<
          collisionName << std::endl;
    }
    rendering::VisualPtr colVisual(new rendering::Visual(collisionName,
        linkVisual, false));

    // Collision pose
    ignition::math::Pose3d collisionPose;
    if (collisionElem->HasElement("pose"))
      collisionPose = collisionElem->Get<ignition::math::Pose3d>("pose");
    else
      collisionPose.Set(0, 0, 0, 0, 0, 0);

    // Make a visual element from the collision element
    sdf::ElementPtr colVisualElem =  this->dataPtr->modelTemplateSDF->Root()
        ->GetElement("model")->GetElement("link")->GetElement("visual");

    sdf::ElementPtr geomElem = colVisualElem->GetElement("geometry");
    geomElem->ClearElements();
    geomElem->Copy(collisionElem->GetElement("geometry"));

    colVisual->Load(colVisualElem);
    colVisual->SetPose(collisionPose);
    colVisual->SetMaterial("Gazebo/Orange");
    colVisual->SetTransparency(ignition::math::clamp(
        ModelData::GetEditTransparency() * 2.0, 0.0, 0.8));
    ModelData::UpdateRenderGroup(colVisual);

    // Add to link
    msgs::Collision colMsg = msgs::CollisionFromSDF(collisionElem);
    link->AddCollision(colVisual, &colMsg);

    collisionElem = collisionElem->GetNextElement("collision");
  }

  linkVisual->SetVisibilityFlags(GZ_VISIBILITY_GUI | GZ_VISIBILITY_SELECTABLE);

  gui::model::Events::linkInserted(linkName);

  {
    std::lock_guard<std::recursive_mutex> lock(this->dataPtr->updateMutex);
    this->dataPtr->allLinks[linkName] = link;
  }

  this->ModelChanged();

  return link;
}

/////////////////////////////////////////////////
void ModelCreator::RemoveNestedModelImpl(const std::string &_nestedModelName)
{
  if (!this->dataPtr->previewVisual)
  {
    this->Reset();
    return;
  }

  NestedModelData *modelData = NULL;
  {
    std::lock_guard<std::recursive_mutex> lock(this->dataPtr->updateMutex);
    if (this->dataPtr->allNestedModels.find(_nestedModelName) ==
        this->dataPtr->allNestedModels.end())
    {
      return;
    }
    modelData = this->dataPtr->allNestedModels[_nestedModelName];
  }

  if (!modelData)
    return;

  // Copy before reference is deleted.
  std::string nestedModelName(_nestedModelName);

  // remove all its models
  for (auto &modelIt : modelData->models)
    this->RemoveNestedModelImpl(modelIt.first);

  // remove all its links and joints
  for (auto &linkIt : modelData->links)
  {
    // if it's a link
    if (this->dataPtr->allLinks.find(linkIt.first) !=
        this->dataPtr->allLinks.end())
    {
      if (this->dataPtr->jointMaker)
      {
        this->dataPtr->jointMaker->RemoveJointsByLink(linkIt.first);
      }
      this->RemoveLinkImpl(linkIt.first);
    }
  }

  rendering::ScenePtr scene = modelData->modelVisual->GetScene();
  if (scene)
  {
    scene->RemoveVisual(modelData->modelVisual);
  }

  modelData->modelVisual.reset();
  {
    std::lock_guard<std::recursive_mutex> lock(this->dataPtr->updateMutex);
    this->dataPtr->allNestedModels.erase(_nestedModelName);
    delete modelData;
  }
  gui::model::Events::nestedModelRemoved(nestedModelName);

  this->ModelChanged();
}

/////////////////////////////////////////////////
void ModelCreator::RemoveLinkImpl(const std::string &_linkName)
{
  if (!this->dataPtr->previewVisual)
  {
    this->Reset();
    return;
  }

  LinkData *link = NULL;
  {
    std::lock_guard<std::recursive_mutex> lock(this->dataPtr->updateMutex);
    auto linkIt = this->dataPtr->allLinks.find(_linkName);
    if (linkIt == this->dataPtr->allLinks.end())
      return;
    link = linkIt->second;
  }

  if (!link)
    return;

  // Copy before reference is deleted.
  std::string linkName(_linkName);

  rendering::ScenePtr scene = link->linkVisual->GetScene();
  if (scene)
  {
    for (auto &it : link->visuals)
    {
      rendering::VisualPtr vis = it.first;
      scene->RemoveVisual(vis);
    }
    scene->RemoveVisual(link->linkVisual);
    for (auto &colIt : link->collisions)
    {
      rendering::VisualPtr vis = colIt.first;
      scene->RemoveVisual(vis);
    }

    scene->RemoveVisual(link->linkVisual);
  }

  link->linkVisual.reset();
  {
    std::lock_guard<std::recursive_mutex> lock(this->dataPtr->updateMutex);
    this->dataPtr->allLinks.erase(linkName);
    delete link;
  }
  gui::model::Events::linkRemoved(linkName);

  this->ModelChanged();
}

/////////////////////////////////////////////////
void ModelCreator::Reset()
{
  delete this->dataPtr->saveDialog;
  this->dataPtr->saveDialog = new SaveDialog(SaveDialog::MODEL);

  this->dataPtr->jointMaker->Reset();
  this->dataPtr->selectedLinks.clear();
  this->dataPtr->selectedNestedModels.clear();

  if (g_copyAct)
    g_copyAct->setEnabled(false);

  if (g_pasteAct)
    g_pasteAct->setEnabled(false);

  this->dataPtr->currentSaveState = NEVER_SAVED;
  this->SetModelName(this->dataPtr->modelDefaultName);
  this->dataPtr->serverModelName = "";
  this->dataPtr->serverModelSDF.reset();
  this->dataPtr->serverModelVisible.clear();
  this->dataPtr->canonicalLink = "";
  this->dataPtr->canonicalModel = "";

  this->dataPtr->modelTemplateSDF.reset(new sdf::SDF);
  this->dataPtr->modelTemplateSDF->SetFromString(
      ModelData::GetTemplateSDFString());

  this->dataPtr->modelSDF.reset(new sdf::SDF);

  this->dataPtr->isStatic = false;
  this->dataPtr->autoDisable = true;
  gui::model::Events::modelPropertiesChanged(this->dataPtr->isStatic,
      this->dataPtr->autoDisable);
  gui::model::Events::modelNameChanged(this->ModelName());

  while (!this->dataPtr->allLinks.empty())
    this->RemoveLinkImpl(this->dataPtr->allLinks.begin()->first);
  this->dataPtr->allLinks.clear();

  while (!this->dataPtr->allNestedModels.empty())
    this->RemoveNestedModelImpl(this->dataPtr->allNestedModels.begin()->first);
  this->dataPtr->allNestedModels.clear();

  this->dataPtr->allModelPlugins.clear();

  if (!gui::get_active_camera() ||
    !gui::get_active_camera()->GetScene())
  return;

  rendering::ScenePtr scene = gui::get_active_camera()->GetScene();
  if (this->dataPtr->previewVisual)
    scene->RemoveVisual(this->dataPtr->previewVisual);

  std::stringstream previewModelName;
  previewModelName << this->dataPtr->previewName << "_" <<
      this->dataPtr->modelCounter++;
  this->dataPtr->previewVisual.reset(new rendering::Visual(
      previewModelName.str(), scene->WorldVisual(), false));

  this->dataPtr->previewVisual->Load();
  this->dataPtr->modelPose = ignition::math::Pose3d::Zero;
  this->dataPtr->previewVisual->SetPose(this->dataPtr->modelPose);
}

/////////////////////////////////////////////////
void ModelCreator::SetModelName(const std::string &_modelName)
{
  this->dataPtr->modelName = _modelName;
  this->dataPtr->saveDialog->SetModelName(_modelName);

  this->dataPtr->folderName = this->dataPtr->saveDialog->
      GetFolderNameFromModelName(this->dataPtr->modelName);

  if (this->dataPtr->currentSaveState == NEVER_SAVED)
  {
    // Set new saveLocation
    boost::filesystem::path oldPath(
        this->dataPtr->saveDialog->GetSaveLocation());

    auto newPath = oldPath.parent_path() / this->dataPtr->folderName;
    this->dataPtr->saveDialog->SetSaveLocation(newPath.string());
  }
}

/////////////////////////////////////////////////
std::string ModelCreator::ModelName() const
{
  return this->dataPtr->modelName;
}

/////////////////////////////////////////////////
void ModelCreator::SetStatic(const bool _static)
{
  this->dataPtr->isStatic = _static;
  this->ModelChanged();
}

/////////////////////////////////////////////////
void ModelCreator::SetAutoDisable(const bool _auto)
{
  this->dataPtr->autoDisable = _auto;
  this->ModelChanged();
}

/////////////////////////////////////////////////
void ModelCreator::FinishModel()
{
  if (!this->dataPtr->serverModelName.empty())
  {
    // this resets the material properites of the model
    // (important if we want to reuse it later, e.g. saving to same model name,
    // if this is not done then the new model with same name will be invisible)
    this->SetModelVisible(this->dataPtr->serverModelName, true);

    // delete model on server first before spawning the updated one.
    transport::request(gui::get_world(), "entity_delete",
        this->dataPtr->serverModelName);
    int timeoutCounter = 0;
    int timeout = 100;
    // wait for entity to be deleted on server side
    while (timeoutCounter < timeout)
    {
      auto response = transport::request(gui::get_world(), "entity_info",
          this->dataPtr->serverModelName);
      // Make sure the response is correct
      if (response->response() == "nonexistent")
        break;

      common::Time::MSleep(100);
      QCoreApplication::processEvents();
      timeoutCounter++;
    }

    timeoutCounter = 0;
    // wait for client scene to acknowledge the deletion
    rendering::ScenePtr scene = gui::get_active_camera()->GetScene();
    rendering::VisualPtr visual = scene->GetVisual(
        this->dataPtr->serverModelName);
    while (timeoutCounter < timeout)
    {
      visual = scene->GetVisual(this->dataPtr->serverModelName);
      if (!visual)
        break;
      common::Time::MSleep(100);
      QCoreApplication::processEvents();
      timeoutCounter++;
    }
  }

  event::Events::setSelectedEntity("", "normal");
  this->CreateTheEntity();
  this->Reset();
}

/////////////////////////////////////////////////
void ModelCreator::CreateTheEntity()
{
  if (!this->dataPtr->modelSDF->Root()->HasElement("model"))
  {
    gzerr << "Generated invalid SDF! Cannot create entity." << std::endl;
    return;
  }

  msgs::Factory msg;
  // Create a new name if the model exists
  auto modelElem = this->dataPtr->modelSDF->Root()->GetElement("model");
  std::string modelElemName = modelElem->Get<std::string>("name");
  if (modelElemName != this->dataPtr->serverModelName &&
      has_entity_name(modelElemName))
  {
    int i = 0;
    while (has_entity_name(modelElemName))
    {
      modelElemName = modelElem->Get<std::string>("name") + "_" +
        std::to_string(i++);
    }
    modelElem->GetAttribute("name")->Set(modelElemName);
  }

  msg.set_sdf(this->dataPtr->modelSDF->ToString());
  msgs::Set(msg.mutable_pose(), this->dataPtr->modelPose);
  this->dataPtr->makerPub->Publish(msg);
}

/////////////////////////////////////////////////
void ModelCreator::AddEntity(const sdf::ElementPtr &_sdf)
{
  if (!this->dataPtr->previewVisual)
  {
    this->Reset();
  }

  this->Stop();

  if (_sdf->GetName() == "model")
  {
    this->dataPtr->addEntityType = ENTITY_MODEL;
    NestedModelData *modelData = this->AddModel(_sdf);
    if (modelData)
      this->dataPtr->mouseVisual = modelData->modelVisual;
  }
}

/////////////////////////////////////////////////
sdf::ElementPtr ModelCreator::GetEntitySDF(const std::string &_name)
{
  auto it = this->dataPtr->allNestedModels.find(_name);
  if (it != this->dataPtr->allNestedModels.end())
  {
    if (it->second)
      return it->second->modelSDF;
  }
  return NULL;
}

/////////////////////////////////////////////////
void ModelCreator::AddLink(const EntityType _type)
{
  if (!this->dataPtr->previewVisual)
  {
    this->Reset();
  }

  this->Stop();

  this->dataPtr->addEntityType = _type;
  if (_type != ENTITY_NONE)
  {
    LinkData *linkData = this->AddShape(_type);
    linkData->SetIsPreview(true);
    if (linkData)
      this->dataPtr->mouseVisual = linkData->linkVisual;
  }
}

/////////////////////////////////////////////////
void ModelCreator::Stop()
{
  if (this->dataPtr->addEntityType != ENTITY_NONE && this->dataPtr->mouseVisual)
  {
    this->RemoveEntity(this->dataPtr->mouseVisual->GetName());
    this->dataPtr->mouseVisual.reset();
    emit LinkAdded();
  }
  if (this->dataPtr->jointMaker)
    this->dataPtr->jointMaker->Stop();
}

/////////////////////////////////////////////////
void ModelCreator::OnDelete()
{
  if (this->dataPtr->inspectName.empty())
    return;

  this->OnDelete(this->dataPtr->inspectName);
  this->dataPtr->inspectName = "";
}

/////////////////////////////////////////////////
void ModelCreator::OnDelete(const std::string &_entity)
{
  // if it's a nestedModel
  auto nestedModel = this->dataPtr->allNestedModels.find(_entity);
  if (nestedModel != this->dataPtr->allNestedModels.end())
  {
    // Get data to use after the model has been deleted
    auto name = nestedModel->second->Name();
    auto sdf = nestedModel->second->modelSDF;
    auto scopedName = nestedModel->second->modelVisual->GetName();

    this->RemoveNestedModelImpl(_entity);

    auto cmd = MEUserCmdManager::Instance()->NewCmd(
        "Delete [" + name + "]",
        MEUserCmd::DELETING_NESTED_MODEL);
    cmd->SetSDF(sdf);
    cmd->SetScopedName(scopedName);

    return;
  }

  // if it's a link
  auto link = this->dataPtr->allLinks.find(_entity);
  if (link != this->dataPtr->allLinks.end())
  {
    // First delete joints
//    if (this->dataPtr->jointMaker)
//      this->dataPtr->jointMaker->RemoveJointsByLink(_entity);

    // Then register command
    auto cmd = MEUserCmdManager::Instance()->NewCmd(
        "Delete [" + link->second->Name() + "]", MEUserCmd::DELETING_LINK);
    cmd->SetSDF(this->GenerateLinkSDF(link->second));
    cmd->SetScopedName(link->second->linkVisual->GetName());

    // Then delete link
    this->RemoveLinkImpl(_entity);
    return;
  }
}

/////////////////////////////////////////////////
void ModelCreator::RemoveEntity(const std::string &_entity)
{
  std::lock_guard<std::recursive_mutex> lock(this->dataPtr->updateMutex);

  // if it's a nestedModel
  if (this->dataPtr->allNestedModels.find(_entity) !=
      this->dataPtr->allNestedModels.end())
  {
    this->RemoveNestedModelImpl(_entity);
    return;
  }

  // if it's a link
  if (this->dataPtr->allLinks.find(_entity) != this->dataPtr->allLinks.end())
  {
    if (this->dataPtr->jointMaker)
      this->dataPtr->jointMaker->RemoveJointsByLink(_entity);
    this->RemoveLinkImpl(_entity);
    return;
  }

  // if it's a visual
  rendering::VisualPtr vis =
      gui::get_active_camera()->GetScene()->GetVisual(_entity);
  if (vis)
  {
    rendering::VisualPtr parentLink = vis->GetParent();
    std::string parentLinkName = parentLink->GetName();

    if (this->dataPtr->allLinks.find(parentLinkName) !=
        this->dataPtr->allLinks.end())
    {
      // remove the parent link if it's the only child
      if (parentLink->GetChildCount() == 1)
      {
        if (this->dataPtr->jointMaker)
          this->dataPtr->jointMaker->RemoveJointsByLink(parentLink->GetName());
        this->RemoveLinkImpl(parentLink->GetName());
        return;
      }
    }
  }
}

/////////////////////////////////////////////////
void ModelCreator::OnRemoveModelPlugin(const QString &_name)
{
  // User request from right-click menu
  auto it = this->dataPtr->allModelPlugins.find(_name.toStdString());
  if (it != this->dataPtr->allModelPlugins.end())
  {
    auto cmd = MEUserCmdManager::Instance()->NewCmd(
        "Delete plugin [" + _name.toStdString() + "]",
        MEUserCmd::DELETING_MODEL_PLUGIN);
    cmd->SetSDF(it->second->modelPluginSDF);
    cmd->SetScopedName(_name.toStdString());
  }

  this->RemoveModelPlugin(_name.toStdString());
}

/////////////////////////////////////////////////
void ModelCreator::RemoveModelPlugin(const std::string &_name)
{
  std::lock_guard<std::recursive_mutex> lock(this->dataPtr->updateMutex);

  auto it = this->dataPtr->allModelPlugins.find(_name);
  if (it == this->dataPtr->allModelPlugins.end())
  {
    return;
  }

  ModelPluginData *data = it->second;

  // Remove from map
  this->dataPtr->allModelPlugins.erase(_name);
  delete data;

  // Notify removal
  gui::model::Events::modelPluginRemoved(_name);
}

/////////////////////////////////////////////////
bool ModelCreator::OnKeyPress(const common::KeyEvent &_event)
{
  if (_event.key == Qt::Key_Escape)
  {
    this->Stop();
  }
  else if (_event.key == Qt::Key_Delete)
  {
    for (const auto &nestedModelVis : this->dataPtr->selectedNestedModels)
    {
      this->OnDelete(nestedModelVis->GetName());
    }

    for (const auto &linkVis : this->dataPtr->selectedLinks)
    {
      this->OnDelete(linkVis->GetName());
    }

    for (const auto &plugin : this->dataPtr->selectedModelPlugins)
    {
      this->RemoveModelPlugin(plugin);
    }
    this->DeselectAll();
  }
  else if (_event.control)
  {
    if (_event.key == Qt::Key_C && _event.control)
    {
      g_copyAct->trigger();
      return true;
    }
    if (_event.key == Qt::Key_V && _event.control)
    {
      g_pasteAct->trigger();
      return true;
    }
  }
  return false;
}

/////////////////////////////////////////////////
bool ModelCreator::OnMousePress(const common::MouseEvent &_event)
{
  rendering::UserCameraPtr userCamera = gui::get_active_camera();
  if (!userCamera)
    return false;

  if (this->dataPtr->jointMaker->State() != JointMaker::JOINT_NONE)
  {
    userCamera->HandleMouseEvent(_event);
    return true;
  }

  rendering::VisualPtr vis = userCamera->GetVisual(_event.Pos());
  if (vis)
  {
    if (!vis->IsPlane() && gui::get_entity_id(vis->GetRootVisual()->GetName()))
    {
      // Handle snap from GLWidget
      if (g_snapAct->isChecked())
        return false;

      // Prevent interaction with other models, send event only to
      // user camera
      userCamera->HandleMouseEvent(_event);
      return true;
    }
  }
  return false;
}

/////////////////////////////////////////////////
bool ModelCreator::OnMouseRelease(const common::MouseEvent &_event)
{
  rendering::UserCameraPtr userCamera = gui::get_active_camera();
  if (!userCamera)
    return false;

  std::lock_guard<std::recursive_mutex> lock(this->dataPtr->updateMutex);

  // case when inserting an entity
  if (this->dataPtr->mouseVisual)
  {
    if (_event.Button() == common::MouseEvent::RIGHT)
      return true;

    // set the link data pose
    auto linkIt = this->dataPtr->allLinks.find(
        this->dataPtr->mouseVisual->GetName());
    if (linkIt != this->dataPtr->allLinks.end())
    {
      LinkData *link = linkIt->second;
      link->SetPose(this->dataPtr->mouseVisual->GetWorldPose().Ign() -
          this->dataPtr->modelPose);
      link->SetIsPreview(false);
      gui::model::Events::linkInserted(this->dataPtr->mouseVisual->GetName());

      auto cmd = MEUserCmdManager::Instance()->NewCmd(
          "Insert [" + link->Name() + "]",
          MEUserCmd::INSERTING_LINK);
      cmd->SetSDF(this->GenerateLinkSDF(link));
      cmd->SetScopedName(link->linkVisual->GetName());
    }
    else
    {
      auto modelIt = this->dataPtr->allNestedModels.find(
          this->dataPtr->mouseVisual->GetName());
      if (modelIt != this->dataPtr->allNestedModels.end())
      {
        NestedModelData *modelData = modelIt->second;
        modelData->SetPose(this->dataPtr->mouseVisual->GetWorldPose().Ign() -
            this->dataPtr->modelPose);

        this->EmitNestedModelInsertedEvent(this->dataPtr->mouseVisual);

        auto cmd = MEUserCmdManager::Instance()->NewCmd(
            "Insert [" + modelData->Name() + "]",
            MEUserCmd::INSERTING_NESTED_MODEL);
        cmd->SetSDF(modelData->modelSDF);
        cmd->SetScopedName(modelData->modelVisual->GetName());
      }
    }

    // reset and return
    emit LinkAdded();
    this->dataPtr->mouseVisual.reset();
    this->AddLink(ENTITY_NONE);
    return true;
  }

  // Set all links as not preview
  for (auto &link : this->dataPtr->allLinks)
    link.second->SetIsPreview(false);

  // End moving links
  for (auto link : this->dataPtr->linkPoseUpdate)
  {
    auto cmd = MEUserCmdManager::Instance()->NewCmd(
        "Move " + link.first->Name(), MEUserCmd::MOVING_LINK);
    cmd->SetScopedName(link.first->linkVisual->GetName());
    cmd->SetPoseChange(link.first->Pose(), link.second);

    link.first->SetPose(link.second);
  }
  if (!this->dataPtr->linkPoseUpdate.empty())
    this->ModelChanged();
  this->dataPtr->linkPoseUpdate.clear();

  // End moving nested models
  for (auto nestedModel : this->dataPtr->nestedModelPoseUpdate)
  {
    auto cmd = MEUserCmdManager::Instance()->NewCmd(
        "Move " + nestedModel.first->Name(), MEUserCmd::MOVING_NESTED_MODEL);
    cmd->SetScopedName(nestedModel.first->modelVisual->GetName());
    cmd->SetPoseChange(nestedModel.first->Pose(), nestedModel.second);

    nestedModel.first->SetPose(nestedModel.second);
  }
  if (!this->dataPtr->nestedModelPoseUpdate.empty())
    this->ModelChanged();
  this->dataPtr->nestedModelPoseUpdate.clear();

  // End scaling links
  for (auto link : this->dataPtr->linkScaleUpdate)
  {
    auto cmd = MEUserCmdManager::Instance()->NewCmd(
        "Scale " + link.first->Name(), MEUserCmd::SCALING_LINK);
    cmd->SetScopedName(link.first->linkVisual->GetName());
    cmd->SetScaleChange(link.first->Scale(), link.second);

    link.first->SetScale(link.second);
  }
  if (!this->dataPtr->linkScaleUpdate.empty())
    this->ModelChanged();
  this->dataPtr->linkScaleUpdate.clear();

  // mouse selection and context menu events
  rendering::VisualPtr vis = userCamera->GetVisual(_event.Pos());
  if (vis)
  {
    rendering::VisualPtr topLevelVis = vis->GetNthAncestor(2);
    if (!topLevelVis)
      return false;

    bool isLink = this->dataPtr->allLinks.find(topLevelVis->GetName()) !=
        this->dataPtr->allLinks.end();
    bool isNestedModel = this->dataPtr->allNestedModels.find(
        topLevelVis->GetName()) != this->dataPtr->allNestedModels.end();

    bool isSelectedLink = false;
    bool isSelectedNestedModel = false;
    if (isLink)
    {
      isSelectedLink = std::find(this->dataPtr->selectedLinks.begin(),
          this->dataPtr->selectedLinks.end(), topLevelVis) !=
          this->dataPtr->selectedLinks.end();
    }
    else if (isNestedModel)
    {
      isSelectedNestedModel = std::find(
          this->dataPtr->selectedNestedModels.begin(),
          this->dataPtr->selectedNestedModels.end(), topLevelVis) !=
          this->dataPtr->selectedNestedModels.end();
    }

    // trigger context menu on right click
    if (_event.Button() == common::MouseEvent::RIGHT)
    {
      if (!isLink && !isNestedModel)
      {
        // user clicked on background model
        this->DeselectAll();
        QMenu menu;
        menu.addAction(g_copyAct);
        menu.addAction(g_pasteAct);
        menu.exec(QCursor::pos());
        return true;
      }

      // if right clicked on entity that's not previously selected then
      // select it
      if (!isSelectedLink && !isSelectedNestedModel)
      {
        this->DeselectAll();
        this->SetSelected(topLevelVis, true);
      }

      this->dataPtr->inspectName = topLevelVis->GetName();

      this->ShowContextMenu(this->dataPtr->inspectName);
      return true;
    }

    // Handle snap from GLWidget
    if (g_snapAct->isChecked())
      return false;

    // Is link / nested model
    if (isLink || isNestedModel)
    {
      // Not in multi-selection mode.
      if (!(QApplication::keyboardModifiers() & Qt::ControlModifier))
      {
        this->DeselectAll();
        this->SetSelected(topLevelVis, true);
      }
      // Multi-selection mode
      else
      {
        this->DeselectAllModelPlugins();

        // Highlight and select clicked entity if not already selected
        if (!isSelectedLink && !isSelectedNestedModel)
        {
          this->SetSelected(topLevelVis, true);
        }
        // Deselect if already selected
        else
        {
          this->SetSelected(topLevelVis, false);
        }
      }

      if (this->dataPtr->manipMode == "translate" ||
          this->dataPtr->manipMode == "rotate" ||
          this->dataPtr->manipMode == "scale")
      {
        this->OnManipMode(this->dataPtr->manipMode);
      }

      return true;
    }
    // Not link or nested model
    else
    {
      this->DeselectAll();

      g_alignAct->setEnabled(false);
      g_copyAct->setEnabled(!this->dataPtr->selectedLinks.empty() ||
          !this->dataPtr->selectedNestedModels.empty());

      if (!vis->IsPlane())
        return true;
    }
  }
  return false;
}

/////////////////////////////////////////////////
void ModelCreator::EmitNestedModelInsertedEvent(
    const rendering::VisualPtr &_vis) const
{
  if (!_vis)
    return;

  auto modelIt = this->dataPtr->allNestedModels.find(_vis->GetName());
  if (modelIt != this->dataPtr->allNestedModels.end())
    gui::model::Events::nestedModelInserted(_vis->GetName());
  else
    return;

  for (unsigned int i = 0; i < _vis->GetChildCount(); ++i)
    this->EmitNestedModelInsertedEvent(_vis->GetChild(i));
}

/////////////////////////////////////////////////
void ModelCreator::ShowContextMenu(const std::string &_entity)
{
  QMenu menu;
  menu.setObjectName("ModelEditorContextMenu");
  auto linkIt = this->dataPtr->allLinks.find(_entity);
  bool isLink = linkIt != this->dataPtr->allLinks.end();
  bool isNestedModel = false;
  if (!isLink)
  {
    auto nestedModelIt = this->dataPtr->allNestedModels.find(_entity);
    isNestedModel = nestedModelIt != this->dataPtr->allNestedModels.end();
    if (!isNestedModel)
      return;
  }
  else
  {
    // disable interacting with nested links for now
    LinkData *link = linkIt->second;
    if (link->nested)
      return;
  }

  // context menu for link
  if (isLink)
  {
    this->dataPtr->inspectName = _entity;
    if (this->dataPtr->inspectAct)
    {
      menu.addAction(this->dataPtr->inspectAct);

      menu.addSeparator();
      menu.addAction(g_copyAct);
      menu.addAction(g_pasteAct);
      menu.addSeparator();

      if (this->dataPtr->jointMaker)
      {
        std::vector<JointData *> joints =
            this->dataPtr->jointMaker->JointDataByLink(_entity);

        if (!joints.empty())
        {
          QMenu *jointsMenu = menu.addMenu(tr("Open Joint Inspector"));

          for (auto joint : joints)
          {
            QAction *jointAct = new QAction(tr(joint->name.c_str()), this);
            connect(jointAct, SIGNAL(triggered()), joint,
                SLOT(OnOpenInspector()));
            jointsMenu->addAction(jointAct);
          }
        }
      }
    }
  }
  // context menu for nested model
  else if (isNestedModel)
  {
    this->dataPtr->inspectName = _entity;
    menu.addAction(g_copyAct);
    menu.addAction(g_pasteAct);
  }

  // delete menu option
  menu.addSeparator();

  QAction *deleteAct = new QAction(tr("Delete"), this);
  connect(deleteAct, SIGNAL(triggered()), this, SLOT(OnDelete()));
  menu.addAction(deleteAct);

  menu.exec(QCursor::pos());
}

/////////////////////////////////////////////////
void ModelCreator::ShowModelPluginContextMenu(const std::string &_name)
{
  auto it = this->dataPtr->allModelPlugins.find(_name);
  if (it == this->dataPtr->allModelPlugins.end())
    return;

  // Open inspector
  QAction *inspectorAct = new QAction(tr("Open Model Plugin Inspector"), this);

  // Map signals to pass argument
  QSignalMapper *inspectorMapper = new QSignalMapper(this);

  connect(inspectorAct, SIGNAL(triggered()), inspectorMapper, SLOT(map()));
  inspectorMapper->setMapping(inspectorAct, QString::fromStdString(_name));

  connect(inspectorMapper, SIGNAL(mapped(QString)), this,
      SLOT(OnOpenModelPluginInspector(QString)));

  // Delete
  QAction *deleteAct = new QAction(tr("Delete"), this);

  // Map signals to pass argument
  QSignalMapper *deleteMapper = new QSignalMapper(this);

  connect(deleteAct, SIGNAL(triggered()), deleteMapper, SLOT(map()));
  deleteMapper->setMapping(deleteAct, QString::fromStdString(_name));

  connect(deleteMapper, SIGNAL(mapped(QString)), this,
      SLOT(OnRemoveModelPlugin(QString)));

  // Menu
  QMenu menu;
  menu.addAction(inspectorAct);
  menu.addAction(deleteAct);

  menu.exec(QCursor::pos());
}

/////////////////////////////////////////////////
bool ModelCreator::OnMouseMove(const common::MouseEvent &_event)
{
  this->dataPtr->lastMouseEvent = _event;
  rendering::UserCameraPtr userCamera = gui::get_active_camera();
  if (!userCamera)
    return false;

  if (!this->dataPtr->mouseVisual)
  {
    rendering::VisualPtr vis = userCamera->GetVisual(_event.Pos());
    if (vis && !vis->IsPlane())
    {
      rendering::VisualPtr topLevelVis = vis->GetNthAncestor(2);
      if (!topLevelVis)
        return false;

      auto link = this->dataPtr->allLinks.find(topLevelVis->GetName());
      auto nestedModel =
          this->dataPtr->allNestedModels.find(topLevelVis->GetName());
      // Main window models always handled here
      if (link == this->dataPtr->allLinks.end() &&
          nestedModel == this->dataPtr->allNestedModels.end())
      {
        // Prevent highlighting for snapping
        if (this->dataPtr->manipMode == "snap" ||
            this->dataPtr->manipMode == "select" ||
            this->dataPtr->manipMode == "")
        {
          // Don't change cursor on hover
          QApplication::setOverrideCursor(QCursor(Qt::ArrowCursor));
          userCamera->HandleMouseEvent(_event);
        }
        // Allow ModelManipulator to work while dragging handle over this
        else if (_event.Dragging())
        {
          ModelManipulator::Instance()->OnMouseMoveEvent(_event);
        }
        return true;
      }
      // During RTS manipulation
      else if (_event.Dragging())
      {
        if (link != this->dataPtr->allLinks.end())
          link->second->SetIsPreview(true);
      }
    }
    return false;
  }

  auto pose = this->dataPtr->mouseVisual->GetWorldPose().Ign();
  pose.Pos() = ModelManipulator::GetMousePositionOnPlane(
      userCamera, _event).Ign();

  // there is a problem detecting control key from common::MouseEvent, so
  // check using Qt for now
  if ((QApplication::keyboardModifiers() & Qt::ControlModifier))
  {
    pose.Pos() = ModelManipulator::SnapPoint(pose.Pos()).Ign();
  }
  pose.Pos().Z(this->dataPtr->mouseVisual->GetWorldPose().Ign().Pos().Z());

  this->dataPtr->mouseVisual->SetWorldPose(pose);

  return true;
}

/////////////////////////////////////////////////
bool ModelCreator::OnMouseDoubleClick(const common::MouseEvent &_event)
{
  // open the link inspector on double click
  rendering::VisualPtr vis = gui::get_active_camera()->GetVisual(_event.Pos());
  if (!vis)
    return false;

  std::lock_guard<std::recursive_mutex> lock(this->dataPtr->updateMutex);

  auto it = this->dataPtr->allLinks.find(vis->GetParent()->GetName());
  if (it != this->dataPtr->allLinks.end())
  {
    this->OpenInspector(vis->GetParent()->GetName());
    return true;
  }

  return false;
}

/////////////////////////////////////////////////
void ModelCreator::OnOpenInspector()
{
  if (this->dataPtr->inspectName.empty())
    return;

  this->OpenInspector(this->dataPtr->inspectName);
  this->dataPtr->inspectName = "";
}

/////////////////////////////////////////////////
void ModelCreator::OpenInspector(const std::string &_name)
{
  std::lock_guard<std::recursive_mutex> lock(this->dataPtr->updateMutex);
  auto it = this->dataPtr->allLinks.find(_name);
  if (it == this->dataPtr->allLinks.end())
  {
    gzerr << "Link [" << _name << "] not found." << std::endl;
    return;
  }

  // disable interacting with nested links for now
  LinkData *link = it->second;
  if (link->nested)
    return;

  link->SetPose(link->linkVisual->GetWorldPose().Ign() -
      this->dataPtr->modelPose);
  link->UpdateConfig();
  link->inspector->Open();
}

/////////////////////////////////////////////////
void ModelCreator::OnCopy()
{
  if (!g_editModelAct->isChecked())
    return;

  if (this->dataPtr->selectedLinks.empty() &&
      this->dataPtr->selectedNestedModels.empty())
  {
    return;
  }

  this->dataPtr->copiedNames.clear();

  for (auto vis : this->dataPtr->selectedLinks)
  {
    this->dataPtr->copiedNames.push_back(vis->GetName());
  }
  for (auto vis : this->dataPtr->selectedNestedModels)
  {
    this->dataPtr->copiedNames.push_back(vis->GetName());
  }
  g_pasteAct->setEnabled(true);
}

/////////////////////////////////////////////////
void ModelCreator::OnPaste()
{
  if (this->dataPtr->copiedNames.empty() || !g_editModelAct->isChecked())
  {
    return;
  }

  std::lock_guard<std::recursive_mutex> lock(this->dataPtr->updateMutex);

  ignition::math::Pose3d clonePose;
  rendering::UserCameraPtr userCamera = gui::get_active_camera();
  if (userCamera)
  {
    ignition::math::Vector3d mousePosition =
        ModelManipulator::GetMousePositionOnPlane(
        userCamera, this->dataPtr->lastMouseEvent).Ign();
    clonePose.Pos().X() = mousePosition.X();
    clonePose.Pos().Y() = mousePosition.Y();
  }

  // For now, only copy the last selected (nested models come after)
  auto it = this->dataPtr->allLinks.find(this->dataPtr->copiedNames.back());
  // Copy a link
  if (it != this->dataPtr->allLinks.end())
  {
    LinkData *copiedLink = it->second;
    if (!copiedLink)
      return;

    this->Stop();
    this->DeselectAll();

    if (!this->dataPtr->previewVisual)
    {
      this->Reset();
    }

    // Propagate copied entity's Z position and rotation
    ignition::math::Pose3d copiedPose = copiedLink->Pose();
    clonePose.Pos().Z() = this->dataPtr->modelPose.Pos().Z() +
        copiedPose.Pos().Z();
    clonePose.Rot() = copiedPose.Rot();

    LinkData *clonedLink = this->CloneLink(it->first);
    clonedLink->linkVisual->SetWorldPose(clonePose);
    clonedLink->SetIsPreview(true);

    this->dataPtr->addEntityType = ENTITY_MESH;
    this->dataPtr->mouseVisual = clonedLink->linkVisual;
  }
  else
  {
    auto it2 = this->dataPtr->allNestedModels.find(
        this->dataPtr->copiedNames.back());
    if (it2 != this->dataPtr->allNestedModels.end())
    {
      NestedModelData *copiedNestedModel = it2->second;
      if (!copiedNestedModel)
        return;

      this->Stop();
      this->DeselectAll();

      if (!this->dataPtr->previewVisual)
      {
        this->Reset();
      }

      // Propagate copied entity's Z position and rotation
      ignition::math::Pose3d copiedPose = copiedNestedModel->Pose();
      clonePose.Pos().Z() = this->dataPtr->modelPose.Pos().Z() +
          copiedPose.Pos().Z();
      clonePose.Rot() = copiedPose.Rot();

      NestedModelData *clonedNestedModel = this->CloneNestedModel(it2->first);
      clonedNestedModel->modelVisual->SetWorldPose(clonePose);
      this->dataPtr->addEntityType = ENTITY_MODEL;
      this->dataPtr->mouseVisual = clonedNestedModel->modelVisual;
    }
  }
}

/////////////////////////////////////////////////
JointMaker *ModelCreator::JointMaker() const
{
  return this->dataPtr->jointMaker;
}

/////////////////////////////////////////////////
void ModelCreator::UpdateNestedModelSDF(sdf::ElementPtr /*_modelElem*/)
{
  return;

  // do we still need the code below?
  /* if (this->modelName == this->serverModelName)
    return;

  if (_modelElem->HasElement("joint"))
  {
    sdf::ElementPtr jointElem = _modelElem->GetElement("joint");
    while (jointElem)
    {
      sdf::ElementPtr parentElem = jointElem->GetElement("parent");
      std::string parentName = parentElem->Get<std::string>();
      sdf::ElementPtr childElem = jointElem->GetElement("child");
      std::string childName = childElem->Get<std::string>();

      if (this->serverModelName.empty())
      {
        parentName = this->modelName + "::" + parentName;
        childName = this->modelName + "::" + childName;
        parentElem->Set(parentName);
        childElem->Set(childName);
      }
      else
      {
        size_t pos = parentName.find("::");
        if (pos != std::string::npos &&
            parentName.substr(0, pos) == this->serverModelName)
        {
          parentName = this->modelName + parentName.substr(pos);
          parentElem->Set(parentName);
        }


        pos = childName.find("::");
        if (pos != std::string::npos &&
            parentName.substr(0, pos) == this->serverModelName)
        {
          childName = this->modelName + childName.substr(pos);
          childElem->Set(childName);
        }
      }

      jointElem = jointElem->GetNextElement("joint");
    }
  }

  if (_modelElem->HasElement("model"))
  {
    sdf::ElementPtr modelElem = _modelElem->GetElement("model");
    while (modelElem)
    {
      this->UpdateNestedModelSDF(modelElem);
      modelElem = modelElem->GetNextElement("model");
    }
  }*/
}

/////////////////////////////////////////////////
void ModelCreator::GenerateSDF()
{
  sdf::ElementPtr modelElem;

  this->dataPtr->modelSDF.reset(new sdf::SDF);
  this->dataPtr->modelSDF->SetFromString(ModelData::GetTemplateSDFString());

  modelElem = this->dataPtr->modelSDF->Root()->GetElement("model");

  modelElem->ClearElements();
  modelElem->GetAttribute("name")->Set(this->dataPtr->folderName);

  std::lock_guard<std::recursive_mutex> lock(this->dataPtr->updateMutex);

  if (this->dataPtr->serverModelName.empty())
  {
    // set center of all links and nested models to be origin
    /// \todo issue #1485 set a better origin other than the centroid
    ignition::math::Vector3d mid;
    int entityCount = 0;
    for (auto &linksIt : this->dataPtr->allLinks)
    {
      LinkData *link = linksIt.second;
      if (link->nested)
        continue;
      mid += link->Pose().Pos();
      entityCount++;
    }
    for (auto &nestedModelsIt : this->dataPtr->allNestedModels)
    {
      NestedModelData *modelData = nestedModelsIt.second;

      // get only top level nested models
      if (modelData->Depth() != 2)
        continue;

      mid += modelData->Pose().Pos();
      entityCount++;
    }

    // Put the origin in the ground so when the model is inserted it is fully
    // above ground.
    // TODO set a better origin other than the centroid
    mid.Z(0);

    if (!(this->dataPtr->allLinks.empty() &&
          this->dataPtr->allNestedModels.empty()))
    {
      mid /= entityCount;
    }

    this->dataPtr->modelPose.Pos() = mid;
  }

  // Update poses in case they changed
  for (auto &linksIt : this->dataPtr->allLinks)
  {
    LinkData *link = linksIt.second;
    if (link->nested)
      continue;
    ignition::math::Pose3d linkPose =
        link->linkVisual->GetWorldPose().Ign() - this->dataPtr->modelPose;
    link->SetPose(linkPose);
    link->linkVisual->SetPose(linkPose);
  }
  for (auto &nestedModelsIt : this->dataPtr->allNestedModels)
  {
    NestedModelData *modelData = nestedModelsIt.second;

    if (!modelData->modelVisual)
      continue;

    // get only top level nested models
    if (modelData->Depth() != 2)
      continue;

    ignition::math::Pose3d nestedModelPose =
        modelData->modelVisual->GetWorldPose().Ign() - this->dataPtr->modelPose;
    modelData->SetPose(nestedModelPose);
    modelData->modelVisual->SetPose(nestedModelPose);
  }

  // generate canonical link sdf first.
  // TODO: Model with no links and only nested models
  if (!this->dataPtr->canonicalLink.empty())
  {
    auto canonical = this->dataPtr->allLinks.find(this->dataPtr->canonicalLink);
    if (canonical != this->dataPtr->allLinks.end())
    {
      LinkData *link = canonical->second;
      if (!link->nested)
      {
        link->UpdateConfig();
        sdf::ElementPtr newLinkElem = this->GenerateLinkSDF(link);
        modelElem->InsertElement(newLinkElem);
      }
    }
  }

  // loop through rest of all links and generate sdf
  for (auto &linksIt : this->dataPtr->allLinks)
  {
    LinkData *link = linksIt.second;

    if (linksIt.first == this->dataPtr->canonicalLink || link->nested)
      continue;

    link->UpdateConfig();

    sdf::ElementPtr newLinkElem = this->GenerateLinkSDF(link);
    modelElem->InsertElement(newLinkElem);
  }

  // generate canonical model sdf first.
  if (!this->dataPtr->canonicalModel.empty())
  {
    auto canonical = this->dataPtr->allNestedModels.find(
        this->dataPtr->canonicalModel);
    if (canonical != this->dataPtr->allNestedModels.end())
    {
      NestedModelData *nestedModelData = canonical->second;
      // TODO do we need this update call below?
      // this->UpdateNestedModelSDF(nestedModelData->modelSDF);
      modelElem->InsertElement(nestedModelData->modelSDF);
    }
  }

  // loop through rest of all nested models and add sdf
  for (auto &nestedModelsIt : this->dataPtr->allNestedModels)
  {
    NestedModelData *nestedModelData = nestedModelsIt.second;
    if (nestedModelsIt.first == this->dataPtr->canonicalModel ||
        nestedModelData->Depth() != 2)
      continue;
    // TODO do we need this update call below?
    // this->UpdateNestedModelSDF(nestedModelData->modelSDF);
    modelElem->InsertElement(nestedModelData->modelSDF);
  }

  // Add joint sdf elements
  this->dataPtr->jointMaker->GenerateSDF();
  sdf::ElementPtr jointsElem = this->dataPtr->jointMaker->SDF();

  sdf::ElementPtr jointElem;
  if (jointsElem->HasElement("joint"))
    jointElem = jointsElem->GetElement("joint");
  while (jointElem)
  {
    modelElem->InsertElement(jointElem->Clone());
    jointElem = jointElem->GetNextElement("joint");
  }

  // Model settings
  modelElem->GetElement("static")->Set(this->dataPtr->isStatic);
  modelElem->GetElement("allow_auto_disable")->Set(this->dataPtr->autoDisable);

  // Add plugin elements
  for (auto modelPlugin : this->dataPtr->allModelPlugins)
    modelElem->InsertElement(modelPlugin.second->modelPluginSDF->Clone());

  // update root visual pose at the end after link, model, joint visuals
  this->dataPtr->previewVisual->SetWorldPose(this->dataPtr->modelPose);


/*  // If we're editing an existing model, copy the original plugin sdf elements
  // since we are not generating them.
  if (this->serverModelSDF)
  {
    if (this->serverModelSDF->HasElement("plugin"))
    {
      sdf::ElementPtr pluginElem = this->serverModelSDF->GetElement("plugin");
      while (pluginElem)
      {
        modelElem->InsertElement(pluginElem->Clone());
        pluginElem = pluginElem->GetNextElement("plugin");
      }
    }
  }*/

  // Append custom SDF - only plugins for now
  sdf::ElementPtr pluginElem;
  if (this->dataPtr->sdfToAppend->HasElement("plugin"))
     pluginElem = this->dataPtr->sdfToAppend->GetElement("plugin");
  while (pluginElem)
  {
    modelElem->InsertElement(pluginElem->Clone());
    pluginElem = pluginElem->GetNextElement("plugin");
  }
}

/////////////////////////////////////////////////
const sdf::ElementPtr ModelCreator::GetSDFToAppend()
{
  return this->dataPtr->sdfToAppend;
}

/////////////////////////////////////////////////
sdf::ElementPtr ModelCreator::GenerateLinkSDF(LinkData *_link)
{
  std::stringstream visualNameStream;
  std::stringstream collisionNameStream;
  visualNameStream.str("");
  collisionNameStream.str("");

  sdf::ElementPtr newLinkElem = _link->linkSDF->Clone();
  newLinkElem->GetElement("pose")->Set(_link->Pose());

  // visuals
  for (auto const &it : _link->visuals)
  {
    rendering::VisualPtr visual = it.first;
    msgs::Visual visualMsg = it.second;
    sdf::ElementPtr visualElem = visual->GetSDF()->Clone();

    visualElem->GetElement("transparency")->Set<double>(
        visualMsg.transparency());
    newLinkElem->InsertElement(visualElem);
  }

  // collisions
  for (auto const &colIt : _link->collisions)
  {
    sdf::ElementPtr collisionElem = msgs::CollisionToSDF(colIt.second);
    newLinkElem->InsertElement(collisionElem);
  }
  return newLinkElem;
}

/////////////////////////////////////////////////
void ModelCreator::OnAlignMode(const std::string &_axis,
    const std::string &_config, const std::string &_target, const bool _preview,
    const bool _inverted)
{
  ModelAlign::Instance()->AlignVisuals(this->dataPtr->selectedLinks, _axis,
      _config, _target, !_preview, _inverted);

  if (_preview)
    return;

  // Register user commands
  auto count = this->dataPtr->selectedLinks.size();
  for (unsigned int i = 0; i < count; ++i)
  {
    // Target didn't move
    if ((_target == "first" && i == 0) || (_target == "last" && i == count - 1))
      continue;

    auto link = this->dataPtr->allLinks.find(
        this->dataPtr->selectedLinks[i]->GetName());
    if (link != this->dataPtr->allLinks.end())
    {
      auto newPose = link->second->linkVisual->GetPose().Ign();

      auto cmd = MEUserCmdManager::Instance()->NewCmd(
          "Move " + link->second->Name(), MEUserCmd::MOVING_LINK);
      cmd->SetScopedName(link->second->linkVisual->GetName());
      cmd->SetPoseChange(link->second->Pose(), newPose);

      link->second->SetPose(newPose);
      this->ModelChanged();
    }
  }
}

/////////////////////////////////////////////////
void ModelCreator::DeselectAll()
{
  this->DeselectAllLinks();
  this->DeselectAllNestedModels();
  this->DeselectAllModelPlugins();
}

/////////////////////////////////////////////////
void ModelCreator::DeselectAllLinks()
{
  while (!this->dataPtr->selectedLinks.empty())
  {
    rendering::VisualPtr vis = this->dataPtr->selectedLinks[0];

    vis->SetHighlighted(false);
    this->dataPtr->selectedLinks.erase(this->dataPtr->selectedLinks.begin());
    model::Events::setSelectedLink(vis->GetName(), false);
  }
  this->dataPtr->selectedLinks.clear();
}

/////////////////////////////////////////////////
void ModelCreator::DeselectAllNestedModels()
{
  while (!this->dataPtr->selectedNestedModels.empty())
  {
    rendering::VisualPtr vis = this->dataPtr->selectedNestedModels[0];
    vis->SetHighlighted(false);
    this->dataPtr->selectedNestedModels.erase(
        this->dataPtr->selectedNestedModels.begin());
    model::Events::setSelectedLink(vis->GetName(), false);
  }
  this->dataPtr->selectedNestedModels.clear();
}

/////////////////////////////////////////////////
void ModelCreator::DeselectAllModelPlugins()
{
  while (!this->dataPtr->selectedModelPlugins.empty())
  {
    auto it = this->dataPtr->selectedModelPlugins.begin();
    std::string name = this->dataPtr->selectedModelPlugins[0];
    this->dataPtr->selectedModelPlugins.erase(it);
    model::Events::setSelectedModelPlugin(name, false);
  }
}

/////////////////////////////////////////////////
void ModelCreator::SetSelected(const std::string &_name, const bool _selected)
{
  auto it = this->dataPtr->allLinks.find(_name);
  if (it != this->dataPtr->allLinks.end())
  {
    this->SetSelected((*it).second->linkVisual, _selected);
  }
  else
  {
    auto itNestedModel = this->dataPtr->allNestedModels.find(_name);
    if (itNestedModel != this->dataPtr->allNestedModels.end())
      this->SetSelected((*itNestedModel).second->modelVisual, _selected);
  }
}

/////////////////////////////////////////////////
void ModelCreator::SetSelected(const rendering::VisualPtr &_entityVis,
    const bool _selected)
{
  if (!_entityVis)
    return;

  _entityVis->SetHighlighted(_selected);

  auto itLink = this->dataPtr->allLinks.find(_entityVis->GetName());
  auto itNestedModel =
      this->dataPtr->allNestedModels.find(_entityVis->GetName());

  auto itLinkSelected = std::find(this->dataPtr->selectedLinks.begin(),
      this->dataPtr->selectedLinks.end(), _entityVis);
  auto itNestedModelSelected = std::find(
      this->dataPtr->selectedNestedModels.begin(),
      this->dataPtr->selectedNestedModels.end(), _entityVis);

  if (_selected)
  {
    if (itLink != this->dataPtr->allLinks.end() &&
        itLinkSelected == this->dataPtr->selectedLinks.end())
    {
      this->dataPtr->selectedLinks.push_back(_entityVis);
      model::Events::setSelectedLink(_entityVis->GetName(), _selected);
    }
    else if (itNestedModel != this->dataPtr->allNestedModels.end() &&
             itNestedModelSelected == this->dataPtr->selectedNestedModels.end())
    {
      this->dataPtr->selectedNestedModels.push_back(_entityVis);
      model::Events::setSelectedLink(_entityVis->GetName(), _selected);
    }
  }
  else
  {
    if (itLink != this->dataPtr->allLinks.end() &&
        itLinkSelected != this->dataPtr->selectedLinks.end())
    {
      this->dataPtr->selectedLinks.erase(itLinkSelected);
      model::Events::setSelectedLink(_entityVis->GetName(), _selected);
    }
    else if (itNestedModel != this->dataPtr->allNestedModels.end() &&
             itNestedModelSelected != this->dataPtr->selectedNestedModels.end())
    {
      this->dataPtr->selectedNestedModels.erase(itNestedModelSelected);
      model::Events::setSelectedLink(_entityVis->GetName(), _selected);
    }
  }

  g_copyAct->setEnabled(this->dataPtr->selectedLinks.size() +
      this->dataPtr->selectedNestedModels.size() > 0u);
  g_alignAct->setEnabled(this->dataPtr->selectedLinks.size() +
      this->dataPtr->selectedNestedModels.size() > 1u);
}

/////////////////////////////////////////////////
void ModelCreator::OnManipMode(const std::string &_mode)
{
  if (!this->dataPtr->active)
    return;

  this->dataPtr->manipMode = _mode;

  if (!this->dataPtr->selectedLinks.empty())
  {
    ModelManipulator::Instance()->SetAttachedVisual(
        this->dataPtr->selectedLinks.back());
  }
  else if (!this->dataPtr->selectedNestedModels.empty())
  {
    ModelManipulator::Instance()->SetAttachedVisual(
        this->dataPtr->selectedNestedModels.back());
  }

  ModelManipulator::Instance()->SetManipulationMode(_mode);
  ModelSnap::Instance()->Reset();

  // deselect 0 to n-1 models.
  if (!this->dataPtr->selectedLinks.empty())
  {
    rendering::VisualPtr link =
        this->dataPtr->selectedLinks[this->dataPtr->selectedLinks.size()-1];
    this->DeselectAll();
    this->SetSelected(link, true);
  }
  else if (!this->dataPtr->selectedNestedModels.empty())
  {
    rendering::VisualPtr nestedModel =
        this->dataPtr->selectedNestedModels[
        this->dataPtr->selectedNestedModels.size()-1];
    this->DeselectAll();
    this->SetSelected(nestedModel, true);
  }
}

/////////////////////////////////////////////////
void ModelCreator::OnSetSelectedEntity(const std::string &/*_name*/,
    const std::string &/*_mode*/)
{
  this->DeselectAll();
}

/////////////////////////////////////////////////
void ModelCreator::OnSetSelectedLink(const std::string &_name,
    const bool _selected)
{
  this->SetSelected(_name, _selected);
}

/////////////////////////////////////////////////
void ModelCreator::OnSetSelectedModelPlugin(const std::string &_name,
    const bool _selected)
{
  auto plugin = this->dataPtr->allModelPlugins.find(_name);
  if (plugin == this->dataPtr->allModelPlugins.end())
    return;

  auto it = std::find(this->dataPtr->selectedModelPlugins.begin(),
      this->dataPtr->selectedModelPlugins.end(), _name);
  if (_selected && it == this->dataPtr->selectedModelPlugins.end())
  {
    this->dataPtr->selectedModelPlugins.push_back(_name);
  }
  else if (!_selected && it != this->dataPtr->selectedModelPlugins.end())
  {
    this->dataPtr->selectedModelPlugins.erase(it);
  }
}

/////////////////////////////////////////////////
void ModelCreator::ModelChanged()
{
  if (this->dataPtr->currentSaveState != NEVER_SAVED)
    this->dataPtr->currentSaveState = UNSAVED_CHANGES;
}

/////////////////////////////////////////////////
void ModelCreator::OnEntityScaleChanged(const std::string &_name,
  const gazebo::math::Vector3 &_scale)
{
  std::lock_guard<std::recursive_mutex> lock(this->dataPtr->updateMutex);
  for (auto linksIt : this->dataPtr->allLinks)
  {
    std::string linkName;
    size_t pos = _name.rfind("::");
    if (pos != std::string::npos)
      linkName = _name.substr(0, pos);
    if (_name == linksIt.first || linkName == linksIt.first)
    {
      this->dataPtr->linkScaleUpdate[linksIt.second] = _scale.Ign();
      break;
    }
  }
}

/////////////////////////////////////////////////
void ModelCreator::OnEntityMoved(const std::string &_name,
  const ignition::math::Pose3d &_pose, const bool _finalPoseForSure)
{
  std::lock_guard<std::recursive_mutex> lock(this->dataPtr->updateMutex);
  for (auto linksIt : this->dataPtr->allLinks)
  {
    std::string linkName;
    size_t pos = _name.rfind("::");
    if (pos != std::string::npos)
      linkName = _name.substr(0, pos);
    if (_name == linksIt.first || linkName == linksIt.first)
    {
      // Register user command
      if (_finalPoseForSure)
      {
        auto cmd = MEUserCmdManager::Instance()->NewCmd(
            "Move " + linksIt.second->Name(), MEUserCmd::MOVING_LINK);
        cmd->SetScopedName(linksIt.second->linkVisual->GetName());
        cmd->SetPoseChange(linksIt.second->Pose(), _pose);

        linksIt.second->SetPose(_pose);
        this->ModelChanged();
      }
      // Only register command on MouseRelease
      else
      {
        this->dataPtr->linkPoseUpdate[linksIt.second] = _pose;
      }

      break;
    }
  }
  for (auto nestedModelsIt : this->dataPtr->allNestedModels)
  {
    std::string nestedModelName;
    size_t pos = _name.rfind("::");
    if (pos != std::string::npos)
      nestedModelName = _name.substr(0, pos);
    if (_name == nestedModelsIt.first ||
        nestedModelName == nestedModelsIt.first)
    {
      this->dataPtr->nestedModelPoseUpdate[nestedModelsIt.second] = _pose;
      break;
    }
  }
}

/////////////////////////////////////////////////
void ModelCreator::SetModelVisible(const std::string &_name,
    const bool _visible)
{
  rendering::ScenePtr scene = gui::get_active_camera()->GetScene();
  rendering::VisualPtr visual = scene->GetVisual(_name);
  if (!visual)
    return;

  this->SetModelVisible(visual, _visible);

  if (_visible)
    visual->SetHighlighted(false);
}

/////////////////////////////////////////////////
void ModelCreator::SetModelVisible(const rendering::VisualPtr &_visual,
    const bool _visible)
{
  if (!_visual)
    return;

  for (unsigned int i = 0; i < _visual->GetChildCount(); ++i)
    this->SetModelVisible(_visual->GetChild(i), _visible);

  if (!_visible)
  {
    // store original visibility
    this->dataPtr->serverModelVisible[_visual->GetId()] = _visual->GetVisible();
    _visual->SetVisible(_visible);
  }
  else
  {
    // restore original visibility
    auto it = this->dataPtr->serverModelVisible.find(_visual->GetId());
    if (it != this->dataPtr->serverModelVisible.end())
    {
      _visual->SetVisible(it->second, false);
    }
  }
}

/////////////////////////////////////////////////
ModelCreator::SaveState ModelCreator::CurrentSaveState() const
{
  return this->dataPtr->currentSaveState;
}

/////////////////////////////////////////////////
void ModelCreator::AppendPluginElement(const std::string &_name,
    const std::string &_filename, sdf::ElementPtr _sdfElement)
{
  // Insert into existing plugin element
  sdf::ElementPtr pluginElem;
  if (this->dataPtr->sdfToAppend->HasElement("plugin"))
     pluginElem = this->dataPtr->sdfToAppend->GetElement("plugin");
  while (pluginElem)
  {
    if (pluginElem->Get<std::string>("name") == _name)
    {
      pluginElem->InsertElement(_sdfElement);
      _sdfElement->SetParent(pluginElem);
      return;
    }
    pluginElem = pluginElem->GetNextElement("plugin");
  }

  // Create new plugin element
  pluginElem.reset(new sdf::Element);
  pluginElem->SetName("plugin");
  pluginElem->AddAttribute("name", "string", _name, "0", "name");
  pluginElem->AddAttribute("filename", "string", _filename, "0", "filename");

  pluginElem->InsertElement(_sdfElement);
  _sdfElement->SetParent(pluginElem);

  this->dataPtr->sdfToAppend->InsertElement(pluginElem);
}

/////////////////////////////////////////////////
static bool MatchingElement(sdf::ElementPtr e1, sdf::ElementPtr e2)
{
  // No comparison operator in Element class?
  return e1->ToString("") == e2->ToString("");
}

/////////////////////////////////////////////////
void ModelCreator::RemovePluginElement(const std::string &_name,
    const std::string &_filename, sdf::ElementPtr _sdfElement)
{
  sdf::ElementPtr pluginElem = NULL;

  if (this->dataPtr->sdfToAppend->HasElement("plugin"))
     pluginElem = this->dataPtr->sdfToAppend->GetElement("plugin");

  while (pluginElem)
  {
    if (pluginElem->Get<std::string>("name") == _name &&
        pluginElem->Get<std::string>("filename") == _filename)
    {
      std::string childName = _sdfElement->GetName();
      if (pluginElem->HasElement(childName))
      {
        sdf::ElementPtr childElem = pluginElem->GetElement(childName);
        while (childElem)
        {
          if (MatchingElement(_sdfElement, childElem))
            pluginElem->RemoveChild(childElem);
          childElem = childElem->GetNextElement(childName);
        }
      }
    }
    //  Remove plugin element if last element was deleted
    if (pluginElem->GetFirstElement() == NULL)
    {
      this->dataPtr->sdfToAppend->RemoveChild(pluginElem);
      break;
    }
    pluginElem = pluginElem->GetNextElement("plugin");
  }
}

/////////////////////////////////////////////////
void ModelCreator::OnAddModelPlugin(const std::string &_name,
    const std::string &_filename, const std::string &_innerxml)
{
  if (_name.empty() || _filename.empty())
  {
    gzerr << "Cannot add model plugin. Empty name or filename" << std::endl;
    return;
  }

  // Use the SDF parser to read all the inner xml.
  sdf::ElementPtr modelPluginSDF(new sdf::Element);
  sdf::initFile("plugin.sdf", modelPluginSDF);
  std::stringstream tmp;
  tmp << "<sdf version='" << SDF_VERSION << "'>";
  tmp << "<plugin name='" << _name << "' filename='" << _filename << "'>";
  tmp << _innerxml;
  tmp << "</plugin></sdf>";

  if (sdf::readString(tmp.str(), modelPluginSDF))
  {
    this->AddModelPlugin(modelPluginSDF);
    this->ModelChanged();
  }
  else
  {
    gzerr << "Error reading Plugin SDF. Unable to parse Innerxml:\n"
        << _innerxml << std::endl;
  }
}

/////////////////////////////////////////////////
void ModelCreator::AddModelPlugin(const sdf::ElementPtr &_pluginElem)
{
  if (_pluginElem->HasAttribute("name"))
  {
    std::string name = _pluginElem->Get<std::string>("name");

    // Create data
    ModelPluginData *modelPlugin = new ModelPluginData();
    modelPlugin->Load(_pluginElem);

    // Add to map
    {
      std::lock_guard<std::recursive_mutex> lock(this->dataPtr->updateMutex);
      this->dataPtr->allModelPlugins[name] = modelPlugin;
    }

    // Notify addition
    gui::model::Events::modelPluginInserted(name);
  }
}

/////////////////////////////////////////////////
ModelPluginData *ModelCreator::ModelPlugin(const std::string &_name)
{
  auto it = this->dataPtr->allModelPlugins.find(_name);
  if (it != this->dataPtr->allModelPlugins.end())
    return it->second;
  return NULL;
}

/////////////////////////////////////////////////
void ModelCreator::OnOpenModelPluginInspector(const QString &_name)
{
  this->OpenModelPluginInspector(_name.toStdString());
}

/////////////////////////////////////////////////
void ModelCreator::OpenModelPluginInspector(const std::string &_name)
{
  std::lock_guard<std::recursive_mutex> lock(this->dataPtr->updateMutex);

  auto it = this->dataPtr->allModelPlugins.find(_name);
  if (it == this->dataPtr->allModelPlugins.end())
  {
    gzerr << "Model plugin [" << _name << "] not found." << std::endl;
    return;
  }

  ModelPluginData *modelPlugin = it->second;
  modelPlugin->inspector->move(QCursor::pos());
  modelPlugin->inspector->show();
}

/////////////////////////////////////////////////
void ModelCreator::OnRequestLinkMove(const std::string &_name,
    const ignition::math::Pose3d &_pose)
{
  auto link = this->dataPtr->allLinks.find(_name);
  if (link == this->dataPtr->allLinks.end())
    return;

  link->second->linkVisual->SetPose(_pose);
  link->second->SetPose(_pose);
}

/////////////////////////////////////////////////
void ModelCreator::OnRequestNestedModelMove(const std::string &_name,
    const ignition::math::Pose3d &_pose)
{
  auto nestedModel = this->dataPtr->allNestedModels.find(_name);
  if (nestedModel == this->dataPtr->allNestedModels.end())
    return;

  nestedModel->second->modelVisual->SetPose(_pose);
  nestedModel->second->SetPose(_pose);
}

/////////////////////////////////////////////////
void ModelCreator::OnRequestLinkScale(const std::string &_name,
    const ignition::math::Vector3d &_scale)
{
  auto link = this->dataPtr->allLinks.find(_name);
  if (link == this->dataPtr->allLinks.end())
    return;

  auto vis = link->second->linkVisual;
  for (unsigned int i = 0; i < vis->GetChildCount(); ++i)
  {
    auto childVis = vis->GetChild(i);
    auto geomType = childVis->GetGeometryType();
    if (geomType != "" && geomType != "mesh")
    {
      /// \todo Different geoms might be scaled differently
      childVis->SetScale(_scale);
    }
  }

  link->second->SetScale(_scale);
}
