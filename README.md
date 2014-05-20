FAT32-FS-Driver
===============

Operating Systems Assignment 3

- [ ] Todo
- [X] Done

Status of implementation

- [X] Read the first 512 bytes of the device
- [X] Parse BPB sector, read basic information (sector size, sectors per cluster, FAT
      size, current FAT id, etc.) and verify the filesystem is FAT32
- [X] Locate and read raw data from the correct FAT table, then make sure you can
      follow cluster allocation chains
- [X] Implement basic root directory parsing | make sure you can enumerate raw
      entries from the root directory until the final entry is indicated
- [X] Implement basic short entry handling | parse short name (remove space-padding),
      attributes, size, first cluster, skip all other entries
- [X] Add an ability to read the content of a file and a directory given the number of
      their first cluster
- [X] Now you can start integrating with fuse | make sure you can list root ("/")
      directory and fill stat entries correctly (especially attribute S_IFREG and S_IFDIR). 
      Make sure you can read top-level files
- [X] Add multi-level directory resolution | traverse directory structure ("/dir1/dir2/file")
- [X] Add support for long names. You will need to keep some state before you finally
      read the short entry holding the information about the file and you should check
      that long name entries are correct.
- [X] Fill other stat fields (atime/mtime/ctime)

More tasks...

- [X] See if videos or music works
      -> Yes, just make follow these steps: 1. `mount` testfs.fat 2. Insert your media file 3. 
- [X] find what's wrong about uids
      -> Had to declare mount_[gu]id static to avoid modifications to it
- [X] get atime, ctime,... correct
      -> Made the conversion between FAT format for date and linux format for date (time_t)
- [X] see if -1 as end of cluster works
      -> No, must mask MSB and check intervall [0xFFFFFF8,0xFFFFFFF]
- [X] iconv UTF16 for long names
      -> Used iconv to convert the long filenames encoded in UTF-16le to UTF8
- [X] directories should have size
      Size of directory = number of clusters used for its directories entries (like linux `mount`)
- [X] ignore volume ID entries
      -> In order to not print NoFileName file
- [X] remove most magic numbers
      -> like root cluster no is not always 2
- [X] Write the report/readme
