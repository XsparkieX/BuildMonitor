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

#include "Settings.h"

#include <qdir.h>
#include <qstandardpaths.h>
#include <qjsonarray.h>
#include <qjsondocument.h>
#include <qjsonobject.h>

Settings::Settings(QObject* parent) :
	QObject(parent),
	projectSettingsFolder(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)),
	serverAddress("239.255.13.37:8090"),
	multicast(true),
	showDisabledProjects(false),
	closeToTrayOnStartup(false),
	windowMaximized(false),
	windowSizeX(640),
	windowSizeY(360),
	windowPosX(320),
	windowPosY(180)
{
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

	QJsonValue serverAddressValue = root.value("serverAddress");
	if (serverAddressValue.isString())
	{
		serverAddress = serverAddressValue.toString().toStdString();
	}

	QJsonValue multicastValue = root.value("multicast");
	if (multicastValue.isBool())
	{
		multicast = multicastValue.toBool();
	}

	QJsonValue ignoreUserListValue = root.value("ignoreUserList");
	if (ignoreUserListValue.isArray())
	{
		ignoreUserList.clear();
		QJsonArray ignoreUserListArray = ignoreUserListValue.toArray();
		for (const QJsonValue& userValue : std::as_const(ignoreUserListArray))
		{
			if (userValue.isString())
			{
				ignoreUserList.emplace_back(userValue.toString().toStdString());
			}
		}
	}

	QJsonValue showDisabledProjectsValue = root.value("showDisabledProjects");
	if (showDisabledProjectsValue.isBool())
	{
		showDisabledProjects = showDisabledProjectsValue.toBool();
	}

	QJsonValue notifyListValue = root.value("notifyList");
	if (notifyListValue.isArray())
	{
		notifyList.clear();
		QJsonArray notifyListArray = notifyListValue.toArray();
		for (const QJsonValue& notifyProject : std::as_const(notifyListArray))
		{
			if (notifyProject.isString())
			{
				notifyList.emplace_back(notifyProject.toString().toULongLong());
			}
		}
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

	emit settingsChanged(true);

	return true;
}

void Settings::saveSettings(bool serverPropertiesChanged)
{
	QJsonObject root;

	root.insert("serverAddress", QString::fromStdString(serverAddress));
	root.insert("multicast", multicast);

	QJsonArray ignoreUserListArray;
	for (const auto& user : ignoreUserList)
	{
		ignoreUserListArray.push_back(QString::fromStdString(user));
	}
	root.insert("ignoreUserList", ignoreUserListArray);

	root.insert("showDisabledProjects", showDisabledProjects);

	QJsonArray notifyListArray;
	for (const uint64_t entry : notifyList)
	{
		notifyListArray.push_back(QString::number(entry));
	}
	root.insert("notifyList", notifyListArray);

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

	emit settingsChanged(serverPropertiesChanged);
}
