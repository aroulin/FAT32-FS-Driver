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
#include <stdbool.h>

#include "vfat.h"

#define DEBUG_PRINT printf
#define END_OF_DIRECTORY 0
#define DIRECTORY_NOT_FINISHED 1

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

	if(read(vfat_info.fs,&vfat_info.fat_boot, 512) != 512) {
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
		((char*)&vfat_info.fat_boot)[511] != (char) 0xAA) {
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
	if(lseek(vfat_info.fs, 0, SEEK_SET) == -1) {
		err(1, "lseek(0)");
	}
}

unsigned char
chkSum (unsigned char *pFcbName) {
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
read_cluster(uint32_t cluster_no, fuse_fill_dir_t filler, void *fillerdata) {
	uint8_t check_sum = '\0';
	char buffer[260]; // Max size of name: 13 * 0x14 = 260
	int i, j, seq_nb = 0, index_buffer = 0;
	struct fat32_direntry short_entry;
	struct fat32_direntry_long long_entry;
	memset(buffer, 0, 260);

	seek_cluster(cluster_no);

	for(i = 0; i < vfat_info.fat_boot.sectors_per_cluster*vfat_info.fat_boot.bytes_per_sector; i+=32) {
		if(read(vfat_info.fs, &short_entry, 32) != 32){
			err(1, "read(short_dir)");
		}
		
		if(((uint8_t) short_entry.nameext[0]) == 0xE5){
			continue;
		} else if(short_entry.nameext[0] == 0x00) {
			return END_OF_DIRECTORY;
		} else if(short_entry.nameext[0] == 0x05) {
			short_entry.nameext[0] = (char) 0xE5;
		}

		if((short_entry.attr & ATTR_LONG_NAME) == ATTR_LONG_NAME) {
			long_entry = *((struct fat32_direntry_long *) &short_entry);
			if((long_entry.seq & 0x40) == 0x40) {
				seq_nb = (long_entry.seq & 0x0f) - 1;
				check_sum = long_entry.csum;

				for(j = 0; j < 13; j++) {
					if(j < 5 && long_entry.name1[j] != 0xFFFF) {
						buffer[j] = long_entry.name1[j];
					} else if(j < 11 && long_entry.name2[j - 5] != 0xFFFF) {
						buffer[j] = long_entry.name2[j - 5];
					} else if(j < 13 && long_entry.name3[j - 11] != 0xFFFF) {
						buffer[j] = long_entry.name3[j - 11];
					}
				}
			} else if (check_sum == long_entry.csum  && long_entry.seq == seq_nb) {
				seq_nb -= 1;

				char tmp[260];
				memset(tmp, 0, 260);

				for(j = 0; j < 260; j++) {
					tmp[j] = buffer[j];
				}

				memset(buffer, 0, 260);

				for(j = 0; j < 260; j++) {
					if(j < 5 && long_entry.name1[j] != 0xFFFF) {
						buffer[j] = long_entry.name1[j];
					} else if(j < 11 && long_entry.name2[j - 5] != 0xFFFF) {
						buffer[j] = long_entry.name2[j - 5];
					} else if(j < 13 && long_entry.name3[j - 11] != 0xFFFF) {
						buffer[j] = long_entry.name3[j - 11];
					} else if(tmp[j - 13] != (char) 0xFF){
						buffer[j] = tmp[j - 13];
					}
				}
			} else {
				seq_nb = 0;
				check_sum = '\0';
				memset(buffer, 0, 260);
				err(1, "error: Bad sequence number or checksum\n");
			}
		} else if(check_sum == chkSum(&(short_entry.nameext)) && seq_nb == 0) {
			char *filename = buffer;
			setStat(short_entry,filename,filler,fillerdata, cluster_no);
			check_sum = '\0';
			memset(buffer, 0, 260);
		} else {
			char *filename = buffer;
			getfilename(short_entry.nameext, filename);
			setStat(short_entry,filename,filler,fillerdata, cluster_no);
		}
	}

	return DIRECTORY_NOT_FINISHED;
}

void 
setStat(struct fat32_direntry dir_entry, char* buffer, fuse_fill_dir_t filler, void *fillerdata, uint32_t cluster_no){
	struct stat* stat_str = malloc(sizeof(struct stat));
			stat_str->st_dev = 0; // Ignored by FUSE
			stat_str->st_ino = cluster_no; // Ignored by FUSE unless overridden
			if(dir_entry.attr & ATTR_READ_ONLY){
				stat_str->st_mode = S_IRUSR | S_IRGRP | S_IROTH;
			}
			else{
				stat_str->st_mode = S_IRWXU | S_IRWXG | S_IRWXO;
			}
			if(dir_entry.attr & ATTR_DIRECTORY) {
				stat_str->st_mode |= S_IFDIR;
			}
			else {
				stat_str->st_mode |= S_IFREG;
			}
			stat_str->st_nlink = 1;
			stat_str->st_uid = mount_uid;
			stat_str->st_gid = mount_gid;
			stat_str->st_rdev = 0;
			stat_str->st_size = dir_entry.size;
			stat_str->st_blksize = 0; // Ignored by FUSE
			stat_str->st_blocks = 1;
			stat_str->st_atime = dir_entry.atime_date;
			stat_str->st_mtime = dir_entry.mtime_time;
			stat_str->st_ctime = 0;
			filler(fillerdata, buffer, stat_str, 0);
}

char*
getfilename(char* nameext, char* filename) {
	if(nameext[0] == 0x20) {
	    err(1, "filename[0] is 0x20");
	}

	uint32_t fileNameCnt = 0;
	bool before_extension = true;
	bool in_spaces = false;
	bool in_extension = false;

	for(int i = 0; i < 11; i++) {
		if(nameext[i] < 0x20 ||
			nameext[i] == 0x22 ||
			nameext[i] == 0x2A ||
			nameext[i] == 0x2B ||
			nameext[i] == 0x2C ||
			nameext[i] == 0x2E ||
			nameext[i] == 0x2F ||
			nameext[i] == 0x3A ||
			nameext[i] == 0x3B ||
			nameext[i] == 0x3C ||
			nameext[i] == 0x3D ||
			nameext[i] == 0x3E ||
			nameext[i] == 0x3F ||
			nameext[i] == 0x5B ||
			nameext[i] == 0x5C ||
			nameext[i] == 0x5D ||
			nameext[i] == 0x7C) {

			err(1, "invalid character in filename %x at %d\n", nameext[i] & 0xFF, i);
		}

		if(before_extension) {
		    if(nameext[i] == 0x20) {
			before_extension = false;
			in_spaces = true;
			filename[fileNameCnt++] = '.';
		    } else if(i == 8) {
			before_extension = false;
			in_spaces = true;
			filename[fileNameCnt++] = '.';
			filename[fileNameCnt++] = nameext[i];
			in_extension = true;
		    } else {
			filename[fileNameCnt++] = nameext[i];
		    }
		} else if(in_spaces) {
			if(nameext[i] != 0x20) {
			    in_spaces = false;
			    in_extension = true;
			    filename[fileNameCnt++] = nameext[i];
			}
		} else if(in_extension) {
			if(nameext[i] == 0x20) {
			    break;
			} else {
			    filename[fileNameCnt++] = nameext[i];
			}
		}
	}

	if(filename[fileNameCnt - 1] == '.') {
	    filename--;
	}
	filename[fileNameCnt] = '\0';
	return filename;
}

static int
vfat_readdir(uint32_t cluster_no, fuse_fill_dir_t filler, void *fillerdata)
{
	struct stat st; // we can reuse same stat entry over and over again
	void *buf = NULL;
	struct vfat_direntry *e;
	char *name;
	uint32_t next_cluster_no;
	bool eof = false;
	int end_of_read;
	const uint32_t maxEntryCnt = vfat_info.fat_boot.bytes_per_sector * vfat_info.fat_boot.sectors_per_cluster;

	memset(&st, 0, sizeof(st));
	st.st_uid = mount_uid;
	st.st_gid = mount_gid;
	st.st_nlink = 1;

	while(!eof) {
		end_of_read = read_cluster(cluster_no, filler, fillerdata);
		
		if(end_of_read == END_OF_DIRECTORY) {
			eof = true;
		} else {
			next_cluster_no = next_cluster(cluster_no);
			if(next_cluster_no == 0xFFFFFFFF) {
				eof = true;
			} else {
				vfat_readdir(next_cluster_no, filler, fillerdata);
			}
		}
	}
}

uint32_t
next_cluster(uint32_t cluster_no) {
	uint32_t next_cluster, next_cluster_check;
	uint32_t first_fat = vfat_info.fat_boot.reserved_sectors * vfat_info.fat_boot.bytes_per_sector;

	if(lseek(vfat_info.fs, first_fat + cluster_no * sizeof(uint32_t), SEEK_SET) == -1) {
		err(1, "lseek(%d)", first_fat + cluster_no * sizeof(uint32_t));
	}
	
	if(read(vfat_info.fs, &next_cluster, sizeof(uint32_t)) != sizeof(uint32_t)) {
		err(1, "read(%d)",sizeof(uint32_t));
	}
	
	if(lseek(vfat_info.fs, first_fat + vfat_info.fat_boot.fat32.sectors_per_fat * vfat_info.fat_boot.bytes_per_sector , SEEK_SET) == -1) {
		err(1, "lseek(%d)", first_fat);
	}
	
	if(read(vfat_info.fs, &next_cluster_check, sizeof(uint32_t)) != sizeof(uint32_t)) {
		err(1, "read(%d)", sizeof(uint32_t));
	}
	
	if(next_cluster_check == next_cluster) {
		return next_cluster;
	}
	
	err(1, "FAT is corrupted !");
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
	int i;
	char* final_name;
	sd.name = path;
	sd.st = st;
	
	for(i = strlen(path); path[i] != '/'; i--);
	final_name = path + i + 1;
	i = 0;
	
	DEBUG_PRINT("Searching for path %s\n", path);
	vfat_readdir(2, vfat_search_entry, &sd);
	
	if(sd.found == 1) {
		while(strcmp(sd.name, final_name) != 0) {
			for(; path[i] != '/'; i++);
			sd.name = path + i + 1;
			vfat_readdir(((uint32_t) (sd.st)->st_ino), vfat_search_entry, &sd);
			if(sd.found != 1) {
				return -ENOENT;
			}
		}
		return 0;
	} else {
		return -ENOENT;
	}
}

// Get file attributes
static int
vfat_fuse_getattr(const char *path, struct stat *st)
{
	/* XXX: This is example code, replace with your own implementation */
	DEBUG_PRINT("fuse getattr %s\n", path);
	// No such file
	if (strcmp(path, "/") == 0) {
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
	if(vfat_resolve(path + 1, st) != 0) {
		return -ENOENT;
	} else {
		return 0;
	}
}


static int
vfat_fuse_readdir(const char *path, void *buf,
		  fuse_fill_dir_t filler, off_t offs, struct fuse_file_info *fi)
{
	DEBUG_PRINT("fuse readdir %s\n", path);
	//assert(offs == 0);
	vfat_readdir(vfat_info.fat_boot.fat32.root_cluster, filler, buf);
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
	return (fuse_main(args.argc, args.argv, &vfat_available_ops, NULL));
}
