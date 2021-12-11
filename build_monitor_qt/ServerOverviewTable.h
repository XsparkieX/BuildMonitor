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

#include <qstandarditemmodel.h>
#include <qtreewidget.h>

class ServerOverviewTable : public QTreeWidget
{
	Q_OBJECT

public:
	ServerOverviewTable(QWidget* parent);

	void setSettings(class Settings* inSettings);
	void setIcons(const QIcon* inSucceeded, const QIcon* inSucceededBuilding,
		const QIcon* inFailed, const QIcon* inFailedBuilding, const QIcon* inUnknown);
	void setProjectInformation(const std::vector<ProjectsFFI>& inProjectInformation);

Q_SIGNALS:
	void volunteerToFix(const uint64_t projectID);
	void viewBuildLog(const uint64_t projectID);

private:
	void tick();
	void openContextMenu(const QPoint& location);
	void onTreeRowDoubleClicked(const class QModelIndex& index);
	void onTreeRowItemChanged(class QTreeWidgetItem* item, int column);

	const QIcon* succeeded;
	const QIcon* succeededBuilding;
	const QIcon* failed;
	const QIcon* failedBuilding;
	const QIcon* unknown;
	class Settings* settings;

	QStringList headerLabels;
	class QTimer* tickTimer;
};
