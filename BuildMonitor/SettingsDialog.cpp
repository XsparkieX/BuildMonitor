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

#include "SettingsDialog.h"

#include "Settings.h"

#include <qdialogbuttonbox.h>

SettingsDialog::SettingsDialog(QWidget* parent, class Settings& inSettings) :
	QDialog(parent),
	settings(inSettings)
{
	ui.setupUi(this);

	QString serverURLList = inSettings.serverURLs.size() > 0 ? inSettings.serverURLs[0].toString() : "";
	for (size_t i = 1; i < inSettings.serverURLs.size(); ++i)
	{
		serverURLList += "\n" + inSettings.serverURLs[i].toString();
	}
	ui.fixServerAddress->setText(inSettings.fixServerAddress);
	ui.serverURLs->setPlainText(serverURLList);
	QString ignoreUserList = inSettings.ignoreUserList.size() > 0 ? inSettings.ignoreUserList[0] : "";
	for (size_t i = 1; i < inSettings.ignoreUserList.size(); ++i)
	{
		ignoreUserList += ", " + inSettings.ignoreUserList[i];
	}
	ui.nameIgnoreList->setText(ignoreUserList);
	ui.refreshInterval->setValue(inSettings.refreshIntervalInSeconds);
	ui.useRegExProjectFilter->setChecked(inSettings.useRegExProjectFilter);
	ui.projectIncludeRegExp->setText(inSettings.projectIncludeRegEx.pattern());
	ui.projectExcludeRegExp->setText(inSettings.projectExcludeRegEx.pattern());
	ui.showDisabledBuilds->setChecked(inSettings.showDisabledProjects);
	ui.showProgressForProject->setText(inSettings.showProgressForProject);
	ui.closeToTrayOnStartup->setChecked(inSettings.closeToTrayOnStartup);

	connect(ui.buttonBox, &QDialogButtonBox::clicked, this, &SettingsDialog::onButtonClicked);
}

void SettingsDialog::onButtonClicked(QAbstractButton* button)
{
	QDialogButtonBox::ButtonRole role = ui.buttonBox->buttonRole(button);
	if (role == QDialogButtonBox::AcceptRole || role == QDialogButtonBox::ApplyRole)
	{
		QStringList serverURLs = ui.serverURLs->toPlainText().split("\n");
		settings.serverURLs.clear();
		for (QString& serverURL : serverURLs)
		{
			serverURL = serverURL.trimmed();
			if (!serverURL.isEmpty())
			{
				settings.serverURLs.emplace_back(serverURL);
			}
		}
		settings.fixServerAddress = ui.fixServerAddress->text();
		QStringList ignoreUsers = ui.nameIgnoreList->text().split(",");
		settings.ignoreUserList.clear();
		for (const QString& user : ignoreUsers)
		{
			settings.ignoreUserList.emplace_back(user.trimmed());
		}
		settings.refreshIntervalInSeconds = ui.refreshInterval->value();
		settings.useRegExProjectFilter = ui.useRegExProjectFilter->isChecked();
		settings.projectIncludeRegEx.setPattern(ui.projectIncludeRegExp->text());
		settings.projectExcludeRegEx.setPattern(ui.projectExcludeRegExp->text());
		settings.showDisabledProjects = ui.showDisabledBuilds->isChecked();
		settings.showProgressForProject = ui.showProgressForProject->text();
		settings.closeToTrayOnStartup = ui.closeToTrayOnStartup->isChecked();
		settings.saveSettings();
	}
}
