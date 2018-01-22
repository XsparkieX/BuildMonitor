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

#include "ProjectPickerDialog.h"

#include "Settings.h"

#include <qabstractbutton.h>
#include <qstandarditemmodel.h>

ProjectPickerDialog::ProjectPickerDialog(QWidget* parent, Settings& inSettings, const std::vector<QString>& inProjects) :
	QDialog(parent),
	settings(inSettings),
	projects(inProjects)
{
	ui.setupUi(this);

	constructListEntries();

	connect(ui.confirmationButtons, &QDialogButtonBox::accepted, this, &ProjectPickerDialog::onAccepted);
}

void ProjectPickerDialog::constructListEntries()
{
	QStandardItemModel* model = new QStandardItemModel(ui.projectList);

	for (size_t i = 0; i < projects.size(); ++i)
	{
		QStandardItem* item = new QStandardItem();
		item->setCheckable(true);
		item->setCheckState(std::find(settings.enabledProjectList.begin(),
			settings.enabledProjectList.end(), projects[i]) == settings.enabledProjectList.end() ?
				Qt::CheckState::Unchecked : Qt::CheckState::Checked);
		item->setText(projects[i]);
		model->setItem(static_cast<int>(i), item);
	}

	ui.projectList->setModel(model);
}

void ProjectPickerDialog::onAccepted()
{
	settings.enabledProjectList.clear();

	QStandardItemModel* model = static_cast<QStandardItemModel*>(ui.projectList->model());
	const int rowCount = model->rowCount();
	for (int i = 0; i < rowCount; ++i)
	{
		const QStandardItem* item = model->takeItem(i);
		if (item->checkState() == Qt::CheckState::Checked)
		{
			settings.enabledProjectList.emplace_back(item->text());
		}
	}

	settings.saveSettings();
}
