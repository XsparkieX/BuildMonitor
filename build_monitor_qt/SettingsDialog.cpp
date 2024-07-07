/* BuildMonitor - Monitor the state of projects in CI.
 * Copyright (C) 2017-2021 Sander Brattinga

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

	ui.serverAddress->setText(QString::fromStdString(inSettings.serverAddress));
	ui.multicast->setChecked(inSettings.multicast);
	auto ignoreUserList = inSettings.ignoreUserList.size() > 0 ? inSettings.ignoreUserList[0] : "";
	for (size_t i = 1; i < inSettings.ignoreUserList.size(); ++i)
	{
		ignoreUserList += ", " + inSettings.ignoreUserList[i];
	}
	ui.nameIgnoreList->setText(QString::fromStdString(ignoreUserList));
	ui.showDisabledBuilds->setChecked(inSettings.showDisabledProjects);
	ui.closeToTrayOnStartup->setChecked(inSettings.closeToTrayOnStartup);

	connect(ui.buttonBox, &QDialogButtonBox::clicked, this, &SettingsDialog::onButtonClicked);
}

void SettingsDialog::onButtonClicked(QAbstractButton* button)
{
	QDialogButtonBox::ButtonRole role = ui.buttonBox->buttonRole(button);
	if (role == QDialogButtonBox::AcceptRole || role == QDialogButtonBox::ApplyRole)
	{
		settings.serverAddress = ui.serverAddress->text().trimmed().toStdString();
		settings.multicast = ui.multicast->isChecked();
		QStringList ignoreUsers = ui.nameIgnoreList->text().split(",");
		settings.ignoreUserList.clear();
		for (const QString& user : std::as_const(ignoreUsers))
		{
			settings.ignoreUserList.emplace_back(user.trimmed().toStdString());
		}
		settings.showDisabledProjects = ui.showDisabledBuilds->isChecked();
		settings.closeToTrayOnStartup = ui.closeToTrayOnStartup->isChecked();
		settings.saveSettings();
	}
}
