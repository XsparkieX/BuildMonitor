/* BuildMonitor - Monitor the state of projects in CI.
 * Copyright (C) 2017-2021 Sander Brattinga

 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "ServerOverviewTable.h"

#include "ServerOverviewTableEntry.h"
#include "Settings.h"

#include <algorithm>
#include <cassert>
#include <qdesktopservices.h>
#include <qheaderview.h>
#include <qmenu.h>
#include <QMouseEvent>
#include <qtablewidget.h>
#include <qtextstream.h>
#include <qtimer.h>
#include <qtooltip.h>
#include <vector>

ServerOverviewTable::ServerOverviewTable(QWidget* parent) :
	QTreeWidget(parent),
	succeeded(nullptr),
	succeededBuilding(nullptr),
	failed(nullptr),
	failedBuilding(nullptr),
	settings(nullptr),
	tickTimer(new QTimer(this))
{
	headerLabels.push_back("Notify");
	headerLabels.push_back("Project");
	headerLabels.push_back("Status");
	headerLabels.push_back("Remaining Time");
	headerLabels.push_back("Duration");
	headerLabels.push_back("Last Successful Build");
	headerLabels.push_back("Volunteer");
	headerLabels.push_back("Initiated By");

	connect(tickTimer, &QTimer::timeout, this, &ServerOverviewTable::tick);
	tickTimer->setSingleShot(false);
	tickTimer->start(1000);

	setColumnCount(headerLabels.size());
	setHeaderLabels(headerLabels);

	setContextMenuPolicy(Qt::CustomContextMenu);
	connect(this, &ServerOverviewTable::customContextMenuRequested, this, &ServerOverviewTable::openContextMenu);
	connect(this, &ServerOverviewTable::doubleClicked, this, &ServerOverviewTable::onTreeRowDoubleClicked);
	connect(this, &ServerOverviewTable::itemChanged, this, &ServerOverviewTable::onTreeRowItemChanged);
}

void ServerOverviewTable::setSettings(Settings* inSettings)
{
	settings = inSettings;
}

void ServerOverviewTable::setIcons(const QIcon* inSucceeded, const QIcon* inSucceededBuilding,
	const QIcon* inFailed, const QIcon* inFailedBuilding, const QIcon* inUnknown)
{
	succeeded = inSucceeded;
	succeededBuilding = inSucceededBuilding;
	failed = inFailed;
	failedBuilding = inFailedBuilding;
	unknown = inUnknown;
}

void ServerOverviewTable::setProjectInformation(const std::vector<ProjectsFFI>& projectInformation)
{
	assert(settings);

	// NOTE: Mark all items pending delete, so we know which ones need to be removed from
	//       the table at the end.
	{
		const auto numItems = topLevelItemCount();
		for (auto index = 0; index < numItems; ++index)
		{
			static_cast<ServerOverviewTableEntry*>(topLevelItem(index))->pendingDelete = true;
		}
	}

	for (const auto& project : projectInformation)
	{
		if (project.status == ProjectStatusFFI::Disabled && !settings->showDisabledProjects)
		{
			continue;
		}

		const auto numItems = topLevelItemCount();
		ServerOverviewTableEntry* item = nullptr;
		for (auto index = 0; index < numItems; ++index)
		{
			auto rowItem = static_cast<ServerOverviewTableEntry*>(topLevelItem(index));
			if (rowItem->projectID == project.id)
			{
				item = rowItem;
				item->pendingDelete = false;
				break;
			}
		}

		if (!item)
		{
			item = new ServerOverviewTableEntry(this);
			addTopLevelItem(item);
		}

		const auto notify = std::find(settings->notifyList.begin(), settings->notifyList.end(), project.id) !=
			settings->notifyList.end();
		item->update(project, notify, settings->ignoreUserList, succeeded, succeededBuilding, failed, failedBuilding, unknown);
	}

	// NOTE: Remove items that are no longer in use.
	{
		const auto numItems = topLevelItemCount();
		for (auto index = numItems - 1; index >= 0; --index)
		{
			if (static_cast<ServerOverviewTableEntry*>(topLevelItem(index))->pendingDelete)
			{
				delete takeTopLevelItem(index);
			}
		}
	}

	// NOTE: Sort them by project name.
	sortByColumn(1, Qt::AscendingOrder);

	// NOTE: Fix the layout of the table once we have all information set.
	expandAll();
	for (qint32 i = 0; i < headerLabels.size(); ++i)
	{
		resizeColumnToContents(i);
	}
	header()->setSectionResizeMode(headerLabels.size() - 1, QHeaderView::Stretch);
}

void ServerOverviewTable::tick()
{
	const auto numItems = topLevelItemCount();
	for (auto index = 0; index < numItems; ++index)
	{
		static_cast<ServerOverviewTableEntry*>(topLevelItem(index))->tick();
	}
}

void ServerOverviewTable::openContextMenu(const QPoint& location)
{
	QPoint globalLocation = viewport()->mapToGlobal(location);

	QMenu contextMenu;

	QAction* volunteerToFixAction = contextMenu.addAction("Volunteer to Fix");
	QAction* viewBuildLogAction = contextMenu.addAction("View Build Log");
	if (const ServerOverviewTableEntry* item = static_cast<ServerOverviewTableEntry*>(itemAt(location)))
	{
		volunteerToFixAction->setEnabled(item->status == ProjectStatusFFI::Failed ||
			item->status == ProjectStatusFFI::Unstable || item->status == ProjectStatusFFI::Aborted);
		viewBuildLogAction->setEnabled(true);
	}
	else
	{
		volunteerToFixAction->setEnabled(false);
		viewBuildLogAction->setEnabled(false);
	}

	const QAction* selectedContextMenuItem = contextMenu.exec(globalLocation);
	if (selectedContextMenuItem)
	{
		if (selectedContextMenuItem == volunteerToFixAction)
		{
			if (const ServerOverviewTableEntry* item = static_cast<ServerOverviewTableEntry*>(itemAt(location)))
			{
				emit volunteerToFix(item->projectID);
			}
		}
		else if (selectedContextMenuItem == viewBuildLogAction)
		{
			if (const ServerOverviewTableEntry* item = static_cast<ServerOverviewTableEntry*>(itemAt(location)))
			{
				emit viewBuildLog(item->projectID);
			}
		}
	}
}

void ServerOverviewTable::onTreeRowDoubleClicked(const QModelIndex& index)
{
	const QTreeWidgetItem* item = itemFromIndex(index);
	if (item)
	{
		QDesktopServices::openUrl(item->toolTip(1));
	}
}

void ServerOverviewTable::onTreeRowItemChanged(QTreeWidgetItem* item, int column)
{
	if (settings && item && column == 0)
	{
		if (item->checkState(0) == Qt::Checked)
		{
			auto row = static_cast<ServerOverviewTableEntry*>(item);
			if (std::find(settings->notifyList.begin(), settings->notifyList.end(), row->projectID) == settings->notifyList.end())
			{
				settings->notifyList.emplace_back(row->projectID);
				settings->saveSettings();
			}
		}
		else
		{
			auto row = static_cast<ServerOverviewTableEntry*>(item);
			auto remove = std::remove(settings->notifyList.begin(), settings->notifyList.end(), row->projectID);
			if (remove != settings->notifyList.end())
			{
				settings->notifyList.erase(remove, settings->notifyList.end());
				settings->saveSettings();
			}
		}
	}
}

