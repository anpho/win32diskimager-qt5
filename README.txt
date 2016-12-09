Image Writer for Microsoft Windows
Release 1.0.0 - The "Holy cow, we made a 1.0 Release" release.
======
About:
======
This utility is used to read and write raw image files to SD and USB memory devices.
Simply run the utility, point it at your raw image, and then select the
removable device to write to.

This utility can not write CD-ROMs.  USB Floppy is NOT supported at this time.

Future releases and source code are available on our Sourceforge project:
http://sourceforge.net/projects/win32diskimager/

This program is Beta, and has no warranty. It may eat your files,
call you names, or explode in a massive shower of code. The authors take
no responsibility for these possible events.

===================
Build Instructions:
===================
Requirements:
1. Now using QT 5.7/MinGW 5.3.  Snapshot available in the Build Tools directory at
https://sourceforge.net/projects/win32diskimager/files/Build%20Tools/

Short Version:
1. Install the Qt Full SDK and use QT Creator to build.  Included batch files 
   no longer updated and may be deleted in the future.
   See DEVEL.txt for details

=============
New Features:
=============
Add user defined file types capability.
Some additional language translations.

=============
Known Issues:
=============

*  Lack of reformat capabilities.
*  Lack of file compression support
*  Does not work with USB Floppy drives.

These are being looked into for future releases.

======
Legal:
======
Image Writer for Windows is licensed under the General Public
License v2. The full text of this license is available in 
GPL-2.

This project uses and includes binaries of the MinGW runtime library,
which is available at http://www.mingw.org

This project uses and includes binaries of the Qt library, licensed under the 
"Library General Public License" and is available at 
http://www.qt-project.org/.

The license text is available in LGPL-2.1

Original version developed by Justin Davis <tuxdavis@gmail.com>
Maintained by the ImageWriter developers (http://sourceforge.net/projects/win32diskimager).

