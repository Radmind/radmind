Radmind
=======

Radmind is a suite of Unix command-line tools and a server designed to remotely administer the file systems of multiple Unix machines.

Copyright (c) 2003 Regents of The University of Michigan.
All Rights Reserved. See COPYRIGHT.

## Contents

* [Quick Installation Instructions](#quick-installation-instructions)
* [Detailed Installation Instructions](#detailed-installation-instructions)
  * [Getting the Source](#getting-the-source)
  * [Configuring for RedHat 9](#configuring-for-redhat-9)
  * [Configuring and Building](#configuring-and-building)
* [Building an OS X Installer Package](#building-an-os-x-installer-package)
* [Known Issues](#known-issues)
* [More Information](#more-informatino)
* [References](#references)

## Quick Installation Instructions

From within the source directory:

```
$ ./configure
$ make
$ make install
```

## Detailed Installation Instructions

### Getting the Source

You can either download the source from the [Radmind project homepage](http://radmind.org/) and uncompress it into a directory of your choice, or else you can use git to build the most recent development source of the project.

Building Radmind from the git repository is a good way to ensure you've got the most up-to-date version of everything. You can also help contribute by filing bug reports on the [Radmind GitHub page](https://github.com/Radmind/radmind).

First clone the repository locally:

```
$ git clone https://github.com/Radmind/radmind.git radmind
```

Then move into the directory and check out the required submodules [1]:

```
$ cd radmind
$ git submodule init
$ git submodule update
```

### Configuring for RedHat 9

To properly build Radmind on RedHat 9 with SSL support, you have to specify the location of your Kerberos files:

```
$ export CPPFLAGS=-I/usr/kerberos/include
```

### Configuring for macOS X 10.12

macOS no longer includes everything needed to build radmind.  Install Xcode.  Download libsnet and place it in your radmind source directory.  The easiest way to get the other components is with [brew](https://brew.sh).

```
$ brew install autoconf
$ brew install openssl
```

From within the source directory:

```
$ /usr/local/bin/autoconf
$ ./configure CPPFLAGS="-I/usr/local/opt/openssl/include" LDFLAGS="-L/usr/local/opt/openssl/lib"
$ make
$ make install
```

### Configuring and Building

Now that everything is set up, we have to actually do the configuration and installation. Configure the build:

```
$ ./configure
```

Note that the configure scripts take several options. To see them all, do `./configure --help`.

Now we're ready to actually build everything together:

```
$ make
$ make install
```

## Building an OS X Installer Package

The Radmind Makefile contains a target called `package`, which will construct a Mac OS X installer package suitable for distribution. To make the package, log in as an administrator, enter the Radmind source directory, and follow the steps below:

```
$ ./configure
$ make package
```

During the build process, you will be prompted for your password.

PackageMaker currently does not work with `make`, so at the end of the build process you will see "make: *** [package] Error 2" even though that package was created successfully.

After the source has been built and the package created, you will be left with a package called 'RadmindTools.pkg' in the parent directory of the Radmind source. This file may be double-clicked in the Finder to launch the Installer.

This target will fail if it is used on a system other than Mac OS X.

## Known Issues

* On OpenDarwin based systems, the message "hfs_bwrite: called with lock bit set" is logged when you are doing a high volume of writes to a volume.
* `lcksum`'s progress output currently does not provide steady feedback increments.

## More Information

If you have any problems with this source, you may want to check [the issue tracker](../../issues) to see if any problems have reports. You can also contact the Radmind development team by e-mailing [mlib-its-mac-github-radmind@lists.utah.edu](mailto:mlib-its-mac-github-radmind@lists.utah.edu)

An archived e-mail discussion list has also been set up. See the website for details on how to join.

In June of 2015, management of this project was transferred from the University of Michigan to the University of Utah. The University of Utah decided to migrate the project from the rapidly-deteriorating SourceForge hosting site over to GitHub. We felt that this would help keep the project alive and make it easier to maintain. Note that the University of Utah, while longtime users of Radmind, are no longer contributing to the upkeep of the project. If you feel you would be a better steward for Radmind's future, contact us.

The transfer of issues/bugs and their comments was automated using the [gosf2github](https://github.com/cmungall/gosf2github) script. Because no username map was readily available, all of the issues and comments were automatically assigned to the member of the University of Utah's team who managed the migration, [@pdarragh](https://github.com/pdarragh).

## References

[1]: Current submodules:
* [libsnet](http://sourceforge.net/projects/libsnet), a networking library with TLS support
