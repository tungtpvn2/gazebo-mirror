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

#include "gazebo/gui/model_editor/RectItem.hh"
#include "gazebo/gui/model_editor/BuildingItem.hh"
#include "gazebo/gui/model_editor/PolylineItem.hh"
#include "gazebo/gui/model_editor/LineSegmentItem.hh"
#include "gazebo/gui/model_editor/WallInspectorDialog.hh"
#include "gazebo/gui/model_editor/BuildingMaker.hh"
#include "gazebo/gui/model_editor/CornerGrabber.hh"
#include "gazebo/gui/model_editor/WallItem.hh"

using namespace gazebo;
using namespace gui;

/////////////////////////////////////////////////
WallItem::WallItem(QPointF _start, QPointF _end)
    : PolylineItem(_start, _end), BuildingItem()
{
  this->editorType = "Wall";
  this->scale = BuildingMaker::conversionScale;

  this->level = 0;

  this->wallThickness = 20;
  this->wallHeight = 250;

  this->SetThickness(this->wallThickness);

  this->selectedSegment = NULL;

  this->setAcceptHoverEvents(true);

  this->inspector = new WallInspectorDialog();
  this->inspector->setModal(false);
  connect(this->inspector, SIGNAL(Applied()), this, SLOT(OnApply()));

  this->openInspectorAct = new QAction(tr("&Open Wall Inspector"), this);
  this->openInspectorAct->setStatusTip(tr("Open Wall Inspector"));
  connect(this->openInspectorAct, SIGNAL(triggered()),
    this, SLOT(OnOpenInspector()));
}

/////////////////////////////////////////////////
WallItem::~WallItem()
{
  delete this->inspector;
}

/////////////////////////////////////////////////
double WallItem::GetHeight()
{
  return this->wallHeight;
}

/////////////////////////////////////////////////
void WallItem::SetHeight(double _height)
{
  this->wallHeight = _height;
}

/////////////////////////////////////////////////
WallItem *WallItem::Clone()
{
  WallItem *wallItem = new WallItem(this->scenePos(),
      this->scenePos() + QPointF(1, 0));

  LineSegmentItem *segment = this->segments[0];
  wallItem->SetVertexPosition(0, this->mapToScene(segment->line().p1()));
  wallItem->SetVertexPosition(1, this->mapToScene(segment->line().p2()));


  // TODO: Code below just simiulates the way wall are created through user
  // interactions. There should be a better way of doing this
  for (unsigned int i = 1; i < this->segments.size(); ++i)
  {
    segment = this->segments[i];
    wallItem->AddPoint(this->mapToScene(segment->line().p1()) + QPointF(1, 0));
    wallItem->SetVertexPosition(wallItem->GetVertexCount()-1,
        this->mapToScene(segment->line().p2()));
  }
  wallItem->AddPoint(this->mapToScene(
      this->segments[wallItem->GetSegmentCount()-1]->line().p2())
      + QPointF(1, 0));
  wallItem->PopEndPoint();

  wallItem->SetLevel(this->level);
  wallItem->SetHeight(this->wallHeight);
  wallItem->SetThickness(this->wallThickness);

  return wallItem;
}

