/* BuildMonitor - Monitor the state of projects in CI.
 * Copyright (C) 2017-2019 Sander Brattinga

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

#include <qstandarditemmodel.h>
#include <qtablewidget.h>

#include "FixInfo.h"

class FixOverviewTable : public QTableWidget
{
	Q_OBJECT

public:
	FixOverviewTable(QWidget* parent);

	void setFixInformation(const std::vector<FixInfo>& fixInformation);

private:
	std::vector<class QTableWidgetItem*> itemPool;
	QStringList headerLabels;
};
