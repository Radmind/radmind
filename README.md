Radmind
=======

Radmind is a suite of Unix command-line tools and a server designed to remotely administer the file systems of multiple Unix machines.

Copyright (c) 2003 Regents of The University of Michigan.
All Rights Reserved. See COPYRIGHT.

## Contents

* [Installation](#installation)
* [Known Issues](#known-issues)
* [More Information](#more-information)
* [References](#references)

## Installation

Until we get pre-built binaries you have to compile it yourself. [Please see BUILD.md](BUILD.md).

## Known Issues

* On OpenDarwin based systems, the message "hfs_bwrite: called with lock bit set" is logged when you are doing a high volume of writes to a volume.
* `lcksum`'s progress output currently does not provide steady feedback increments.

## More Information

If you have any problems with this source, you may want to check [the issue tracker](../../issues) to see if any problems have reports. You can also contact the Radmind development team by e-mailing [mlib-its-mac-github-radmind@lists.utah.edu](mailto:mlib-its-mac-github-radmind@lists.utah.edu)

An archived e-mail discussion list has also been set up. See the website for details on how to join.

In June of 2015, management of this project was transferred from the University of Michigan to the University of Utah. The University of Utah decided to migrate the project from SourceForge to GitHub. Note that the University of Utah, while longtime users of Radmind, are no longer using it or contributing to the upkeep of the project except to merge pull requests, make sure it builds, and keep this readme updated. If you feel you would be a better steward for Radmind's future, contact us.

The transfer of issues/bugs and their comments was automated using the [gosf2github](https://github.com/cmungall/gosf2github) script. Because no username map was readily available, all of the issues and comments were automatically assigned to the member of the University of Utah's team who managed the migration, [@pdarragh](https://github.com/pdarragh).

## References

[1]: Current submodules:
* [libsnet](https://github.com/Radmind/libsnet), a networking library with TLS support