/////////////////////////////////////////////////
bool WallItem::cornerEventFilter(CornerGrabber* _corner,
    QEvent *_event)
{
  QGraphicsSceneMouseEvent *mouseEvent =
    dynamic_cast<QGraphicsSceneMouseEvent*>(_event);

  switch (_event->type())
  {
    case QEvent::GraphicsSceneMousePress:
    {
      _corner->SetMouseState(QEvent::GraphicsSceneMousePress);
      QPointF scenePosition =  _corner->mapToScene(mouseEvent->pos());

      _corner->SetMouseDownX(scenePosition.x());
      _corner->SetMouseDownY(scenePosition.y());
      break;
    }
    case QEvent::GraphicsSceneMouseRelease:
    {
      _corner->SetMouseState(QEvent::GraphicsSceneMouseRelease);
      break;
    }
    case QEvent::GraphicsSceneMouseMove:
    {
      _corner->SetMouseState(QEvent::GraphicsSceneMouseMove);
      break;
    }
    case QEvent::GraphicsSceneHoverEnter:
    case QEvent::GraphicsSceneHoverMove:
    {
      QApplication::setOverrideCursor(QCursor(Qt::CrossCursor));
      return true;
    }
    case QEvent::GraphicsSceneHoverLeave:
    {
      QApplication::restoreOverrideCursor();
      return true;
    }
    default:
    {
      break;
    }
  }

  if (!mouseEvent)
    return false;

  if (_corner->GetMouseState() == QEvent::GraphicsSceneMouseMove)
  {
    QPointF scenePosition = _corner->mapToScene(mouseEvent->pos());
    int cornerIndex = _corner->GetIndex();

    // Snap wall rotations to fixed size increments
    QPointF newScenePos = scenePosition;
    if ((cornerIndex <= static_cast<int>(this->GetSegmentCount())))
    {
      int segmentIndex = std::max(cornerIndex-1, 0);
      LineSegmentItem *segment = this->GetSegment(segmentIndex);
      QPointF lineOrigin = segment->line().p1();
      if (cornerIndex == 0)
        lineOrigin = segment->line().p2();
      QLineF lineToPoint(lineOrigin,
          segment->mapFromScene(scenePosition));
      QPointF startScenePoint = segment->mapToScene(lineOrigin);
      double angle = QLineF(startScenePoint,
          scenePosition).angle();
      double range = 15;
      int increment = angle / range;
      if ((angle - range*increment) > range/2)
        increment++;
      angle = -range*increment;
      double lineLength = lineToPoint.length();

      newScenePos.setX(startScenePoint.x()
          + cos(GZ_DTOR(angle))*lineLength);
      newScenePos.setY(startScenePoint.y()
          + sin(GZ_DTOR(angle))*lineLength);
    }

    this->SetVertexPosition(cornerIndex, newScenePos);
    this->update();

    // re-align child items when a vertex is moved
    for (int i = cornerIndex; i > cornerIndex - 2; --i)
    {
      if ((i - this->GetSegmentCount()) != 0 && i >= 0)
      {
        LineSegmentItem *segment = this->GetSegment(i);
        QList<QGraphicsItem *> children = segment->childItems();
        for (int j = 0; j < children.size(); ++j)
        {
          // TODO find a more generic way than casting child as rect item
          RectItem *rectItem = dynamic_cast<RectItem *>(children[j]);
          if (rectItem)
          {
            rectItem->SetRotation(-segment->line().angle());
            QPointF delta = rectItem->pos() - segment->line().p1();
            QPointF deltaLine = segment->line().p2() - segment->line().p1();
            double deltaRatio = sqrt(delta.x()*delta.x() + delta.y()*delta.y())
                / segment->line().length();
            rectItem->setPos(segment->line().p1() + deltaRatio*deltaLine);

          }
        }
      }
    }
  }
  return true;
}

/////////////////////////////////////////////////
bool WallItem::segmentEventFilter(LineSegmentItem *_segment,
    QEvent *_event)
{
  QGraphicsSceneMouseEvent *mouseEvent =
    dynamic_cast<QGraphicsSceneMouseEvent*>(_event);

  QPointF scenePosition;
  if (mouseEvent)
    scenePosition =  mouseEvent->scenePos();

  switch (_event->type())
  {
    case QEvent::GraphicsSceneMousePress:
    {
      _segment->SetMouseState(QEvent::GraphicsSceneMousePress);
      _segment->SetMouseDownX(scenePosition.x());
      _segment->SetMouseDownY(scenePosition.y());
      this->segmentMouseMove = scenePosition;
      break;
    }
    case QEvent::GraphicsSceneMouseRelease:
    {
      _segment->SetMouseState(QEvent::GraphicsSceneMouseRelease);
      break;
    }
    case QEvent::GraphicsSceneMouseMove:
    {
      _segment->SetMouseState(QEvent::GraphicsSceneMouseMove);
      break;
    }
    case QEvent::GraphicsSceneContextMenu:
    {
      this->selectedSegment = _segment;
      QMenu menu;
      menu.addAction(this->openInspectorAct);
      menu.exec(dynamic_cast<QGraphicsSceneContextMenuEvent*>(
          _event)->screenPos());
      return true;
    }
    case QEvent::GraphicsSceneMouseDoubleClick:
    {
      this->selectedSegment = _segment;
      this->OnOpenInspector();
      _segment->SetMouseState(QEvent::GraphicsSceneMouseDoubleClick);
      break;
    }
    case QEvent::GraphicsSceneHoverEnter:
    case QEvent::GraphicsSceneHoverMove:
    {
      QApplication::setOverrideCursor(QCursor(Qt::SizeAllCursor));
      return true;
    }
    case QEvent::GraphicsSceneHoverLeave:
    {
      QApplication::restoreOverrideCursor();
      return true;
    }
    default:
    {
      break;
    }
  }

  if (!mouseEvent)
    return false;

  if (_segment->GetMouseState() == QEvent::GraphicsSceneMouseMove)
  {
    QPointF trans = scenePosition - segmentMouseMove;

    this->TranslateVertex(_segment->GetIndex(), trans);
    this->TranslateVertex(_segment->GetIndex() + 1, trans);

    this->segmentMouseMove = scenePosition;

    this->update();

    QList<QGraphicsItem *> children = _segment->childItems();
    for ( int i = 0; i < children.size(); ++i)
      children[i]->moveBy(trans.x(), trans.y());
  }
  return true;
}

