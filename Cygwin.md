Running the CE 424 Vagrant VM on Windows](https://gist.github.com/rogerhub/456ae31427aafe5b70f7)
========================================

**Note**: If you have the latest version of Windows 10 or Windows 11, you'll probably be OK with PowerShell or GitBash and will not need Cygwin. However, if you encounter errors, we strongly recommend using Cygwin.

There are some challenges to running Vagrant on Windows. This file is meant to provide assistance to Windows users.

### Set up Cygwin

Cygwin provides a set of linux tools for Windows computers. You should install Cygwin and use the Cygwin terminal for SSH.

1. Go to [the Cygwin website](http://cygwin.com/install.html) and download `setupx84_64.exe` (Cygwin for 64-bit versions of Windows).
1. Run the `setupx86_64.exe` file that you downloaded.
1. At the welcome screen, press "Next".
1. At "Choose A Download Source", you should select "Install from Internet" and press "Next".
1. At "Select Root Install Directory", you should leave the default value of "Root Directory" and install for "All Users". Press "Next".
1. At "Select Local Package Directory", you should leave the default value of "Local Package Directory" and press "Next".
1. At "Select Your Internet Connection", you should leave the default value of "Direct Connection" and press "Next".
1. (No action required.) Cygwin will now download a list of mirrors (e.g. servers that host available Cygwin software).
1. At "Choose A Download Site", select any one of the download sites, and press "Next".
1. (No action required.) Cygwin will now retrieve a list of available software from the mirror.
1. You should now be at a screen titled "Select Packages". I suggest you click the "View" button at the top right corner, so that the label says "Full" instead of "Category". Make sure "Cur" is selected for the package version channel (this is the default).
1. Search for EACH of the following packages, and click the "Skip" label to select them for installation. When you click the "Skip" label, it should turn into a version number like "2.1.4-1". Some of these may already be selected for installation, in which case it will have the version number already there instead of "Skip".
  1. bash: THE GNU Bourne Again SHell
  1. git: Distributed version control system
  1. git-completion: Bash completion for Git version control system
1. Click "Next" to review the packages to install.
1. At "Resolving Dependencies", make sure "Select required packages" is selected, and press "Next".
1. (No action required.) Cygwin will now install your packages.
1. At "Create Icons", I recommend you add both the Desktop and Start Menu icons, so Cygwin Terminal is easier to find. Press "Finish" to exit the installer.