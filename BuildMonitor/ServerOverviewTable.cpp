/* BuildMonitor - Monitor the state of projects in CI.
 * Copyright (C) 2017 Sander Brattinga

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

#include "ProjectInformation.h"

#include <qdatetime.h>
#include <qheaderview.h>
#include <qmenu.h>
#include <QMouseEvent>
#include <qtablewidget.h>
#include <qtextstream.h>
#include <qtooltip.h>

ServerOverviewTable::ServerOverviewTable(QWidget* parent) :
	QTreeWidget(parent),
	succeeded(nullptr),
	succeededBuilding(nullptr),
	failed(nullptr),
	failedBuilding(nullptr),
	projectInformation(nullptr)
{
	headerLabels.push_back("Project");
	headerLabels.push_back("Status");
	headerLabels.push_back("Remaining Time");
	headerLabels.push_back("Duration");
	headerLabels.push_back("Last Successful Build");
	headerLabels.push_back("Volunteer");
	headerLabels.push_back("Initiated By");

	setColumnCount(headerLabels.size());
	setHeaderLabels(headerLabels);

	setContextMenuPolicy(Qt::CustomContextMenu);
	connect(this, &ServerOverviewTable::customContextMenuRequested, this, &ServerOverviewTable::openContextMenu);
}

void ServerOverviewTable::setIcons(const QIcon* inSucceeded, const QIcon* inSucceededBuilding,
	const QIcon* inFailed, const QIcon* inFailedBuilding)
{
	succeeded = inSucceeded;
	succeededBuilding = inSucceededBuilding;
	failed = inFailed;
	failedBuilding = inFailedBuilding;
}

void ServerOverviewTable::setProjectInformation(const ProjectInformationFolder& inProjectInformation)
{
	projectInformation = &inProjectInformation;

	clear();

	qint32 numProjects = 0;
	
	std::vector<std::pair<QTreeWidgetItem*, const ProjectInformationFolder*> > foldersToParse;
	for (const std::shared_ptr<ProjectInformationFolder>& folder : inProjectInformation.folders)
	{
		foldersToParse.emplace_back(nullptr, folder.get());
	}
	
	std::vector<std::pair<QTreeWidgetItem*, const ProjectInformation*> > projectsToParse;
	while (!foldersToParse.empty())
	{
		bool isEmptyTree = true;
		ForEachProjectInformationWithBreak(*foldersToParse[0].second, [&isEmptyTree] (const ProjectInformation& info)
		{
			isEmptyTree = false;
			return true;
		});

		if (isEmptyTree)
		{
			foldersToParse.erase(foldersToParse.begin());
			continue;
		}
		
		QTreeWidgetItem* parent = new QTreeWidgetItem(foldersToParse[0].first);
		if (foldersToParse[0].first == nullptr)
		{
			addTopLevelItem(parent);
		}
		
		parent->setText(0, foldersToParse[0].second->folderName);		
		for (const std::shared_ptr<ProjectInformationFolder>& folder : foldersToParse[0].second->folders)
		{
			foldersToParse.emplace_back(parent, folder.get());
		}
		
		for (const std::shared_ptr<ProjectInformation>& info : foldersToParse[0].second->projects)
		{
			projectsToParse.emplace_back(parent, info.get());
		}
		
		foldersToParse.erase(foldersToParse.begin());
	}
		
	for (const std::pair<QTreeWidgetItem*, const ProjectInformation*>& pair : projectsToParse)
	{
		const ProjectInformation* info = pair.second;
		QTreeWidgetItem* item = new QTreeWidgetItem(pair.first);
		item->setText(0, info->projectName);
		item->setToolTip(0, info->projectUrl.toString());
		if (projectStatus_isFailure(info->status))
		{
			if (info->isBuilding && failedBuilding != nullptr)
			{
				item->setIcon(0, *failedBuilding);
			}
			else if (!info->isBuilding && failed != nullptr)
			{
				item->setIcon(0, *failed);
			}
		}
		else
		{
			if (info->isBuilding && succeededBuilding != nullptr)
			{
				item->setIcon(0, *succeededBuilding);
			}
			else if (!info->isBuilding && succeeded != nullptr)
			{
				item->setIcon(0, *succeeded);
			}
		}

		item->setText(1, projectStatus_toString(info->status));

		if (info->isBuilding)
		{
			qint32 estimatedRemainingTime = info->estimatedRemainingTime / 1000;
			const char* timeUnit = "minute(s)";
			if (estimatedRemainingTime < 60 && estimatedRemainingTime > -60)
			{
				timeUnit = "second(s)";
			}
			else
			{
				estimatedRemainingTime /= 60;
			}

			QString estimatedRemainingTimeFull;
			QTextStream estimatedRemainingTimeStream(&estimatedRemainingTimeFull);
			if (estimatedRemainingTime < 0)
			{
				estimatedRemainingTime = abs(estimatedRemainingTime);
				estimatedRemainingTimeStream << "Taking " << estimatedRemainingTime << " " << timeUnit << " longer";
			}
			else
			{
				estimatedRemainingTimeStream << estimatedRemainingTime << " " << timeUnit;
			}
			estimatedRemainingTimeStream.flush();

			item->setText(2, estimatedRemainingTimeFull);
		}
		else
		{
			item->setText(2, "-");
		}
		item->setText(3, QString::number(info->inProgressFor / 60 / 1000) + " minutes");

		if (info->lastSuccessfulBuildTime != -1)
		{
			QDateTime lastSuccessfulBuild;
			lastSuccessfulBuild.setMSecsSinceEpoch(info->lastSuccessfulBuildTime);
			item->setText(4, lastSuccessfulBuild.toLocalTime().toString("hh:mm dd-MM-yyyy"));
		}
		else
		{
			item->setText(4, "Unavailable");
		}

		item->setText(5, info->volunteer);

		QString initiators;
		for (size_t i = 0; i < info->initiatedBy.size(); ++i)
		{
			if (i != 0)
			{
				if (i == info->initiatedBy.size() - 1)
				{
					initiators += " and ";
				}
				else
				{
					initiators += ", ";
				}
			}
			initiators += info->initiatedBy[i];
		}
		item->setText(6, initiators);
		item->setToolTip(6, initiators);

		++numProjects;
	}
	
	expandAll();
	for (qint32 i = 0; i < headerLabels.size(); ++i)
	{
		resizeColumnToContents(i);
	}
	header()->setSectionResizeMode(headerLabels.size() - 1, QHeaderView::Stretch);
}

void ServerOverviewTable::openContextMenu(const QPoint& location)
{
	QPoint globalLocation = viewport()->mapToGlobal(location);

	QMenu contextMenu;

	QAction* volunteerToFixAction = contextMenu.addAction("Volunteer to Fix");
	QString projectName = itemAt(location)->text(0);
	bool volunteerOptionEnabled = false;
	if (projectInformation)
	{
		const auto& pos = FindProjectInformation(*projectInformation, [&projectName](const ProjectInformation& projectInformation) { return projectInformation.projectName == projectName; });
		volunteerOptionEnabled = pos && projectStatus_isFailure(pos->status);
	}
	volunteerToFixAction->setEnabled(volunteerOptionEnabled);

	QAction* viewBuildLogAction = contextMenu.addAction("View Build Log");
	bool viewBuildLogActionEnabled = false;
	if (projectInformation)
	{
		const auto& pos = FindProjectInformation(*projectInformation, [&projectName](const ProjectInformation& projectInformation) { return projectInformation.projectName == projectName; });
		viewBuildLogActionEnabled = pos && pos->buildNumber != 0;
	}
	viewBuildLogAction->setEnabled(viewBuildLogActionEnabled);

	const QAction* selectedContextMenuItem = contextMenu.exec(globalLocation);
	if (selectedContextMenuItem)
	{
		if (selectedContextMenuItem == volunteerToFixAction)
		{
			volunteerToFix(projectName);
		}
		else if (selectedContextMenuItem == viewBuildLogAction)
		{
			viewBuildLog(projectName);
		}
	}
}