/////////////////////////////////////////////////
void WallItem::contextMenuEvent(QGraphicsSceneContextMenuEvent *_event)
{
  QMenu menu;
  menu.addAction(this->openInspectorAct);
  menu.exec(_event->screenPos());
  _event->accept();
}

/////////////////////////////////////////////////
void WallItem::OnOpenInspector()
{
  if (!this->selectedSegment)
    return;
  QLineF line = this->selectedSegment->line();
  double segmentLength = line.length() + this->wallThickness;
  QPointF segmentStartPoint = this->mapToScene(line.p1());
  QPointF segmentEndPoint = this->mapToScene(line.p2());

  this->inspector->SetThickness(this->wallThickness * this->scale);
  this->inspector->SetHeight(this->wallHeight * this->scale);
  this->inspector->SetLength(segmentLength * this->scale);
  QPointF startPos = segmentStartPoint * this->scale;
  startPos.setY(-startPos.y());
  this->inspector->SetStartPosition(startPos);
  QPointF endPos = segmentEndPoint * this->scale;
  endPos.setY(-endPos.y());
  this->inspector->SetEndPosition(endPos);
  this->inspector->show();
}

/////////////////////////////////////////////////
void WallItem::OnApply()
{
  WallInspectorDialog *dialog =
     qobject_cast<WallInspectorDialog *>(QObject::sender());

  QLineF line = this->selectedSegment->line();
  double segmentLength = line.length() + this->wallThickness;
  this->wallThickness = dialog->GetThickness() / this->scale;
  this->SetThickness(this->wallThickness);
  this->wallHeight = dialog->GetHeight() / this->scale;
  this->WallChanged();

  double newLength = dialog->GetLength() / this->scale;
  // The if statement below limits the change to either the length of
  // the wall segment or its start/end pos.
  // Comparison between doubles up to 1 decimal place
  if (fabs(newLength - segmentLength) > 0.1)
  {
    line.setLength(newLength - this->wallThickness);
    this->SetVertexPosition(this->selectedSegment->GetIndex()+1,
        this->mapToScene(line.p2()));
  }
  else
  {
    QPointF newStartPoint = dialog->GetStartPosition() / this->scale;
    newStartPoint.setY(-newStartPoint.y());
    QPointF newEndPoint = dialog->GetEndPosition() / this->scale;
    newEndPoint.setY(-newEndPoint.y());

    this->SetVertexPosition(this->selectedSegment->GetIndex(),
      newStartPoint);
    this->SetVertexPosition(this->selectedSegment->GetIndex() + 1,
      newEndPoint);
  }
}

/////////////////////////////////////////////////
void WallItem::WallChanged()
{
  emit depthChanged(this->wallThickness);
  emit heightChanged(this->wallHeight);
}

/////////////////////////////////////////////////
void WallItem::Update()
{
  this->WallChanged();
  PolylineItem::Update();
}
