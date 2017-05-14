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

#include "BuildMonitor.h"

#include "BuildMonitorServerCommunication.h"
#include "JenkinsCommunication.h"
#include "ProjectInformation.h"
#include "Settings.h"
#include "SettingsDialog.h"

#include <qdesktopservices.h>
#include <qevent.h>
#include <qmessagebox.h>
#include <qsettings.h>
#include <qtimer.h>
#ifdef _MSC_VER
#include <qwintaskbarbutton.h>
#include <qwintaskbarprogress.h>
#endif

BuildMonitor::BuildMonitor(QWidget *parent) :
	QMainWindow(parent),
	noInformationIcon(":/BuildMonitor/Resources/no_information.png"),
	successfulBuildIcon(":/BuildMonitor/Resources/successful_build.png"),
	successfulBuildInProgressIcon(":/BuildMonitor/Resources/successful_build_in-progress.png"),
	failedBuildIcon(":/BuildMonitor/Resources/failed_build.png"),
	failedBuildInProgressIcon(":/BuildMonitor/Resources/failed_build_in-progress.png"),
	tray(new QSystemTrayIcon(QIcon(), this)),
#ifdef _MSC_VER
	winTaskbarButton(new QWinTaskbarButton(this)),
#endif
	settingsDialog(nullptr),
	buildMonitorServerCommunication(new BuildMonitorServerCommunication(this)),
	jenkins(new JenkinsCommunication(this)),
	projectBuildStatusGlobal(EProjectStatus::Unknown),
	exitApplication(false)
{
	ui.setupUi(this);
	ui.serverOverviewTable->setIcons(&successfulBuildIcon, &successfulBuildInProgressIcon,
		&failedBuildIcon, &failedBuildInProgressIcon);

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

	connect(jenkins, &JenkinsCommunication::projectInformationError, this, &BuildMonitor::onProjectInformationError);
	connect(jenkins, &JenkinsCommunication::projectInformationUpdated, this, &BuildMonitor::onProjectInformationUpdated);

	connect(buildMonitorServerCommunication, &BuildMonitorServerCommunication::onFixInformationUpdated, this, &BuildMonitor::onFixInformationUpdated);

	connect(ui.actionAdd_to_Startup, &QAction::triggered, this, &BuildMonitor::addToStartup);
	connect(ui.actionRemove_from_Startup, &QAction::triggered, this, &BuildMonitor::removeFromStartup);
	connect(ui.actionExit, &QAction::triggered, this, &BuildMonitor::exit);
	connect(ui.actionSettings, &QAction::triggered, this, &BuildMonitor::showSettings);
	connect(ui.serverOverviewTable, &ServerOverviewTable::doubleClicked, this, &BuildMonitor::onTableRowDoubleClicked);
	connect(ui.serverOverviewTable, &ServerOverviewTable::volunteerToFix, this, &BuildMonitor::onVolunteerToFix);
	connect(ui.serverOverviewTable, &ServerOverviewTable::viewBuildLog, this, &BuildMonitor::onViewBuildLog);

	tray->show();
	connect(tray, &QSystemTrayIcon::messageClicked, this, &BuildMonitor::showWindow);
	connect(tray, &QSystemTrayIcon::activated, this, &BuildMonitor::onTrayActivated);

	updateIcons();

	if (QCoreApplication::arguments().contains("--minimized"))
	{
		if (settings.closeToTrayOnStartup)
		{
			close();
		}
		else
		{
			showMinimized();
		}
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
	case EProjectStatus::Aborted:
	case EProjectStatus::NotBuilt:
	case EProjectStatus::Succeeded:
		icon = projectBuildStatusGlobalIsBuilding ? &successfulBuildInProgressIcon : &successfulBuildIcon;
		break;

	case EProjectStatus::Failed:
	case EProjectStatus::Unstable:
		icon = projectBuildStatusGlobalIsBuilding ? &failedBuildInProgressIcon : &failedBuildIcon;
		break;

	default:
		break;
	}

	tray->setIcon(*icon);
#if _MSC_VER
	winTaskbarButton->setWindow(windowHandle());
	winTaskbarButton->setOverlayIcon(*icon);
#endif
}

