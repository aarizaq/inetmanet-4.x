
// Module that implements a Node with its virtualization layer (hypervisor).
//
// This class models a node with an hypervisor. It is able to execute applications at node and virtual machines
//	requested by tenants.
//
// @author Gabriel González Castañé
// @date 2014-12-12

package inet.icancloud.Architecture.Node.NodeVL;

import inet.common.MessageDispatcher;
import inet.common.lifecycle.NodeStatus;
import inet.mobility.contract.IMobility;
import inet.node.contract.INetworkNode;
import inet.power.contract.IEnergyGenerator;
import inet.power.contract.IEnergyManagement;
import inet.power.contract.IEnergyStorage;
import inet.common.packet.recorder.PcapRecorder;
import inet.linklayer.contract.IEthernetInterface;
import inet.linklayer.contract.IExternalInterface;
import inet.linklayer.contract.ILoopbackInterface;
import inet.linklayer.contract.IPppInterface;
import inet.linklayer.contract.ITunnelInterface;
import inet.linklayer.contract.IWirelessInterface;
import inet.linklayer.contract.IVirtualInterface;
import inet.networklayer.common.InterfaceTable;
import inet.transportlayer.contract.ITcp;
import inet.linklayer.contract.IEthernetLayer;
import inet.linklayer.contract.IIeee8021qLayer;
import inet.linklayer.contract.IIeee8021rLayer;
import inet.linklayer.contract.IIeee8022Llc;
import inet.node.contract.IEthernetNetworkNode;
import inet.protocolelement.contract.IProtocolLayer;

import inet.networklayer.contract.INetworkLayer;

import inet.icancloud.Architecture.Node.INode;
import inet.icancloud.Architecture.NodeComponents.Hardware.CPUs.ICPU_Module;
import inet.icancloud.Architecture.NodeComponents.Hardware.Memories.MainMemories.IMainMemory;
import inet.icancloud.Architecture.NodeComponents.Hardware.Storage.StorageSystems.StorageSystem;
import inet.icancloud.Architecture.NodeComponents.OperatingSystems.OperatingSystemModules.OperatingSystemModule;
import inet.icancloud.EnergySystem.EnergyMeter.EnergyMeter;
import inet.icancloud.EnergySystem.PSU.IPSU;
import inet.icancloud.Virtualization.Hypervisor.IHypervisor;
import inet.linklayer.loopback.LoopbackInterface;

module NodeVL like INode

{
    parameters:

        @class("icancloud::NodeVL");
        @networkNode;
        @labels(node,ethernet-node,wireless-node);
        @display("i=old/telnet");



        bool hasTcp = default(firstAvailableOrEmpty("Tcp", "TcpLwip", "TcpNsc") != "");

        bool hasStatus = default(false);
        string osgModel = default(""); // 3D model for OSG visualization, no 3D model by default
        string osgModelColor = default(""); // tint color, no colorization by default
        string canvasImage = default(""); // image for canvas visualization, no image by default
        string canvasImageColor = default(""); // tint color, no colorization by default
        mobility.subjectModule = default("^");

        // Default parameters
        int numNetworkInterfaces = default(1);													// As default, 1 network interface
        //string tcpType = default(firstAvailable("TCP", "TCP_lwIP", "TCP_NSC", "TCP_None"));   	// Type of TCP protocol
        string ip @mutable = default("");															// Node's IP
        string routingFile = default("");														// Routing File
        int namid = default(-1);																// Nam ID
        bool storageNode = default(false);														// is this a atorage Node? If no, then its a compute node
        int storage_local_port = default(2049);													// Port for listening the incoming connections
        string hostName = default("");															// Node's hostname

        bool activeEnergyMeter = default(true);

        bool forwarding = default(false);
        bool multicastForwarding = default(false);



        int numPcapRecorders = default(0);
        int numLoInterfaces = default(1);
        int numWlanInterfaces = default(0);
        int numEthInterfaces = default(0);  // minimum number of ethernet interfaces
        int numExtInterfaces = default(0);
        int numPppInterfaces = default(0);  // minimum number of PPP interfaces
        int numTunInterfaces = default(0);
        int numVirtInterfaces = default(0);

