#include<stdio.h>
#include<stdlib.h>
#include<libvirt/libvirt.h>
#include<math.h>
#include<string.h>
#include<unistd.h>
#include<limits.h>
#include<signal.h>
#define MIN(a,b) ((a)<(b)?a:b)
#define MAX(a,b) ((a)>(b)?a:b)

int is_exit = 0; // DO NOT MODIFY THE VARIABLE

void MemoryScheduler(virConnectPtr conn,int interval);

/*
DO NOT CHANGE THE FOLLOWING FUNCTION
*/
void signal_callback_handler()
{
	printf("Caught Signal");
	is_exit = 1;
}

/*
DO NOT CHANGE THE FOLLOWING FUNCTION
*/
int main(int argc, char *argv[])
{
	virConnectPtr conn;

	if(argc != 2)
	{
		printf("Incorrect number of arguments\n");
		return 0;
	}

	// Gets the interval passes as a command line argument and sets it as the STATS_PERIOD for collection of balloon memory statistics of the domains
	int interval = atoi(argv[1]);
	
	conn = virConnectOpen("qemu:///system");
	if(conn == NULL)
	{
		fprintf(stderr, "Failed to open connection\n");
		return 1;
	}

	signal(SIGINT, signal_callback_handler);

	while(!is_exit)
	{
		// Calls the MemoryScheduler function after every 'interval' seconds
		MemoryScheduler(conn, interval);
		sleep(interval);
	}

	// Close the connection
	virConnectClose(conn);
	return 0;
}

long minThreshold = 200 * 1024;
long maxThreshold = 2048 * 1024;

/*
COMPLETE THE IMPLEMENTATION
*/
void MemoryScheduler(virConnectPtr conn, int interval)
{
	virDomainPtr *vms = NULL;
	int numActiveVM = virConnectListAllDomains(conn, &vms, VIR_CONNECT_LIST_DOMAINS_RUNNING | VIR_CONNECT_LIST_DOMAINS_PERSISTENT);
	if (numActiveVM < 0) return; 

	unsigned int nr_stats = VIR_DOMAIN_MEMORY_STAT_NR;
	virDomainMemoryStatStruct stats[nr_stats];
	long *unusedMemory = (long*)calloc(numActiveVM, sizeof(long));
	long *availableMemory = (long*)calloc(numActiveVM, sizeof(long));
	long *balloonValue = (long*)calloc(numActiveVM, sizeof(long));
	unsigned long long hostTotal, hostFree;

	for(int i=0; i<numActiveVM; i++){
		virDomainSetMemoryStatsPeriod(vms[i],1,VIR_DOMAIN_AFFECT_LIVE);
		int numParams = virDomainMemoryStats(vms[i], stats, nr_stats, 0);
		if (numParams < 0) continue;

		for(int j=0; j<nr_stats; j++){
			if(stats[j].tag == VIR_DOMAIN_MEMORY_STAT_UNUSED) unusedMemory[i] = stats[j].val;
			else if(stats[j].tag == VIR_DOMAIN_MEMORY_STAT_AVAILABLE) availableMemory[i] = stats[j].val;
			else if(stats[j].tag == VIR_DOMAIN_MEMORY_STAT_ACTUAL_BALLOON) balloonValue[i] = stats[j].val;
		}

		//printf("UnsuedMemory %d : %ld\n",i,unusedMemory[i]/1024);
		//printf("BalloonValue %d : %ld\n",i,balloonValue[i]/1024);
		//printf("Usage percentage : %ld\n",unusedMemory[i]/balloonValue[i]);
	}

	for(int i=0; i<numActiveVM; i++){
		//memory wasted
		if((unusedMemory[i]*100)/balloonValue[i] >= 60){
			long newAlloc = balloonValue[i] - (availableMemory[i]/2);
			if(newAlloc < 200*1024){
				newAlloc = 200*1024;
			}
			virDomainSetMemory(vms[i],newAlloc);
		}
		//need more memory
		else if((unusedMemory[i]*100)/balloonValue[i] <= 25){
			int nparams = 0;
            		if (virNodeGetMemoryStats(conn, VIR_NODE_MEMORY_STATS_ALL_CELLS, NULL, &nparams, 0) == 0 && nparams != 0){
                		virNodeMemoryStatsPtr params = calloc(nparams, sizeof(virNodeMemoryStats));
                		virNodeGetMemoryStats(conn, VIR_NODE_MEMORY_STATS_ALL_CELLS, params, &nparams, 0); 
                		for(int j=0; j<nparams; j++){
                    			if(params[i].field == "total") hostTotal = params[i].value;
                    			else if (params[i].field == "free") hostFree = params[i].value;
                		}
            		}
            		printf("host free : %lld %lld \n",hostFree/1024, hostTotal/1024);
			long newAlloc = balloonValue[i] + 200*1024;
			long  maxMemory = virDomainGetMaxMemory(vms[i]);
			if(newAlloc > maxMemory){
				newAlloc = maxMemory;
			}
			if(hostFree - newAlloc < 200*1024){
				newAlloc = hostFree-(200*1024);
			}
			virDomainSetMemory(vms[i],newAlloc);
		}
	}

	for(int i=0; i<numActiveVM; i++){
		virDomainFree(vms[i]);
	}
	free(unusedMemory);
	free(availableMemory);
	free(balloonValue);
}
