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

#include "ServerOverviewTableEntry.h"

#include "ServerOverviewTable.h"
#include "Utils.h"

#include <chrono>
#include <qdatetime.h>
#include <regex>
#include <sstream>

namespace
{
	static void AppendMinutesAndSeconds(std::stringstream& stream, int64_t millis)
	{
		const int32_t minutes = static_cast<int32_t>(millis / 1000 / 60);
		const int32_t seconds = static_cast<int32_t>(millis / 1000 % 60);
		if (minutes > 0)
		{
			const char* text = minutes != 1 ? " minutes" : " minute";
			stream << minutes << text;
			if (seconds > 0)
			{
				stream << " and ";
			}
		}
		if (seconds > 0)
		{
			const char* seconds_text = seconds != 1 ? " seconds" : " second";
			stream << seconds << seconds_text;
		}

		if (minutes == 0 && seconds == 0)
		{
			const char* milliseconds_text = millis != 1 ? " milliseconds" : " millisecond";
			stream << millis << milliseconds_text;
		}
	}
}

ServerOverviewTableEntry::ServerOverviewTableEntry(ServerOverviewTable* table) :
	QTreeWidgetItem(table),
	projectID(std::numeric_limits<uint64_t>::max()),
	isBuilding(false),
	status(ProjectStatusFFI::Unknown),
	pendingDelete(false)
{
}

ServerOverviewTableEntry::ServerOverviewTableEntry(ServerOverviewTableEntry* entry) :
	QTreeWidgetItem(entry),
	projectID(std::numeric_limits<uint64_t>::max()),
	isBuilding(false),
	status(ProjectStatusFFI::Unknown),
	pendingDelete(false)
{
}

void ServerOverviewTableEntry::tick()
{
	if (isBuilding)
	{
		std::stringstream stream;
		const auto now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
		const int64_t millis = estimatedDuration - static_cast<int64_t>(now - timestamp);
		if (millis < 0)
		{
			stream << "Taking ";
		}
		AppendMinutesAndSeconds(stream, std::abs(millis));
		if (millis < 0)
		{
			stream << " longer";
		}
		setText(3, stream.str().c_str());
	}
}

void ServerOverviewTableEntry::update(const ProjectsFFI& project, bool notify,
	const std::vector<std::string>& ignoreUserList, const QIcon* succeeded,
	const QIcon* succeededBuilding, const QIcon* failed, const QIcon* failedBuilding, const QIcon* unknown)
{
	projectID = project.id;
	isBuilding = project.is_building;
	status = project.status;
	estimatedDuration = project.estimated_duration;
	timestamp = project.timestamp;

	auto ToString = [] (ProjectStatusFFI status)
	{
		switch (status)
		{
			case ProjectStatusFFI::Success: return "Success";
			case ProjectStatusFFI::Unstable: return "Unstable";
			case ProjectStatusFFI::Failed: return "Failed";
			case ProjectStatusFFI::NotBuilt: return "Not Built";
			case ProjectStatusFFI::Aborted: return "Aborted";
			case ProjectStatusFFI::Disabled: return "Disabled";
			case ProjectStatusFFI::Unknown: return "Unknown";
			default: return "Invalid status given.";
		}
	};

	setCheckState(0, notify ? Qt::Checked : Qt::Unchecked);
	if (project.status == ProjectStatusFFI::Disabled || project.status == ProjectStatusFFI::Unknown)
	{
		setIcon(1, *unknown);
	}
	else if (project.is_building)
	{
		if (project.status == ProjectStatusFFI::Success || project.status == ProjectStatusFFI::NotBuilt)
		{
			setIcon(1, *succeededBuilding);
		}
		else
		{
			setIcon(1, *failedBuilding);
		}
	}
	else
	{
		if (project.status == ProjectStatusFFI::Success || project.status == ProjectStatusFFI::NotBuilt)
		{
			setIcon(1, *succeeded);
		}
		else
		{
			setIcon(1, *failed);
		}
	}

	setText(1, QString(std::regex_replace(project.folder_name, std::regex("\\/"), " » ").c_str()) + project.project_name);
	setToolTip(1, project.url);
	setText(2, ToString(project.status));

	if (project.is_building)
	{
		std::stringstream stream;
		const auto now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
		AppendMinutesAndSeconds(stream, project.estimated_duration - static_cast<int64_t>(now - project.timestamp));
		setText(3, stream.str().c_str());
	}
	else 
	{
		setText(3, "-");
	}

	{
		std::stringstream stream;
		AppendMinutesAndSeconds(stream, project.is_building ? project.estimated_duration : project.duration);
		setText(4, stream.str().c_str());
	}

	if (project.last_successful_build_time > 0)
	{
		QDateTime lastSuccessfulBuild;
		lastSuccessfulBuild.setMSecsSinceEpoch(project.last_successful_build_time);
		setText(5, lastSuccessfulBuild.toLocalTime().toString("hh:mm dd-MM-yyyy"));
	}
	else
	{
		setText(5, "Unavailable");
	}

	setText(6, project.volunteer);

	std::string culprits = GetDisplayableUserList(ToStringVector(project.culprits, project.culprits_num),
		ignoreUserList, true, false);
	setText(7, culprits.c_str());
	setToolTip(7, culprits.c_str());
}
