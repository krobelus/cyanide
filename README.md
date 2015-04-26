Cyanide is a client for [Tox](https://tox.im).
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
- Multiprofile

Planned Features
----------------

- audio/video calls
- groupchats
- chat logs

Installing
----------
Use the [warehouse app](https://openrepos.net/content/basil/warehouse-sailfishos) for easy installation and updates

Translations
------------

You can use [Weblate](https://hosted.weblate.org/projects/cyanide/) to create
and improve translations. Alternatively, send a patch or use github.

Building
--------

I use the sailfish sdk to build this program. It requires several
libraries which are not in the official repositories. I built them
using the [Mer Project Open Build Service](https://build.merproject.org/)

You can use this script:

``
$ sh get_libraries.sh
``

It will download and extract the rpms I built to the "res" folder,
   then compiling should work in QtCreator.
