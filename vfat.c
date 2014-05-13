// vim: noet:ts=8:sts=8
#define FUSE_USE_VERSION 26
#define _GNU_SOURCE

#include <sys/mman.h>
#include <assert.h>
#include <endian.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <fuse.h>
#include <iconv.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "vfat.h"

#define DEBUG_PRINT printf

// A kitchen sink for all important data about filesystem
struct vfat_data {
	const char	*dev;
	int		fs;
	struct fat_boot  fat_boot;
	/* XXX add your code here */
};

struct vfat_data vfat_info;
iconv_t iconv_utf16;

uid_t mount_uid;
gid_t mount_gid;
time_t mount_time;


static void
vfat_init(const char *dev)
{
	uint16_t rootDirSectors;
	uint32_t fatSz,totSec,dataSec,countofClusters;
	iconv_utf16 = iconv_open("utf-8", "utf-16"); // from utf-16 to utf-8
	// These are useful so that we can setup correct permissions in the mounted directories
	mount_uid = getuid();
	mount_gid = getgid();

	// Use mount time as mtime and ctime for the filesystem root entry (e.g. "/")
	mount_time = time(NULL);

	// Open the FAT file
	vfat_info.fs = open(dev, O_RDONLY);

	if (vfat_info.fs < 0) {
		err(1, "open(%s)", dev);
	}

	if(read(vfat_info.fs,&vfat_info.fat_boot,512) != 512) {
		err(1,"read(%s)",dev);
	}

	// Fat Type Determination:
	if(vfat_info.fat_boot.root_max_entries != 0) {
		err(1,"error: should be 0\n");
	}
	rootDirSectors = ((vfat_info.fat_boot.root_max_entries * 32) +
		(vfat_info.fat_boot.bytes_per_sector - 1)) / vfat_info.fat_boot.bytes_per_sector;

	if(vfat_info.fat_boot.sectors_per_fat_small != 0){
		fatSz = vfat_info.fat_boot.sectors_per_fat_small;
	} else{
		fatSz = vfat_info.fat_boot.fat32.sectors_per_fat;
	}

	if(vfat_info.fat_boot.total_sectors_small != 0){
		totSec = vfat_info.fat_boot.total_sectors_small;
	} else {
		totSec = vfat_info.fat_boot.total_sectors;
	}

	dataSec = totSec - (vfat_info.fat_boot.reserved_sectors +
	(vfat_info.fat_boot.fat_count * fatSz) + rootDirSectors);
	countofClusters = dataSec / vfat_info.fat_boot.sectors_per_cluster;

	if(countofClusters < 4085) {
		err(1,"error: Volume is FAT12.\n");
	} else if(countofClusters < 65525) {
		err(1,"error: Volume is FAT16.\n");
	} else {
		printf("Volume is FAT32.\n");
	}

	// Check all other fields
	if(((char*)&vfat_info.fat_boot)[510] != 0x55 &&
		((char*)&vfat_info.fat_boot)[511] != 0xAA) {
		err(1, "Magic number 0xAA55 not present\n");
	}

	if(vfat_info.fat_boot.jmp_boot[0] == 0xEB) {
		if(vfat_info.fat_boot.jmp_boot[2] != 0x90) {
			err(1, "jmp_boot[2] is wrong\n");
		}
	} else if(vfat_info.fat_boot.jmp_boot[0] != 0xE9){
		err(1, "jmp_boot[0] is wrong\n");
	}

	if(vfat_info.fat_boot.bytes_per_sector != 512 &&
		vfat_info.fat_boot.bytes_per_sector != 1024 &&
		vfat_info.fat_boot.bytes_per_sector != 2048 &&
		vfat_info.fat_boot.bytes_per_sector != 5096) {

		err(1, "bytes_per_sector is wrong\n");
	}

	if(vfat_info.fat_boot.sectors_per_cluster != 1 &&
		vfat_info.fat_boot.sectors_per_cluster % 2 != 0) {
		err(1, "sectors_per_cluster is wrong\n");
	}

	if(vfat_info.fat_boot.sectors_per_cluster *
		vfat_info.fat_boot.bytes_per_sector > 32 * 1024) {
		err(1, "bytes_per_sector * sectors_per_cluster is too large\n");
	}

	if(vfat_info.fat_boot.reserved_sectors == 0) {
		err(1, "reserved_sectors is zero\n");
	}

	if(vfat_info.fat_boot.fat_count < 2) {
		err(1, "fat count is less than two\n");
	}

	if(vfat_info.fat_boot.root_max_entries != 0) {
		err(1, "root_max_entries must be zero\n");
	}

	if(vfat_info.fat_boot.total_sectors_small != 0) {
		err(1, "total_sectors_small must be zero\n");
	}

	if(vfat_info.fat_boot.media_info != 0xF0 &&
		vfat_info.fat_boot.media_info < 0xF8) {
		err(1, "wrong media info\n");
		//TODO CHECK same number in FAT[0]
	}

	if(vfat_info.fat_boot.sectors_per_fat_small != 0) {
		err(1, "sectors per fat small must be zero\n");
	}

	if(vfat_info.fat_boot.total_sectors == 0) {
		err(1, "total_sectors must be non-zero\n");
	}

	// Microsoft specs do not say anything to be forced about sectors_per_fat
	// and other fields of fat_boot_fat32 so we don't check them

	printf("Volume seems really FAT32.\n");
	if(lseek(vfat_info.fs, -512, SEEK_CUR) == -1) {
		err(1, "lseek(-512)");
	}
}

