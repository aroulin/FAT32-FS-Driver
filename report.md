John Gaspoz

Andy Roulin

Valentin Rutz


OS assignement 3 report:
========================

1. Status of the implementation:

See readme.md for full details of the implemented steps. Everything should be implemented from parsing names to reading videos.


2. Experiences about implementation and coding:


  * CLANG:

   First of all, we used the 'clang' compiler in order to have more details on the errors we could have made. It came very handy when we used masks:
```c
short_entry.attr & 0x3f == 0x3f
```
was not exactly what we expected due to the priority of operators and thanks to 'clang' we corrected it to:
```c
(short_entry.attr & 0x3f) == 0x3f
```
This is just a simple example of how 'clang' was useful.


  * GDB, valgrind:


   Since we manipulated a couple of pointers, we had several segmentation faults. To find these, we used 'gdb' and 'valgrind'.
'gdb' is the classic C debugger and 'valgrind' showed us where exactly the memory leak was.


  * HEXDUMP:
 

   It was difficult to see if we were on the right position in the filesystem. For example, to read the root cluster, we had some values but we didn't know if they were right.
In that case, we used hexdump on testfs.fat that would display half byte per half byte the file so we could verify the values in fat_boot or in the directory entries.


  * lseek and read:

As advised in the assignement, we used lseek() and read() to read from the disk into the structures all the needed data.


  * Windows documentation:

   The Windows documentation was very thorough so it was really easy to implement the FAT32 verification, the Windows time standard was a painful but not impossible.
One of the difficulties we had was to correctly implement the long names parsing. We shared the task as one of us did the short names and the other the long names. We just had to merge the results.


  * iconv:

   We had a lot of trouble to use iconv. We did not know if we should give a char** that we previously modified or if we had to let C do its magic. 
