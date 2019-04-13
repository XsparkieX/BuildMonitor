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

enum class EProjectStatus
{
	Aborted,
	Disabled,
	Succeeded,
	Unstable,
	Failed,
	NotBuilt,
	Unknown
};

inline const char* projectStatus_toString(EProjectStatus status)
{
	switch (status)
	{
	case EProjectStatus::Aborted: return "Aborted";
	case EProjectStatus::Disabled: return "Disabled";
	case EProjectStatus::Succeeded: return "Succeeded";
	case EProjectStatus::Unstable: return "Unstable";
	case EProjectStatus::Failed: return "Failed";
	case EProjectStatus::NotBuilt: return "Not Built";
	case EProjectStatus::Unknown: return "Unknown";
	}

	return "Unknown";
}

inline bool projectStatus_isFailure(EProjectStatus status)
{
	return status == EProjectStatus::Failed || status == EProjectStatus::Unstable;
}