unsigned char
chkSum (unsigned char *pFcbName)
	{
		short fcbNameLen;
		unsigned char sum;

		sum = 0;
		for (fcbNameLen=11; fcbNameLen!=0; fcbNameLen--) {
			// NOTE: The operation is an unsigned char rotate right
			sum = ((sum & 1) ? 0x80 : 0) + (sum >> 1) + *pFcbName++;
		}
		return (sum);
	}


static int
test_read(void) {
	unsigned char check_sum = '\0';
	char buffer[260]; // Max size of name: 13 * 0x14 = 260
	int i, j, seq_nb = 0, index_buffer = 0;
	uint32_t fatSz, first_data_sec;
	struct fat32_direntry short_entry;
	struct fat32_direntry_long long_entry;

        fatSz = vfat_info.fat_boot.fat32.sectors_per_fat;

	first_data_sec = vfat_info.fat_boot.reserved_sectors + fatSz * vfat_info.fat_boot.fat_count;

	if(lseek(vfat_info.fs, first_data_sec*vfat_info.fat_boot.bytes_per_sector, SEEK_CUR) == -1) {
		err(1, "lseek(%d)", first_data_sec*vfat_info.fat_boot.bytes_per_sector);
	}

	for(i = 0; i < vfat_info.fat_boot.sectors_per_cluster*vfat_info.fat_boot.bytes_per_sector; i+=32) {
		if(read(vfat_info.fs, &short_entry, 32) != 32){
			err(1, "read(short_dir)");
		}

		// Long name
		if((short_entry.attr & 0x0F) == 0x0F) {
			long_entry = *((struct fat32_direntry_long *) (&short_entry));

			// Last of the sequence of long names
			if((long_entry.seq & 0xF0) == 0x40) {
				// Check 1st byte of name
				if(long_entry.name1[0] == 0xE5) {
					continue;
				} else if(long_entry.name1[0] == 0x00) {
					break;
				} else if(long_entry.name1[0] == 0x05) {
					long_entry.name1[0] = 0xE5;
				}

				// Copy long name into buffer
				for(j = 0; j < 5 && long_entry.name1[j] != 0xFF; j++) {
					buffer[index_buffer] = long_entry.name1[j];
					index_buffer += 1;
				}
				for(j = 0; j < 6 && long_entry.name2[j] != 0xFF; j++) {
					buffer[index_buffer] = long_entry.name2[j];
					index_buffer += 1;
				}
				for(j = 0; j < 3 && long_entry.name3[j] != 0xFF; j++) {
					buffer[index_buffer] = long_entry.name3[j];
					index_buffer += 1;
				}

				seq_nb = long_entry.seq & 0x0F;
				check_sum = long_entry.csum;
			}
			// Middle of sequence of long names
			else if(long_entry.csum == check_sum && seq_nb == (long_entry.seq + 1)) {
				seq_nb -= 1;
				// Check 1st byte of name
				if(long_entry.name1[0] == 0xE5) {
					continue;
				} else if(long_entry.name1[0] == 0x00) {
					break;
				} else if(long_entry.name1[0] == 0x05) {
					long_entry.name1[0] = 0xE5;
				}

				// Add name in the first part of the buffer
				char tmp[260];
				for(j = 0; j < 260; j++) {
					tmp[j] = buffer[j];
				}

				// Copy long name into buffer
				for(j = 0; j < 5; j++) {
					if(j < 5 && long_entry.name1[j] != 0xFF) {
						buffer[j] = long_entry.name1[j];
						index_buffer += 1;
					} else if(j < 11 && long_entry.name2[j - 5] != 0xFF) {
						buffer[j] = long_entry.name2[j - 5];
						index_buffer += 1;
					} else if(j < 14 && long_entry.name3[j - 11] != 0xFF) {
						buffer[j] = long_entry.name3[j - 11];
						index_buffer += 1;
					} else {
						buffer[j] = tmp[j - 14];
					}
				}
			}
			// Error
			else {
				seq_nb = 0;
				check_sum = '\0';
				for(j = 0; j < 260; j++) {
					buffer[j] = '\0';
				}
			}
		}
		// Short name after a sequence of long names
		else if(check_sum != '\0' && seq_nb == 0 && chkSum(&(short_entry.nameext)) == check_sum) {
			// Check 1st byte of name
			if(short_entry.nameext[0] == 0xE5) {
				continue;
			} else if(short_entry.nameext[0] == 0x00) {
				break;
			} else if(short_entry.nameext[0] == 0x05) {
				short_entry.nameext[0] = 0xE5;
			}

			DEBUG_PRINT("Long name of file/dir: ");
			for(j = 0; j < 260 && j != '\0'; j++) {
				DEBUG_PRINT("%c", buffer[j]);
			}
			DEBUG_PRINT("\n");
			// Do stuff...

			// Init buffer, check_sum. No need for seq_nb since it is already 0
			for(j = 0; j < 260; j++) {
				buffer[j] = '\0';
			}
			check_sum = '\0';
		}
		// Short name in the middle of a sequence of long names or placed alone (normally)
		else {
			// Init buffer, check_sum, seq_nb
			for(j = 0; j < 260; j++) {
				buffer[j] = '\0';
			}
			check_sum = '\0';

			// Check 1st byte of name
			if(short_entry.nameext[0] == 0xE5) {
				continue;
			} else if(short_entry.nameext[0] == 0x00) {
				break;
			} else if(short_entry.nameext[0] == 0x05) {
				short_entry.nameext[0] = 0xE5;
			}

			DEBUG_PRINT("Short name of file/dir: ");
			for(j = 0; j < 5 && j != 0xFFFF; j++) {
				DEBUG_PRINT("%c", short_entry.nameext[j]);
			}
			DEBUG_PRINT("\n");
			// Do stuff...
		}
	}

	return 0;
}