        bool hasIpv4 = default(true);
        bool hasIpv6 = default(false);
        bool hasGn = default(false);

        *.forwarding = this.forwarding;
        *.multicastForwarding = this.multicastForwarding;

        *.mobilityModule = default(exists(mobility) ? absPath(".mobility") : "");
        *.energySourceModule = default(exists(energyStorage) ? absPath(".energyStorage") : "");
        *.energyGeneratorModule = default(exists(energyGenerator) ? absPath(".energyGenerator") : "");
        *.routingTableModule = default(absPath(".ipv4.routingTable"));

        *.interfaceTableModule = default(absPath(".interfaceTable"));


        // Main Hardware subsystems
        string cpuModuleType = default("CPU_Module");				// CPUModule type as CPU type
        string memoryType = default("RAMmemory");					// RAMmemory as memory type
        string psuModuleType = default("MainPSU");					// PSU module

         // Main Software configuration
        string hypervisorType = default("HypervisorNoOverhead");			// Main hypervisor		

        // Node parameters
        int numCores @mutable;												// Number of CPU cores		
        int memorySize_MB @unit(MiB) @mutable;										// Total amount of memory of this node (in MB)
        int memoryBlockSize_KB @unit(KiB) @mutable= default(128KiB);
        int numStorageSystems @mutable = default(1);							// Number of Storage systems. As default, 1 storage system
        int storageSize_GB @unit(GiB) @mutable;									// Total amount of space for each storage system
        string initialState;										// state or the node  NODE_STATE_OFF | NODE_STATE_IDLE | NODE_STATE_RUNNING
        int numFS = default(1);													// Number of File Systems

    gates:
        input radioIn[numWlanInterfaces] @directIn;
        inout pppg[numPppInterfaces] @labels(PppFrame-conn) @allowUnconnected;
        inout ethg[numEthInterfaces] @labels(EtherFrame-conn) @allowUnconnected;

         //storage server
        input fromVMStorageServers[];
        output toVMStorageServers[];

        //cpu
        input fromVMCPU[];
        output toVMCPU[];

        //memory
        input fromVMMemoryI[];
        input fromVMMemoryO[];
        output toVMMemoryI[];
        output toVMMemoryO[];

        // Hypervisor net
        input fromVMNet[];
        output toVMNet[];


    submodules:

        status: NodeStatus if hasStatus {
            parameters:
                @display("p=53,180;is=s");
        }
        energyStorage: <default("")> like IEnergyStorage if typename != "" {
            parameters:
                @display("p=53,259;is=s");
        }
        energyManagement: <default("")> like IEnergyManagement if typename != "" {
            parameters:
                @display("p=53,314;is=s");
        }
        energyGenerator: <default("")> like IEnergyGenerator if typename != "" {
            parameters:
                @display("p=53,426;is=s");
        }
        // TODO: move mobility right after status to have it in the same order as in their positions, changes fingerprints
        mobility: <default("")> like IMobility if typename != "" {
            parameters:
                @display("p=53,480;is=s");
        }



        at: MessageDispatcher {
            parameters:
                @display("p=749,179;b=627,5,,,,1");
        }

        tn: MessageDispatcher {
            parameters:
                @display("p=749,300;b=627,5,,,,1");
        }

        nl: MessageDispatcher {
            parameters:
                @display("p=750,466;b=664,5,,,,1");
        }


        ipv4: <default("Ipv4NetworkLayer")> like INetworkLayer if hasIpv4 {
            parameters:
                @display("p=496,388;q=queue");
        }
        ipv6: <default("Ipv6NetworkLayer")> like INetworkLayer if hasIpv6 {
            parameters:
                @display("p=603,395;q=queue");
        }
        generic: <default("")> like INetworkLayer if hasGn {
            parameters:
                @display("p=693,388;q=queue");
        }

