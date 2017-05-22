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

#pragma once

#include "FixInformation.h"
#include "ProjectInformation.h"
#include "ProjectStatus.h"
#include "Settings.h"
#include "TrayContextAction.h"

#include <qsystemtrayicon.h>

#include <QtWidgets/QMainWindow>
#include "ui_BuildMonitor.h"

class BuildMonitor : public QMainWindow
{
	Q_OBJECT

public:
	BuildMonitor(QWidget* parent = Q_NULLPTR);

#if 0
	virtual void changeEvent(QEvent* event) override;
#endif
	virtual void showEvent(QShowEvent* event) override;
	virtual void closeEvent(QCloseEvent* event) override;
	virtual void moveEvent(QMoveEvent* moveEvent) override;
	virtual void resizeEvent(QResizeEvent* resizeEvent) override;

private:
	void updateIcons();
	void updateTaskbarProgress();
	void showWindow();
	void addToStartup();
	void removeFromStartup();
	void exit();
	void showSettings();
	void setWindowPositionAndSize();

	void onSettingsChanged();
	void onTrayActivated(QSystemTrayIcon::ActivationReason reason);
	void onTrayContextActionExecuted(TrayContextAction action);
	void onProjectInformationUpdated(const std::vector<ProjectInformation>& projectInformation);
	void onFixInformationUpdated(const std::vector<FixInformation>& fixInformation);
	void onProjectInformationError(const QString& errorMessage);
	void onTableRowDoubleClicked(const class QModelIndex& index);
	void onVolunteerToFix(const QString& projectName);
	void onViewBuildLog(const QString& projectName);

	Ui::BuildMonitorClass ui;
	QIcon noInformationIcon;
	QIcon successfulBuildIcon;
	QIcon successfulBuildInProgressIcon;
	QIcon failedBuildIcon;
	QIcon failedBuildInProgressIcon;
	class QSystemTrayIcon* tray;
	class TrayContextMenu* trayContextMenu;
#ifdef _MSC_VER
	class QWinTaskbarButton* winTaskbarButton;
#endif
	Settings settings;
	class SettingsDialog* settingsDialog;
	class BuildMonitorServerCommunication* buildMonitorServerCommunication;
	class JenkinsCommunication* jenkins;
	EProjectStatus projectBuildStatusGlobal;
	bool projectBuildStatusGlobalIsBuilding;
	std::vector<ProjectInformation> lastProjectInformation;
	bool exitApplication;
};
