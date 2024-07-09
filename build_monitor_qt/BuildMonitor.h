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
#include "Settings.h"
#include "TrayContextAction.h"

#include <atomic>
#include <map>
#include <mutex>
#include <qsystemtrayicon.h>
#include <thread>
#include <vector>

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
	void showWindow();
	void addToStartup();
	void removeFromStartup();
	void exit();
	void showSettingsDialog();
	void setWindowPositionAndSize();
	void startCommunicationThread();
	void stopCommunicationThread();

	void onSettingsChanged(bool serverSettingsChanged);
	void onTrayActivated(QSystemTrayIcon::ActivationReason reason);
	void onTrayContextActionExecuted(TrayContextAction action);
	void onProjectInformationUpdated();
	void onServerInformationUpdated();
	void onVolunteerToFix(uint64_t projectID);
	void onViewBuildLog(uint64_t projectID);

	std::thread communicationThread;
	std::atomic<bool> communicationThreadRunning;
	std::atomic<void*> buildMonitorHandle;
	std::vector<ProjectsFFI> projects;
	std::map<uint64_t, ProjectStatusFFI> lastProjectStatus;
	std::mutex projectsMutex;

	Ui::BuildMonitorClass ui;
	QIcon noInformationIcon;
	QIcon successfulBuildIcon;
	QIcon successfulBuildInProgressIcon;
	QIcon failedBuildIcon;
	QIcon failedBuildInProgressIcon;
	class QSystemTrayIcon* tray;
	class TrayContextMenu* trayContextMenu;
#ifdef _WIN32
	class QWinTaskbarButton* winTaskbarButton;
#endif
	Settings settings;
	ProjectStatusFFI projectBuildStatusGlobal;
	bool projectBuildStatusGlobalIsBuilding;
	bool exitApplication;

Q_SIGNALS:
	void projectInformationUpdated();
	void serverInformationUpdated();
};
