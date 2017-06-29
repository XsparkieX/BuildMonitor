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

#include "JenkinsCommunication.h"

#include <qjsonarray.h>
#include <qjsondocument.h>
#include <qjsonobject.h>
#include <qnetworkaccessmanager.h>
#include <qnetworkreply.h>
#include <qnetworkrequest.h>
#include <qtimer.h>

JenkinsCommunication::JenkinsCommunication(QObject* parent) :
	QObject(parent),
	showDisabledProjects(false),
	networkAccessManager(new QNetworkAccessManager(this)),
	refreshTimer(new QTimer(this)),
	jenkinsServerRepliesCount(0),
	projectRetrievalRepliesCount(0)
{
	connect(refreshTimer, &QTimer::timeout, this, &JenkinsCommunication::refresh);
}

void JenkinsCommunication::setServerURLs(const std::vector<QUrl>& inServerURLs)
{
	if (serverURLs != inServerURLs)
	{
		serverURLs = inServerURLs;
	}
}

void JenkinsCommunication::setRefreshInterval(qint32 refreshInterval)
{
	refreshInterval *= 1000;
	if (refreshTimer->interval() != refreshInterval)
	{
		if (refreshTimer->isActive())
		{
			refreshTimer->stop();
		}
		refreshTimer->start(refreshInterval);
	}
}

void JenkinsCommunication::setIgnoreUserList(const std::vector<QString>& inIgnoreUserList)
{
	ignoreUserList = inIgnoreUserList;
}

void JenkinsCommunication::setShowDisabledProjects(bool inShowDisabledProjects)
{
	showDisabledProjects = inShowDisabledProjects;
}

void JenkinsCommunication::setProjectIncludePattern(const QRegExp& regExp)
{
	projectIncludePattern = regExp;
}

const std::vector<ProjectInformation>& JenkinsCommunication::getProjectInformation() const
{
	return projectInformation;
}

void JenkinsCommunication::refresh()
{
	if (!jenkinsServerReplies.empty() && !projectRetrievalReplies.empty())
	{
		return;
	}

	startJenkinsServerInformationRetrieval();
}

void JenkinsCommunication::startJenkinsServerInformationRetrieval()
{
	jenkinsServerRepliesCount = 0;
	jenkinsServerReplies.clear();
	projectInformation.clear();
	for (QUrl jenkinsRequest : serverURLs)
	{
		jenkinsRequest.setPath("/api/json");
		QNetworkRequest projectInformationRequest(jenkinsRequest);
		projectInformationRequest.setHeader(QNetworkRequest::ServerHeader, "application/json");

		jenkinsServerReplies.emplace_back(networkAccessManager->get(projectInformationRequest));
		connect(jenkinsServerReplies.back(), &QNetworkReply::finished, this, &JenkinsCommunication::onJenkinsInformationReceived);
	}
}

void JenkinsCommunication::startProjectInformationRetrieval()
{
	projectRetrievalRepliesCount = 0;
	if (projectInformation.empty())
	{
		projectInformationUpdated(projectInformation);
	}
	else
	{
		for (ProjectInformation& info : projectInformation)
		{
			QUrl projectRequest = info.projectUrl;
			projectRequest.setPath("/job/" + info.projectName + "/lastBuild/api/json");
			QNetworkRequest projectInformationRequest(projectRequest);
			projectInformationRequest.setHeader(QNetworkRequest::ServerHeader, "application/json");
			projectRetrievalReplies.emplace_back(&info, networkAccessManager->get(projectInformationRequest));
			connect(projectRetrievalReplies.back().second, &QNetworkReply::finished, this, &JenkinsCommunication::onProjectInformationReceived);
		}
	}
}

void JenkinsCommunication::startLastSuccesfulProjectInformationRetrieval()
{
	projectRetrievalRepliesCount = 0;
	if (projectInformation.empty())
	{
		projectInformationUpdated(projectInformation);
	}
	else
	{
		for (ProjectInformation& info : projectInformation)
		{
			QUrl projectRequest = info.projectUrl;
			projectRequest.setPath("/job/" + info.projectName + "/lastSuccessfulBuild/api/json");
			QNetworkRequest projectInformationRequest(projectRequest);
			projectInformationRequest.setHeader(QNetworkRequest::ServerHeader, "application/json");
			projectRetrievalReplies.emplace_back(&info, networkAccessManager->get(projectInformationRequest));
			connect(projectRetrievalReplies.back().second, &QNetworkReply::finished, this, &JenkinsCommunication::onLastSuccesfulProjectInformationReceived);
		}
	}
}

