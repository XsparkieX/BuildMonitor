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

#include "BuildMonitor.h"

#include "Settings.h"
#include "SettingsDialog.h"
#include "TrayContextMenu.h"
#include "Utils.h"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <qdesktopservices.h>
#include <qevent.h>
#include <qmessagebox.h>
#include <qsettings.h>
#include <qstatusbar.h>
#include <qtimer.h>
#include <regex>
#include <sstream>

BuildMonitor::BuildMonitor(QWidget *parent) :
	QMainWindow(parent),
	communicationThreadRunning(false),
	buildMonitorHandle(nullptr),
	noInformationIcon(":/BuildMonitor/Resources/no_information.png"),
	successfulBuildIcon(":/BuildMonitor/Resources/successful_build.png"),
	successfulBuildInProgressIcon(":/BuildMonitor/Resources/successful_build_in-progress.png"),
	failedBuildIcon(":/BuildMonitor/Resources/failed_build.png"),
	failedBuildInProgressIcon(":/BuildMonitor/Resources/failed_build_in-progress.png"),
	tray(new QSystemTrayIcon(QIcon(), this)),
	trayContextMenu(new TrayContextMenu(this)),
	projectBuildStatusGlobal(ProjectStatusFFI::Unknown),
	projectBuildStatusGlobalIsBuilding(false),
	exitApplication(false)
{
	ui.setupUi(this);
	ui.serverOverviewTable->setIcons(&successfulBuildIcon, &successfulBuildInProgressIcon,
		&failedBuildIcon, &failedBuildInProgressIcon, &noInformationIcon);
	ui.serverOverviewTable->setSettings(&settings);

	connect(this, &BuildMonitor::projectInformationUpdated, this, &BuildMonitor::onProjectInformationUpdated);
	connect(this, &BuildMonitor::serverInformationUpdated, this, &BuildMonitor::onServerInformationUpdated);

	connect(&settings, &Settings::settingsChanged, this, &BuildMonitor::onSettingsChanged);
	if (!settings.loadSettings())
	{
		onSettingsChanged();
	}

	resize(settings.windowSizeX, settings.windowSizeY);
	move(settings.windowPosX, settings.windowPosY);
	if (settings.windowMaximized)
	{
		showMaximized();
	}

	connect(ui.actionAdd_to_Startup, &QAction::triggered, this, &BuildMonitor::addToStartup);
	connect(ui.actionRemove_from_Startup, &QAction::triggered, this, &BuildMonitor::removeFromStartup);
	connect(ui.actionExit, &QAction::triggered, this, &BuildMonitor::exit);
	connect(ui.actionSettings, &QAction::triggered, this, &BuildMonitor::showSettingsDialog);
	connect(ui.serverOverviewTable, &ServerOverviewTable::volunteerToFix, this, &BuildMonitor::onVolunteerToFix);
	connect(ui.serverOverviewTable, &ServerOverviewTable::viewBuildLog, this, &BuildMonitor::onViewBuildLog);

	tray->setContextMenu(trayContextMenu);
	connect(tray, &QSystemTrayIcon::messageClicked, this, &BuildMonitor::showWindow);
	connect(tray, &QSystemTrayIcon::activated, this, &BuildMonitor::onTrayActivated);
	connect(trayContextMenu, &TrayContextMenu::contextActionExecuted, this, &BuildMonitor::onTrayContextActionExecuted);

	updateIcons();
	tray->show();

	if (QCoreApplication::arguments().contains("--minimized"))
	{
		if (settings.closeToTrayOnStartup)
		{
			hide();
		}
		else
		{
			showMinimized();
		}
	}
	else
	{
		show();
	}
}

#if 0
void BuildMonitor::changeEvent(QEvent * event)
{
	QMainWindow::changeEvent(event);

	if (event->type() == QEvent::WindowStateChange)
	{
		if (isMinimized())
		{
			settings.saveSettings();
			hide();
		}
	}
}
#endif

void BuildMonitor::showEvent(QShowEvent* event)
{
	QMainWindow::showEvent(event);

	// It's possible that the icons were updated before the window was shown. If the window isn't shown yet,
	// the small icon and progress in the taskbar won't update its state.
	updateIcons();
}

void BuildMonitor::closeEvent(QCloseEvent* event)
{
	QMainWindow::closeEvent(event);
	settings.saveSettings();
	if (exitApplication)
	{
		close();
	}
	else
	{
		hide();
		event->ignore();
	}
}

void BuildMonitor::moveEvent(QMoveEvent* moveEvent)
{
	QMainWindow::moveEvent(moveEvent);

	setWindowPositionAndSize();
}

void BuildMonitor::resizeEvent(QResizeEvent* resizeEvent)
{
	QMainWindow::resizeEvent(resizeEvent);

	setWindowPositionAndSize();
}

