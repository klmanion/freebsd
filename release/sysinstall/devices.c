/*
 * The new sysinstall program.
 *
 * This is probably the last program in the `sysinstall' line - the next
 * generation being essentially a complete rewrite.
 *
 * $Id: devices.c,v 1.49.2.16 1998/03/15 16:16:29 jkh Exp $
 *
 * Copyright (c) 1995
 *	Jordan Hubbard.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    verbatim and that no modifications are made prior to this
 *    point in the file.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY JORDAN HUBBARD ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL JORDAN HUBBARD OR HIS PETS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, LIFE OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include "sysinstall.h"
#include <sys/fcntl.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <netinet/in.h>
#include <netinet/in_var.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <unistd.h>

/* how much to bias minor number for a given /dev/<ct#><un#>s<s#> slice */
#define SLICE_DELTA	(0x10000)

static Device *Devices[DEV_MAX];
static int numDevs;

static struct _devname {
    DeviceType type;
    char *name;
    char *description;
    int major, minor, delta, max;
    char dev_type;
} device_names[] = {
    { DEVICE_TYPE_CDROM,	"cd%dc",	"SCSI CDROM drive",	6, 2, 8, 4, 'b'				},
    { DEVICE_TYPE_CDROM,	"mcd%dc",	"Mitsumi (old model) CDROM drive",	7, 2, 8, 4, 'b'		},
    { DEVICE_TYPE_CDROM,	"scd%dc",	"Sony CDROM drive - CDU31/33A type",	16, 2, 8, 4, 'b'	},
    { DEVICE_TYPE_CDROM,	"matcd%dc",	"Matsushita CDROM ('sound blaster' type)", 17, 2, 8, 4, 'b'	},
    { DEVICE_TYPE_CDROM,	"wcd%dc",	"ATAPI IDE CDROM",	19, 2, 8, 4, 'b'			},
    { DEVICE_TYPE_TAPE, 	"rst%d",	"SCSI tape drive",	14, 0, 16, 4, 'c'			},
    { DEVICE_TYPE_TAPE, 	"rft%d",	"Floppy tape drive (QIC-02)",	9, 32, 64, 4, 'c'		},
    { DEVICE_TYPE_TAPE, 	"rwt%d",	"Wangtek tape drive",	10, 0, 1, 4, 'c'			},
    { DEVICE_TYPE_DISK, 	"sd%d",		"SCSI disk device",	4, 65538, 8, 32, 'b'			},
    { DEVICE_TYPE_DISK, 	"rsd%d",	"SCSI disk device",	13, 65538, 8, 32, 'c'			},
    { DEVICE_TYPE_DISK, 	"wd%d",		"IDE/ESDI/MFM/ST506 disk device",	0, 65538, 8, 32, 'b'	},
    { DEVICE_TYPE_DISK, 	"rwd%d",	"IDE/ESDI/MFM/ST506 disk device",	3, 65538, 8, 32, 'c'	},
    { DEVICE_TYPE_DISK, 	"od%d",		"SCSI optical disk device",	20, 65538, 8, 4, 'b'		},
    { DEVICE_TYPE_DISK, 	"rod%d",	"SCSI optical disk device",	70, 65538, 8, 4, 'c'		},
    { DEVICE_TYPE_DISK, 	"wfd%d",	"ATAPI FLOPPY (LS-120) device",	24, 65538, 8, 4, 'b'		},
    { DEVICE_TYPE_DISK, 	"rwfd%d",	"ATAPI FLOPPY (LS-120) device",	87, 65538, 8, 4, 'c'		},
    { DEVICE_TYPE_FLOPPY,	"fd%d",		"floppy drive unit A",	2, 0, 64, 4, 'b'			},
    { DEVICE_TYPE_FLOPPY,	"worm%d",	"SCSI optical disk / CDR",	23, 0, 1, 4, 'b'		},
    { DEVICE_TYPE_NETWORK,	"de",		"DEC DE435 PCI NIC or other DC21040-AA based card"		},
    { DEVICE_TYPE_NETWORK,	"fxp",		"Intel EtherExpress Pro/100B PCI Fast Ethernet card"		},
    { DEVICE_TYPE_NETWORK,	"ed",		"WD/SMC 80xx; Novell NE1000/2000; 3Com 3C503 card"		},
    { DEVICE_TYPE_NETWORK,	"ep",		"3Com 3C509 ethernet card"					},
    { DEVICE_TYPE_NETWORK,	"el",		"3Com 3C501 ethernet card"					},
    { DEVICE_TYPE_NETWORK,	"ex",		"Intel EtherExpress Pro/10 ethernet card"			},
    { DEVICE_TYPE_NETWORK,	"fe",		"Fujitsu MB86960A/MB86965A ethernet card"			},
    { DEVICE_TYPE_NETWORK,	"ie",		"AT&T StarLAN 10 and EN100; 3Com 3C507; NI5210"			},
    { DEVICE_TYPE_NETWORK,	"ix",		"Intel Etherexpress ethernet card"				},
    { DEVICE_TYPE_NETWORK,	"le",		"DEC EtherWorks 2 or 3 ethernet card"				},
    { DEVICE_TYPE_NETWORK,	"lnc",		"Lance/PCnet (Isolan/Novell NE2100/NE32-VL) ethernet"		},
    { DEVICE_TYPE_NETWORK,	"tx",		"SMC 9432TX ethernet card"					},
    { DEVICE_TYPE_NETWORK,	"vx",		"3COM 3c590 / 3c595 / 3c9xx ethernet card"			},
    { DEVICE_TYPE_NETWORK,	"ze",		"IBM/National Semiconductor PCMCIA ethernet card"		},
    { DEVICE_TYPE_NETWORK,	"zp",		"3Com Etherlink III PCMCIA ethernet card"			},
    { DEVICE_TYPE_NETWORK,	"cuaa%d",	"%s on device %s (COM%d)",	28, 128, 1, 16, 'c'		},
    { DEVICE_TYPE_NETWORK,	"lp",		"Parallel Port IP (PLIP) peer connection"			},
    { DEVICE_TYPE_NETWORK,	"lo",		"Loop-back (local) network interface"				},
    { 0 },
};

