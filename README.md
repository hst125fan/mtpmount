# mtpmount
Mounts Media Transfer Protocol (MTP) devices as (removable) drives on Windows for access via command line.

Project is built using Visual Studio 2017. An Installation of Dokan is required for this program to run (https://dokan-dev.github.io/).

Usage only from command line. Once mtpmount executable is in a %PATH% directory, these commands should do it:

- mtpmount list available: This shows you the connected MTP devices/storages.
- mtpmount mount #x: Mount a storage media as drive. x is an Index received from previous command.
- mtpmount unmount #x: Unmount it again once you are done.

In addition to that, you can also mount a storage by its name:
- mtpmount mount <devicename> <storagename> (<driveletter>).
Also, the letter of the virtual drive can be set as shown in the example. This makes things easier when used in a script.
