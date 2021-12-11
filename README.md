# BuildMonitor
BuildMonitor is a tool that helps you monitor the state of Jenkins projects.

Features include:
* Notifications when the project state changes.
* Easy overview of the projects.
* Easy way to navigate to the important pages.
* Communicate with your team that a fix is being worked on.

# End User Installation
On the releases page you will find an installer for the Windows platform. Other platforms will have to build manually, see the sections [Building the Project](#Building-the-Project) and [Building the User Interface](#Building-the-User-Interface).

# Server Setup
The server is required for the UI project to receive information about the projects. Every client will listen for messages on the multicast address.

1. Download `build_monitor_cli.exe` from the releases page.
2. Run `build_monitor_cli.exe --server {Jenkins URL} 239.255.13.37:8090`. Where Jenkins URL is the location where Jenkins is hosted.

The server should now be up and running and broadcasting your Jenkins projects over the network.

# Building the Project
## Prerequisites
* Rust
* cbindgen

## Steps for the CAPI Library
This is the most likely one you'll need to build. It is required for the User Interface project.

1. Navigate to build_monitor\capi
2. Run build.bat or build.sh depending on your platform.

## Steps for the CLI
The CLI project is an easy way to verify if your changes work in the Rust library. At the moment of writing it's also the only way to start a server.

Navigate to build_monitor\cli to find the project.

Run `cargo run --release -- --client {multicast_address}` for verifying the client functionality works.

Run `cargo run --release -- --retrieveinfo {jenkins_url}` for checking if you can get all information from Jenkins without broadcasting.

Run `cargo run --release -- --server {jenkins_url} {multicast_address}` to start a server


# Building the User Interface
## Prerequisites
* Qt 5.13
* Qt Creator
* NSIS Installer framework (if you want to create an installer on Windows)

## Steps
1. Open the BuildMonitor.pro file located in the build_monitor_qt folder and associate your Qt Kit with the project.
2. Build for the configurtion you want and it will result in the executable being generated.
3. Optional: If you need an installer on Windows, run build_monitor_qt\Installer\CreateInstaller.bat

# License
BuildMonitor has various licenses, depending on which part is looked at.

## Main Project
The **build_monitor** project is licenced under either of
* Apache License, Version 2.0, ([build_monitor/LICENSE-APACHE](build_monitor/LICENSE-APACHE) or http://www.apache.org/licenses/LICENSE-2.0)
* MIT license ([build_monitor/LICENSE-MIT](build_monitor/LICENSE-MIT) or http://opensource.org/licenses/MIT)

at your option.

### Contribution
Unless you explicitly state otherwise, any contribution intentionally submitted for inclusion in the work by you, as defined in the Apache-2.0 license, shall be dual licensed as above, without any additional terms or conditions.

## User Interface
The **build_monitor_qt** project is licensed under:
* GNU General Public License 3.0 ([build_monitor_qt/LICENSE](build_monitor_qt/LICENSE) or https://www.gnu.org/licenses/gpl-3.0.html).

# Support
If you like my work, you can support me by buying me a beer:

<a href="https://www.buymeacoffee.com/sparkie" target="_blank"><img src="https://cdn.buymeacoffee.com/buttons/v2/default-yellow.png" alt="Buy Me A Beer" style="height: 60px !important;width: 217px !important;" ></a>
