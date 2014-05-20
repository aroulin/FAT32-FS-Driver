John Gaspoz

Andy Roulin

Valentin Rutz


OS assignement 3 report:
========================

### 1. Status of the implementation:

See readme.md for full details of the implemented steps. Everything should be implemented from parsing names to reading videos.


### 2. Experiences about implementation and coding:


####   clang:

First of all, we used the 'clang' compiler in order to have more details on the errors we could have made.

####   gdb and valgrind:

Since we manipulated a couple of pointers, we had several segmentation faults. To find these, we used 'gdb' and 'valgrind'.
'gdb' is the classic C debugger and 'valgrind' showed us where exactly the memory leak was.


#### hexdump:

It was difficult to see if we were on the right position in the filesystem. For example, to read the root cluster, we had some values but we didn't know if they were right.
In that case, we used hexdump on testfs.fat that would display half byte per half byte the file so we could verify the values in fat_boot or in the directory entries.


#### lseek() and read():

As advised in the assignement, we used lseek() and read() to read from the disk into the structures all the needed data.


#### Windows documentation:

The Windows documentation was very thorough so it was really easy to implement the FAT32 verification, the Windows time standard was a painful but not impossible.
One of the difficulties we had was to correctly implement the long names parsing. We shared the task as one of us did the short names and the other the long names. We just had to merge the results.


#### iconv:

We had a lot of trouble to use iconv. We did not know if we should give a char** that we previously modified or if we had to let C do its magic.
There was no real API for iconv and that made the task more difficult.


#### Testing:

In order to test whether our driver worked or not, we used the given commands in the project description.
But usually, we just launched 'ls' in the directory to see if it worked compared to a normal driver.
Moreover, we read a .mp4 file and a .mp3 file on a usb key to see if everything was working.

#### Implementation:


