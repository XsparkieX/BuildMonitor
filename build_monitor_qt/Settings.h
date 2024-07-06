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

#include <qdir.h>
#include <qobject.h>
#include <qstring.h>
#include <qurl.h>

class Settings : public QObject
{
	Q_OBJECT
public:
	Settings(QObject* parent = Q_NULLPTR);

	bool loadSettings();
	void saveSettings();

	const QDir projectSettingsFolder;

	std::string serverAddress;
	bool multicast;
	std::vector<std::string> ignoreUserList;
	bool showDisabledProjects;
	std::vector<uint64_t> notifyList;
	bool closeToTrayOnStartup;
	bool windowMaximized;
	qint32 windowSizeX;
	qint32 windowSizeY;
	qint32 windowPosX;
	qint32 windowPosY;

Q_SIGNALS:
	void settingsChanged();
};