Device *
new_device(char *name)
{
    Device *dev;

    dev = safe_malloc(sizeof(Device));
    bzero(dev, sizeof(Device));
    if (name)
	SAFE_STRCPY(dev->name, name);
    return dev;
}

/* Stubs for unimplemented strategy routines */
Boolean
dummyInit(Device *dev)
{
    return TRUE;
}

FILE *
dummyGet(Device *dev, char *dist, Boolean probe)
{
    return NULL;
}

void
dummyShutdown(Device *dev)
{
    return;
}

static int
deviceTry(struct _devname dev, char *try, int i)
{
    int fd;
    char unit[80];
    mode_t m;
    dev_t d;
    int fail;

    snprintf(unit, sizeof unit, dev.name, i);
    snprintf(try, FILENAME_MAX, "/dev/%s", unit);
    fd = open(try, O_RDONLY);
    if (fd >= 0)
	return fd;
    m = 0640;
    if (dev.dev_type == 'c')
	m |= S_IFCHR;
    else
	m |= S_IFBLK;
    d = makedev(dev.major, dev.minor + (i * dev.delta));
    fail = mknod(try, m, d);
    fd = open(try, O_RDONLY);
    if (fd >= 0)
	return fd;
    else if (!fail)
	(void)unlink(try);
    /* Don't try a "make-under" here since we're using a fixit floppy in this case */
    snprintf(try, FILENAME_MAX, "/mnt/dev/%s", unit);
    fd = open(try, O_RDONLY);
    return fd;
}

