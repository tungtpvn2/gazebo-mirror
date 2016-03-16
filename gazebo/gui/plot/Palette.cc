/*
 * Copyright (C) 2016 Open Source Robotics Foundation
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

#include <set>
#include <string>
#include <vector>
#include <google/protobuf/message.h>

#include "gazebo/common/CommonIface.hh"
#include "gazebo/common/Console.hh"
#include "gazebo/common/URI.hh"

#include "gazebo/gui/ConfigWidget.hh"
#include "gazebo/gui/plot/Palette.hh"

#include "gazebo/transport/TransportIface.hh"

#include "gazebo/util/IntrospectionClient.hh"

using namespace gazebo;
using namespace gui;

/////////////////////////////////////////////////
/// \brief Delegate that handles drawing the topic tree
class PlotItemDelegate : public QStyledItemDelegate
{
  /// \brief The data roles
  public: enum DataRole
  {
    /// \brief Text which will be displayed for the user.
    DISPLAY_NAME = Qt::UserRole + 100,

    /// \brief URI including detailed query about a single plot value. This is
    /// the information carried during a drag-drop operation.
    URI_QUERY,

    /// \brief Data type name, such as "Double" or "Bool", used to display type
    /// information to the user. Or something like "model", "link", used to
    /// choose icons.
    TYPE,

    /// \brief Flag indicating whether to expand the item or not.
    TO_EXPAND
  };

  /// \brief Constructor
  public: PlotItemDelegate() = default;

  /// \brief Destructor
  public: virtual ~PlotItemDelegate() = default;

  /// \brief Custom paint function.
  /// \param[in] _painter Pointer to the QT painter.
  /// \param[in] _opt Item options.
  /// \param[in] _index Item model index.
  public: void paint(QPainter *_painter, const QStyleOptionViewItem &_opt,
      const QModelIndex &_index) const
  {
    auto textRect = _opt.rect;

    // Custom options
    QString topicName = qvariant_cast<QString>(_index.data(DISPLAY_NAME));
    QString typeName = qvariant_cast<QString>(_index.data(TYPE));

    // TODO: Change to QApplication::font() once Roboto is used everywhere
    QFont font;
    auto fontWeight = QFont::Normal;
    if (typeName == "title")
    {
      font.setFamily("Roboto Bold");
      fontWeight = QFont::Bold;

      // Erase the branch image for titles
      QRectF titleRect = _opt.rect;
      titleRect.setLeft(titleRect.left() - 13);
      // FIXME: Find a non-hardcoded way to get the bg color
      QBrush brush(QColor("#e2e2e2"));
      _painter->save();
      _painter->fillRect(titleRect, brush);
      _painter->restore();
    }
    else
    {
      font.setFamily("Roboto Regular");
    }
    QFontMetrics fm(font);

    // Handle hover style
    if (typeName != "title" && _opt.state & QStyle::State_MouseOver)
    {
      _painter->setPen(QPen(QColor(200, 200, 200, 0), 0));
      _painter->setBrush(QColor(200, 200, 200));
      _painter->drawRect(_opt.rect);
    }

    // Paint the type icon
    if (typeName == "model" || typeName == "link" || typeName == "collision" ||
        typeName == "visual" || typeName == "joint")
    {
      double iconSize = 15;

      textRect.adjust(iconSize + 6, 5, 0, -5);
      QRectF iconRect = _opt.rect;
      iconRect.setTop(iconRect.top() + (_opt.rect.height()/2.0 - iconSize/2.0));

      QIcon icon(":/images/" + typeName  + ".svg");
      _painter->drawPixmap(iconRect.left(), iconRect.top(),
          icon.pixmap(iconSize, iconSize));
    }
    // Titles
    else if (typeName == "title")
    {
      textRect.adjust(-15, 5, 0, -5);
    }
    // Plottable items
    else if (!typeName.isEmpty())
    {
      // Paint icon
      double iconSize = 20;

      QRectF iconRect = _opt.rect;
      iconRect.setTop(iconRect.top() + (_opt.rect.height()/2.0 - iconSize/2.0));

      QIcon icon(":/images/graph_line.svg");
      _painter->drawPixmap(iconRect.left(), iconRect.top(),
          icon.pixmap(iconSize, iconSize));

      // Move text
      textRect.adjust(iconSize + 5, 5, 0, -5);
    }
    // Normal expandable items
    else
    {
      // Otherwise use a rectangle that is sized for the just the topic name
      textRect.adjust(0, 5, 0, -5);
    }

    _painter->setFont(QFont(font.family(), font.pointSize(), fontWeight));
    _painter->setPen(QColor(30, 30, 30));
    _painter->drawText(textRect, topicName);
  }

  /// \brief Size hint tells QT how big an item is.
  /// \param[in] _option Style options
  /// \param[in] _index Item model index
  public: QSize sizeHint(const QStyleOptionViewItem &_option,
                         const QModelIndex &_index) const
  {
    QSize size = QStyledItemDelegate::sizeHint(_option, _index);

    // TODO: Change to QApplication::font() once Roboto is used everywhere
    QFont font("Roboto Regular");
    QFontMetrics fm(font);

    // Make it slightly larger
    size.setHeight(fm.height() + 10);

    return size;
  }
};

/////////////////////////////////////////////////
/// Customize the item model so that we can pass along the correct MIME
/// information during a drag-drop.
class PlotItemModel : public QStandardItemModel
{
  /////////////////////////////////////////////////
  /// \brief Custom MIME data function.
  /// \param[in] _indexes List of selected items
  /// \return Mime data for the selected items.
  public: QMimeData *mimeData(const QModelIndexList &_indexes) const
  {
    QMimeData *curMimeData = new QMimeData();

    for (auto const &idx : _indexes)
    {
      if (idx.isValid())
      {
        QString text = this->data(idx,
            PlotItemDelegate::URI_QUERY).toString();
        curMimeData->setData("application/x-item", text.toLatin1().data());

        break;
      }
    }

    return curMimeData;
  }
};

/////////////////////////////////////////////////
/// Customize the search model.
class SearchModel : public QSortFilterProxyModel
{
  /////////////////////////////////////////////////
  /// \brief Customize so we accept rows where:
  /// 1. Each of the words can be found in its ancestors or itself, but not
  /// necessarily all words on the same row, or
  /// 2. One of its descendants matches rule 1, or
  /// 3. One of its ancestors matches rule 1.
  ///
  /// For example this structure:
  /// - a
  /// -- b
  /// -- c
  /// --- d
  ///
  /// * A search of "a" will display all rows.
  /// * A search of "b" or "a b" will display "a" and "b".
  /// * A search of "c", "d", "a c", "a d", "a c d" or "c d" will display "a",
  /// "c" and "d".
  /// * A search of "a b c d", "b c" or "b d" will display nothing.
  ///
  /// \param[in] _srcRow Row on the source model.
  /// \param[in] _srcParent Parent on the source model.
  /// \return True if row is accepted.
  public: bool filterAcceptsRow(const int _srcRow,
      const QModelIndex &_srcParent) const
  {
    // Empty search matches nothing
    if (this->search == "")
      return false;

    // Item index in search model
    auto id = this->sourceModel()->index(_srcRow, 0, _srcParent);

    // Ignore titles
    if (this->sourceModel()->data(id, PlotItemDelegate::TYPE).toString() ==
        "title")
    {
      return false;
    }

    // Collapsed by default
    this->sourceModel()->setData(id, false, PlotItemDelegate::TO_EXPAND);

    // Each word must match at least once, either self, parent or child
    auto words = this->search.split(" ");
    for (auto word : words)
    {
      if (word == "")
        continue;

      // Expand this if at least one child contains the word
      // Note that this is not enough for this to be accepted, we need to match
      // all words
      if (this->hasChildAcceptsItself(id, word))
      {
        this->sourceModel()->setData(id, true, PlotItemDelegate::TO_EXPAND);
      }

      // At least one of the children fits rule 1
      if (this->hasAcceptedChildren(_srcRow, _srcParent))
        continue;

      // Row itself contains this word
      if (this->filterAcceptsRowItself(_srcRow, _srcParent, word))
        continue;

      // One of the ancestors contains this word
      QModelIndex parentIndex = _srcParent;
      bool parentAccepted = false;
      while (parentIndex.isValid())
      {
        if (this->filterAcceptsRowItself(parentIndex.row(),
            parentIndex.parent(), word))
        {
          parentAccepted = true;
          break;
        }
        parentIndex = parentIndex.parent();
      }

      if (parentAccepted)
        continue;

      // This word can't be found on the row or a parent, and no child is fully
      // accepted.
      return false;
    }

    return true;
  }

  /// \brief Check if row contains the word on itself.
  /// \param[in] _srcRow Row on the source model.
  /// \param[in] _srcParent Parent on the source model.
  /// \return True if row matches.
  public: bool filterAcceptsRowItself(const int _srcRow, const
      QModelIndex &_srcParent, QString _word) const
  {
    auto id = this->sourceModel()->index(_srcRow, 0, _srcParent);

    return (this->sourceModel()->data(id,
        this->filterRole()).toString().contains(_word, Qt::CaseInsensitive));
  }

  /// \brief Check if any of the children is fully accepted.
  /// \param[in] _srcRow Row on the source model.
  /// \param[in] _srcParent Parent on the source model.
  /// \return True if any of the children match.
  public: bool hasAcceptedChildren(const int _srcRow,
      const QModelIndex &_srcParent) const
  {
    auto item = sourceModel()->index(_srcRow, 0, _srcParent);

    if (!item.isValid())
      return false;

    for (int i = 0; i < item.model()->rowCount(item); ++i)
    {
      if (this->filterAcceptsRow(i, item))
        return true;
    }

    return false;
  }

  /// \brief Check if any of the children accepts a specific word.
  /// \param[in] _srcParent Parent on the source model.
  /// \param[in] _word Word to be checked.
  /// \return True if any of the children match.
  public: bool hasChildAcceptsItself(const QModelIndex &_srcParent,
      const QString &_word) const
  {
    for (int i = 0; i < this->sourceModel()->rowCount(_srcParent); ++i)
    {
      // Check immediate children
      if (this->filterAcceptsRowItself(i, _srcParent, _word))
        return true;

      // Check gradchildren
      auto item = this->sourceModel()->index(i, 0, _srcParent);
      if (this->hasChildAcceptsItself(item, _word))
        return true;
    }

    return false;
  }

  /// \brief Set a new search value.
  /// \param[in] _search Full search string.
  public: void setSearch(const QString &_search)
  {
    this->search = _search;
    this->filterChanged();
  }

  /// \brief Full search string.
  public: QString search;
};

/////////////////////////////////////////////////
/// \brief Private data for the Palette class
class gazebo::gui::PalettePrivate
{
  /// \brief Model to hold topics data.
  public: PlotItemModel *topicsModel;

  /// \brief Model to hold models data.
  public: PlotItemModel *modelsModel;

  /// \brief Model to hold sim data.
  public: PlotItemModel *simModel;

  /// \brief Proxy model to filter topics data.
  public: SearchModel *searchTopicsModel;

  /// \brief Proxy model to filter models data.
  public: SearchModel *searchModelsModel;

  /// \brief Proxy model to filter sim data.
  public: SearchModel *searchSimModel;

  /// \brief View holding the search topics tree.
  public: QTreeView *searchTopicsTree;

  /// \brief View holding the search models tree.
  public: QTreeView *searchModelsTree;

  /// \brief View holding the search sim tree.
  public: QTreeView *searchSimTree;
};

/////////////////////////////////////////////////
Palette::Palette(QWidget *_parent) : QWidget(_parent),
    dataPtr(new PalettePrivate)
{
  // The tab bar along the top.
  auto tabBar = new QTabBar;
  tabBar->setObjectName("plottingTabBar");
  tabBar->addTab("TOPICS");
  tabBar->addTab("MODELS");
  tabBar->addTab("SIM");
  tabBar->addTab("SEARCH");
  tabBar->setExpanding(true);
  tabBar->setDrawBase(false);
  tabBar->setFocusPolicy(Qt::NoFocus);

  // Create a view delegate, to handle drawing items in the tree view
  auto plotItemDelegate = new PlotItemDelegate;

  // The model that will hold data to be displayed in the topic tree view
  this->dataPtr->topicsModel = new PlotItemModel;
  this->dataPtr->topicsModel->setObjectName("plotTopicsModel");
  this->dataPtr->topicsModel->setParent(this);
  this->FillTopics(this->dataPtr->topicsModel);

  // A proxy model to filter topic model
  this->dataPtr->searchTopicsModel = new SearchModel;
  this->dataPtr->searchTopicsModel->setFilterRole(
      PlotItemDelegate::DISPLAY_NAME);
  this->dataPtr->searchTopicsModel->setSourceModel(this->dataPtr->topicsModel);

  // A tree to visualize the topics and their messages.
  auto topicsTree = new QTreeView;
  topicsTree->setObjectName("plotTree");
  topicsTree->setAnimated(true);
  topicsTree->setHeaderHidden(true);
  topicsTree->setExpandsOnDoubleClick(true);
  topicsTree->setModel(this->dataPtr->topicsModel);
  topicsTree->setItemDelegate(plotItemDelegate);
  topicsTree->setEditTriggers(QAbstractItemView::NoEditTriggers);
  topicsTree->setDragEnabled(true);
  topicsTree->setDragDropMode(QAbstractItemView::DragOnly);

  // The model that will hold data to be displayed in the model tree view
  this->dataPtr->modelsModel = new PlotItemModel;
  this->dataPtr->modelsModel->setObjectName("plotModelsModel");
  this->dataPtr->modelsModel->setParent(this);
  this->FillModels(this->dataPtr->modelsModel);

  // A proxy model to filter models model
  this->dataPtr->searchModelsModel = new SearchModel;
  this->dataPtr->searchModelsModel->setFilterRole(
      PlotItemDelegate::DISPLAY_NAME);
  this->dataPtr->searchModelsModel->setSourceModel(this->dataPtr->modelsModel);

  // A tree to visualize models and their properties
  auto modelsTree = new QTreeView;
  modelsTree->setObjectName("plotTree");
  modelsTree->setAnimated(true);
  modelsTree->setHeaderHidden(true);
  modelsTree->setExpandsOnDoubleClick(true);
  modelsTree->setModel(this->dataPtr->modelsModel);
  modelsTree->setItemDelegate(plotItemDelegate);
  modelsTree->setEditTriggers(QAbstractItemView::NoEditTriggers);
  modelsTree->setDragEnabled(true);
  modelsTree->setDragDropMode(QAbstractItemView::DragOnly);

  // The model that will hold data to be displayed in the sim tree view
  this->dataPtr->simModel = new PlotItemModel;
  this->FillSim(this->dataPtr->simModel);

  // A proxy model to filter sim model
  this->dataPtr->searchSimModel = new SearchModel;
  this->dataPtr->searchSimModel->setFilterRole(
      PlotItemDelegate::DISPLAY_NAME);
  this->dataPtr->searchSimModel->setSourceModel(this->dataPtr->simModel);

  // A tree to visualize sim variables
  auto simTree = new QTreeView;
  simTree->setObjectName("plotTree");
  simTree->setAnimated(true);
  simTree->setHeaderHidden(true);
  simTree->setExpandsOnDoubleClick(true);
  simTree->setModel(this->dataPtr->simModel);
  simTree->setItemDelegate(plotItemDelegate);
  simTree->setEditTriggers(QAbstractItemView::NoEditTriggers);
  simTree->setDragEnabled(true);
  simTree->setDragDropMode(QAbstractItemView::DragOnly);

  // Search field
  auto searchIcon = new QLabel();
  searchIcon->setPixmap(QPixmap(":/images/search.svg"));

  auto searchEdit = new QLineEdit();
  searchEdit->setObjectName("plotLineEdit");
  this->connect(searchEdit, SIGNAL(textChanged(QString)), this,
      SLOT(UpdateSearch(QString)));

  auto searchField = new QHBoxLayout();
  searchField->addWidget(searchIcon);
  searchField->addWidget(searchEdit);

  // A tree to visualize topics search results
  this->dataPtr->searchTopicsTree = new QTreeView;
  this->dataPtr->searchTopicsTree->setObjectName("plotTree");
  this->dataPtr->searchTopicsTree->setAnimated(true);
  this->dataPtr->searchTopicsTree->setHeaderHidden(true);
  this->dataPtr->searchTopicsTree->setExpandsOnDoubleClick(true);
  this->dataPtr->searchTopicsTree->setModel(this->dataPtr->searchTopicsModel);
  this->dataPtr->searchTopicsTree->setItemDelegate(plotItemDelegate);
  this->dataPtr->searchTopicsTree->setEditTriggers(
      QAbstractItemView::NoEditTriggers);
  this->dataPtr->searchTopicsTree->setDragEnabled(true);
  this->dataPtr->searchTopicsTree->setDragDropMode(QAbstractItemView::DragOnly);

  // A tree to visualize models search results
  this->dataPtr->searchModelsTree = new QTreeView;
  this->dataPtr->searchModelsTree->setObjectName("plotTree");
  this->dataPtr->searchModelsTree->setAnimated(true);
  this->dataPtr->searchModelsTree->setHeaderHidden(true);
  this->dataPtr->searchModelsTree->setExpandsOnDoubleClick(true);
  this->dataPtr->searchModelsTree->setModel(this->dataPtr->searchModelsModel);
  this->dataPtr->searchModelsTree->setItemDelegate(plotItemDelegate);
  this->dataPtr->searchModelsTree->setEditTriggers(
      QAbstractItemView::NoEditTriggers);
  this->dataPtr->searchModelsTree->setDragEnabled(true);
  this->dataPtr->searchModelsTree->setDragDropMode(QAbstractItemView::DragOnly);

  // A tree to visualize sim search results
  this->dataPtr->searchSimTree = new QTreeView;
  this->dataPtr->searchSimTree->setObjectName("plotTree");
  this->dataPtr->searchSimTree->setAnimated(true);
  this->dataPtr->searchSimTree->setHeaderHidden(true);
  this->dataPtr->searchSimTree->setExpandsOnDoubleClick(true);
  this->dataPtr->searchSimTree->setModel(this->dataPtr->searchSimModel);
  this->dataPtr->searchSimTree->setItemDelegate(plotItemDelegate);
  this->dataPtr->searchSimTree->setEditTriggers(
      QAbstractItemView::NoEditTriggers);
  this->dataPtr->searchSimTree->setDragEnabled(true);
  this->dataPtr->searchSimTree->setDragDropMode(QAbstractItemView::DragOnly);

  // Search layout
  auto topicsLabel = new QLabel(tr("TOPICS"));
  topicsLabel->setObjectName("plottingSearchLabel");

  auto topicsLayout = new QVBoxLayout();
  topicsLayout->addWidget(topicsLabel);
  topicsLayout->addWidget(this->dataPtr->searchTopicsTree);

  auto topicsWidget = new QWidget();
  topicsWidget->setLayout(topicsLayout);

  auto modelsLabel = new QLabel(tr("MODELS"));
  modelsLabel->setObjectName("plottingSearchLabel");

  auto modelsLayout = new QVBoxLayout();
  modelsLayout->addWidget(modelsLabel);
  modelsLayout->addWidget(this->dataPtr->searchModelsTree);

  auto modelsWidget = new QWidget();
  modelsWidget->setLayout(modelsLayout);

  auto simLabel = new QLabel(tr("SIM"));
  simLabel->setObjectName("plottingSearchLabel");

  auto simLayout = new QVBoxLayout();
  simLayout->addWidget(simLabel);
  simLayout->addWidget(this->dataPtr->searchSimTree);

  auto simWidget = new QWidget();
  simWidget->setLayout(simLayout);

  auto splitter = new QSplitter(Qt::Vertical, this);
  splitter->addWidget(topicsWidget);
  splitter->addWidget(modelsWidget);
  splitter->addWidget(simWidget);
  splitter->setCollapsible(0, false);
  splitter->setCollapsible(1, false);
  splitter->setCollapsible(2, false);
  splitter->setStretchFactor(0, 1);
  splitter->setStretchFactor(1, 1);
  splitter->setStretchFactor(2, 1);

  auto searchLayout = new QVBoxLayout();
  searchLayout->addLayout(searchField);
  searchLayout->addWidget(splitter);

  auto searchWidget = new QWidget();
  searchWidget->setLayout(searchLayout);

  // The stacked layout is used by the TabBar to switch active layouts
  auto tabStackedLayout = new QStackedLayout;
  tabStackedLayout->setContentsMargins(0, 0, 0, 0);
  tabStackedLayout->addWidget(topicsTree);
  tabStackedLayout->addWidget(modelsTree);
  tabStackedLayout->addWidget(simTree);
  tabStackedLayout->addWidget(searchWidget);

  // Connect TabBar to StackedLayout
  connect(tabBar, SIGNAL(currentChanged(int)),
          tabStackedLayout, SLOT(setCurrentIndex(int)));

  // Main frame
  auto mainFrameLayout = new QVBoxLayout;
  mainFrameLayout->addWidget(tabBar);
  mainFrameLayout->addLayout(tabStackedLayout);
  mainFrameLayout->setContentsMargins(0, 0, 0, 0);

  auto mainFrame = new QFrame(this);
  mainFrame->setObjectName("plotPaletteFrame");
  mainFrame->setLayout(mainFrameLayout);

  auto mainLayout = new QHBoxLayout;
  mainLayout->addWidget(mainFrame);
  mainLayout->setContentsMargins(0, 0, 0, 0);
  mainLayout->setSpacing(0);

  this->setMinimumWidth(350);
  this->setLayout(mainLayout);

  this->UpdateSearch("");
}

/////////////////////////////////////////////////
Palette::~Palette()
{
}

/////////////////////////////////////////////////
void Palette::FillTopics(QStandardItemModel *_topicsModel)
{
  // Get all topics, independent of message type
  std::set<std::string> topics;

  // Get all the unique topics
  auto msgTopics = transport::getAdvertisedTopics();
  for (auto msgTopic : msgTopics)
  {
    for (auto topic : msgTopic.second)
    {
      topics.emplace(topic);
    }
  }

  // Populate widget
  for (auto topic : topics)
  {
    // Shorten topic name
    std::string prefix = "/gazebo/default";
    auto shortName = topic;
    auto idX = shortName.find(prefix);
    if (idX != std::string::npos)
      shortName.replace(0, prefix.size(), "~");

    auto topicItem = new QStandardItem();
    topicItem->setData(shortName.c_str(), PlotItemDelegate::DISPLAY_NAME);
    _topicsModel->appendRow(topicItem);

    // Create a message from this topic to find out its fields
    auto msgType = transport::getTopicMsgType(topic);
    if (msgType == "")
    {
      gzwarn << "Couldn't find message type for topic [" << topic << "]"
        << std::endl;
      return;
    }

    auto msg = msgs::MsgFactory::NewMsg(msgType);
    this->FillFromMsg(msg.get(), topicItem, topic+"?p=");
  }
}

/////////////////////////////////////////////////
void Palette::FillModels(QStandardItemModel *_modelsModel)
{
  gazebo::util::IntrospectionClient client;

  // Wait for the managers to come online
  auto managerIds = client.WaitForManagers(std::chrono::seconds(2));

  if (managerIds.empty())
  {
    std::cerr << "No introspection managers detected." << std::endl;
    std::cerr << "Is a gzserver running?" << std::endl;
    return;
  }

  // Pick up the first manager.
  std::string id = *managerIds.begin();

  // This is a blocking call
  std::set<std::string> items;
  if (!client.Items(id, items))
  {
    gzerr << "It wasn't possible to get items from introspection manager [" <<
        id << "]" << std::endl;
    return;
  }

  // FIXME: Check if there is at least one model?
  if (!items.empty())
  {
    auto title = new QStandardItem();
    title->setData("MODELS", PlotItemDelegate::DISPLAY_NAME);
    title->setData("title", PlotItemDelegate::TYPE);
    _modelsModel->appendRow(title);
  }

  // Populate widget
  for (auto item : items)
  {
    common::URI itemURI(item);

    // Only take data
    if (itemURI.Scheme() != "data")
      continue;

    // Only take model data
    auto pathStr = itemURI.Path().Str();
    if (pathStr.find("model") == std::string::npos)
      continue;

    // Make sure there is a query
    auto queryStr = itemURI.Query().Str();
    if (queryStr.empty())
      continue;

    // Process path
    auto pathParts = common::split(pathStr, "/");

    QStandardItem *previousItem = NULL;
    unsigned int i = 0;
    while (i < pathParts.size())
    {
      // Create model item based on part
      auto part = pathParts[i];
      auto nextPart = pathParts[i+1];
      if (part == "model")
      {
        // Check if it has already been added
        QStandardItem *existingItem = NULL;

        // Check top level (model)
        if (!previousItem)
        {
          auto modelList = _modelsModel->findItems(nextPart.c_str());
          if (!modelList.isEmpty())
            existingItem = modelList[0];
        }
        // Check a QStandardItem
        else
        {
          // TODO: Make a helper function for this
          for (int k = 0; k < previousItem->rowCount(); ++k)
          {
            auto childItem = previousItem->child(k, 0);
            if (childItem && childItem->text().toStdString() == nextPart)
            {
              existingItem = childItem;
              break;
            }
          }
        }

        if (!existingItem)
        {
          auto newItem = new QStandardItem(nextPart.c_str());
          newItem->setData(nextPart.c_str(),
              PlotItemDelegate::DISPLAY_NAME);
          newItem->setData("model", PlotItemDelegate::TYPE);

          if (!previousItem)
          {
            _modelsModel->appendRow(newItem);
            previousItem = newItem;
          }
          else
          {
            // Add title if there isn't one yet
            bool hasTitle = false;
            for (int k = 0; k < previousItem->rowCount(); ++k)
            {
              auto childItem = previousItem->child(k, 0);
              if (childItem &&
                  childItem->data(PlotItemDelegate::TYPE) == "title" &&
                  childItem->data(PlotItemDelegate::DISPLAY_NAME) == "MODELS")
              {
                hasTitle = true;
                break;
              }
            }

            if (!hasTitle)
            {
              auto title = new QStandardItem();
              title->setData("MODELS", PlotItemDelegate::DISPLAY_NAME);
              title->setData("title", PlotItemDelegate::TYPE);
              previousItem->appendRow(title);
            }

            previousItem->appendRow(newItem);
            previousItem = newItem;
          }
        }
        else
        {
          previousItem = existingItem;
        }
      }
      else if (part == "link")
      {
        // Check if it has already been added
        QStandardItem *existingItem = NULL;

        // Check top level (model)
        if (!previousItem)
        {
          gzerr << "A link cannot be outside of a model" << std::endl;
          continue;
        }
        // Check a QStandardItem
        else
        {
          // TODO: Make a helper function for this
          for (int k = 0; k < previousItem->rowCount(); ++k)
          {
            auto childItem = previousItem->child(k, 0);
            if (childItem && childItem->text().toStdString() == nextPart)
            {
              existingItem = childItem;
              break;
            }
          }
        }

        if (!existingItem)
        {
          auto newItem = new QStandardItem(nextPart.c_str());
          newItem->setData(nextPart.c_str(),
              PlotItemDelegate::DISPLAY_NAME);
          newItem->setData("link", PlotItemDelegate::TYPE);

          // Add title if there isn't one yet
          bool hasTitle = false;
          for (int k = 0; k < previousItem->rowCount(); ++k)
          {
            auto childItem = previousItem->child(k, 0);
            if (childItem &&
                childItem->data(PlotItemDelegate::TYPE) == "title" &&
                childItem->data(PlotItemDelegate::DISPLAY_NAME) == "LINKS")
            {
              hasTitle = true;
              break;
            }
          }

          if (!hasTitle)
          {
            auto title = new QStandardItem();
            title->setData("LINKS", PlotItemDelegate::DISPLAY_NAME);
            title->setData("title", PlotItemDelegate::TYPE);
            previousItem->appendRow(title);
          }

          previousItem->appendRow(newItem);
          previousItem = newItem;
        }
        else
        {
          previousItem = existingItem;
        }
      }
      else if (part == "joint")
      {
        // Check if it has already been added
        QStandardItem *existingItem = NULL;

        // Check top level (model)
        if (!previousItem)
        {
          gzerr << "A joint cannot be outside of a model" << std::endl;
          continue;
        }
        // Check a QStandardItem
        else
        {
          // TODO: Make a helper function for this
          for (int k = 0; k < previousItem->rowCount(); ++k)
          {
            auto childItem = previousItem->child(k, 0);
            if (childItem && childItem->text().toStdString() == nextPart)
            {
              existingItem = childItem;
              break;
            }
          }
        }

        if (!existingItem)
        {
          auto newItem = new QStandardItem(nextPart.c_str());
          newItem->setData(nextPart.c_str(),
              PlotItemDelegate::DISPLAY_NAME);
          newItem->setData("joint", PlotItemDelegate::TYPE);

          // Add title if there isn't one yet
          bool hasTitle = false;
          for (int k = 0; k < previousItem->rowCount(); ++k)
          {
            auto childItem = previousItem->child(k, 0);
            if (childItem &&
                childItem->data(PlotItemDelegate::TYPE) == "title" &&
                childItem->data(PlotItemDelegate::DISPLAY_NAME) == "JOINTS")
            {
              hasTitle = true;
              break;
            }
          }

          if (!hasTitle)
          {
            auto title = new QStandardItem();
            title->setData("JOINTS", PlotItemDelegate::DISPLAY_NAME);
            title->setData("title", PlotItemDelegate::TYPE);
            previousItem->appendRow(title);
          }

          previousItem->appendRow(newItem);
          previousItem = newItem;
        }
        else
        {
          previousItem = existingItem;
        }
      }
      i += 2;
    }

    if (!previousItem)
      return;

    // Process query
    queryStr = queryStr.substr(queryStr.find("=")+1);
    auto queryParts = common::split(queryStr, "/");

    std::string queryValue(queryParts[1]);
    for (unsigned int part = 2; part < queryParts.size(); ++part)
    {
      queryValue = queryValue + "/" + queryParts[part];
    }

    if (queryParts[0] == "pose3d")
    {
      this->InsertPoseItem(previousItem, itemURI, queryValue);
    }
    else if (queryParts[0] == "vector3d")
    {
      this->InsertVector3dItem(previousItem, itemURI, queryValue);
    }
    else if (queryParts[0] == "axis")
    {
      this->InsertAxisItem(previousItem, itemURI, queryValue);
    }
  }
}

/////////////////////////////////////////////////
void Palette::FillSim(QStandardItemModel *_simModel)
{
  // Hard-coded values for the sim tab
  std::multimap<std::string, std::string> simFields = {
      {"~/world_stats", "sim_time"},
      {"~/world_stats", "real_time"},
      {"~/world_stats", "iterations"}};

  for (auto field : simFields)
  {
    auto childItem = new QStandardItem();
    childItem->setDragEnabled(true);

    auto humanName = ConfigWidget::HumanReadableKey(field.second);
    childItem->setData(humanName.c_str(), PlotItemDelegate::DISPLAY_NAME);

    if (field.second == "iterations")
      childItem->setData("Uint 64", PlotItemDelegate::TYPE);
    else
      childItem->setData("Double", PlotItemDelegate::TYPE);

    // TODO: subclass QStandardItem and override setToolTip to do this
    // automatically
    std::string typeName =
        "<font size=3><p><b>Type</b>: " + childItem->data(
        PlotItemDelegate::TYPE).toString().toStdString() +
        "</p></font>";
    childItem->setToolTip(QString::fromStdString(typeName));

    std::string dataName = field.first + "?p=/" + field.second;
    childItem->setData(dataName.c_str(), PlotItemDelegate::URI_QUERY);

    _simModel->appendRow(childItem);
  }
}

/////////////////////////////////////////////////
void Palette::FillFromMsg(google::protobuf::Message *_msg,
    QStandardItem *_item, const std::string &_uri)
{
  if (!_msg || !_item)
    return;

  auto ref = _msg->GetReflection();
  if (!ref)
    return;

  auto descriptor = _msg->GetDescriptor();
  if (!descriptor)
    return;

  auto count = descriptor->field_count();

  // Go through all fields in this message
  for (int i = 0; i < count; ++i)
  {
    auto field = descriptor->field(i);
    if (!field)
      return;

    auto name = field->name();

    switch (field->type())
    {
      case google::protobuf::FieldDescriptor::TYPE_DOUBLE:
      case google::protobuf::FieldDescriptor::TYPE_FLOAT:
      case google::protobuf::FieldDescriptor::TYPE_INT64:
      case google::protobuf::FieldDescriptor::TYPE_UINT64:
      case google::protobuf::FieldDescriptor::TYPE_INT32:
      case google::protobuf::FieldDescriptor::TYPE_UINT32:
      case google::protobuf::FieldDescriptor::TYPE_BOOL:
      {
        auto humanName = ConfigWidget::HumanReadableKey(name);

        auto *childItem = new QStandardItem();
        childItem->setData(humanName.c_str(),
            PlotItemDelegate::DISPLAY_NAME);

        switch (field->type())
        {
          case google::protobuf::FieldDescriptor::TYPE_DOUBLE:
            childItem->setData("Double", PlotItemDelegate::TYPE);
            break;
          case google::protobuf::FieldDescriptor::TYPE_FLOAT:
            childItem->setData("Float", PlotItemDelegate::TYPE);
            break;
          case google::protobuf::FieldDescriptor::TYPE_INT64:
            childItem->setData("Int 64", PlotItemDelegate::TYPE);
            break;
          case google::protobuf::FieldDescriptor::TYPE_UINT64:
            childItem->setData("Uint 64", PlotItemDelegate::TYPE);
            break;
          case google::protobuf::FieldDescriptor::TYPE_INT32:
            childItem->setData("Int 32", PlotItemDelegate::TYPE);
            break;
          case google::protobuf::FieldDescriptor::TYPE_UINT32:
            childItem->setData("Uint 32", PlotItemDelegate::TYPE);
            break;
          case google::protobuf::FieldDescriptor::TYPE_BOOL:
            childItem->setData("Bool", PlotItemDelegate::TYPE);
            break;
          default:
            continue;
        }
        childItem->setToolTip(
            "<font size=3><p><b>Type</b>: " + childItem->data(
            PlotItemDelegate::TYPE).toString() +
            "</p></font>");

        std::string dataName = _uri + "/" + name;
        childItem->setData(dataName.c_str(), PlotItemDelegate::URI_QUERY);
        childItem->setDragEnabled(true);

        _item->appendRow(childItem);
        break;
      }
      // Message within a message
      case google::protobuf::FieldDescriptor::TYPE_MESSAGE:
      {
        if (field->is_repeated())
          continue;

        // Treat time as double
        if (field->message_type()->name() == "Time")
        {
          auto humanName = ConfigWidget::HumanReadableKey(name);
          std::string dataName = _uri + "/" + name;

          auto *childItem = new QStandardItem();
          childItem->setData(humanName.c_str(),
              PlotItemDelegate::DISPLAY_NAME);
          childItem->setData(dataName.c_str(), PlotItemDelegate::URI_QUERY);
          childItem->setData("Double", PlotItemDelegate::TYPE);
          childItem->setDragEnabled(true);
          childItem->setToolTip(
              "<font size=3><p><b>Type</b>: " + childItem->data(
              PlotItemDelegate::TYPE).toString() +
              "</p></font>");

          _item->appendRow(childItem);
        }
        // Custom RPY widgets for orientation
        else if (field->message_type()->name() == "Quaternion")
        {
          auto *quatItem = new QStandardItem();
          quatItem->setData(name.c_str(), PlotItemDelegate::DISPLAY_NAME);
          _item->appendRow(quatItem);

          std::vector<std::string> rpy = {"roll", "pitch", "yaw"};
          for (auto it : rpy)
          {
            auto humanName = ConfigWidget::HumanReadableKey(it);
            std::string dataName = _uri + "/" + name + "/" + it;

            auto *childItem = new QStandardItem();
            childItem->setData(QString::fromStdString(humanName),
                PlotItemDelegate::DISPLAY_NAME);
            childItem->setData(dataName.c_str(), PlotItemDelegate::URI_QUERY);
            childItem->setData("Double", PlotItemDelegate::TYPE);
            childItem->setToolTip(
                "<font size=3><p><b>Type</b>: " + childItem->data(
                PlotItemDelegate::TYPE).toString() +
                "</p></font>");
            childItem->setDragEnabled(true);

            quatItem->appendRow(childItem);
          }
        }
        // Create a collapsible list for submessages
        else
        {
          auto fieldMsg = (ref->MutableMessage(_msg, field));
          auto *childItem = new QStandardItem();
          childItem->setData(name.c_str(), PlotItemDelegate::DISPLAY_NAME);
          _item->appendRow(childItem);
          this->FillFromMsg(fieldMsg, childItem, _uri + "/" + name);
        }
        break;
      }
      default:
      {
        continue;
      }
    }
  }
}

/////////////////////////////////////////////////
void Palette::InsertPoseItem(QStandardItem *_item, const common::URI &_uri,
    const std::string &_query)
{
  // Pose
  auto poseItem = new QStandardItem(_query.c_str());
  poseItem->setData("Pose",
      PlotItemDelegate::DISPLAY_NAME);

  // Prepend so it's above titles
  _item->insertRow(0, poseItem);

  // Position
  auto positionItem = new QStandardItem();
  positionItem->setData("Position",
      PlotItemDelegate::DISPLAY_NAME);
  poseItem->appendRow(positionItem);

  common::URI positionURI(_uri.Str() + "/vector3d/position");

  this->InsertVector3dItem(positionItem, positionURI, _query);

  // Orientation
  auto orientationItem = new QStandardItem();
  orientationItem->setData("Orientation",
      PlotItemDelegate::DISPLAY_NAME);
  poseItem->appendRow(orientationItem);

  common::URI orientationURI(_uri.Str() + "/quaterniond/orientation");

  this->InsertQuaterniondItem(orientationItem, orientationURI, _query);
}

/////////////////////////////////////////////////
void Palette::InsertVector3dItem(QStandardItem *_item, const common::URI &_uri,
    const std::string &_query)
{
  // Use the full query as name by default
  auto subItem0Name = _query;

  // Use the input item as the immediate parent item by default
  auto parentItem = _item;

  // If it's linear velocity for example, place it under velocity -> linear
  bool isVel = _query.find("velocity") != std::string::npos;
  bool isAcc = _query.find("acceleration") != std::string::npos;
  bool isLin = _query.find("linear") != std::string::npos;
  bool isAng = _query.find("angular") != std::string::npos;

  if (isVel)
    subItem0Name = "velocity";
  else if (isAcc)
    subItem0Name = "acceleration";

  // Check if it has already been added
  QStandardItem *subItem0 = NULL;
  for (int k = 0; k < _item->rowCount(); ++k)
  {
    auto childItem = _item->child(k, 0);
    if (childItem && childItem->text().toStdString() == subItem0Name)
    {
      subItem0 = childItem;
      break;
    }
  }

  if (isVel || isAcc)
  {
    // Otherwise create new item
    if (!subItem0)
    {
      subItem0 = new QStandardItem(subItem0Name.c_str());

      // Prepend so it's above titles
      _item->insertRow(0, subItem0);
    }

    subItem0Name = ConfigWidget::HumanReadableKey(subItem0Name);
    subItem0->setData(subItem0Name.c_str(), PlotItemDelegate::DISPLAY_NAME);

    // Linear / Angular
    QString subItem1Name;
    if (isLin)
      subItem1Name = "Linear";
    else if (isAng)
      subItem1Name = "Angular";

    // We don't search for this because we assume no one else has added it yet
    auto subItem1 = new QStandardItem();
    subItem1->setData(subItem1Name, PlotItemDelegate::DISPLAY_NAME);
    subItem0->appendRow(subItem1);
    parentItem = subItem1;
  }

  // The Vector3d
  std::vector<std::string> elements = {"x", "y", "z"};
  for (auto element : elements)
  {
    auto humanName = ConfigWidget::HumanReadableKey(element);

    auto childItem = new QStandardItem();
    childItem->setData(humanName.c_str(),
        PlotItemDelegate::DISPLAY_NAME);
    childItem->setData((_uri.Str() + "/double/" + element).c_str(),
        PlotItemDelegate::URI_QUERY);
    childItem->setData("Double", PlotItemDelegate::TYPE);

    std::string typeName =
        "<font size=3><p><b>Type</b>: " + childItem->data(
        PlotItemDelegate::TYPE).toString().toStdString() +
        "</p></font>";
    childItem->setToolTip(QString::fromStdString(typeName));

    parentItem->appendRow(childItem);
  }
}

/////////////////////////////////////////////////
void Palette::InsertQuaterniondItem(QStandardItem *_item,
    const common::URI &_uri, const std::string &/*_query*/)
{
  // Use the input item as the immediate parent item by default
  auto parentItem = _item;

  // The Quaterniond
  std::vector<std::string> elements = {"roll", "pitch", "yaw"};
  for (auto element : elements)
  {
    auto humanName = ConfigWidget::HumanReadableKey(element);

    auto childItem = new QStandardItem();
    childItem->setData(humanName.c_str(),
        PlotItemDelegate::DISPLAY_NAME);
    childItem->setData((_uri.Str() + "/double/" + element).c_str(),
        PlotItemDelegate::URI_QUERY);
    childItem->setData("Double", PlotItemDelegate::TYPE);

    std::string typeName =
        "<font size=3><p><b>Type</b>: " + childItem->data(
        PlotItemDelegate::TYPE).toString().toStdString() +
        "</p></font>";
    childItem->setToolTip(QString::fromStdString(typeName));


    // Prepend so it's above titles
    parentItem->insertRow(0, childItem);
  }
}