void BuildMonitor::updateTaskbarProgress()
{
#if _MSC_VER
	if (!settings.showProgressForProject.isEmpty())
	{
		for (const ProjectInformation& info : lastProjectInformation)
		{
			if (info.projectName == settings.showProgressForProject)
			{
				if (info.isBuilding)
				{
					const qint32 totalDuration = info.estimatedRemainingTime + info.inProgressFor;
					if (totalDuration > 0)
					{
						QWinTaskbarProgress* progress = winTaskbarButton->progress();
						progress->setVisible(true);
						progress->setValue(info.inProgressFor / static_cast<float>(totalDuration) * progress->maximum());
					}
				}
				else if (winTaskbarButton->progress()->isVisible())
				{
					winTaskbarButton->progress()->setVisible(false);
				}

				break;
			}
		}
	}
	else if (projectStatus_isFailure(projectBuildStatusGlobal) && projectBuildStatusGlobalIsBuilding)
	{
		float progressToDisplay = 0.0f;
		for (const ProjectInformation& info : lastProjectInformation)
		{
			if (projectStatus_isFailure(info.status) && info.isBuilding)
			{
				const qint32 totalDuration = info.estimatedRemainingTime + info.inProgressFor;
				if (totalDuration > 0)
				{
					const float progress = info.inProgressFor / static_cast<float>(totalDuration);
					if (progress > progressToDisplay)
					{
						progressToDisplay = progress;
					}
				}
			}
		}
		progressToDisplay = progressToDisplay > 1.0f ? 1.0f : progressToDisplay;
		progressToDisplay = progressToDisplay < 0.0f ? 0.0f : progressToDisplay;

		QWinTaskbarProgress* progress = winTaskbarButton->progress();
		progress->setVisible(true);
		progress->setValue(progressToDisplay * progress->maximum());
	}
	else if (winTaskbarButton->progress()->isVisible())
	{
		winTaskbarButton->progress()->setVisible(false);
	}
#endif
}

void BuildMonitor::showWindow()
{
	show();
	raise();
	activateWindow();
	setFocus();
}

void BuildMonitor::addToStartup()
{
#if _MSC_VER
	QSettings settings("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run", QSettings::NativeFormat);
	settings.setValue("BuildMonitor", "\"" + QCoreApplication::applicationFilePath().replace('/', '\\') + "\" --minimized");
#else
	QMessageBox::information(this, "Not supported", "Currently it's not supported to add application to startup on Linux, you can still do this manually. "
		"If you want it to startup minimized, you can pass the argument: '--minimized' on the command line.", QMessageBox::Close);
#endif
}

void BuildMonitor::removeFromStartup()
{
#if _MSC_VER
	QSettings settings("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run", QSettings::NativeFormat);
	settings.remove("BuildMonitor");
#else
	QMessageBox::information(this, "Not supported", "Currently it's not supported to remove the application from startup on Linux.", QMessageBox::Close);
#endif
}

void BuildMonitor::exit()
{
	exitApplication = true;
	close();
}

void BuildMonitor::showSettings()
{
	if (!settingsDialog)
	{
		settingsDialog = new SettingsDialog(this, settings);
	}
	settingsDialog->show();
}

