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

#include <qstandarditemmodel.h>
#include <qtreewidget.h>

class ServerOverviewTable : public QTreeWidget
{
	Q_OBJECT

public:
	ServerOverviewTable(QWidget* parent);

	void setSettings(const class Settings* inSettings);
	void setIcons(const QIcon* inSucceeded, const QIcon* inSucceededBuilding,
		const QIcon* inFailed, const QIcon* inFailedBuilding);
	void setProjectInformation(const class ProjectInformationFolder& inProjectInformation);

Q_SIGNALS:
	void volunteerToFix(const QString& projectName);
	void viewBuildLog(const QString& projectName);

private:
	void openContextMenu(const QPoint& location);
	void onTreeRowDoubleClicked(const class QModelIndex& index);

	const QIcon* succeeded;
	const QIcon* succeededBuilding;
	const QIcon* failed;
	const QIcon* failedBuilding;
	const class Settings* settings;

	const class ProjectInformationFolder* projectInformation;
	QStringList headerLabels;
};
