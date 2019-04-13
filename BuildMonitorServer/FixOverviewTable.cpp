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

#include "FixOverviewTable.h"

#include <qheaderview.h>
#include <qtextstream.h>

FixOverviewTable::FixOverviewTable(QWidget* parent) :
	QTableWidget(parent)
{
	headerLabels.push_back("Project");
	headerLabels.push_back("User");
	headerLabels.push_back("Build Number");

	setColumnCount(headerLabels.size());
	setHorizontalHeaderLabels(headerLabels);
}

void FixOverviewTable::setFixInformation(const std::vector<FixInfo>& fixInformation)
{
	for (auto item : itemPool)
	{
		delete item;
	}
	itemPool.clear();
	clearContents();

	for (const auto info : fixInformation)
	{
		itemPool.push_back(new QTableWidgetItem(info.projectUrl));
		itemPool.push_back(new QTableWidgetItem(info.userName));
		QString buildNumber;
		QTextStream buildNumberStream(&buildNumber);
		buildNumberStream << info.buildNumber;
		itemPool.push_back(new QTableWidgetItem(buildNumber));
	}

	const qint32 numEntries = static_cast<qint32>(fixInformation.size());
	setRowCount(numEntries);
	const qint32 numHeaders = headerLabels.size();
	for (qint32 row = 0; row < numEntries; ++row)
	{
		for (qint32 column = 0; column < numHeaders; ++column)
		{
			const qint32 itemLocationInArray = row * numHeaders + column;
			QTableWidgetItem* item = itemPool[itemLocationInArray];
			item->setToolTip(item->text());
			setItem(row, column, item);
		}
	}

	resizeColumnsToContents();
}