static int
vfat_readdir(fuse_fill_dir_t filler, void *fillerdata)
{
	struct stat st; // we can reuse same stat entry over and over again
	void *buf = NULL;
	struct vfat_direntry *e;
	char *name;

	memset(&st, 0, sizeof(st));
	st.st_uid = mount_uid;
	st.st_gid = mount_gid;
	st.st_nlink = 1;


}


// Used by vfat_search_entry()
struct vfat_search_data {
	const char	*name;
	int		found;
	struct stat	*st;
};


// You can use this in vfat_resolve as a filler function for vfat_readdir
static int
vfat_search_entry(void *data, const char *name, const struct stat *st, off_t offs)
{
	struct vfat_search_data *sd = data;

	if (strcmp(sd->name, name) != 0)
		return (0);

	sd->found = 1;
	*sd->st = *st;

	return (1);
}

// Recursively find correct file/directory node given the path
static int
vfat_resolve(const char *path, struct stat *st)
{
	struct vfat_search_data sd;

	/* XXX add your code here */
}

// Get file attributes
static int
vfat_fuse_getattr(const char *path, struct stat *st)
{
	/* XXX: This is example code, replace with your own implementation */
	DEBUG_PRINT("fuse getattr %s\n", path);
	// No such file
	if (strcmp(path, "/")==0) {
		st->st_dev = 0; // Ignored by FUSE
		st->st_ino = 0; // Ignored by FUSE unless overridden
		st->st_mode = S_IRWXU | S_IRWXG | S_IRWXO | S_IFDIR;
		st->st_nlink = 1;
		st->st_uid = mount_uid;
		st->st_gid = mount_gid;
		st->st_rdev = 0;
		st->st_size = 0;
		st->st_blksize = 0; // Ignored by FUSE
		st->st_blocks = 1;
		return 0;
	}
	if (strcmp(path, "/a.txt")==0 || strcmp(path, "/b.txt")==0) {
		st->st_dev = 0; // Ignored by FUSE
		st->st_ino = 0; // Ignored by FUSE unless overridden
		st->st_mode = S_IRWXU | S_IRWXG | S_IRWXO | S_IFREG;
		st->st_nlink = 1;
		st->st_uid = mount_uid;
		st->st_gid = mount_gid;
		st->st_rdev = 0;
		st->st_size = 10;
		st->st_blksize = 0; // Ignored by FUSE
		st->st_blocks = 1;
		return 0;
	}

	return -ENOENT;
}