void BuildMonitor::updateIcons()
{
	QIcon* icon = &noInformationIcon;
	switch (projectBuildStatusGlobal)
	{
	case ProjectStatusFFI::NotBuilt:
	case ProjectStatusFFI::Success:
		icon = projectBuildStatusGlobalIsBuilding ? &successfulBuildInProgressIcon : &successfulBuildIcon;
		break;

	case ProjectStatusFFI::Aborted:
	case ProjectStatusFFI::Unstable:
	case ProjectStatusFFI::Failed:
		icon = projectBuildStatusGlobalIsBuilding ? &failedBuildInProgressIcon : &failedBuildIcon;
		break;

	default:
		break;
	}

	tray->setIcon(*icon);
}

void BuildMonitor::showWindow()
{
	show();
	setWindowState((windowState() & ~Qt::WindowMinimized) | Qt::WindowActive);
	raise();
	activateWindow();
}

void BuildMonitor::addToStartup()
{
#ifdef _WIN32
	QSettings settings("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run", QSettings::NativeFormat);
	settings.setValue("BuildMonitor", "\"" + QCoreApplication::applicationFilePath().replace('/', '\\') + "\" --minimized");
#else
	QMessageBox::information(this, "Not supported", "Currently it's not supported to add application to startup on Linux, you can still do this manually. "
		"If you want it to startup minimized, you can pass the argument: '--minimized' on the command line.", QMessageBox::Close);
#endif
}

void BuildMonitor::removeFromStartup()
{
#ifdef _WIN32
	QSettings settings("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run", QSettings::NativeFormat);
	settings.remove("BuildMonitor");
#else
	QMessageBox::information(this, "Not supported", "Currently it's not supported to remove the application from startup on Linux.", QMessageBox::Close);
#endif
}

void BuildMonitor::exit()
{
	exitApplication = true;
	stopCommunicationThread();
	close();
}

void BuildMonitor::showSettingsDialog()
{
	auto settingsDialog = new SettingsDialog(this, settings);
	settingsDialog->setAttribute(Qt::WA_DeleteOnClose);
	settingsDialog->show();
}

void BuildMonitor::setWindowPositionAndSize()
{
	// Lame way to work around an issue with isMaximized()...
	QTimer::singleShot(25, this, [&]()
	{
		settings.windowMaximized = isMaximized();
		if (!settings.windowMaximized)
		{
			const QSize windowSize = size();
			settings.windowSizeX = windowSize.width();
			settings.windowSizeY = windowSize.height();

			const QPoint windowPos = pos();
			settings.windowPosX = windowPos.x();
			settings.windowPosY = windowPos.y();
		}
	});
}

void BuildMonitor::startCommunicationThread()
{
	assert(!communicationThreadRunning);

	communicationThreadRunning = true;
	communicationThread = std::thread([&] ()
	{
		buildMonitorHandle = bm_create("");
		assert(buildMonitorHandle);
		while (communicationThreadRunning)
		{
			if (bm_start_client(buildMonitorHandle, settings.serverAddress.c_str(), settings.multicast) > 0)
			{
				break;
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(1000));
		}

		while (communicationThreadRunning)
		{
			if (bm_refresh_projects(buildMonitorHandle) > 0)
			{
				const uint32_t numProjects = bm_get_num_projects(buildMonitorHandle);
				std::vector<ProjectsFFI> myProjects;
				myProjects.resize(numProjects);
				bm_acquire_projects(buildMonitorHandle, numProjects, myProjects.data());

				projectsMutex.lock();
				bm_release_projects(static_cast<uint32_t>(projects.size()), projects.data());
				projects = std::move(myProjects);
				projectsMutex.unlock();

				emit serverInformationUpdated();
				emit projectInformationUpdated();
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(1000));
		}

		bm_destroy(buildMonitorHandle);
		buildMonitorHandle = nullptr;
	});
}

void BuildMonitor::stopCommunicationThread()
{
	assert(communicationThreadRunning);
	communicationThreadRunning = false;
	communicationThread.join();
}

void BuildMonitor::onSettingsChanged()
{
	if (!exitApplication)
	{
		projects.clear();
		if (communicationThreadRunning)
		{
			stopCommunicationThread();
		}
		startCommunicationThread();
		emit projectInformationUpdated();
	}
}

void BuildMonitor::onTrayActivated(QSystemTrayIcon::ActivationReason reason)
{
	if (reason != QSystemTrayIcon::Context)
	{
		if (isHidden() || isMinimized())
		{
			showWindow();
		}
		else
		{
			hide();
		}
	}
}

void BuildMonitor::onTrayContextActionExecuted(TrayContextAction action)
{
	switch (action)
	{
	case TrayContextAction::Exit:
		exit();
		break;

	case TrayContextAction::Show:
		showWindow();
		break;
	}
}

