# Lines starting with '#' and blank lines are ignored. 
# To uncomment a line, remove the '#'

# Used to store per user trash on the root volume
d /.Trashes                             0333     0    99 AAAAAAAAAABAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=

# Used by the system and the NFS process.
d /.vol                                 0444     0     0

d /Library/Caches                       0755     0     0
d /Library/ColorSync/Profiles/Displays  0775     0    80
d /Library/Logs				0775     0    80

# Created on restart - not sure where
f /Library/Preferences/.GlobalPreferences.plist 0644     0    80 1038862030       0 2jmj7l5rSw0yVb/vlWAYkK/YBwk=

d /Network/Servers                      0555     0     0
d /System/Library/Caches                0755     0     0

# Extensions.mkext is the kernel extension cache file.
# It is (re)created on boot if:
#   * The file is missing
#   * The file is older then /System/Library/Extensions/
f /System/Library/Extensions.mkext      0644     0     0 1015476977       0 2jmj7l5rSw0yVb/vlWAYkK/YBwk=

# Touched on restart - not sure where
f /System/Library/Filesystems/hfs.fs/hfs.label 0644     0     0 1038862030       0 2jmj7l5rSw0yVb/vlWAYkK/YBwk=
f /System/Library/Filesystems/hfs.fs/hfs.name 0644     0     0 1038862030       0 2jmj7l5rSw0yVb/vlWAYkK/YBwk=

# User space
d /Users                                0755     0     0

# Mount point for other drives
d /Volumes                             1777     0     0 AAAAAAAAAABAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=

# Managed by the OS
d /dev                                  0555     0     0

# Managed by /etc/rc
f /mach.sym                             0444     0    80 1033481640       0 2jmj7l5rSw0yVb/vlWAYkK/YBwk=

# Cups cert created on restart
d /private/etc/cups/certs               0711     1    80

d /private/tmp                          1777     0     0

#  /private/var/db contains databases of system information.  This includes
#  netinfo and printing spools.  Uncomment this line if you do not want radmind
#  to maintain such information.
f /private/var/backups/local.nidump     0644     0     0 1037607307       0 2jmj7l5rSw0yVb/vlWAYkK/YBwk=

d /private/var/log                      0755     0     0
f /private/var/msgs/bounds              0664     0     0 1037607307       0 2jmj7l5rSw0yVb/vlWAYkK/YBwk=
d /private/var/radmind/client           0755     0     0
d /private/var/root                     0750     0     0
d /private/var/run                      0775     0     1

d /private/var/spool/cups               0700     1    80

d /private/var/tmp                      1777     0     0
d /private/var/vm                       0755     0     0