        // linklayer
        interfaceTable: InterfaceTable {
            parameters:
                @display("p=53,364;is=s");
        }

        pcapRecorder[numPcapRecorders]: PcapRecorder {
            parameters:
                @display("p=53,556;is=s");
        }

        llc: <default("")> like IIeee8022Llc if typename != "" {
            @display("p=375,525");
        }
        cb: MessageDispatcher {
            @display("p=750,600;b=1000,5");
        }
        bridging: <default("")> like IProtocolLayer if typename != "" {
            @display("p=693,675");
        }
        bl: MessageDispatcher {
            @display("p=750,750;b=1000,5");
        }
        ethernet: <default(sizeof(ethg) > 0 ? "EthernetEncapsulation" : "")> like IEthernetLayer if typename != "" {
            @display("p=375,825");
        }
        ieee8021q: <default("")> like IIeee8021qLayer if typename != "" {
            @display("p=525,825");
        }
        ieee8021r: <default("")> like IIeee8021rLayer if typename != "" {
            @display("p=675,825");
        }
        li: MessageDispatcher {
            @display("p=750,900;b=1000,5,,,,1");
        }
        lo[numLoInterfaces]: <default("LoopbackInterface")> like ILoopbackInterface {
            @display("p=750,975,row,150");
        }
        // TODO move wlan interfaces after eth interfaces, but it changes IP address assignment and breaks examples/inet/configurator/complex.ini
        wlan[numWlanInterfaces]: <default("Ieee80211Interface")> like IWirelessInterface {
            @display("p=375,1000,row,150;q=queue");
        }
        ppp[sizeof(pppg)]: <default("PppInterface")> like IPppInterface {
            @display("p=300,975,row,150;q=txQueue");
        }
        eth[sizeof(ethg)]: <default("EthernetInterface")> like IEthernetInterface {
            @display("p=900,975,row,150;q=txQueue");
        }
        tun[numTunInterfaces]: <default("TunInterface")> like ITunnelInterface {
            @display("p=975,1000,row,150;q=txQueue");
        }
        virt[numVirtInterfaces]: <default("VirtualInterface")> like IVirtualInterface {
            @display("p=975,1000,row,150;q=txQueue");
        }


        energyMeter: EnergyMeter {
            parameters:
                activeEnergyMeter = parent.activeEnergyMeter;
                @display("p=53,729");
        }

        psu: <psuModuleType> like IPSU {
            @display("p=53,644");
        }

        tcp: <default(firstAvailableOrEmpty("Tcp", "TcpLwip", "TcpNsc"))> like ITcp if hasTcp {
            parameters:
                @display("p=504,236;is=s");
        }


        osModule: OperatingSystemModule {
            parameters:
                numFS = parent.numFS;
                numCPUs = parent.numCores;
                numStorageSystems = parent.numStorageSystems;
                @display("p=939,108");
            gates:
                fromApps[];
                toApps[];
                fromCPU[this.numCPUs];
                toCPU[this.numCPUs];
                fromStorageSystem[this.numStorageSystems];
                toStorageSystem[this.numStorageSystems];
                fromInputMemory;
                fromOutputMemory;
                toInputMemory;
                toOutputMemory;
                fromNet_TCP;
                toNet_TCP;
        }

        cpuModule: <cpuModuleType> like ICPU_Module {
            parameters:
                numCPUs = parent.numCores;
                @display("p=831,29");
            gates:
                fromOS[this.numCPUs];
                toOS[this.numCPUs];

        }
        storageSystem: StorageSystem {
            parameters:
                numStorageSystems = parent.numStorageSystems;
                @display("i=device/blockManager;p=642,42,row");
            gates:
                fromHypervisor[this.numStorageSystems];
                toDevice[this.numStorageSystems];

        }
        memory: <memoryType> like IMainMemory {
            parameters:
                blockSize_KB = parent.memoryBlockSize_KB;
                size_MB = parent.memorySize_MB;
                @display("p=580,133");
            gates:
                fromInput;
                toInput;
        }

