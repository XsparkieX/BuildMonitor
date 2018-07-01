/* BuildMonitor - Monitor the state of projects in CI.
 * Copyright (C) 2017-2018 Sander Brattinga

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

#include "AcceptThread.h"

#include "Server.h"

#include <qjsonarray.h>
#include <qjsondocument.h>
#include <qjsonobject.h>

AcceptThread::AcceptThread(qintptr socketDescriptor, Server& inServer, QObject* parent) :
	QThread(parent),
	server(inServer),
	socketDescriptor(socketDescriptor)
{
}

void AcceptThread::run()
{
	QTcpSocket socket;
	if (!socket.setSocketDescriptor(socketDescriptor))
	{
		emit error(socket.error());
		return;
	}

	if (socket.waitForReadyRead(3000))
	{
		const QByteArray data = socket.readAll();
		const QJsonDocument json = QJsonDocument::fromBinaryData(data);
		const QJsonObject root = json.object();
		if (root["version"].toInt() == 1)
		{
			if (root["request_type"].toString() == "report_fixing")
			{
				const QJsonObject requestInfo = root["request_info"].toObject();
				const FixInfo fixInfo = {
					requestInfo["project_name"].toString(),
					requestInfo["user_name"].toString(),
					requestInfo["build_number"].toInt()
				};

				emit fixStarted(fixInfo);
			}
			else if (root["request_type"].toString() == "fix_state")
			{
				const QJsonObject requestInfo = root["request_info"].toObject();
				const QJsonArray projectsArray = requestInfo["projects"].toArray();
				std::vector<QString> projects;
				for (const QJsonValue& element : projectsArray)
				{
					projects.emplace_back(element.toString());
				}

				const std::vector<FixInfo> state = server.getProjectsState(projects);

				QJsonObject root;
				root["version"] = 1;
				root["response_type"] = "fix_state";
				QJsonArray responseArray = QJsonArray();
				for (const FixInfo& info : state)
				{
					QJsonObject fixStateObject;
					QString projectName = info.projectUrl;
					const qint32 lastSlashIndex = projectName.lastIndexOf('/');
					if (lastSlashIndex > 0)
					{
						projectName = projectName.right(projectName.size() - lastSlashIndex - 1);
					}
					fixStateObject["project_name"] = projectName;
					fixStateObject["user_name"] = info.userName;
					fixStateObject["build_number"] = info.buildNumber;
					responseArray.push_back(fixStateObject);
				}
				root["response_info"] = responseArray;

				QJsonDocument document;
				document.setObject(root);
				socket.write(document.toBinaryData());
				socket.flush();
				socket.waitForBytesWritten(3000);
			}
			else if (root["request_type"].toString() == "mark_fixed")
			{
				const QJsonObject requestInfo = root["request_info"].toObject();
				emit markFixed(requestInfo["project_name"].toString(), requestInfo["build_number"].toInt());
			}
		}
		else if (root["version"].toInt() == 2)
		{
			if (root["request_type"].toString() == "report_fixing")
			{
				const QJsonObject requestInfo = root["request_info"].toObject();
				const FixInfo fixInfo = {
					requestInfo["project_url"].toString(),
					requestInfo["user_name"].toString(),
					requestInfo["build_number"].toInt()
				};

				emit fixStarted(fixInfo);
			}
			else if (root["request_type"].toString() == "fix_state")
			{
				const QJsonObject requestInfo = root["request_info"].toObject();
				const QJsonArray projectsArray = requestInfo["projects"].toArray();
				std::vector<QString> projects;
				for (const QJsonValue& element : projectsArray)
				{
					projects.emplace_back(element.toString());
				}

				const std::vector<FixInfo> state = server.getProjectsState(projects);

				QJsonObject root;
				root["version"] = 2;
				root["response_type"] = "fix_state";
				QJsonArray responseArray = QJsonArray();
				for (const FixInfo& info : state)
				{
					QJsonObject fixStateObject;
					fixStateObject["project_url"] = info.projectUrl;
					fixStateObject["user_name"] = info.userName;
					fixStateObject["build_number"] = info.buildNumber;
					responseArray.push_back(fixStateObject);
				}
				root["response_info"] = responseArray;

				QJsonDocument document;
				document.setObject(root);
				socket.write(document.toBinaryData());
				socket.flush();
				socket.waitForBytesWritten(3000);
			}
			else if (root["request_type"].toString() == "mark_fixed")
			{
				const QJsonObject requestInfo = root["request_info"].toObject();
				emit markFixed(requestInfo["project_url"].toString(), requestInfo["build_number"].toInt());
			}
		}
	}

	socket.disconnectFromHost();
}
