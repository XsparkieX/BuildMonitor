# BuildMonitor
BuildMonitor is a tool that helps you monitor the state of Jenkins projects that are matching your criteria.
It supports an additional server you can setup so you can show who is working on getting the build stable again.
Notifications are shown when the build becomes unstable and when it's back stable again.

# Building BuildMonitor
## Linux
### Prerequisites
* Qt5
* Qt Creator

### Steps
1. Open the .pro file located in the BuildMonitor folder and associate your Qt Kit with the project.
2. Build for the configurtion you want and it will result in the executable being generated.

## Windows
### Prerequisites
* Qt5
* MingW compiler toolchain from Qt5 installer
* NSIS Installer framework (if you want to create an installer)

### Steps
1. Go to .\BuildMonitor\Installer
2. Double click CreateInstaller.bat. This will compile the project and create an installer.

or

1. Open the .pro file located in the BuildMonitor folder and associate your Qt Kit with the project.
2. Build for the configurtion you want and it will result in the executable being generated.

