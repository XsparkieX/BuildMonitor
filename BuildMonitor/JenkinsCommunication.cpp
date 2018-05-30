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
#include "Settings.h"

#include <qjsonarray.h>
#include <qjsondocument.h>
#include <qjsonobject.h>
#include <qnetworkaccessmanager.h>
#include <qnetworkreply.h>
#include <qnetworkrequest.h>
#include <qtimer.h>

JenkinsCommunication::JenkinsCommunication(QObject* parent) :
	QObject(parent),
	networkAccessManager(new QNetworkAccessManager(this)),
	refreshTimer(new QTimer(this)),
	jenkinsFolderRepliesCount(0),
	projectRetrievalRepliesCount(0)
{
	connect(refreshTimer, &QTimer::timeout, this, &JenkinsCommunication::refresh);
}

void JenkinsCommunication::setSettings(const Settings* inSettings)
{
	settings = inSettings;
}

void JenkinsCommunication::refreshSettings()
{
	int refreshInterval = settings->refreshIntervalInSeconds * 1000;
	if (refreshTimer->interval() != refreshInterval)
	{
		if (refreshTimer->isActive())
		{
			refreshTimer->stop();
		}
		refreshTimer->start(refreshInterval);
	}
}

const std::vector<QString> JenkinsCommunication::getAllAvailableProjects() const
{
	std::vector<QString> projects;
	ForEachProjectInformation(projectInformation, [&projects] (const ProjectInformation& info)
	{
		projects.push_back(info.projectName);
	});
	
	return projects;
}

void JenkinsCommunication::refresh()
{
	// We are already in the process of receiving information.
	if (!jenkinsFolderReplies.empty() && !projectRetrievalReplies.empty())
	{
		return;
	}

	startJenkinsServerInformationRetrieval();
}

void JenkinsCommunication::startJenkinsServerInformationRetrieval()
{
	jenkinsFolderRepliesCount = 0;
	jenkinsFolderReplies.clear();
	projectInformation = ProjectInformationFolder("Root");
	for (QUrl jenkinsRequest : settings->serverURLs)
	{
		projectInformation.folders.push_back(std::shared_ptr<ProjectInformationFolder>(new ProjectInformationFolder(jenkinsRequest.host())));
		
		jenkinsRequest.setPath("/api/json");
		QNetworkRequest projectInformationRequest(jenkinsRequest);
		projectInformationRequest.setHeader(QNetworkRequest::ServerHeader, "application/json");

		jenkinsFolderReplies.emplace_back(projectInformation.folders.back(), networkAccessManager->get(projectInformationRequest));
		connect(jenkinsFolderReplies.back().second, &QNetworkReply::finished, this, &JenkinsCommunication::onJenkinsInformationReceived);
	}
}

void JenkinsCommunication::startJenkinsFolderInformationRetrieval(const std::vector<std::shared_ptr<ProjectInformationFolder> >& folders)
{
	for (const auto& folder : folders)
	{
		QUrl jenkinsRequest = folder->folderUrl;
		jenkinsRequest.setPath(jenkinsRequest.path() + "api/json");
		QNetworkRequest projectInformationRequest(jenkinsRequest);
		projectInformationRequest.setHeader(QNetworkRequest::ServerHeader, "application/json");

		jenkinsFolderReplies.emplace_back(folder, networkAccessManager->get(projectInformationRequest));
		connect(jenkinsFolderReplies.back().second, &QNetworkReply::finished, this, &JenkinsCommunication::onJenkinsInformationReceived);
	}
}

void JenkinsCommunication::startProjectInformationRetrieval()
{
	projectRetrievalRepliesCount = 0;
	if (projectInformation.folders.empty() && projectInformation.projects.empty())
	{
		projectInformationUpdated(projectInformation);
	}
	else
	{
		ForEachProjectInformation(projectInformation, [this] (ProjectInformation& info)
		{
			QUrl projectRequest = info.projectUrl;
			projectRequest.setPath(projectRequest.path() + "lastBuild/api/json");
			QNetworkRequest projectInformationRequest(projectRequest);
			projectInformationRequest.setHeader(QNetworkRequest::ServerHeader, "application/json");
			projectRetrievalReplies.emplace_back(&info, networkAccessManager->get(projectInformationRequest));
			connect(projectRetrievalReplies.back().second, &QNetworkReply::finished, this, &JenkinsCommunication::onProjectInformationReceived);
		});
	}
}