/* Register a new device in the devices array */
Device *
deviceRegister(char *name, char *desc, char *devname, DeviceType type, Boolean enabled,
	       Boolean (*init)(Device *), FILE * (*get)(Device *, char *, Boolean),
	       void (*shutdown)(Device *), void *private)
{
    Device *newdev = NULL;

    if (numDevs == DEV_MAX)
	msgFatal("Too many devices found!");
    else {
	newdev = new_device(name);
	newdev->description = desc;
	newdev->devname = devname;
	newdev->type = type;
	newdev->enabled = enabled;
	newdev->init = init ? init : dummyInit;
	newdev->get = get ? get : dummyGet;
	newdev->shutdown = shutdown ? shutdown : dummyShutdown;
	newdev->private = private;
	Devices[numDevs] = newdev;
	Devices[++numDevs] = NULL;
    }
    return newdev;
}

/* Get all device information for devices we have attached */
void
deviceGetAll(void)
{
    int i, j, fd, s;
    struct ifconf ifc;
    struct ifreq *ifptr, *end;
    int ifflags;
    char buffer[INTERFACE_MAX * sizeof(struct ifreq)];
    char **names;

    /* First go for the network interfaces.  Stolen shamelessly from ifconfig! */
    ifc.ifc_len = sizeof(buffer);
    ifc.ifc_buf = buffer;

    s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0) {
	msgConfirm("ifconfig: socket");
	goto skipif;	/* Jump over network iface probing */
    }
    if (ioctl(s, SIOCGIFCONF, (char *) &ifc) < 0) {
	msgConfirm("ifconfig (SIOCGIFCONF)");
	goto skipif;	/* Jump over network iface probing */
    }
    ifflags = ifc.ifc_req->ifr_flags;
    end = (struct ifreq *) (ifc.ifc_buf + ifc.ifc_len);
    for (ifptr = ifc.ifc_req; ifptr < end; ifptr++) {
	char *descr;

	/* If it's not a link entry, forget it */
	if (ifptr->ifr_ifru.ifru_addr.sa_family != AF_LINK)
	    continue;

	/* Eliminate network devices that don't make sense */
	if (!strncmp(ifptr->ifr_name, "lo", 2))
	    continue;

	/* If we have a slip device, don't register it */
	if (!strncmp(ifptr->ifr_name, "sl", 2)) {
	    continue;
	}
	/* And the same for ppp */
	if (!strncmp(ifptr->ifr_name, "tun", 3) || !strncmp(ifptr->ifr_name, "ppp", 3)) {
	    continue;
	}
	/* Try and find its description */
	for (i = 0, descr = NULL; device_names[i].name; i++) {
	    int len = strlen(device_names[i].name);

	    if (!ifptr->ifr_name || !ifptr->ifr_name[0])
		continue;
	    else if (!strncmp(ifptr->ifr_name, device_names[i].name, len)) {
		descr = device_names[i].description;
		break;
	    }
	}
	if (!descr)
	    descr = "<unknown network interface type>";

	deviceRegister(ifptr->ifr_name, descr, strdup(ifptr->ifr_name), DEVICE_TYPE_NETWORK, TRUE,
		       mediaInitNetwork, NULL, mediaShutdownNetwork, NULL);
	msgDebug("Found a network device named %s\n", ifptr->ifr_name);
	close(s);
	if ((s = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
	    msgConfirm("ifconfig: socket");
	    continue;
	}
	if (ifptr->ifr_addr.sa_len)	/* I'm not sure why this is here - it's inherited */
	    ifptr = (struct ifreq *)((caddr_t)ifptr + ifptr->ifr_addr.sa_len - sizeof(struct sockaddr));
    }

skipif:
    /* Next, try to find all the types of devices one might need
     * during the second stage of the installation.
     */
    for (i = 0; device_names[i].name; i++) {
	for (j = 0; j < device_names[i].max; j++) {
	    char try[FILENAME_MAX];

	    switch(device_names[i].type) {
	    case DEVICE_TYPE_CDROM:
		fd = deviceTry(device_names[i], try, j);
		if (fd >= 0 || errno == EBUSY) {	/* EBUSY if already mounted */
		    char n[BUFSIZ];

		    if (fd >= 0) close(fd);
		    snprintf(n, sizeof n, device_names[i].name, j);
		    deviceRegister(strdup(n), device_names[i].description, strdup(try),
					 DEVICE_TYPE_CDROM, TRUE, mediaInitCDROM, mediaGetCDROM,
					 mediaShutdownCDROM, NULL);
		    msgDebug("Found a CDROM device for %s\n", try);
		}
		break;

	    case DEVICE_TYPE_TAPE:
		fd = deviceTry(device_names[i], try, j);
		if (fd >= 0) {
		    char n[BUFSIZ];

		    close(fd);
		    snprintf(n, sizeof n, device_names[i].name, j);
		    deviceRegister(strdup(n), device_names[i].description, strdup(try),
				   DEVICE_TYPE_TAPE, TRUE, mediaInitTape, mediaGetTape, mediaShutdownTape, NULL);
		    msgDebug("Found a TAPE device for %s\n", try);
		}
		break;

	    case DEVICE_TYPE_DISK:
		fd = deviceTry(device_names[i], try, j);
		if (fd >= 0 && RunningAsInit) {
		    dev_t d;
		    int s, fail;
		    char slice[80];

		    close(fd);
		    /* Make associated slice entries */
		    for (s = 1; s < 33; s++) {
			snprintf(slice, sizeof slice, "/dev/%ss%d", device_names[i].name, s);
			d = makedev(device_names[i].major, device_names[i].minor +
				    (j * device_names[i].delta) + (s * SLICE_DELTA));
			fail = mknod(slice, 0640 | S_IFBLK, d);
			fd = open(slice, O_RDONLY);
			if (fd >= 0)
			    close(fd);
			else if (!fail)
			    (void)unlink(slice);
		    }
		}
		break;

	    case DEVICE_TYPE_FLOPPY:
		fd = deviceTry(device_names[i], try, j);
		if (fd >= 0) {
		    char n[BUFSIZ];

		    close(fd);
		    snprintf(n, sizeof n, device_names[i].name, j);
		    deviceRegister(strdup(n), device_names[i].description, strdup(try),
				   DEVICE_TYPE_FLOPPY, TRUE, mediaInitFloppy, mediaGetFloppy,
				   mediaShutdownFloppy, NULL);
		    msgDebug("Found a floppy device for %s\n", try);
		}
		break;

	    case DEVICE_TYPE_NETWORK:
		fd = deviceTry(device_names[i], try, j);
		/* The only network devices that you can open this way are serial ones */
		if (fd >= 0) {
		    char *newdesc, *cp;

		    close(fd);
		    cp = device_names[i].description;
		    /* Serial devices get a slip and ppp device each, if supported */
		    newdesc = safe_malloc(strlen(cp) + 40);
		    sprintf(newdesc, cp, "SLIP interface", try, j + 1);
		    deviceRegister("sl0", newdesc, strdup(try), DEVICE_TYPE_NETWORK, TRUE, mediaInitNetwork,
				   NULL, mediaShutdownNetwork, NULL);
		    msgDebug("Add mapping for %s to sl0\n", try);
		    newdesc = safe_malloc(strlen(cp) + 50);
		    sprintf(newdesc, cp, "PPP interface", try, j + 1);
		    deviceRegister("ppp0", newdesc, strdup(try), DEVICE_TYPE_NETWORK, TRUE, mediaInitNetwork,
				   NULL, mediaShutdownNetwork, NULL);
		    msgDebug("Add mapping for %s to ppp0\n", try);
		}
		break;

	    default:
		break;
	    }
	}
    }

    /* Finally, go get the disks and look for DOS partitions to register */
    if ((names = Disk_Names()) != NULL) {
	int i;

	for (i = 0; names[i]; i++) {
	    Chunk *c1;
	    Disk *d;

	    d = Open_Disk(names[i]);
	    if (!d)
		msgFatal("Unable to open disk %s", names[i]);

	    deviceRegister(names[i], names[i], d->name, DEVICE_TYPE_DISK, FALSE,
			   dummyInit, dummyGet, dummyShutdown, d);
	    msgDebug("Found a disk device named %s\n", names[i]);

	    /* Look for existing DOS partitions to register as "DOS media devices" */
	    for (c1 = d->chunks->part; c1; c1 = c1->next) {
		if (c1->type == fat || c1->type == extended) {
		    Device *dev;
		    char devname[80];

		    /* Got one! */
		    snprintf(devname, sizeof devname, "/dev/%s", c1->name);
		    dev = deviceRegister(c1->name, c1->name, strdup(devname), DEVICE_TYPE_DOS, TRUE,
					 mediaInitDOS, mediaGetDOS, mediaShutdownDOS, NULL);
		    dev->private = c1;
		    msgDebug("Found a DOS partition %s on drive %s\n", c1->name, d->name);
		}
	    }
	}
	free(names);
    }
}

