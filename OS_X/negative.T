# Lines starting with '#' and blank lines are ignored. 
# To uncomment a line, remove the '#'

# Used by the system and the NFS process.
d /.vol                                 0444     0     0

d /Library/Caches                       0755     0     0
d /Library/Logs                         0755     0     0
d /Network/Servers                      0555     0     0
d /System/Library/Caches                0755     0     0

f /System/Library/Extensions.kextcache  0644     0     0 1044312493       0 2jmj7l5rSw0yVb/vlWAYkK/YBwk=
# Extensions.mkext is the kernel extension cache file.
# It is (re)created on boot if:
#   * The file is missing
#   * The file is older then /System/Library/Extensions/
f /System/Library/Extensions.mkext      0644     0     0 1015476977       0 2jmj7l5rSw0yVb/vlWAYkK/YBwk=

d /Users                                0755     0     0
d /Volumes                              1777     0     0
d /automount                            0555     0     0
d /dev                                  0555     0     0
f /mach.sym                             0444     0     0 1014310508  563812 6rG3b+tUiem3ycBySprZ7IuRy8o=
d /private/Network                                      0755     0     0
d /private/etc/httpd/users              0755     0     0

d /private/tmp                          1777     0     0

#  /private/var/db contains databases of system information.  This includes
#  netinfo and printing spools.  Uncomment this line if you do not want radmind
#  to maintain such information.
# d /private/var/db                      0755     0     0

d /private/var/log                      0755     0     0
d /private/var/radmind/client           0755     0     0
d /private/var/root                     0750     0     0
d /private/var/run                      0775     0     1
d /private/var/tmp                      1777     0     0
d /private/var/vm                       0755     0     0
