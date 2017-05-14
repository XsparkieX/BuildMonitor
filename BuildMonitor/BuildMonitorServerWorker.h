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

#include <qmutex.h>
#include <qobject.h>
#include <qtcpsocket.h>

class BuildMonitorServerWorker : public QObject
{
	Q_OBJECT

public:
	struct Request
	{
		QByteArray data;
		enum class Type
		{
			FixInformation,
			ReportFixing,
			ReportFixed
		} type;

		Request(const QByteArray& inData, const Type& inType) :
			data(inData),
			type(inType)
		{
		}
	};

	BuildMonitorServerWorker(QThread& workerThread);
	virtual ~BuildMonitorServerWorker();

	void setServerAddress(const QString& inServerAddress, const quint16& inServerPort);

	void addToQueue(const Request& request);
	bool containsRequestType(const Request::Type& type);
	
public slots:
	void processQueue();

signals:
	void responseGenerated(QByteArray data);
	void failure(BuildMonitorServerWorker::Request::Type type);

private:

	void onResponseGenerated();
	void onDisconnected();

	QMutex connectMutex;
	QString serverAddress;
	quint16 serverPort;
	QTcpSocket socket;

	QMutex requestMutex;
	std::vector<Request> requests;

	bool isProcessingRequest;
};

Q_DECLARE_METATYPE(BuildMonitorServerWorker::Request::Type);