        hypervisor: <hypervisorType> like IHypervisor {
            parameters:
                numStorageServers = parent.numStorageSystems; 							// Number of Storage Servers	
                numCPUs = parent.numCores; 	 										// Number of CPUs
                memorySize_MB = parent.memorySize_MB;
                blockSize_KB = parent.memoryBlockSize_KB;
                numNetworkInterfaces = parent.numNetworkInterfaces;
                ip = parent.ip;
                storageSize_GB = parent.storageSize_GB;
                storageApp_ModuleIndex = 0;
                @display("p=750,81;i=energy/Hypervisor_40");
            gates:

                //Hypervisor to array of gates connections
                //                Storage server
                fromVMStorageServers[];
                toVMStorageServers[];

                //cpu
                fromVMCPU[];
                toVMCPU[];

                //memory
                fromVMMemoryI[];
                fromVMMemoryO[];
                toVMMemoryI[];
                toVMMemoryO[];

				//Hypervisor to HW connections
                fromNodeCPU[this.numCPUs];
                toNodeCPU[this.numCPUs];

                fromNodeStorageServers[this.numStorageServers];
                toNodeStorageServers[this.numStorageServers];

                fromNodeMemoryI;
                fromNodeMemoryO;
                toNodeMemoryI;
                toNodeMemoryO;

                fromNodeNet;			// Input gate from Network (Ethernet TCP)
                toNodeNet;			// Output gate to Network (Ethernet TCP)

        }

    connections allowunconnected:

        // Connections between CPU and Hypervisor
        for i=0..numCores-1 {
            cpuModule.toOS[i] --> hypervisor.fromNodeCPU[i];
            cpuModule.fromOS[i] <-- hypervisor.toNodeCPU[i];

        }

        // Connections between Storage Servers and hypervisor
        for i=0..numStorageSystems-1 {
            storageSystem.toDevice[i] --> hypervisor.fromNodeStorageServers[i];
            storageSystem.fromHypervisor[i] <-- hypervisor.toNodeStorageServers[i];
        }

        // Connections between Memory and hypervisor
        memory.toInput --> hypervisor.fromNodeMemoryI;
        memory.fromInput <-- hypervisor.toNodeMemoryI;
        memory.fromOutput <-- hypervisor.toNodeMemoryO;
        memory.toOutput --> hypervisor.fromNodeMemoryO;

		// Connections between TCP and hypervisor

        at.out++ --> tcp.appIn;
        at.in++ <-- tcp.appOut;

        hypervisor.toNodeNet --> at.in++;
        hypervisor.fromNodeNet <-- at.out++;

        tcp.ipOut --> tn.in++;
        tcp.ipIn <-- tn.out++;

        at.out++ --> tn.in++;
        at.in++ <-- tn.out++;

        tn.out++ --> nl.in++;
        tn.in++ <-- nl.out++;

		// Connections between the BS and the Operative System
        for i=0..numStorageSystems-1 {
            osModule.fromStorageSystem++ <-- hypervisor.toVMStorageServers++;
            osModule.toStorageSystem++ --> hypervisor.fromVMStorageServers++;
        }
        //Conections of the Memory and Operative System
        osModule.fromInputMemory <-- hypervisor.toVMMemoryI++;
        osModule.toInputMemory --> hypervisor.fromVMMemoryI++;
        osModule.fromOutputMemory <-- hypervisor.toVMMemoryO++;
        osModule.toOutputMemory --> hypervisor.fromVMMemoryO++;

		//Conections of the CPU and Operative System
        osModule.fromCPU[0] <-- hypervisor.toVMCPU++;
        osModule.toCPU[0] --> hypervisor.fromVMCPU++;

        // Connections between Operating System and Network
        osModule.toNet_TCP --> hypervisor.fromVMNet++;
        osModule.fromNet_TCP <-- hypervisor.toVMNet++;

        // INET internals...
        ipv4.ifIn <-- nl.out++ if hasIpv4;
        ipv4.ifOut --> nl.in++ if hasIpv4;

