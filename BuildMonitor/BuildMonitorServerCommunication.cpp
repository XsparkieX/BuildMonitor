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

#include "BuildMonitorServerCommunication.h"

#include "ProjectInformation.h"

#include <qjsonarray.h>
#include <qjsondocument.h>
#include <qjsonobject.h>
#include <qthread.h>

constexpr quint16 SERVER_DEFAULT_PORT = 1080;

BuildMonitorServerCommunication::BuildMonitorServerCommunication(QObject* parent) :
	QObject(parent),
	workerThread(new QThread(this)),
	worker(new BuildMonitorServerWorker(*workerThread))
{
	connect(this, &BuildMonitorServerCommunication::processQueue, worker, &BuildMonitorServerWorker::processQueue);
	connect(worker, &BuildMonitorServerWorker::responseGenerated, this, &BuildMonitorServerCommunication::onResponseGenerated);
	connect(worker, &BuildMonitorServerWorker::failure, this, &BuildMonitorServerCommunication::onFailure);
	
	workerThread->setObjectName("BuildMonitorServerCommunicationThread");
	workerThread->start();
}

BuildMonitorServerCommunication::~BuildMonitorServerCommunication()
{
	workerThread->quit();
	workerThread->wait();

	// Intentionally leaking worker, we only have one anyway and this is application exit.
	// Annoying logic is executed related to child reparenting on different threads.
}

void BuildMonitorServerCommunication::setServerAddress(const QString& inServerAddress)
{
	QString serverAddress;
	quint16 serverPort;
	int serverAddressEnd = inServerAddress.indexOf(':');
	if (serverAddressEnd != -1)
	{
		serverAddress = QString(inServerAddress.data(), serverAddressEnd);
		bool bSucceeeded = true;
		serverPort = QString(inServerAddress.data() + serverAddressEnd + 1).toUInt(&bSucceeeded);
		if (!bSucceeeded)
		{
			serverPort = SERVER_DEFAULT_PORT;
		}
	}
	else
	{
		serverAddress = inServerAddress;
		serverPort = SERVER_DEFAULT_PORT;
	}

	worker->setServerAddress(serverAddress, serverPort);
}

void BuildMonitorServerCommunication::requestFixInformation(const ProjectInformationFolder& projects)
{
	if (!worker->containsRequestType(BuildMonitorRequestType::FixInformation))
	{
		QJsonObject root;
		root["version"] = 1;
		root["request_type"] = "fix_state";
		QJsonObject requestInfo;
		QJsonArray projectsArray;
		ForEachProjectInformation(projects, [&projectsArray] (const ProjectInformation& info)
		{
			projectsArray.push_back(info.projectName);
		});
		requestInfo["projects"] = projectsArray;
		root["request_info"] = requestInfo;
		QJsonDocument doc;
		doc.setObject(root);

		worker->addToQueue(BuildMonitorServerWorker::Request(doc.toBinaryData(), BuildMonitorRequestType::FixInformation));
		emit processQueue();
	}
}

void BuildMonitorServerCommunication::requestReportFixing(const QString& projectName, const qint32 buildNumber)
{
	QJsonObject root;
	root["version"] = 1;
	root["request_type"] = "report_fixing";
	QJsonObject requestInfo;
	requestInfo["project_name"] = projectName;
	QString userName = qgetenv("USER");
	if (userName.isEmpty())
	{
		userName = qgetenv("USERNAME");
	}
	requestInfo["user_name"] = userName;
	requestInfo["build_number"] = buildNumber;
	root["request_info"] = requestInfo;
	QJsonDocument doc;
	doc.setObject(root);

	worker->addToQueue(BuildMonitorServerWorker::Request(doc.toBinaryData(), BuildMonitorRequestType::ReportFixing));
	emit processQueue();
}

void BuildMonitorServerCommunication::requestReportFixed(const QString& projectName, const qint32 buildNumber)
{
	QJsonObject root;
	root["version"] = 1;
	root["request_type"] = "mark_fixed";
	QJsonObject requestInfo;
	requestInfo["project_name"] = projectName;
	requestInfo["build_number"] = buildNumber;
	root["request_info"] = requestInfo;
	QJsonDocument doc;
	doc.setObject(root);

	worker->addToQueue(BuildMonitorServerWorker::Request(doc.toBinaryData(), BuildMonitorRequestType::ReportFixed));
	emit processQueue();
}

void BuildMonitorServerCommunication::onResponseGenerated(QByteArray data)
{
	const QJsonDocument json = QJsonDocument::fromBinaryData(data);
	const QJsonObject root = json.object();
	if (root["version"].toInt() == 1)
	{
		if (root["response_type"].toString() == "fix_state")
		{
			fixInformation.clear();
			QJsonArray responseInfo = root["response_info"].toArray();
			for (const QJsonValue& value : responseInfo)
			{
				const QJsonObject object = value.toObject();
				fixInformation.emplace_back(
					object["project_name"].toString(),
					object["user_name"].toString(),
					object["build_number"].toInt()
				);
			}
		}
	}

	onFixInformationUpdated(fixInformation);
	emit processQueue();
}

void BuildMonitorServerCommunication::onFailure(BuildMonitorRequestType type)
{
	if (type == BuildMonitorRequestType::FixInformation)
	{
		onFixInformationUpdated(fixInformation);
	}

	emit processQueue();
}
