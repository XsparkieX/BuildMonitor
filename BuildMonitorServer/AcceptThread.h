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

#pragma once

#include "FixInfo.h"

#include <qtcpsocket.h>
#include <qthread.h>

class AcceptThread : public QThread
{
	Q_OBJECT

public:
	AcceptThread(qintptr socketDescriptor, class Server& inServer, QObject* parent);

	void run();

signals:
	void fixStarted(const FixInfo& fixInfo);
	void markFixed(const QString& projectName, const qint32 buildNumber);
	void error(QTcpSocket::SocketError socketError);

private:
	class Server& server;
	qintptr socketDescriptor;
};
