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

#include <qsharedmemory.h>
#include <qsystemsemaphore.h>

class SingleInstanceMode
{
public:
	SingleInstanceMode(const QString& name) :
		sharedMemory(name + "Memory"),
		lock(name + "Lock", 1)
	{
		lock.acquire();
		{
			QSharedMemory fix(name + "Memory");    // Fix for *nix: http://habrahabr.ru/post/173281/
			fix.attach();
		}
		lock.release();
	}

	virtual ~SingleInstanceMode()
	{
		lock.acquire();
		if (sharedMemory.isAttached())
		{
			sharedMemory.detach();
		}
		lock.release();
	}

	bool isAlreadyRunning()
	{
		lock.acquire();
		if (!sharedMemory.isAttached()) // If we are attached, we are already running ourselves and a copy is being made of this class.
		{
			if (!sharedMemory.attach()) // Check if we cannot attach to the shared object. If we can, there is another instance running.
			{
				if (sharedMemory.create(sizeof(quint64))) // Create automatically attaches us, preventing other applications to attach.
				{
					lock.release();
					return false;
				}
			}
			else
			{
				sharedMemory.detach();
			}
		}
		lock.release();
		return true;
	}

private:
	SingleInstanceMode(const SingleInstanceMode&) = delete;
	SingleInstanceMode& operator = (const SingleInstanceMode&) = delete;

	QSharedMemory sharedMemory;
	QSystemSemaphore lock;
};