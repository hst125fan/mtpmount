# mtpmount
Mounts Media Transfer Protocol (MTP) devices as (removable) drives on Windows for access via command line.

![AppVeyor](https://img.shields.io/appveyor/ci/hst125fan/mtpmount?label=CI%26CD&style=plastic&logo=appveyor)
![License](https://img.shields.io/badge/license-WTFPL-brightgreen?style=plastic)

Project is built using Visual Studio 2017. An Installation of Dokan is required for this program to run (https://dokan-dev.github.io/).

Usage only from command line. Once mtpmount executable is in a %PATH% directory, these commands should do it:

- mtpmount list available: This shows you the connected MTP devices/storages.
- mtpmount mount #x: Mount a storage media as drive. x is an Index received from previous command.
- mtpmount unmount #x: Unmount it again once you are done.

In addition to that, you can also mount a storage by its name:
- mtpmount mount devicename storagename (driveletter).
Also, the letter of the virtual drive can be set as shown in the example. This makes things easier when used in a script.


# Get binaries
find them here: https://ci.appveyor.com/project/hst125fan/mtpmount/build/artifacts (there is one version for 32-bit systems and one for 64-bit systems. The 32-bit executable should also work on 64-bit systems, however.)

You need to install Dokan (https://github.com/dokan-dev/dokany/releases/latest) before you use mtpmount.

# Build it yourself
1. Install Dokan (see above). You will need the "full" package (including development content), the MSI installers have these components opted out by default.
2. Install Visual Studio 2017 (if you don't have that already). Community works fine.
3. Check out repository.
4. Open mtp-2-drive.sln in Visual Studio.
5. Build target "mtpmount" in desired configuration (Debug/Release/x86/x64)

# Testing
mtp-2-drive.sln also contains a "tests" target, which contains some integration tests for the most important features/use cases. The tests will automatically run on build. If you need to debug them, you can do so by starting tests_dbg with attached debugger (F5).
