#ifndef _SYS_UTSNAME_H
#define _SYS_UTSNAME_H

#define _UTSNAME_LENGTH 65

/* Structure describing the system and machine.  */
struct utsname {
    /* Name of the implementation of the operating system.  */
    char sysname[_UTSNAME_LENGTH];
    /* Name of this node on the network.  */
    char nodename[_UTSNAME_LENGTH];
    /* Current release level of this implementation.  */
    char release[_UTSNAME_LENGTH];
    /* Current version level of this release.  */
    char version[_UTSNAME_LENGTH];
    /* Name of the hardware type the system is running on.  */
    char machine[_UTSNAME_LENGTH];
    /* Name of the domain of this node on the network.  */
    char domainname[_UTSNAME_LENGTH];
};

/* Put information about the system in buf.  */
int uname(struct utsname *buf);

#endif