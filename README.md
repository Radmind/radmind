Radmind
=======

Radmind is a suite of Unix command-line tools and a server designed to remotely administer the file systems of multiple Unix machines.

Radmind was popular in Mac OS X administration from about 2003 to 2008 and was slowly superceeded by people switching to Jamf or other utilties. The project is barely being maintained, but it is still useful. However, it lacks support for modern changes like macOS SIPS and SELinux. With the advent of AI Coding Agents, this project might get more attention to bring it up to spec again.

## Contents

* [Installation](#installation)
* [Known Issues](#known-issues)
* [History](#history)
* [References](#references)

## Installation

There should be a macOS package and Ubuntu binaries in the [Releases](releases). We are working on a Homebrew tap.

To compile it, [please see BUILD.md](BUILD.md).

## Known Issues

* On OpenDarwin based systems, the message "hfs_bwrite: called with lock bit set" is logged when you are doing a high volume of writes to a volume.
* `lcksum`'s progress output currently does not provide steady feedback increments.

If you have any problems with this source, you may want to check [the issue tracker](../../issues) to see if any problems have reports. You can also contact the Radmind development team by e-mailing [mlib-its-mac-github-radmind@lists.utah.edu](mailto:mlib-its-mac-github-radmind@lists.utah.edu)

## History

Radmind emerged at the University of Michigan as an open-source response to a stubborn early-2000s systems-administration problem: managing a fleet of Unix computers without giving up the ability to notice when their filesystems had drifted, been damaged, or been altered maliciously. The project was developed by the university's Research Systems Unix Group (RSUG). In its first substantial published description, the UMich developers presented it at the 2003 USENIX LISA conference as an integrated filesystem-management and integrity-checking system.

The key idea was not merely to copy files onto clients. Like Tripwire, Radmind compared a machine's filesystem with an expected specification; unlike a standalone integrity checker, it retained the authoritative files on a server and could restore a machine to that specification. The 2003 paper called the resulting combination a way to detect divergence and reverse it, while avoiding the usual conflict in which ordinary software deployment makes an intrusion-detection baseline appear perpetually out of date. This made Radmind especially attractive to labs and other installations that needed both repeatable desktops and a record of unexpected change.

### The CLI model

Radmind's original and enduring center is a suite of Unix command-line programs plus a server daemon. Administrators describe desired filesystem objects in *transcripts* and assemble them through *command files*. A client can compare local state with the specification, retrieve a revised specification, and apply the corresponding files from the server. The loadset model supports layering: an operating-system baseline can be kept separate from application, departmental, or machine-specific overlays. In other words, Radmind treated the filesystem itself as the deployment artifact and used ordered text specifications to decide what should prevail.

The command names capture that workflow:

- `ktcheck` refreshes local command files and transcripts from the server.
- `fsdiff` reports the differences between the local filesystem and the transcripts.
- `lapply` brings the client into line with a transcript, retrieving files when necessary.
- `lcreate`, `lcksum`, `lfdiff`, and `lmerge` support publishing, verifying, comparing, and composing loadsets.
- The `radmind` daemon exposes command files, transcripts, and content to clients; `twhich` identifies the transcript that covers a file.

This division of labor helps explain the suite's character: it was not a general-purpose remote-execution framework, but a carefully scoped, file-oriented configuration-management system with an audit-and-remediate loop.

### From Unix infrastructure to Mac deployment

Although conceived for diverse Unix systems, Radmind became closely associated with Mac OS X administration. The project's documentation traces a public presentation history through WWDC and Mac-focused events in 2002, 2003, 2004, 2005, and 2008. Its documentation also included a Mac OS X manual and a command-line tutorial for maintaining a group of Macs, while explicitly noting that the underlying lessons applied to any Unix platform. A graphical companion, Radmind Assistant, made the system more approachable on the Mac, but the CLI remained the authoritative operational interface: the project described the Unix manual pages as the most comprehensive reference for how its tools work together.

The University of Utah became an important operational adopter and documentation contributor. Its resource material directed administrators to the official Radmind site for software, mailing lists, and documentation, while Utah's own FAQ and deployment notes supplied practical Mac-lab guidance. This is useful historical evidence: Radmind was sustained not only by its original developers but also by university computing groups that used it in day-to-day fleet maintenance.

### Security and portability

As the project matured, its documentation added deployment-oriented security material, including TLS certificate and PAM cookbooks. The system was presented as capable of authenticated, encrypted client/server operation—not just a network file copier. The project also broadened beyond its initial Unix focus. The official material lists successful use on Linux, macOS, Solaris, and the BSD family, and a Windows port, Radmind-PC, adapted the approach to NTFS and added registry-management commands. These developments kept the central premise intact: compare state against a managed specification, then selectively correct it.

### Current Steward

In June of 2015, management of this project was transferred from the University of Michigan to the University of Utah. The University of Utah decided to migrate the project from SourceForge to GitHub. Note that the University of Utah, while longtime users of Radmind, are no longer using it or contributing to the upkeep of the project except to merge pull requests, make sure it builds, and keep this readme updated. If you feel you would be a better steward for Radmind's future, contact us.

The transfer of issues/bugs and their comments was automated using the [gosf2github](https://github.com/cmungall/gosf2github) script. Because no username map was readily available, all of the issues and comments were automatically assigned to the member of the University of Utah's team who managed the migration, [@pdarragh](https://github.com/pdarragh).

## References

Current submodules:

* [libsnet](https://github.com/Radmind/libsnet), a networking library with TLS support

1. Archived Radmind home page, 16 July 2024: <https://web.archive.org/web/20240716213408/https://radmind.org/>.

2. Archived University of Utah, *Radmind Resources* PDF, 7 January 2026 capture: <https://web.archive.org/web/20260107222140/https://www.macos.utah.edu/documentation/system_deployment/radmind/resources/related/mainColumnParagraphs/02/document/UofU_Radmind_Resources.pdf>.
