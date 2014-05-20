John Gaspoz, Andy Roulin, Valentin Rutz

OS assignement 3 report:
========================

### 1. Status of the implementation:

See readme.pdf for full details of the implemented steps. Everything should be implemented from parsing names to reading videos from usb key.

### 2. Experiences about implementation and coding:

####   clang:

We used the 'clang' compiler in order to have more details on the errors we could have made. The clang compiler, despite being less optimized than gcc, gives clearer error explanations and better warning suggestions.

####   gdb and valgrind:

Since we manipulated a couple of pointers, we had several segmentation faults and problems. To solve these, we used 'gdb' and 'valgrind'. `gdb` is the classic C debugger and `valgrind` showed us where exactly the memory leak was.

#### hexdump:

It was difficult to see if we were on the right position in the filesystem. For example, to read the root cluster, we had some values but we didn't know if they were right.
In that case, we used `hexdump -C testfs.fat` which would display byte per byte the file so we could verify the values in fat_boot or in the directory entries.

#### lseek() and read():

As advised in the assignement, we used lseek() and read() to read from the disk into the structures all the needed data.

#### Windows documentation:

The Windows documentation was very thorough so it was really easy to implement the FAT32 verification, the Windows time standard was a painful but not impossible.
One of the difficulties we had was to correctly implement the long names parsing. We shared the task as one of us did the short names and the other the long names. We just had to merge the results.

#### iconv:

We used iconv to convert UTF-16LE long names to UTF-8.
There was no real API for iconv and that made the task more difficult.

### 3. Testing:

In order to test whether our driver worked or not, we used the given commands in the project description.
But usually, we just ran `diff -r dest dest2` in the directory to see if it worked compared to a normal driver.
Moreover, we read a .mp4 video file and a .mp3 file on a usb key to see if everything was working.

### 4. Implementation:

We followed the steps given in the statement. We always preferred iterative algorithms compared to recursive ones (like finding a file given a path) to avoid the possibility of stack overflows.

We used the `vfat_search_entry` function as a filler function for `vfat_readdir` in order to locate a file and get it's attributes (size, cluster number) in a `stat` structure.

We also thoroughly checked that the file system was really a FAT32 file system (See `vfat_init`).

We choosed to give sizes to directories as linux does. The size of the directory is like the size of file, the number of clusters its directories entries take.

We also implemented the atime,ctime,etc... fields and we had to convert the date format of the FAT32 to the one (time_t) in linux.

In order to really have zero memory leaks in valgrind, we also implemented a CTRL-C signal handler in order to `iconv_close` the iconv converter.