void JenkinsCommunication::onJenkinsInformationReceived()
{
	++jenkinsServerRepliesCount;
	if (jenkinsServerRepliesCount != jenkinsServerReplies.size())
	{
		return; // We are still awaiting response from other jenkins servers.
	}

	for (QNetworkReply* reply : jenkinsServerReplies)
	{
		if (reply->error() == QNetworkReply::NoError)
		{
			QJsonDocument document(QJsonDocument::fromJson(reply->readAll()));
			QJsonObject root = document.object();
			QJsonArray projects = root["jobs"].toArray();

			for (const QJsonValue& project : projects)
			{
				if (project.isObject())
				{
					const QJsonObject object = project.toObject();
					ProjectInformation info;
					info.projectName = object["name"].toString();

					if (!projectIncludePattern.exactMatch(info.projectName))
					{
						continue;
					}

					info.projectUrl = object["url"].toString();
					bool addToList = true;
					const QString buildStatus = object["color"].toString();
					if (buildStatus.startsWith("blue"))
					{
						info.status = EProjectStatus::Succeeded;
					}
					else if (buildStatus.startsWith("red"))
					{
						info.status = EProjectStatus::Failed;
					}
					else if (buildStatus.startsWith("yellow"))
					{
						info.status = EProjectStatus::Unstable;
					}
					else if (buildStatus.startsWith("disabled"))
					{
						addToList = showDisabledProjects;
						info.status = EProjectStatus::Disabled;
					}
					else if (buildStatus.startsWith("aborted"))
					{
						info.status = EProjectStatus::Aborted;
					}
					else if (buildStatus.startsWith("notbuilt"))
					{
						info.status = EProjectStatus::NotBuilt;
					}
					else
					{
						info.status = EProjectStatus::Unknown;
					}
					info.isBuilding = buildStatus.endsWith("_anime");

					if (addToList)
					{
						projectInformation.emplace_back(info);
					}
				}
			}
		}
		else
		{
			projectInformationError(reply->errorString());
		}
	}

	for (QNetworkReply* reply : jenkinsServerReplies)
	{
		delete reply;
	}
	jenkinsServerReplies.clear();

	startProjectInformationRetrieval();
}

void JenkinsCommunication::onProjectInformationReceived()
{
	++projectRetrievalRepliesCount;
	if (projectRetrievalRepliesCount != projectRetrievalReplies.size())
	{
		return; // Still in the process of receiving projects.
	}

	for (std::pair<ProjectInformation*, QNetworkReply*>& pair : projectRetrievalReplies)
	{
		QNetworkReply* reply = pair.second;
		if (reply->error() == QNetworkReply::NoError)
		{
			const QJsonDocument document(QJsonDocument::fromJson(reply->readAll()));
			QJsonObject root = document.object();

			ProjectInformation& info = *pair.first;
			if (root["duration"].toDouble() != 0)
			{
				info.inProgressFor = root["duration"].toDouble();
				info.estimatedRemainingTime = 0;
			}
			else
			{
				const qint64 timestamp = root["timestamp"].toDouble();
				const qint64 currentTime = QDateTime::currentDateTimeUtc().toMSecsSinceEpoch();
				info.inProgressFor = currentTime - timestamp;
				info.estimatedRemainingTime = root["estimatedDuration"].toDouble() - info.inProgressFor;
			}

			info.buildNumber = root["number"].toInt();

			const QJsonArray items = root["changeSet"].toObject()["items"].toArray();
			for (const QJsonValue& item : items)
			{
				if (item.isObject())
				{
					const QString name = item.toObject()["author"].toObject()["fullName"].toString();
					if (std::find(ignoreUserList.begin(), ignoreUserList.end(), name) == ignoreUserList.end() &&
						std::find(info.initiatedBy.begin(), info.initiatedBy.end(), name) == info.initiatedBy.end())
					{
						info.initiatedBy.emplace_back(name);
					}
				}
			}
			std::sort(info.initiatedBy.begin(), info.initiatedBy.end());
		}
		else
		{
			// TODO: Send error to status bar.
			qDebug() << reply->errorString();
		}
	}

	for (std::pair<ProjectInformation*, class QNetworkReply*>& pair : projectRetrievalReplies)
	{
		delete pair.second;
	}
	projectRetrievalReplies.clear();

	std::sort(projectInformation.begin(), projectInformation.end(), [](const ProjectInformation& lhs, const ProjectInformation& rhs)
	{
		return lhs.projectName < rhs.projectName;
	});

	startLastSuccesfulProjectInformationRetrieval();
}

void JenkinsCommunication::onLastSuccesfulProjectInformationReceived()
{
	++projectRetrievalRepliesCount;
	if (projectRetrievalRepliesCount != projectRetrievalReplies.size())
	{
		return; // Still in the process of receiving projects.
	}

	for (std::pair<ProjectInformation*, QNetworkReply*>& pair : projectRetrievalReplies)
	{
		QNetworkReply* reply = pair.second;
		if (reply->error() == QNetworkReply::NoError)
		{
			QJsonObject root = QJsonDocument::fromJson(reply->readAll()).object();
			if (root["timestamp"].isDouble())
			{
				pair.first->lastSuccessfulBuildTime = root["timestamp"].toDouble();
			}
		}
	}

	for (std::pair<ProjectInformation*, class QNetworkReply*>& pair : projectRetrievalReplies)
	{
		delete pair.second;
	}
	projectRetrievalReplies.clear();

	projectInformationUpdated(projectInformation);
}
