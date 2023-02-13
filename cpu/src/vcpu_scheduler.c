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

int is_exit = 0; // DO NOT MODIFY THIS VARIABLE

struct vcpuInfo{
	unsigned int number; //virtual CPU number
	int state; //value from virVcpuState
	unsigned long long cpuTime; //CPU time used in nano seconds
	int cpu; //real CPU number
	int vm; //the virtual machine associated
	double percentUsage;
};

struct pcpuInfo{
	unsigned long long cpuTime;
	double workload;
	double percentUsage;
};

void CPUScheduler(virConnectPtr conn,int interval);

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

	// Get the total number of pCpus in the host
	signal(SIGINT, signal_callback_handler);

	while(!is_exit)
	// Run the CpuScheduler function that checks the CPU Usage and sets the pin at an interval of "interval" seconds
	{
		CPUScheduler(conn, interval);
		sleep(interval);
	}

	// Closing the connection
	virConnectClose(conn);
	return 0;
}

int compare(const void *v1, const void *v2) {
	struct vcpuInfo *i1 = (struct vcpuInfo *)v1;
	struct vcpuInfo *i2 = (struct vcpuInfo *)v2;
	if (i1->percentUsage < i2->percentUsage) return 1;
	else return -1;
}

struct vcpuInfo* vcpuReports;
struct pcpuInfo* pcpuReports;
struct vcpuInfo* prevVcpuReports = NULL;
int prevNumActiveVM = 0;

