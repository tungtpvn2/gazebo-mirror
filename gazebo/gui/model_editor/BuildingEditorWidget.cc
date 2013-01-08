/*
 * Copyright 2012 Open Source Robotics Foundation
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

#include "gazebo/gui/model_editor/BuildingItem.hh"
#include "gazebo/gui/model_editor/GridLines.hh"
#include "gazebo/gui/model_editor/EditorView.hh"
#include "gazebo/gui/model_editor/EditorItem.hh"
#include "gazebo/gui/model_editor/LineSegmentItem.hh"
#include "gazebo/gui/model_editor/PolylineItem.hh"
#include "gazebo/gui/model_editor/WallItem.hh"
#include "gazebo/gui/model_editor/LevelWidget.hh"
#include "gazebo/gui/model_editor/RulerWidget.hh"
#include "gazebo/gui/model_editor/BuildingEditorWidget.hh"

using namespace gazebo;
using namespace gui;

QCursor BuildingEditorWidget::rotateCursor;

/////////////////////////////////////////////////
BuildingEditorWidget::BuildingEditorWidget(QWidget *_parent)
  : QWidget(_parent)
{
  this->setObjectName("buildingEditorWidget");

  QPixmap rotatePixmap(":/images/rotate_object.png");
  rotatePixmap = rotatePixmap.scaledToHeight(25);
  rotatePixmap = rotatePixmap.scaledToWidth(25);
  rotateCursor = QCursor(rotatePixmap);

  EditorView *view = new EditorView();
  QGraphicsScene *scene = new QGraphicsScene();

  QColor c (250,250,250);
  QBrush brush (c, Qt::SolidPattern);
  scene->setBackgroundBrush(brush);

  int boundingWidth = 1240;
  int boundingHeight = 1024;
  scene->setSceneRect(-boundingWidth/2, -boundingHeight/2,
                      boundingWidth, boundingHeight);
  QHBoxLayout *canvasLayout = new QHBoxLayout(this);
  canvasLayout->addWidget(view);
  canvasLayout->setAlignment(Qt::AlignHCenter);

  GridLines *gridLines = new GridLines (boundingWidth, boundingHeight);
  scene->addItem(gridLines);

  view->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Expanding);
  view->setScene(scene);
  view->centerOn(QPointF(0, 0));
  view->setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
  view->setDragMode(QGraphicsView::ScrollHandDrag);

  // TODO remove this wall and make sure scene updates without any items in it
  WallItem *wallItem = new WallItem(QPointF(0, 0), QPointF(0, 0));
  wallItem->SetThickness(0);
  scene->addItem(wallItem);

  this->levelWidget = new LevelWidget(this);
  this->levelWidget->resize(250,50);
//  QGraphicsProxyWidget* proxyWidget = scene->addWidget(levelWidget);
//  proxyWidget->setFlag(QGraphicsItem::ItemIgnoresTransformations);
//  proxyWidget->setPos(QPointF(-levelWidget->width() / 2, 0));

  this->rulerWidget = new RulerWidget(this);
  this->rulerWidget->resize(250,50);

  canvasLayout->setContentsMargins(0, 0, 0, 0);
  this->setLayout(canvasLayout);
}

/////////////////////////////////////////////////
BuildingEditorWidget::~BuildingEditorWidget()
{
}

/////////////////////////////////////////////////
void BuildingEditorWidget::resizeEvent(QResizeEvent *_event)
{
  this->levelWidget->move(_event->size().width()/2
      - levelWidget->size().width()/2, 0);

  this->rulerWidget->move(20, _event->size().height()
      - rulerWidget->size().height() - 20);
}
