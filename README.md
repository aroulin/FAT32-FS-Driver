FAT32-FS-Driver
===============

Operating Systems Assignment 3

- [x] Read the first 512 bytes of the device
- [x] Parse BPB sector, read basic information (sector size, sectors per cluster, FAT
      size, current FAT id, etc.) and verify the filesystem is FAT32
- [X] Locate and read raw data from the correct FAT table, then make sure you can
      follow cluster allocation chains
- [X] Implement basic root directory parsing | make sure you can enumerate raw
      entries from the root directory until the nal entry is indicated
- [X] Implement basic short entry handling | parse short name (remove space-padding),
      attributes, size, first cluster, skip all other entries
- [X] Add an ability to read the content of a file and a directory given the number of
      their first cluster
- [X] Now you can start integrating with fuse | make sure you can list root ("/")
      directory and fill stat entries correctly (especially attribute S_IFREG and S_IFDIR). 
      Make sure you can read top-level files
- [X] Add multi-level directory resolution | traverse directory structure ("/dir1/dir2/file")
- [x] Add support for long names. You will need to keep some state before you finally
      read the short entry holding the information about the file and you should check
      that long name entries are correct.
- [X] Fill other stat fields (atime/mtime/ctime)

More tasks...

- [X] Handle better subdirectories
- [X] find what's wrong about uids
- [X] get atime, ctime ... correct
- [x] see if -1 as end of cluster works
- [X] iconv UTF16
- [ ] look at end signal (0xfffffffd)
- [x] ignore volume ID entry (not long entries)
- [ ] Write the report/readme
- [ ] Change compiler to gcc
- [x] remove magic numbers
