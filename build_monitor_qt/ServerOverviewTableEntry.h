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

#pragma once

#include "build_monitor.h"

#include <qtreewidget.h>
#include <string>
#include <vector>

class ServerOverviewTableEntry : public QTreeWidgetItem
{
public:
	ServerOverviewTableEntry(class ServerOverviewTable* table);
	ServerOverviewTableEntry(class ServerOverviewTableEntry* entry);

	void tick();
	void update(const ProjectsFFI& project, bool notify, const std::vector<std::string>& ignoreUserList,
		const class QIcon* succeeded, const class QIcon* succeededBuilding, const class QIcon* failed,
		const class QIcon* failedBuilding, const class QIcon* unknown);

	uint64_t projectID;
	bool isBuilding;
	ProjectStatusFFI status;
	uint64_t estimatedDuration;
	uint64_t timestamp;

	bool pendingDelete;
};
