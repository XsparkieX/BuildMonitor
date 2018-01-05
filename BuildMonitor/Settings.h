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

	std::vector<QUrl> serverURLs;
	QString fixServerAddress;
	std::vector<QString> ignoreUserList;
	qint32 refreshIntervalInSeconds;
	bool showDisabledProjects;
	QRegExp projectIncludeRegEx;
	QRegExp projectExcludeRegEx;
	QString showProgressForProject;
	bool closeToTrayOnStartup;
	bool windowMaximized;
	qint32 windowSizeX;
	qint32 windowSizeY;
	qint32 windowPosX;
	qint32 windowPosY;

Q_SIGNALS:
	void settingsChanged();
};