/*
 * Find all devices that match the criteria, allowing "wildcarding" as well
 * by allowing NULL or ANY values to match all.  The array returned is static
 * and may be used until the next invocation of deviceFind().
 */
Device **
deviceFind(char *name, DeviceType class)
{
    static Device *found[DEV_MAX];
    int i, j;

    j = 0;
    for (i = 0; i < numDevs; i++) {
	if ((!name || !strcmp(Devices[i]->name, name))
	    && (class == DEVICE_TYPE_ANY || class == Devices[i]->type))
	    found[j++] = Devices[i];
    }
    found[j] = NULL;
    return j ? found : NULL;
}

Device **
deviceFindDescr(char *name, char *desc, DeviceType class)
{
    static Device *found[DEV_MAX];
    int i, j;

    j = 0;
    for (i = 0; i < numDevs; i++) {
	if ((!name || !strcmp(Devices[i]->name, name)) &&
	    (!desc || !strcmp(Devices[i]->description, desc)) &&
	    (class == DEVICE_TYPE_ANY || class == Devices[i]->type))
	    found[j++] = Devices[i];
    }
    found[j] = NULL;
    return j ? found : NULL;
}

int
deviceCount(Device **devs)
{
    int i;

    if (!devs)
	return 0;
    for (i = 0; devs[i]; i++);
    return i;
}

/*
 * Create a menu listing all the devices of a certain type in the system.
 * The passed-in menu is expected to be a "prototype" from which the new
 * menu is cloned.
 */
DMenu *
deviceCreateMenu(DMenu *menu, DeviceType type, int (*hook)(dialogMenuItem *d), int (*check)(dialogMenuItem *d))
{
    Device **devs;
    int numdevs;
    DMenu *tmp = NULL;
    int i, j;

    devs = deviceFind(NULL, type);
    numdevs = deviceCount(devs);
    if (!numdevs)
	return NULL;
    tmp = (DMenu *)safe_malloc(sizeof(DMenu) + (sizeof(dialogMenuItem) * (numdevs + 1)));
    bcopy(menu, tmp, sizeof(DMenu));
    for (i = 0; devs[i]; i++) {
	tmp->items[i].prompt = devs[i]->name;
	for (j = 0; j < numDevs; j++) {
	    if (devs[i] == Devices[j]) {
		tmp->items[i].title = Devices[j]->description;
		break;
	    }
	}
	if (j == numDevs)
	    tmp->items[i].title = "<unknown device type>";
	tmp->items[i].fire = hook;
	tmp->items[i].checked = check;
    }
    tmp->items[i].title = NULL;
    return tmp;
}
