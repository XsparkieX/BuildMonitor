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

	void setServerURLs(const std::vector<QUrl>& inServerURL);
	void setRefreshInterval(qint32 refreshInterval);
	void setIgnoreUserList(const std::vector<QString>& inIgnoreUserList);
	void setShowDisabledProjects(bool inShowDisabledProjects);
	void setProjectIncludePattern(const QRegExp& regExp);
	const std::vector<ProjectInformation>& getProjectInformation() const;

	void refresh();

Q_SIGNALS:
	void projectInformationUpdated(const std::vector<ProjectInformation>& projectInformation);
	void projectInformationError(const QString& errorMessage);

private:
	void startJenkinsServerInformationRetrieval();
	void startProjectInformationRetrieval();
	void startLastSuccesfulProjectInformationRetrieval();

	void onJenkinsInformationReceived();
	void onProjectInformationReceived();
	void onLastSuccesfulProjectInformationReceived();

	std::vector<ProjectInformation> projectInformation;

	std::vector<QUrl> serverURLs;
	std::vector<QString> ignoreUserList;
	bool showDisabledProjects;
	QRegExp projectIncludePattern;

	class QNetworkAccessManager* networkAccessManager;
	class QTimer* refreshTimer;
	std::vector<class QNetworkReply*> jenkinsServerReplies;
	std::vector<std::pair<ProjectInformation*, class QNetworkReply*> > projectRetrievalReplies;
	size_t jenkinsServerRepliesCount;
	size_t projectRetrievalRepliesCount;
};