void BuildMonitor::setWindowPositionAndSize()
{
	// Lame way to work around an issue with isMaximized()...
	QTimer::singleShot(25, [&]()
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

void BuildMonitor::onSettingsChanged()
{
	jenkins->setIgnoreUserList(settings.ignoreUserList);
	jenkins->setShowDisabledProjects(settings.showDisabledProjects);
	jenkins->setProjectIncludePattern(settings.projectRegEx);
	jenkins->setServerURLs(settings.serverURLs);
	jenkins->setRefreshInterval(settings.refreshIntervalInSeconds);

	buildMonitorServerCommunication->setServerAddress(settings.fixServerAddress);

	jenkins->refresh();
}

void BuildMonitor::onTrayActivated(QSystemTrayIcon::ActivationReason /* reason */)
{
	showWindow();
}

void BuildMonitor::onProjectInformationUpdated(const std::vector<ProjectInformation>& projectInformation)
{
	for (size_t index = 0; index < lastProjectInformation.size(); ++index)
	{
		auto switchedToFailed = [](const ProjectInformation& last, const ProjectInformation& now)
		{
			return projectStatus_isFailure(now.status) && last.status == EProjectStatus::Succeeded;
		};

		auto switchedToSuccess = [](const ProjectInformation& last, const ProjectInformation& now)
		{
			return now.status == EProjectStatus::Succeeded && projectStatus_isFailure(last.status);
		};

		auto showMessage = [this](const QString& projectName, const std::vector<QString>& initiators, bool broken)
		{
			QString message(broken ? "Broken by: " : "Fixed by: ");
			if (initiators.empty())
			{
				message += "Unknown";
			}
			else
			{
				for (size_t i = 0; i < initiators.size(); ++i)
				{
					if (i != 0)
					{
						if (i == initiators.size() - 1)
						{
							message += " and/or ";
						}
						else
						{
							message += ", ";
						}
					}

					message += initiators[i];
				}
			}
			tray->showMessage(projectName, message, broken ? QSystemTrayIcon::Critical : QSystemTrayIcon::Information);
		};

		if (projectInformation.size() < index && lastProjectInformation.size() < index &&
			projectInformation[index].projectName == lastProjectInformation[index].projectName)
		{
			if (switchedToFailed(lastProjectInformation[index], projectInformation[index]))
			{
				showMessage(projectInformation[index].projectName, projectInformation[index].initiatedBy, true);
			}
			else if (switchedToSuccess(lastProjectInformation[index], projectInformation[index]))
			{
				showMessage(projectInformation[index].projectName, projectInformation[index].initiatedBy, false);
			}
		}
		else
		{
			for (const ProjectInformation& info : projectInformation)
			{
				if (info.projectName == lastProjectInformation[index].projectName)
				{
					if (switchedToFailed(lastProjectInformation[index], info))
					{
						showMessage(info.projectName, info.initiatedBy, true);
					}
					else if (switchedToSuccess(lastProjectInformation[index], info))
					{
						showMessage(info.projectName, info.initiatedBy, false);
					}
					break;
				}
			}
		}
	}


	lastProjectInformation = projectInformation;

	static const std::vector<EProjectStatus> priorityList = {
		EProjectStatus::Failed,
		EProjectStatus::Unstable,
		EProjectStatus::Aborted,
		EProjectStatus::NotBuilt,
		EProjectStatus::Succeeded,
		EProjectStatus::Disabled,
		EProjectStatus::Unknown
	};
	qint32 newStatusIndex = priorityList.size() - 1;

	bool isBuilding = false;
	for (const ProjectInformation& info : projectInformation)
	{
		qint32 statusIndex = std::find(priorityList.begin(), priorityList.end(), info.status) - priorityList.begin();
		if (statusIndex < newStatusIndex)
		{
			newStatusIndex = statusIndex;
		}
		isBuilding |= info.isBuilding;
	}

	if (priorityList[newStatusIndex] != projectBuildStatusGlobal || isBuilding != projectBuildStatusGlobalIsBuilding)
	{
		projectBuildStatusGlobal = priorityList[newStatusIndex];
		projectBuildStatusGlobalIsBuilding = isBuilding;
		updateIcons();
	}
	updateTaskbarProgress();

	buildMonitorServerCommunication->requestFixInformation(lastProjectInformation);
}

void BuildMonitor::onFixInformationUpdated(const std::vector<FixInformation>& fixInformation)
{
	for (const FixInformation& info : fixInformation)
	{
		const std::vector<ProjectInformation>::iterator pos = std::find_if(lastProjectInformation.begin(), lastProjectInformation.end(),
			[&info](const ProjectInformation& projectInfo)
		{
			return projectInfo.projectName == info.projectName;
		});

		if (pos != lastProjectInformation.end())
		{
			if (pos->buildNumber >= info.buildNumber)
			{
				if (projectStatus_isFailure(pos->status))
				{
					pos->volunteer = info.userName;
				}
				else
				{
					buildMonitorServerCommunication->requestReportFixed(pos->projectName, pos->buildNumber);
				}
			}
		}
	}

	ui.serverOverviewTable->setProjectInformation(lastProjectInformation);
}

void BuildMonitor::onProjectInformationError(const QString& errorMessage)
{
	if (projectBuildStatusGlobal != EProjectStatus::Unknown)
	{
		projectBuildStatusGlobal = EProjectStatus::Unknown;
		updateIcons();
	}
	ui.statusBar->showMessage(errorMessage);
}

void BuildMonitor::onTableRowDoubleClicked(const QModelIndex& index)
{
	const QString projectName = ui.serverOverviewTable->getProjectName(index.row());
	for (const ProjectInformation& info : lastProjectInformation)
	{
		if (info.projectName == projectName)
		{
			QDesktopServices::openUrl(info.projectUrl);
			break;
		}
	}
}

void BuildMonitor::onVolunteerToFix(const QString& projectName)
{
	const std::vector<ProjectInformation>::const_iterator pos = std::find_if(lastProjectInformation.begin(), lastProjectInformation.end(),
		[&projectName](const ProjectInformation& element) { return element.projectName == projectName; });
	if (pos != lastProjectInformation.end() && projectStatus_isFailure(pos->status))
	{
		buildMonitorServerCommunication->requestReportFixing(projectName, pos->buildNumber);
		jenkins->refresh();
	}
}

void BuildMonitor::onViewBuildLog(const QString& projectName)
{
	const std::vector<ProjectInformation>::const_iterator pos = std::find_if(lastProjectInformation.begin(), lastProjectInformation.end(),
		[&projectName](const ProjectInformation& element) { return element.projectName == projectName; });
	if (pos != lastProjectInformation.end() && pos->buildNumber != 0)
	{
		QDesktopServices::openUrl(pos->projectUrl.toString() + QString::number(pos->buildNumber) + "/consoleText");
	}
}
