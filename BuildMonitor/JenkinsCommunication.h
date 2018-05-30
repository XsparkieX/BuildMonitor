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

#include "ProjectInformation.h"

#include <qobject.h>
#include <qurl.h>

class JenkinsCommunication : public QObject
{
	Q_OBJECT

public:
	JenkinsCommunication(QObject* parent);

	void setSettings(const class Settings* settings);
	void refreshSettings();
	
	const std::vector<QString> getAllAvailableProjects() const;

	void refresh();

Q_SIGNALS:
	void projectInformationUpdated(const ProjectInformationFolder& projectInformation);
	void projectInformationError(const QString& errorMessage);

private:
	void startJenkinsServerInformationRetrieval();
	void startJenkinsFolderInformationRetrieval(const std::vector<std::shared_ptr<ProjectInformationFolder> >& folders);
	void startProjectInformationRetrieval();
	void startLastSuccesfulProjectInformationRetrieval();

	void onJenkinsInformationReceived();
	void onProjectInformationReceived();
	void onLastSuccesfulProjectInformationReceived();

	ProjectInformationFolder projectInformation;

	const class Settings* settings;

	class QNetworkAccessManager* networkAccessManager;
	class QTimer* refreshTimer;
	std::vector<std::pair<std::shared_ptr<ProjectInformationFolder>, class QNetworkReply*> > jenkinsFolderReplies;
	std::vector<std::pair<ProjectInformation*, class QNetworkReply*> > projectRetrievalReplies;
	size_t jenkinsFolderRepliesCount;
	size_t projectRetrievalRepliesCount;
};