void BuildMonitor::onProjectInformationUpdated()
{
	auto switchedToFailed = [](ProjectStatusFFI last, ProjectStatusFFI now)
	{
		return (now == ProjectStatusFFI::Failed || now == ProjectStatusFFI::Unstable || now == ProjectStatusFFI::Aborted) && last == ProjectStatusFFI::Success;
	};

	auto switchedToSuccess = [](ProjectStatusFFI last, ProjectStatusFFI now)
	{
		return now == ProjectStatusFFI::Success && (last == ProjectStatusFFI::Failed || last == ProjectStatusFFI::Unstable || last == ProjectStatusFFI::Aborted);
	};

	projectsMutex.lock();

	if (tray->supportsMessages())
	{
		auto showMessage = [this](const std::string& projectName, const std::vector<std::string>& culprits, bool broken)
		{
			std::stringstream message;
			if (broken)
			{
				message << "Broken by: ";
			}
			else
			{
				message << "Fixed by: ";
			}

			message << GetDisplayableUserList(culprits, settings.ignoreUserList, true, true);

			tray->showMessage(projectName.c_str(), message.str().c_str(),
				broken ? QSystemTrayIcon::Critical : QSystemTrayIcon::Information, 3000);
		};

		for (const auto& project : projects)
		{
			if (std::find(settings.notifyList.begin(), settings.notifyList.end(), project.id) == settings.notifyList.end())
			{
				continue;
			}

			if (lastProjectStatus.find(project.id) != lastProjectStatus.end())
			{
				if (switchedToFailed(lastProjectStatus[project.id], project.status))
				{
					std::string projectName(std::regex_replace(project.folder_name, std::regex("\\/"), " » ").c_str());
					projectName += project.project_name;
					showMessage(projectName, ToStringVector(project.culprits, project.culprits_num), true);
				}
				else if (switchedToSuccess(lastProjectStatus[project.id], project.status))
				{
					std::string projectName(std::regex_replace(project.folder_name, std::regex("\\/"), " » ").c_str());
					projectName += project.project_name;
					showMessage(projectName, ToStringVector(project.culprits, project.culprits_num), false);
				}
			}
		}
	}

	lastProjectStatus.clear();
	for (const auto& project : projects)
	{
		lastProjectStatus.emplace(project.id, project.status);
	}

	ui.serverOverviewTable->setProjectInformation(projects);

	projectsMutex.unlock();

	static const std::vector<ProjectStatusFFI> priorityList = {
		ProjectStatusFFI::Failed,
		ProjectStatusFFI::Unstable,
		ProjectStatusFFI::Aborted,
		ProjectStatusFFI::NotBuilt,
		ProjectStatusFFI::Success,
		ProjectStatusFFI::Disabled,
		ProjectStatusFFI::Unknown
	};
	auto newStatusIndex = priorityList.size() - 1;

	bool isBuilding = false;
	for (auto& project : projects)
	{
		if (std::find(settings.notifyList.begin(), settings.notifyList.end(), project.id) == settings.notifyList.end())
		{
			continue;
		}

		auto statusIndex = std::find(priorityList.begin(), priorityList.end(), project.status) - priorityList.begin();
		if (static_cast<decltype(newStatusIndex)>(statusIndex) < newStatusIndex)
		{
			newStatusIndex = statusIndex;
		}
		isBuilding |= project.is_building;
	}

	if (priorityList[newStatusIndex] != projectBuildStatusGlobal || isBuilding != projectBuildStatusGlobalIsBuilding)
	{
		projectBuildStatusGlobal = priorityList[newStatusIndex];
		projectBuildStatusGlobalIsBuilding = isBuilding;
		updateIcons();
	}
}

void BuildMonitor::onServerInformationUpdated()
{
	std::time_t now = std::time(nullptr);
	std::tm localTime = *std::localtime(&now);
	std::stringstream statusBarMessage;
	statusBarMessage << "Last updated: " << std::put_time(&localTime, "%Y-%m-%d %H:%M:%S %Z");

	ui.statusBar->showMessage(QString::fromStdString(statusBarMessage.str()));
}

void BuildMonitor::onVolunteerToFix(uint64_t projectID)
{
	if (buildMonitorHandle != 0)
	{
		bm_set_volunteer(buildMonitorHandle, projectID);
	}
}

void BuildMonitor::onViewBuildLog(uint64_t projectID)
{
	projectsMutex.lock();
	const auto project = std::find_if(projects.begin(), projects.end(),
		[projectID](const ProjectsFFI& project) { return project.id == projectID; });
	if (project != projects.end())
	{
		QDesktopServices::openUrl(QString(project->url) + "lastBuild/consoleText");
	}
	projectsMutex.unlock();
}