void seek_cluster(uint32_t cluster_no) {
    if(cluster_no < 2) {
	err(1, "cluster number < 2");
    }

    uint32_t firstDataSector = vfat_info.fat_boot.reserved_sectors +
	(vfat_info.fat_boot.fat_count * vfat_info.fat_boot.fat32.sectors_per_fat);
    uint32_t firstSectorofCluster = ((cluster_no - 2) * vfat_info.fat_boot.sectors_per_cluster) + firstDataSector;
    if(lseek(vfat_info.fs, firstSectorofCluster * vfat_info.fat_boot.bytes_per_sector, SEEK_SET) == -1) {
	err(1, "lseek cluster_no %d\n", cluster_no);
    }
}

static int
vfat_fuse_readdir(const char *path, void *buf,
		  fuse_fill_dir_t filler, off_t offs, struct fuse_file_info *fi)
{
	DEBUG_PRINT("fuse readdir %s\n", path);
	//assert(offs == 0);
	seek_cluster(vfat_info.fat_boot.fat32.root_cluster);
	vfat_readdir(filler, buf);
	filler(buf, "a.txt", NULL, 0);
	filler(buf, "b.txt", NULL, 0);
	return 0;
}

static int
vfat_fuse_read(const char *path, char *buf, size_t size, off_t offs,
	       struct fuse_file_info *fi)
{
	/* XXX: This is example code, replace with your own implementation */
	DEBUG_PRINT("fuse read %s\n", path);
	assert(size > 1);
	buf[0] = 'X';
	buf[1] = 'Y';
	/* XXX add your code here */
	return 2; // number of bytes read from the file
		  // must be size unless EOF reached, negative for an error
}

////////////// No need to modify anything below this point
static int
vfat_opt_args(void *data, const char *arg, int key, struct fuse_args *oargs)
{
	if (key == FUSE_OPT_KEY_NONOPT && !vfat_info.dev) {
		vfat_info.dev = strdup(arg);
		return (0);
	}
	return (1);
}

static struct fuse_operations vfat_available_ops = {
	.getattr = vfat_fuse_getattr,
	.readdir = vfat_fuse_readdir,
	.read = vfat_fuse_read,
};

int
main(int argc, char **argv)
{
	struct fuse_args args = FUSE_ARGS_INIT(argc, argv);

	fuse_opt_parse(&args, NULL, NULL, vfat_opt_args);

	if (!vfat_info.dev)
		errx(1, "missing file system parameter");

	vfat_init(vfat_info.dev);
	test_read();
	return 0;//(fuse_main(args.argc, args.argv, &vfat_available_ops, NULL));
}
