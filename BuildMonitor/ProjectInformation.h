/* BuildMonitor - Monitor the state of projects in CI.
 * Copyright (C) 2017-2019 Sander Brattinga

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

#pragma once

#include "ProjectStatus.h"

#include <qstring.h>
#include <qurl.h>

#include <memory>
#include <vector>

class ProjectInformation
{
public:
	ProjectInformation() :
		status(EProjectStatus::Unknown),
		isBuilding(false),
		isIgnored(false),
		buildNumber(0),
		estimatedRemainingTime(0),
		inProgressFor(0),
		lastBuildDuration(0),
		lastSuccessfulBuildTime(-1)
	{
	}

	QString projectName;
	QUrl projectUrl;
	EProjectStatus status;
	bool isBuilding;
	bool isIgnored;
	qint32 buildNumber;
	qint64 estimatedRemainingTime;
	qint64 inProgressFor;
	qint64 lastBuildDuration;
	qint64 lastSuccessfulBuildTime;
	QString volunteer;
	std::vector<QString> initiatedBy;
};

class ProjectInformationFolder
{
public:
	ProjectInformationFolder()
	{
	}
	
	ProjectInformationFolder(const QString& inFolderName) :
		folderName(inFolderName)
	{
	}
	
	QString folderName;
	QUrl folderUrl;
	std::vector<std::shared_ptr<ProjectInformationFolder> > folders;
	std::vector<std::shared_ptr<ProjectInformation> > projects;
};

template<typename Fn>
void ForEachProjectInformation(const ProjectInformationFolder& folder, const Fn& function)
{
	for (const auto& element : folder.folders)
	{
		ForEachProjectInformation(*element, function);
	}
	
	for (const auto& element : folder.projects)
	{
		function(*element);
	}
}

template<typename Fn>
std::shared_ptr<ProjectInformation> FindProjectInformation(const ProjectInformationFolder& folder, const Fn& function)
{
	for (const auto& element : folder.folders)
	{
		const auto& result = FindProjectInformation(*element, function);
		if (result)
		{
			return result;
		}
	}
	
	for (const auto& element : folder.projects)
	{
		if (function(*element))
		{
			return element;
		}
	}
	
	return nullptr;	
}

template<typename Fn>
bool ForEachProjectInformationWithBreak(const ProjectInformationFolder& folder, const Fn& function)
{
	bool shouldBreak = false;
	for (const auto& element : folder.folders)
	{
		shouldBreak = ForEachProjectInformationWithBreak(*element, function);
		if (shouldBreak)
		{
			break;
		}
	}
	
	if (!shouldBreak)
	{
		for (const auto& element : folder.projects)
		{
			if (function(*element))
			{
				return true;
			}
		}
	}
	
	return false;
}
