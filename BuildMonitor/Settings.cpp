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

#include "Settings.h"

#include <qdir.h>
#include <qstandardpaths.h>
#include <qjsonarray.h>
#include <qjsondocument.h>
#include <qjsonobject.h>

Settings::Settings(QObject* parent) :
	QObject(parent),
	projectSettingsFolder(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)),
	fixServerAddress("jenkins:1080"),
	refreshIntervalInSeconds(60),
	showDisabledProjects(false),
	useRegExProjectFilter(false),
	projectIncludeRegEx(".*"),
	enabledProjectList(),
	closeToTrayOnStartup(false),
	windowMaximized(false),
	windowSizeX(640),
	windowSizeY(360),
	windowPosX(320),
	windowPosY(180)
{
	serverURLs.emplace_back("http://jenkins:8080/");
}

bool Settings::loadSettings()
{
	QFile settingsFile(projectSettingsFolder.absoluteFilePath("Settings.json"));
	if (!settingsFile.open(QIODevice::ReadOnly | QIODevice::Text))
	{
		return false;
	}

	QJsonDocument settingsJson = QJsonDocument::fromJson(settingsFile.readAll());
	QJsonObject root = settingsJson.object();

	QJsonValue serverURLListValue = root.value("serverURLList");
	if (serverURLListValue.isArray())
	{
		serverURLs.clear();
		QJsonArray serverURLListArray = serverURLListValue.toArray();
		for (const QJsonValue& serverURL : serverURLListArray)
		{
			if (serverURL.isString())
			{
				serverURLs.emplace_back(serverURL.toString());
			}
		}
	}

	QJsonValue fixServerAddressValue = root.value("fixServerAddress");
	if (fixServerAddressValue.isString())
	{
		fixServerAddress = fixServerAddressValue.toString();
	}

	QJsonValue ignoreUserListValue = root.value("ignoreUserList");
	if (ignoreUserListValue.isArray())
	{
		ignoreUserList.clear();
		QJsonArray ignoreUserListArray = ignoreUserListValue.toArray();
		for (const QJsonValue& userValue : ignoreUserListArray)
		{
			if (userValue.isString())
			{
				ignoreUserList.emplace_back(userValue.toString());
			}
		}
	}

	QJsonValue refreshIntervalInSecondsValue = root.value("refreshIntervalInSeconds");
	if (refreshIntervalInSecondsValue.isDouble())
	{
		refreshIntervalInSeconds = refreshIntervalInSecondsValue.toDouble();
	}

	QJsonValue showDisabledProjectsValue = root.value("showDisabledProjects");
	if (showDisabledProjectsValue.isBool())
	{
		showDisabledProjects = showDisabledProjectsValue.toBool();
	}

	/* Legacy support */
	QJsonValue projectRegExValue = root.value("projectRegEx");
	if (projectRegExValue.isString())
	{
		projectIncludeRegEx.setPattern(projectRegExValue.toString());
	}
	/* End legacy support */

	QJsonValue useRegExProjectFilterValue = root.value("useRegExProjectFilter");
	if (useRegExProjectFilterValue.isBool())
	{
		useRegExProjectFilter = useRegExProjectFilterValue.toBool();
	}

	QJsonValue projectIncludeRegExValue = root.value("projectIncludeRegEx");
	if (projectIncludeRegExValue.isString())
	{
		projectIncludeRegEx.setPattern(projectIncludeRegExValue.toString());
	}

	QJsonValue projectExcludeRegExValue = root.value("projectExcludeRegEx");
	if (projectExcludeRegExValue.isString())
	{
		projectExcludeRegEx.setPattern(projectExcludeRegExValue.toString());
	}

	QJsonValue enabledProjectListValue = root.value("enabledProjectList");
	if (enabledProjectListValue.isArray())
	{
		enabledProjectList.clear();
		QJsonArray enabledProjectListArray = enabledProjectListValue.toArray();
		for (const QJsonValue& enabledProject : enabledProjectListArray)
		{
			if (enabledProject.isString())
			{
				enabledProjectList.emplace_back(enabledProject.toString());
			}
		}
	}

	QJsonValue showProgressForProjectValue = root.value("showProgressForProject");
	if (showProgressForProjectValue.isString())
	{
		showProgressForProject = showProgressForProjectValue.toString();
	}

	QJsonValue closeToTrayOnStartupValue = root.value("closeToTrayOnStartup");
	if (closeToTrayOnStartupValue.isBool())
	{
		closeToTrayOnStartup = closeToTrayOnStartupValue.toBool();
	}

	QJsonValue windowMaximizedValue = root.value("windowMaximized");
	if (windowMaximizedValue.isBool())
	{
		windowMaximized = windowMaximizedValue.toBool();
	}

	QJsonValue windowPosXValue = root.value("windowPosX");
	if (windowPosXValue.isDouble())
	{
		windowPosX = windowPosXValue.toInt();
	}

	QJsonValue windowPosYValue = root.value("windowPosY");
	if (windowPosYValue.isDouble())
	{
		windowPosY = windowPosYValue.toInt();
	}

	QJsonValue windowSizeXValue = root.value("windowSizeX");
	if (windowSizeXValue.isDouble())
	{
		windowSizeX = windowSizeXValue.toInt();
	}

	QJsonValue windowSizeYValue = root.value("windowSizeY");
	if (windowSizeYValue.isDouble())
	{
		windowSizeY = windowSizeYValue.toInt();
	}

	settingsChanged();

	return true;
}

void Settings::saveSettings()
{
	QJsonObject root;
	QJsonArray serverURLListArray;
	for (const QUrl& serverURL : serverURLs)
	{
		serverURLListArray.push_back(serverURL.toString());
	}
	root.insert("serverURLList", serverURLListArray);

	root.insert("fixServerAddress", fixServerAddress);

	QJsonArray ignoreUserListArray;
	for (const QString& user : ignoreUserList)
	{
		ignoreUserListArray.push_back(user);
	}
	root.insert("ignoreUserList", ignoreUserListArray);

	root.insert("refreshIntervalInSeconds", refreshIntervalInSeconds);

	root.insert("showDisabledProjects", showDisabledProjects);

	root.insert("useRegExProjectFilter", useRegExProjectFilter);

	root.insert("projectIncludeRegEx", projectIncludeRegEx.pattern());

	root.insert("projectExcludeRegEx", projectExcludeRegEx.pattern());

	QJsonArray enabledProjectListArray;
	for (const QString& project : enabledProjectList)
	{
		enabledProjectListArray.push_back(project);
	}
	root.insert("enabledProjectList", enabledProjectListArray);

	root.insert("showProgressForProject", showProgressForProject);

	root.insert("closeToTrayOnStartup", closeToTrayOnStartup);

	root.insert("windowMaximized", windowMaximized);
	root.insert("windowPosX", windowPosX);
	root.insert("windowPosY", windowPosY);
	root.insert("windowSizeX", windowSizeX);
	root.insert("windowSizeY", windowSizeY);

	QJsonDocument settingsJson;
	settingsJson.setObject(root);

	if (!projectSettingsFolder.exists())
	{
		projectSettingsFolder.mkpath(projectSettingsFolder.absolutePath());
	}
	QFile settingsFile(projectSettingsFolder.absoluteFilePath("Settings.json"));
	if (!settingsFile.open(QIODevice::WriteOnly | QIODevice::Text))
	{
		return;
	}
	settingsFile.write(settingsJson.toJson());

	settingsChanged();
}