/* COMPLETE THE IMPLEMENTATION */
void CPUScheduler(virConnectPtr conn, int interval)
{
	int numPCPU = virNodeGetCPUMap(conn, NULL, NULL, 0);
	int cpumaplen = VIR_CPU_MAPLEN(numPCPU);
	unsigned char *cpumap = (unsigned char*)calloc(cpumaplen, sizeof(unsigned char));

	virDomainPtr *vms = NULL;
		//If there are no active VMs, exit the loop
		int numActiveVM = virConnectListAllDomains(conn, &vms, VIR_CONNECT_LIST_DOMAINS_RUNNING | VIR_CONNECT_LIST_DOMAINS_PERSISTENT);
		//printf("Num active VM  %d \n",numActiveVM);
		if (numActiveVM < 0) return;

		vcpuReports = (struct vcpuInfo*)calloc(numActiveVM, sizeof(struct vcpuInfo));
		pcpuReports = (struct pcpuInfo*)calloc(numPCPU, sizeof(struct pcpuInfo));

		for(int i=0; i<numActiveVM; i++){
		 	int numVCPU = virDomainGetMaxVcpus(vms[i]);
		 	virVcpuInfoPtr vcpuInfo = (virVcpuInfo*)calloc(numVCPU, sizeof(virVcpuInfo));
		 	int ret = virDomainGetVcpus(vms[i], vcpuInfo, numVCPU, NULL, 0);
			//printf("Number of VPCU in %d is %d \n",i,ret);
		 	if (ret < 0) exit(1);

		 	//Assuming one vcpu per VM
		 	vcpuReports[i].cpu = vcpuInfo[0].cpu;
		 	vcpuReports[i].number = 0;
		 	vcpuReports[i].state = vcpuInfo[0].state;
		 	vcpuReports[i].cpuTime = vcpuInfo[0].cpuTime;
		 	vcpuReports[i].vm = i;
			//printf("CPU and VM %d %d \n",vcpuReports[i].cpu,vcpuReports[i].vm);
		 	free(vcpuInfo);
		 }

		 if(prevNumActiveVM != numActiveVM){
		 	//Assign each vcpu to pcpus in order
			//printf("First time scheduling \n");
		 	int vcpuPerPcpu = (numActiveVM/numPCPU) + ((numActiveVM%numPCPU)!=0);
		 	for(int i=0; i<numActiveVM; i++){
		 		if(i%vcpuPerPcpu==0){
		 			for(int j=0; j<cpumaplen; j++){
		 				int pcpuNum = i/vcpuPerPcpu;
		 				if(pcpuNum/8 == j){
		 					cpumap[j] = 0x1 << (pcpuNum%8);
		 				} else {
		 					cpumap[i] = 0x0;
		 				}
		 			}
		 		}
		 		virDomainPinVcpu(vms[i], 0, cpumap, cpumaplen);
		 	}
		 } else {
			//printf("Next time scheduling \n");
		 	for(int i=0; i<numActiveVM; i++){
		 		//Usage percentage for pcpu and vcpus
		 		//double usage = (((double)(vcpuReports[i].cpuTime - prevVcpuReports[i].cpuTime))*100)/((double)(interval*1000000000));
				double diff = vcpuReports[i].cpuTime-prevVcpuReports[i].cpuTime;
				double inter = interval*10000000;
				double usage = diff/inter;
				//printf("current and orevious %llu %llu\n",vcpuReports[i].cpuTime,prevVcpuReports[i].cpuTime);
				//printf("Percentage : %f\n",usage);
		 		vcpuReports[i].percentUsage = usage;
		 		pcpuReports[vcpuReports[i].cpu].percentUsage += vcpuReports[i].percentUsage;
		 	}

		 	int flag=0, maxUse=0, minUse=100;
		 	for(int i=0; i<numPCPU; i++){
				//printf("PCPU  %d usage and load are  %f %f\n",i,pcpuReports[i].percentUsage,pcpuReports[i].workload);
		 		if(pcpuReports[i].percentUsage < minUse) minUse = pcpuReports[i].percentUsage;
		 		if(pcpuReports[i].percentUsage > maxUse) maxUse = pcpuReports[i].percentUsage;
		 		if(pcpuReports[i].percentUsage > 20.0) flag=1;
		 	}
		 	if (flag==1 && maxUse-minUse > 5.0){
		 		//Find vcpu with max usage and pin it to pcpu with min usage to balance out
				//printf("Change the pins as not balanced\n");
		 		struct vcpuInfo* vcpuReportsCopy = (struct vcpuInfo*)calloc(numActiveVM, sizeof(struct vcpuInfo));
		 		memcpy(vcpuReportsCopy, vcpuReports, numActiveVM*sizeof(struct vcpuInfo));
		 		qsort(vcpuReportsCopy,numActiveVM,sizeof(struct vcpuInfo), compare);
		 		for (int i=0; i<numActiveVM; i++){
		 			int minVal = 100, indexMinVal = 0;
		 			for (int j=0; j<numPCPU; j++){
		 				if(minVal > pcpuReports[j].workload){
		 					minVal = pcpuReports[j].workload;
		 					indexMinVal = j;
		 				}
		 			}
					//printf("Index of min cpu %d\n",indexMinVal);

		 			for(int j=0; j<cpumaplen; j++){
		 				if(indexMinVal/8 == j){
		 					cpumap[j] = 0x1 << (indexMinVal%8);
		 				} else {
		 					cpumap[j] = 0x0;
		 				}
		 			}
					//printf("Assigning VCPu %d to PCPU %d with load %f\n",i,indexMinVal,pcpuReports[indexMinVal].workload);
		 			virDomainPinVcpu(vms[vcpuReportsCopy[i].vm],0,cpumap,cpumaplen);
		 			pcpuReports[indexMinVal].workload += vcpuReportsCopy[i].percentUsage;
					//printf("Workload of pcpu %d changed to %f\n",indexMinVal,pcpuReports[indexMinVal].workload);
		 		}
		 		free(vcpuReportsCopy);
		 	}
		 }

		 for(int i=0; i<numActiveVM; i++) virDomainFree(vms[i]);
		 free(prevVcpuReports);

		 prevVcpuReports = vcpuReports;
		 prevNumActiveVM = numActiveVM;

}




