FAT32-FS-Driver
===============

Operating Systems Assignment 3

- [x] Read the first 512 bytes of the device
- [x] Parse BPB sector, read basic information (sector size, sectors per cluster, FAT
      size, current FAT id, etc.) and verify the filesystem is FAT32
- [ ] Locate and read raw data from the correct FAT table, then make sure you can
      follow cluster allocation chains
- [ ] Implement basic root directory parsing | make sure you can enumerate raw
      entries from the root directory until the nal entry is indicated
- [ ] Implement basic short entry handling | parse short name (remove space-padding),
      attributes, size, first cluster, skip all other entries
- [ ] Add an ability to read the content of a file and a directory given the number of
      their first cluster
- [ ] Now you can start integrating with fuse | make sure you can list root ("/")
      directory and fill stat entries correctly (especially attribute S_IFREG and S_IFDIR). 
      Make sure you can read top-level files
- [ ] Add multi-level directory resolution | traverse directory structure ("/dir1/dir2/file")
- [x] Add support for long names. You will need to keep some state before you finally
      read the short entry holding the information about the file and you should check
      that long name entries are correct.
- [ ] Fill other stat fields (atime/mtime/ctime)