void JenkinsCommunication::startLastSuccesfulProjectInformationRetrieval()
{
	projectRetrievalRepliesCount = 0;
	if (projectInformation.folders.empty() && projectInformation.projects.empty())
	{
		projectInformationUpdated(projectInformation);
	}
	else
	{
		ForEachProjectInformation(projectInformation, [this] (ProjectInformation& info)
		{
			QUrl projectRequest = info.projectUrl;
			projectRequest.setPath(projectRequest.path() + "lastSuccessfulBuild/api/json");
			QNetworkRequest projectInformationRequest(projectRequest);
			projectInformationRequest.setHeader(QNetworkRequest::ServerHeader, "application/json");
			projectRetrievalReplies.emplace_back(&info, networkAccessManager->get(projectInformationRequest));
			connect(projectRetrievalReplies.back().second, &QNetworkReply::finished, this, &JenkinsCommunication::onLastSuccesfulProjectInformationReceived);
		});
	}
}

void JenkinsCommunication::onJenkinsInformationReceived()
{
	++jenkinsFolderRepliesCount;
	if (jenkinsFolderRepliesCount != jenkinsFolderReplies.size())
	{
		return; // We are still awaiting response from other jenkins servers.
	}

	std::vector<std::shared_ptr<ProjectInformationFolder> > additionalFoldersToRetrieve;
	for (const auto& folderReply : jenkinsFolderReplies)
	{
		QNetworkReply* reply = folderReply.second;
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
					std::shared_ptr<ProjectInformation> info(new ProjectInformation);
					info->projectName = object["name"].toString();
					if (object["_class"].toString() == "com.cloudbees.hudson.plugins.folder.Folder")
					{
						std::shared_ptr<ProjectInformationFolder> folder(new ProjectInformationFolder(object["name"].toString()));
						folderReply.first->folders.emplace_back(folder);
						folder->parent = folderReply.first;
						folder->folderUrl = object["url"].toString();
						additionalFoldersToRetrieve.emplace_back(folder);
						
						continue;
					}

					if (settings->useRegExProjectFilter)
					{
						if (!settings->projectIncludeRegEx.exactMatch(info->projectName) ||
							settings->projectExcludeRegEx.exactMatch(info->projectName))
						{
							continue;
						}
					}
					else
					{
						if (std::find(settings->enabledProjectList.begin(),
								settings->enabledProjectList.end(), info->projectName) ==
									settings->enabledProjectList.end())
						{
							continue;
						}
					}

					info->projectUrl = object["url"].toString();
					if (info->projectUrl.host() != reply->url().host())
					{
						info->projectUrl.setHost(reply->url().host());
					}
					bool addToList = true;
					const QString buildStatus = object["color"].toString();
					if (buildStatus.startsWith("blue"))
					{
						info->status = EProjectStatus::Succeeded;
					}
					else if (buildStatus.startsWith("red"))
					{
						info->status = EProjectStatus::Failed;
					}
					else if (buildStatus.startsWith("yellow"))
					{
						info->status = EProjectStatus::Unstable;
					}
					else if (buildStatus.startsWith("disabled"))
					{
						addToList = settings->showDisabledProjects;
						info->status = EProjectStatus::Disabled;
					}
					else if (buildStatus.startsWith("aborted"))
					{
						info->status = EProjectStatus::Aborted;
					}
					else if (buildStatus.startsWith("notbuilt"))
					{
						info->status = EProjectStatus::NotBuilt;
					}
					else
					{
						info->status = EProjectStatus::Unknown;
					}
					info->isBuilding = buildStatus.endsWith("_anime");

					if (addToList)
					{
						folderReply.first->projects.push_back(info);
					}
				}
			}
		}
		else
		{
			projectInformationError(reply->errorString());
		}
	}

	for (auto& reply : jenkinsFolderReplies)
	{
		delete reply.second;
	}
	jenkinsFolderReplies.clear();
	jenkinsFolderRepliesCount = 0;

	if (additionalFoldersToRetrieve.empty())
	{
		startProjectInformationRetrieval();
	}
	else
	{
		startJenkinsFolderInformationRetrieval(additionalFoldersToRetrieve);
	}
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

			const QJsonArray culprits = root["culprits"].toArray();
			for (const QJsonValue culprit : culprits)
			{
				if (culprit.isObject())
				{
					const QString name = culprit.toObject()["fullName"].toString();
					if (std::find(settings->ignoreUserList.begin(), settings->ignoreUserList.end(), name) == settings->ignoreUserList.end())
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
