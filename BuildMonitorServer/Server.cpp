/* BuildMonitor - Monitor the state of projects in CI.
 * Copyright (C) 2017-2020 Sander Brattinga

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

#include "Server.h"

#include "AcceptThread.h"

Server::Server(QObject* parent) :
	QTcpServer(parent)
{
	qRegisterMetaType<FixInfo>();
}

Server::~Server()
{
	for (AcceptThread* element : threads)
	{
		element->wait(10000);
	}
}

std::vector<FixInfo> Server::getProjectsState(const std::vector<QString>& projects)
{
	std::vector<FixInfo> result;
	result.reserve(projects.size());

	fixInfoLock.lock();

	for (const FixInfo& info : fixInfos)
	{
		for (const QString& projectUrl : projects)
		{
			if (info.projectUrl == projectUrl)
			{
				result.push_back(info);
				break;
			}
			else if (!projectUrl.contains('/'))
			{
				const qint32 lastSlashIndex = info.projectUrl.lastIndexOf('/');
				if (lastSlashIndex > 0)
				{
					const QString projectName = info.projectUrl.right(info.projectUrl.size() - lastSlashIndex - 1);
					if (projectName == projectUrl)
					{
						result.push_back(info);
					}
				}
			}
			else if (!info.projectUrl.contains('/') && projectUrl.contains('/'))
			{
				const qint32 lastSlashIndex = projectUrl.lastIndexOf('/');
				if (lastSlashIndex > 0)
				{
					const QString projectName = projectUrl.right(projectUrl.size() - lastSlashIndex - 1);
					if (projectName == info.projectUrl)
					{
						result.push_back(info);
					}
				}
			}
		}
	}

	fixInfoLock.unlock();

	return result;
}

void Server::incomingConnection(qintptr socketDescriptor)
{
	AcceptThread* thread = new AcceptThread(socketDescriptor, *this, this);
	connect(thread, &AcceptThread::fixStarted, this, &Server::onFixStarted);
	connect(thread, &AcceptThread::markFixed, this, &Server::onMarkFixed);
	connect(thread, &AcceptThread::finished, this, &Server::onThreadFinished);
	thread->start();
	threads.push_back(thread);
}

void Server::onFixStarted(const FixInfo& fixInfo)
{
	fixInfoLock.lock();

	std::vector<FixInfo>::iterator foundElement = std::find_if(fixInfos.begin(), fixInfos.end(),
		[&fixInfo](const FixInfo& info) { return info.projectUrl == fixInfo.projectUrl; });
	if (foundElement != fixInfos.end())
	{
		*foundElement = fixInfo;
	}
	else
	{
		fixInfos.emplace_back(fixInfo);
	}

	emit fixInfoChanged(fixInfos);

	fixInfoLock.unlock();
}

void Server::onMarkFixed(const QString& projectUrl, const qint32 buildNumber)
{
	fixInfoLock.lock();

	std::vector<FixInfo>::iterator foundElement = std::find_if(fixInfos.begin(), fixInfos.end(),
		[&projectUrl, &buildNumber](const FixInfo& info) { return info.projectUrl == projectUrl && info.buildNumber < buildNumber; });
	if (foundElement != fixInfos.end())
	{
		fixInfos.erase(foundElement);
	}

	emit fixInfoChanged(fixInfos);

	fixInfoLock.unlock();
}

void Server::onThreadFinished()
{
	threads.erase(std::remove_if(threads.begin(), threads.end(), [](const AcceptThread* element)
	{
		return element->isFinished();
	}), threads.end());
}
