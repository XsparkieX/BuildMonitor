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

#include "Utils.h"

#include <algorithm>

std::vector<std::string> ToStringVector(const char* const * const ptr, size_t num)
{
	std::vector<std::string> result;
	for (size_t i = 0; i < num; ++i)
	{
		result.emplace_back(ptr[i]);
	}
	return result;
}

std::string GetDisplayableUserList(const std::vector<std::string>& users,
	const std::vector<std::string>& ignoreList, bool andCase, bool orCase)
{
	std::string result;
	if (!users.empty())
	{
		std::vector<std::string> displayList;
		for (auto& user : users)
		{
			if (std::find(ignoreList.begin(), ignoreList.end(), user) == ignoreList.end())
			{
				displayList.emplace_back(user);
			}
		};

		auto pos = displayList.begin();
		if (pos != displayList.end())
		{
			result += *pos;
			++pos;
			if (pos != displayList.end())
			{
				for (; pos != displayList.end() - 1; ++pos)
				{
					result += ", ";
					result += *pos;
				}

				if (pos != displayList.end())
				{
					if (andCase && orCase)
					{
						result += " and/or ";
					}
					else if (andCase)
					{
						result += " and ";
					}
					else if (orCase)
					{
						result += " or ";
					}
					result += *pos;
				}
			}
		}

		if (displayList.empty())
		{
			result += "Unknown";
		}
	}
	else
	{
		result += "Unknown";
	}

	return result;
}