/////////////////////////////////////////////////
void Palette::InsertAxisItem(QStandardItem *_item, const common::URI &_uri,
    const std::string &_query)
{
  // Position
  // - Axis 0
  // - Axis 1
  // Velocity
  // - Axis 0
  // - Axis 1

  // Position / velocity
  bool isPos = _query.find("position") != std::string::npos;
  bool isVel = _query.find("velocity") != std::string::npos;

  std::string subItem0Name;
  if (isPos)
    subItem0Name = "position";
  else if (isVel)
    subItem0Name = "velocity";
  else
  {
    gzwarn << "Query not supported [" << _query << "]" << std::endl;
    return;
  }

  // Axis
  int axis = 0;
  if (_query.find("1") != std::string::npos)
    axis = 1;
  else if (_query.find("2") != std::string::npos)
    axis = 2;

  // Check if it has already been added
  QStandardItem *subItem0 = NULL;
  for (int k = 0; k < _item->rowCount(); ++k)
  {
    auto childItem = _item->child(k, 0);
    if (childItem && childItem->text().toStdString() == subItem0Name)
    {
      subItem0 = childItem;
      break;
    }
  }

  if (isVel || isPos)
  {
    // Otherwise create new item
    if (!subItem0)
    {
      subItem0 = new QStandardItem(subItem0Name.c_str());

      // Prepend so it's above titles
      _item->insertRow(0, subItem0);
    }

    subItem0Name = ConfigWidget::HumanReadableKey(subItem0Name);
    subItem0->setData(subItem0Name.c_str(), PlotItemDelegate::DISPLAY_NAME);

    // Axis
    QString subItem1Name("Axis " + QString::number(axis));

    // We don't search for this because we assume no one else has added it yet
    auto subItem1 = new QStandardItem();
    subItem1->setData(subItem1Name, PlotItemDelegate::DISPLAY_NAME);
    subItem1->setData((_uri.Str()).c_str(),
        PlotItemDelegate::URI_QUERY);
    subItem1->setData("Double", PlotItemDelegate::TYPE);

    std::string typeName =
        "<font size=3><p><b>Type</b>: " + subItem1->data(
        PlotItemDelegate::TYPE).toString().toStdString() +
        "</p></font>";
    subItem1->setToolTip(QString::fromStdString(typeName));
    subItem0->appendRow(subItem1);
  }
}

