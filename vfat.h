// vim: noet:ts=8:sts=8
#ifndef VFAT_H
#define VFAT_H

#include <sys/types.h>

struct fat_boot_fat32 {
	/*36*/	uint32_t	sectors_per_fat;
	/*40*/	uint16_t	fat_flags;
	/*42*/	uint16_t	version;
	/*44*/	uint32_t	root_cluster;
	/*48*/	uint16_t	fsinfo_sector;
	/*50*/	uint16_t	backup_sector;
	/*52*/	uint8_t		reserved2[12];
	/*64*/	uint8_t		drive_number;
	/*65*/	uint8_t		reserved3;
	/*66*/	uint8_t		ext_sig;
	/*67*/	uint32_t	serial;
	/*71*/	char		label[11];
	/*82*/	char		type[8];
} __attribute__ ((__packed__));

// Boot sector
struct fat_boot {
	/* 0*/	uint8_t		jmp_boot[3];
	/* 3*/	char		oemname[8];
	/*11*/	uint16_t	bytes_per_sector;
	/*13*/	uint8_t		sectors_per_cluster;
	/*14*/	uint16_t	reserved_sectors;
	/*16*/	uint8_t		fat_count;
	/*17*/	uint16_t	root_max_entries;
	/*19*/	uint16_t	total_sectors_small;
	/*21*/	uint8_t		media_info;
	/*22*/	uint16_t	sectors_per_fat_small;
	/*24*/	uint16_t	sectors_per_track;
	/*26*/	uint16_t	head_count;
	/*28*/	uint32_t	fs_offset;
	/*32*/	uint32_t	total_sectors;
	/*36*/	struct fat_boot_fat32  fat32;
} __attribute__ ((__packed__));


struct fat32_direntry {
	/* 0*/	union {
			struct {
				char		name[8];
				char		ext[3];
			};
			char			nameext[11];
		};
	/*11*/	uint8_t		attr;
	/*12*/	uint8_t		res;
	/*13*/	uint8_t		ctime_ms;
	/*14*/	uint16_t	ctime_time;
	/*16*/	uint16_t	ctime_date;
	/*18*/	uint16_t	atime_date;
	/*20*/	uint16_t	cluster_hi;
	/*22*/	uint16_t	mtime_time;
	/*24*/	uint16_t	mtime_date;
	/*26*/	uint16_t	cluster_lo;
	/*28*/	uint32_t	size;
} __attribute__ ((__packed__));

#define VFAT_ATTR_DIR	0x10
#define VFAT_ATTR_LFN	0xf
#define VFAT_ATTR_INVAL	(0x80|0x40|0x08)

struct fat32_direntry_long {
	/* 0*/	uint8_t		seq;
	/* 1*/	uint16_t	name1[5];
	/*11*/	uint8_t		attr;
	/*12*/	uint8_t		type;
	/*13*/	uint8_t		csum;
	/*14*/	uint16_t	name2[6];
	/*26*/	uint16_t	reserved2;
	/*28*/	uint16_t	name3[2];
} __attribute__ ((__packed__));

#define VFAT_LFN_SEQ_START	0x40
#define VFAT_LFN_SEQ_DELETED	0x80
#define VFAT_LFN_SEQ_MASK	0x3f

#endif
