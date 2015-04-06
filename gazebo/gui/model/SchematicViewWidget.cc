/*
 * Copyright (C) 2015 Open Source Robotics Foundation
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

#include "gazebo/common/Events.hh"

#include "gazebo/gui/model/GraphScene.hh"
#include "gazebo/gui/model/GraphView.hh"
#include "gazebo/gui/model/ModelEditorEvents.hh"
#include "gazebo/gui/model/SchematicViewWidget.hh"

using namespace gazebo;
using namespace gui;

/////////////////////////////////////////////////
SchematicViewWidget::SchematicViewWidget(QWidget *_parent)
  : QWidget(_parent)
{
  this->setObjectName("SchematicViewWidget");

  this->scene = new GraphScene(this);
  this->view = new GraphView(_parent);

  this->minimumWidth = 500;
  this->minimumHeight = 500;

  QHBoxLayout *canvasLayout = new QHBoxLayout(this);
  canvasLayout->addWidget(view);
  canvasLayout->setAlignment(Qt::AlignHCenter);

  this->view->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  this->view->setScene(this->scene);
  this->view->centerOn(QPointF(0, 0));
//  this->view->setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
  this->view->setDragMode(QGraphicsView::ScrollHandDrag);
  this->view->show();

  canvasLayout->setContentsMargins(0, 0, 0, 0);
  canvasLayout->setSpacing(0);
  this->setLayout(canvasLayout);
}

/////////////////////////////////////////////////
SchematicViewWidget::~SchematicViewWidget()
{
}

/////////////////////////////////////////////////
void SchematicViewWidget::Reset()
{
  this->edges.clear();
  this->scene->clear();
}

/////////////////////////////////////////////////
void SchematicViewWidget::Init()
{
  this->connections.push_back(gui::model::Events::ConnectLinkInserted(
      boost::bind(&SchematicViewWidget::AddNode, this, _1)));

  this->connections.push_back(gui::model::Events::ConnectLinkRemoved(
      boost::bind(&SchematicViewWidget::RemoveNode, this, _1)));

  this->connections.push_back(gui::model::Events::ConnectJointInserted(
      boost::bind(&SchematicViewWidget::AddEdge, this, _1, _2, _3, _4)));

  this->connections.push_back(gui::model::Events::ConnectJointRemoved(
      boost::bind(&SchematicViewWidget::RemoveEdge, this, _1)));
}

/////////////////////////////////////////////////
std::string SchematicViewWidget::GetLeafName(const std::string &_scopedName)
{
  if (_scopedName.empty())
    return "";

  std::string leafName = _scopedName;
  size_t idx = _scopedName.find_last_of("::");
  if (idx != std::string::npos)
    leafName = _scopedName.substr(idx+1);
  return leafName;
}

/////////////////////////////////////////////////
void SchematicViewWidget::AddNode(const std::string &_node)
{
  std::string node = this->GetLeafName(_node);

  if (this->scene->HasNode(node))
    return;

  // this must be called before making changes to the graph
  this->scene->clearLayout();

  this->scene->AddNode(node);
  this->scene->applyLayout();

  this->FitInView();
}

/////////////////////////////////////////////////
unsigned int SchematicViewWidget::GetNodeCount() const
{
  return static_cast<unsigned int>(this->scene->nodeCount());
}

/////////////////////////////////////////////////
void SchematicViewWidget::RemoveNode(const std::string &_node)
{
  std::string node = this->GetLeafName(_node);

  if (!this->scene->HasNode(node))
    return;

  // this must be called before making changes to the graph
  this->scene->clearLayout();
  this->scene->RemoveNode(node);

  this->scene->applyLayout();
  this->FitInView();
}

/////////////////////////////////////////////////
void SchematicViewWidget::AddEdge(const std::string &_id,
    const std::string &/*_name*/, const std::string &_parent,
    const std::string &_child)
{
  std::string parentNode = this->GetLeafName(_parent);
  std::string childNode = this->GetLeafName(_child);

  this->edges[_id] = std::make_pair(parentNode, childNode);

  // this must be called before making changes to the graph
  this->scene->clearLayout();

  this->scene->AddEdge(parentNode, childNode);
  this->scene->applyLayout();

  this->FitInView();
}

/////////////////////////////////////////////////
void SchematicViewWidget::RemoveEdge(const std::string &_id)
{
  auto it = this->edges.find(_id);
  if (it != this->edges.end())
  {
    std::string parentNode = it->second.first;
    std::string childNode = it->second.second;
    // this must be called before making changes to the graph
    this->scene->clearLayout();

    this->scene->RemoveEdge(parentNode, childNode);
    this->scene->applyLayout();

    this->FitInView();

    this->edges.erase(it);
  }
}

/////////////////////////////////////////////////
unsigned int SchematicViewWidget::GetEdgeCount() const
{
  return static_cast<unsigned int>(this->scene->edgeCount());
}

/////////////////////////////////////////////////
void SchematicViewWidget::resizeEvent(QResizeEvent */*_event*/)
{
  this->FitInView();
}

/////////////////////////////////////////////////
void SchematicViewWidget::FitInView()
{
  QRectF newRect;
  QRectF sceneRect = this->scene->itemsBoundingRect();
  newRect.setWidth(std::max(static_cast<int>(sceneRect.width()),
      this->minimumWidth));
  newRect.setHeight(std::max(static_cast<int>(sceneRect.height()),
      this->minimumHeight));
  newRect.setX(sceneRect.x());
  newRect.setY(sceneRect.y());

  this->view->fitInView(newRect, Qt::KeepAspectRatio);
  this->view->centerOn(newRect.x() + newRect.width()*0.5,
      newRect.y() + newRect.height()*0.5);
}