/////////////////////////////////////////////////
void Palette::UpdateSearch(const QString &_search)
{
  this->dataPtr->searchTopicsModel->setSearch(_search);
  this->dataPtr->searchModelsModel->setSearch(_search);
  this->dataPtr->searchSimModel->setSearch(_search);

  // Expand / collapse
  this->ExpandChildren(this->dataPtr->searchTopicsModel,
      this->dataPtr->searchTopicsTree, QModelIndex());
  this->ExpandChildren(this->dataPtr->searchModelsModel,
      this->dataPtr->searchModelsTree, QModelIndex());
  this->ExpandChildren(this->dataPtr->searchSimModel,
      this->dataPtr->searchSimTree, QModelIndex());
}

/////////////////////////////////////////////////
void Palette::ExpandChildren(QSortFilterProxyModel *_model,
    QTreeView *_tree, const QModelIndex &_srcParent) const
{
  if (!_model || !_tree)
    return;

  for (int i = 0; i < _model->rowCount(_srcParent); ++i)
  {
    auto item = _model->index(i, 0, _srcParent);
    if (!item.isValid())
      return;

    bool expand = _model->data(item,
        PlotItemDelegate::TO_EXPAND).toBool();

    _tree->setExpanded(item, expand);

    this->ExpandChildren(_model, _tree, item);
  }
}
