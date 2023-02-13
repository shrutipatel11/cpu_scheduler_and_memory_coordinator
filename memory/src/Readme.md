
## Project Instruction Updates:

1. Complete the function MemoryScheduler() in memory_coordinator.c
2. If you are adding extra files, make sure to modify Makefile accordingly.
3. Compile the code using the command `make all`
4. You can run the code by `./memory_coordinator <interval>`

### Notes:

1. Make sure that the VMs and the host has sufficient memory after any release of memory.
2. Memory should be released gradually. For example, if VM has 300 MB of memory, do not release 200 MB of memory at one-go.
3. Domain should not release memory if it has less than or equal to 100MB of unused memory.
4. Host should not release memory if it has less than or equal to 200MB of unused memory.
5. While submitting, write your algorithm and logic in this Readme.

## Algorithm:
* Set the period of getting statistics using virDomainSetMemoryStatsPeriod function.
* Get memory statistics (available memory, unused memory and actual memory) for each active virtual machine using virDomainMemoryStats function.
* If unused memory percentage for any VM is greater than 60% (wasting memory), then reduce the available memory of that VM by 50%.
* If unused memory percentage for any VM is less than 25% (starving VM), then :
  * Give 200MB extra memory to the VM if it does not exceed the maximum memory allowed for the VM. Reduce the memory if the host memory decreases below 200MB.
  * If it exceeds the maximum memory allowed, then give maximum memory to the VM. Reduce the memory if the host memory decreases below 200MB.
