To help contribute translations, you can either download the base translation for the current release
(diskimager_en.ts) or clone the entire git repository (see DEVEL.txt).  Here are the basic steps:

===========
Getting Qt:
===========
You will need QT to do the translation.  Download it from:
http://download.qt.io/official_releases/online_installers/qt-unified-windows-x86-online.exe
The installation is as follows:
welcome-> next
qt account -> leave all empty, next
setup -> next
installation folder: c:\qt -> next
select components: qt 5.7, deselect all, select minGW 5.3.0 -> next
license Agreement: agree -> next
start menu shortcuts -> next
install
installation finished -> next -> launch

======================
Add a new translation:
======================
First, install QT as described above.
Next, run QT Linguist from the Windows Start Menu->QT folder.
File -> Open diskimager_en.ts
edit-> translation file settings, set target language and country/region.
file-> save as Open diskimager_??.ts (where ?? is the region code for your translation).
translate all the strings into the new language, save the file.  To test it, select
file->Release As and save teh resulting diskimager_??.qm to the translations directory where you
installed Win32DiskImager.


======================
Share your translation:  
======================
You can share this easily by uploading the diskimager_??.ts file as an attachment to the Feature Requests,
email me a copy, or if you cloned the git repository, follow the instructions on Sourceforge to fork and do a 
merge request.
