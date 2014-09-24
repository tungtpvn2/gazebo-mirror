/*
 * Copyright (C) 2012-2014 Open Source Robotics Foundation
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

#include "gazebo/common/Exception.hh"
#include "gazebo/gui/building/EditorView.hh"
#include "gazebo/gui/building/EditorItem.hh"
#include "gazebo/gui/building/LineSegmentItem.hh"
#include "gazebo/gui/building/MeasureItem.hh"

using namespace gazebo;
using namespace gui;

/////////////////////////////////////////////////
MeasureItem::MeasureItem(const QPointF &_start, const QPointF &_end)
    : PolylineItem(_start, _end)
{
  this->editorType = "Measure";

  this->setFlag(QGraphicsItem::ItemSendsGeometryChanges);
  this->setAcceptHoverEvents(true);

  this->SetThickness(10);
  this->SetColor(QColor(247, 142, 30));
}

/////////////////////////////////////////////////
MeasureItem::~MeasureItem()
{
}

/////////////////////////////////////////////////
double MeasureItem::GetDistance()
{
  LineSegmentItem *segment = this->GetSegment(0);

  return segment->line().length();
}
