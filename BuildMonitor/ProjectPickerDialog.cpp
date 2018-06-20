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
#include <qtreewidget.h>

ProjectPickerDialog::ProjectPickerDialog(QWidget* parent, Settings& inSettings, const ProjectInformationFolder& inProjects) :
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
	ui.projectTree->clear();
	
	qint32 numProjects = 0;
		
	std::vector<std::pair<QTreeWidgetItem*, const ProjectInformationFolder*> > foldersToParse;
	for (const std::shared_ptr<ProjectInformationFolder>& folder : projects.folders)
	{
		foldersToParse.emplace_back(nullptr, folder.get());
	}
	
	std::vector<std::pair<QTreeWidgetItem*, const ProjectInformation*> > projectsToParse;
	while (!foldersToParse.empty())
	{
		QTreeWidgetItem* parent = new QTreeWidgetItem(foldersToParse[0].first);
		if (foldersToParse[0].first == nullptr)
		{
			ui.projectTree->addTopLevelItem(parent);
		}
		
		parent->setText(0, foldersToParse[0].second->folderName);		
		for (const std::shared_ptr<ProjectInformationFolder>& folder : foldersToParse[0].second->folders)
		{
			foldersToParse.emplace_back(parent, folder.get());
		}
		
		for (const std::shared_ptr<ProjectInformation>& info : foldersToParse[0].second->projects)
		{
			projectsToParse.emplace_back(parent, info.get());
		}
		
		foldersToParse.erase(foldersToParse.begin());
	}
	
	for (const std::pair<QTreeWidgetItem*, const ProjectInformation*>& pair : projectsToParse)
	{
		const ProjectInformation* info = pair.second;
		QTreeWidgetItem* item = new QTreeWidgetItem(pair.first);
		item->setText(0, info->projectName);
		const auto findPred = [&info] (const QString& name)
		{
			return info->projectName == name;
		};
		
		item->setCheckState(0, std::find_if(settings.enabledProjectList.begin(),
			settings.enabledProjectList.end(), findPred) == settings.enabledProjectList.end() ?
				Qt::CheckState::Unchecked : Qt::CheckState::Checked);

		++numProjects;
	}
	
	ui.projectTree->expandAll();
}

void ProjectPickerDialog::onAccepted()
{
	settings.enabledProjectList.clear();
	
	for (QTreeWidgetItemIterator pos(ui.projectTree); *pos; ++pos)
	{
		if ((*pos)->checkState(0) == Qt::CheckState::Checked)
		{
			settings.enabledProjectList.emplace_back((*pos)->text(0));
		}
	}

	settings.saveSettings();
}
