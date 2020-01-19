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

#include "BuildMonitorServerWorker.h"

#include <qapplication.h>
#include <qdebug.h>
#include <qthread.h>

BuildMonitorServerWorker::BuildMonitorServerWorker(QThread& workerThread) :
	QObject(nullptr),
	socket(this),
	isProcessingRequest(false)
{
	qRegisterMetaType<BuildMonitorRequestType>();

	connect(&socket, &QIODevice::readyRead, this, &BuildMonitorServerWorker::onResponseGenerated);
	connect(&socket, &QAbstractSocket::disconnected, this, &BuildMonitorServerWorker::onDisconnected);

	moveToThread(&workerThread);
}

BuildMonitorServerWorker::~BuildMonitorServerWorker()
{
	moveToThread(QApplication::instance()->thread());
	socket.moveToThread(QApplication::instance()->thread());
}

void BuildMonitorServerWorker::setServerAddress(const QString& inServerAddress, const quint16& inServerPort)
{
	connectMutex.lock();
	serverAddress = inServerAddress;
	serverPort = inServerPort;
	connectMutex.unlock();
}

void BuildMonitorServerWorker::addToQueue(const Request& request)
{
	requestMutex.lock();
	requests.emplace_back(request);
	requestMutex.unlock();
}

bool BuildMonitorServerWorker::containsRequestType(const BuildMonitorRequestType& type)
{
	requestMutex.lock();
	std::vector<Request>::iterator pos = std::find_if(requests.begin(), requests.end(),
		[type](const Request& element) { return element.type == type; });
	const bool containsRequest = pos != requests.end();
	requestMutex.unlock();
	return containsRequest;
}

void BuildMonitorServerWorker::processQueue()
{
	requestMutex.lock();
	if (requests.empty() || isProcessingRequest)
	{
		requestMutex.unlock();
		return;
	}

	isProcessingRequest = true;
	Request request = *requests.begin();
	requestMutex.unlock();

	connectMutex.lock();
	socket.abort();
	socket.connectToHost(serverAddress, serverPort);
	if (socket.waitForConnected(3000))
	{
		socket.write(request.data);
		socket.flush();
		socket.waitForBytesWritten();
		connectMutex.unlock();
	}
	else
	{
		connectMutex.unlock();
		onDisconnected();
	}
}

void BuildMonitorServerWorker::onResponseGenerated()
{
	QByteArray data = socket.readAll();
	isProcessingRequest = false;
	emit responseGenerated(data);
}

void BuildMonitorServerWorker::onDisconnected()
{
	isProcessingRequest = false;
	requestMutex.lock();
	if (requests.size() > 0)
	{
		const BuildMonitorRequestType type = requests.begin()->type;
		requests.erase(requests.begin());
		requestMutex.unlock();
		
		emit failure(type);
	}
	else
	{
		requestMutex.unlock();
	}
}
