## Project Instruction Updates:

1. Complete the function CPUScheduler() in vcpu_scheduler.c
2. If you are adding extra files, make sure to modify Makefile accordingly.
3. Compile the code using the command `make all`
4. You can run the code by `./vcpu_scheduler <interval>`
5. While submitting, write your algorithm and logic in this Readme.

## Algorithm
* Get the number of active virtual machines using virConnectListAllDomains function.
* For each active VM, get the statistics using the virDomainGetMaxVcpus function.
* When scheduling in the first cycle, schedule each VCPU to the PCPU in order.
* If not scheduling during the first cycle :
  * Calculate the VCPU usage using the previousCPU time and currentCPU time for each VCPU using the formula : (currCPUTime - PrevCPUTime)*100/interval.
  * Calculate the PCPU usage by adding the VCPU usage of the VCPUs pinned to the PCPU.
  * Calculate the PCPU with the highest usage and the lowest usage.
  * If the difference between the highest usage and lowest usage is greater than 5 and if any of the PCPU's usage is greater than 20, then:
    * Get the VPCU with the highest usage.
    * Get the PCPU with the lowest usage.
    * Pin the highest usage VCPU to the lowest usage PCPU.
    
 ## Main Idea
 The main idea is to pin the highest usgae VPCU to the lowest usage VCPU to balance out the usage. For a stable pinning, change the pinning only when the usage difference between any two PCPU is greate than 5 and if any of the PCPU's usage is greater than 20.
