Cyanide is a client for [Tox](https://tox.chat).
Some features are missing and there may be some bugs.

Features
--------

- 1v1 messaging
- DNS discovery
- Typing notification
- Sound, LED & lockscreen notifications
- Avatars
- Inline Images
- File Transfers (no resuming across restarts yet)
- History
- Multiprofile

Planned Features
----------------

- audio/video calls
- groupchats

Installing
----------
Use the [warehouse app](https://openrepos.net/content/basil/warehouse-sailfishos) for easy installation and updates

Translations
------------

You can use [Weblate](https://hosted.weblate.org/projects/cyanide/) to create
and improve translations. Alternatively, send a patch or use github.

Building
--------

First install the [Sailfish OS SDK](https://sailfishos.org/wiki/Application_SDK_Installation).

Then you need some libraries which are not in the official repositories.  I
built them using the [Mer Project Open Build
Service](https://build.merproject.org/)

You can use this script:

``
$ sh get_libraries.sh
``

It will download the RPMs I built to the "res" folder. Then you need to install
those on the Mer SDK VM. In the VM there is the folder `/home/merdsk/share`
which is a shared folder of the home directory of the host system.

``
$ ssh -p 2222 -i ~/SailfishOS/vmshare/ssh/private_keys/engine/mersdk mersdk@localhost
$ sb2 -t SailfishOS-armv7hl -m sdk-install -R rpm -i /home/merdsk/share/path/to/cyanide/res/*.rpm
``