        ipv6.ifIn <-- nl.out++ if hasIpv6;
        ipv6.ifOut --> nl.in++ if hasIpv6;

        generic.ifIn <-- nl.out++ if exists(generic);
        generic.ifOut --> nl.in++ if exists(generic);

        tn.out++ --> ipv4.transportIn if hasIpv4;
        tn.in++ <-- ipv4.transportOut if hasIpv4;

        tn.out++ --> ipv6.transportIn if hasIpv6;
        tn.in++ <-- ipv6.transportOut if hasIpv6;

        tn.out++ --> generic.transportIn if hasGn;
        tn.in++ <-- generic.transportOut if hasGn;

        cb.out++ --> nl.in++;
        cb.in++ <-- nl.out++;

        llc.upperLayerOut --> nl.in++ if exists(llc);
        llc.upperLayerIn <-- nl.out++ if exists(llc);

        llc.lowerLayerOut --> cb.in++ if exists(llc);
        llc.lowerLayerIn <-- cb.out++ if exists(llc);

        cb.out++ --> bridging.upperLayerIn if exists(bridging);
        bridging.upperLayerOut --> cb.in++ if exists(bridging);

        bridging.lowerLayerOut --> bl.in++ if exists(bridging);
        bl.out++ --> bridging.lowerLayerIn if exists(bridging);

        cb.out++ --> bl.in++ if !exists(bridging);
        bl.out++ --> cb.in++ if !exists(bridging);

        bl.out++ --> li.in++;
        li.out++ --> bl.in++;

        bl.out++ --> ieee8021q.upperLayerIn if exists(ieee8021q);
        ieee8021q.upperLayerOut --> bl.in++ if exists(ieee8021q);

        bl.out++ --> ieee8021r.upperLayerIn if exists(ieee8021r);
        ieee8021r.upperLayerOut --> bl.in++ if exists(ieee8021r);

        bl.out++ --> ethernet.upperLayerIn if exists(ethernet);
        ethernet.upperLayerOut --> bl.in++ if exists(ethernet);

        ieee8021q.lowerLayerOut --> li.in++ if exists(ieee8021q);
        li.out++ --> ieee8021q.lowerLayerIn if exists(ieee8021q);

        ieee8021r.lowerLayerOut --> li.in++ if exists(ieee8021r);
        li.out++ --> ieee8021r.lowerLayerIn if exists(ieee8021r);

        ethernet.lowerLayerOut --> li.in++ if exists(ethernet);
        li.out++ --> ethernet.lowerLayerIn if exists(ethernet);

        for i=0..sizeof(radioIn)-1 {
            radioIn[i] --> { @display("m=s"); } --> wlan[i].radioIn;
        }

        for i=0..sizeof(ethg)-1 {
            ethg[i] <--> { @display("m=s"); } <--> eth[i].phys;
        }

        for i=0..sizeof(pppg)-1 {
            pppg[i] <--> { @display("m=s"); } <--> ppp[i].phys;
        }

        for i=0..numLoInterfaces-1 {
            li.out++ --> lo[i].upperLayerIn;
            lo[i].upperLayerOut --> li.in++;
        }

        for i=0..sizeof(radioIn)-1 {
            wlan[i].upperLayerOut --> li.in++;
            wlan[i].upperLayerIn <-- li.out++;
        }

        for i=0..sizeof(ethg)-1 {
            eth[i].upperLayerOut --> li.in++;
            eth[i].upperLayerIn <-- li.out++;
        }

        for i=0..sizeof(pppg)-1 {
            ppp[i].upperLayerOut --> li.in++;
            ppp[i].upperLayerIn <-- li.out++;
        }

        for i=0..numTunInterfaces-1 {
            tun[i].upperLayerOut --> li.in++;
            tun[i].upperLayerIn <-- li.out++;
        }

        for i=0..numVirtInterfaces-1 {
            virt[i].upperLayerOut --> li.in++;
            virt[i].upperLayerIn <-- li.out++;
        }

}
