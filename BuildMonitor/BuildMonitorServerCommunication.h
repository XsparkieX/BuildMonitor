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

#include "BuildMonitorServerWorker.h"
#include "FixInformation.h"

#include <qdatastream.h>
#include <qobject.h>
#include <qtcpsocket.h>

class BuildMonitorServerCommunication : public QObject
{
	Q_OBJECT

public:
	BuildMonitorServerCommunication(QObject* parent);
	virtual ~BuildMonitorServerCommunication();
	
	void setServerAddress(const QString& serverAddress);
	void requestFixInformation(const class ProjectInformationFolder& projects);
	void requestReportFixing(const QString& projectName, const qint32 buildNumber);
	void requestReportFixed(const QString& projectName, const qint32 buildNumber);

Q_SIGNALS:
	void onFixInformationUpdated(const std::vector<FixInformation>& fixInformation);
	void processQueue();

private slots:
	void onFailure(BuildMonitorRequestType type);
	void onResponseGenerated(QByteArray data);

private:
	class QThread* workerThread;
	class BuildMonitorServerWorker* worker;

	std::vector<FixInformation> fixInformation;
};
