# System-Programming---Character-Device-Driver

1. Defined a device structure and embedded a structure cdev in that structure:

struct asp_mycdev {

                    struct cdev dev;
                    char *ramdisk;
                    struct semaphore sem;
                    int devNo;

                    };
                    
                    
2. Supports a variable number of devices that can be set at load time
(default is 3). The device nodes will be
named /dev/mycdev0, /dev/mycdev1, ..., /dev/mycdevN-1, where N is
the number of devices. The device driver creates the device
nodes.


3. Provided an entry function that would be accessed via lseek()
function. That entry function updates the file position pointer
based on the offset requested and sets the file position pointer as requested as long as it does not go out of bounds, i.e., 0 ≤ requestedposition.
In the case of a request that goes beyond end of the buffer, the implementation expands the buffer and fill the new region with zeros.


4. Provided an entry function that would be accessed via ioctl()
function. letting the user application clear the data stored in the
ramdisk. Defined symbol ASP CLEAR BUF for the command to
clear the buffer. The driver function also resets the file position
pointer to 0. 


5. Each device can be opened concurrently and therefore can be
accessed for read, write, lseek, and ioctl concurrently avoiding the race condition.
