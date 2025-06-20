//
// Copyright (C) 2003 Andras Varga; CTIE, Monash University, Australia
//               2010 Zoltan Bojthe
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

package inet.node.ethernet;

import inet.applications.contract.IApp;
import inet.clock.contract.IClock;
import inet.common.IMeasurer;
import inet.common.MessageDispatcher;
import inet.common.lifecycle.NodeStatus;
import inet.common.packet.recorder.PcapRecorder;
import inet.linklayer.configurator.contract.IL2NodeConfigurator;
import inet.linklayer.contract.IEthernetInterface;
import inet.linklayer.contract.IEthernetLayer;
import inet.linklayer.contract.IGptp;
import inet.linklayer.contract.IIeee8021qLayer;
import inet.linklayer.contract.IIeee8021rLayer;
import inet.linklayer.contract.IIeee8022Llc;
import inet.linklayer.contract.IMacForwardingTable;
import inet.linklayer.contract.IMrp;
import inet.linklayer.contract.ISpanningTree;
import inet.networklayer.common.InterfaceTable;
import inet.node.contract.IEthernetNetworkNode;
import inet.protocolelement.contract.IProtocolLayer;

//
// EthernetSwitch models a Layer 2 Ethernet switch with support for various IEEE 802.1
// protocols and features. It provides frame forwarding based on MAC addresses and
// implements multiple switching technologies.
//
// The switch maintains a MAC address table (forwarding database) that maps MAC addresses
// to ports. It learns these mappings by examining the source address of incoming frames.
// When a frame arrives with an unknown destination, the switch floods it to all ports
// except the incoming one.
//
// Key features:
// - MAC address learning and forwarding
// - Support for Spanning Tree Protocol (STP/RSTP) to prevent loops
// - Optional VLAN support through IEEE 802.1Q
// - Optional Media Redundancy Protocol (MRP) support
// - Optional Generalized Precision Time Protocol (gPTP) for time synchronization
// - Support for cut-through switching for reduced latency
// - Configurable number of Ethernet interfaces
// - Optional PCAP recording for traffic analysis
//
module EthernetSwitch like IEthernetNetworkNode
{
    parameters:
        @networkNode();
        @labels(node,ethernet-node);
        @display("i=device/switch");
        bool recordPcap = default(false);
        int numPcapRecorders = default(recordPcap ? 1 : 0);
        int numApps = default(0);
        bool hasStatus = default(false);
        bool hasStp = default(false);
        bool hasMrp = default(false);
        bool hasGptp = default(false);
        bool hasCutthroughSwitching = default(false);
        string fcsMode @enum("declared","computed") = default("declared");
        string spanningTreeProtocol = default("Stp");
        int numEthInterfaces = default(0);  // Minimum number of ethernet interfaces
        eth[*].encap.typename = default("");
        *.fcsMode = this.fcsMode;
        **.interfaceTableModule = default(absPath(".interfaceTable"));
        **.macTableModule = default(absPath(".macTable"));
        **.mrpRelayModule = default(absPath(".bridging"));
        *.clockModule = default(exists(clock) ? absPath(".clock") : "");

        llc.registerProtocol = true;
        eth[*].mac.promiscuous = default(true);
        ethernet.*.promiscuous = default(true);
        ethernet.registerProtocol = default(true);

    gates:
        inout ethg[numEthInterfaces] @labels(EtherFrame-conn);
    submodules:
        macTable: <default(hasMrp ? "MrpMacForwardingTable" : "MacForwardingTable")> like IMacForwardingTable {
            @display("p=100,100;is=s");
        }
        interfaceTable: InterfaceTable {
            @display("p=100,200;is=s");
        }
        l2NodeConfigurator: <default("L2NodeConfigurator")> like IL2NodeConfigurator if hasStp || hasMrp {
            @display("p=100,300;is=s");
        }
        status: NodeStatus if hasStatus {
            @display("p=100,400;is=s");
        }
        clock: <default(hasGptp ? "SettableClock" : "")> like IClock if typename != "" {
            @display("p=100,500;is=s");
        }
        pcapRecorder[numPcapRecorders]: PcapRecorder {
            @display("p=100,600;is=s");
        }
        measurer: <default("")> like IMeasurer if typename != "" {
            @display("p=100,700;is=s");
        }
        mrp: <default("Mrp")> like IMrp if hasMrp {
            @display("p=500,50");
        }
        stp: <spanningTreeProtocol> like ISpanningTree if hasStp {
            @display("p=500,75");
        }
        gptp: <default("Gptp")> like IGptp if hasGptp {
            @display("p=700,75");
            gptpNodeType = default("BRIDGE_NODE"); // @enum("gptpNodeType"): MASTER_NODE, BRIDGE_NODE, SLAVE_NODE
        }
        app[numApps]: <> like IApp {
            parameters:
                @display("p=900,75,row,150");
        }
        sc: MessageDispatcher {
            @display("p=800,150;b=1200,5");
        }
        llc: <default(hasMrp ? "Ieee8022Llc" : "")> like IIeee8022Llc if typename != "" || hasMrp {
            @display("p=700,225");
        }
        cb: MessageDispatcher {
            @display("p=800,300;b=1200,5");
        }
        bridging: <default(hasMrp ? "MrpRelay" : firstAvailable("Ieee8021dRelay","MacRelayUnit"))> like IProtocolLayer {
            @display("p=800,375;is=m");
        }
        bl: MessageDispatcher {
            @display("p=800,450;b=1200,5");
        }
        ethernet: <default("EthernetEncapsulation")> like IEthernetLayer if typename != "" {
            @display("p=500,525");
        }
        ieee8021q: <default("")> like IIeee8021qLayer if typename != "" {
            @display("p=700,525");
        }
        ieee8021r: <default("")> like IIeee8021rLayer if typename != "" {
            @display("p=900,525");
        }
        li: MessageDispatcher {
            @display("p=800,600;b=1200,5");
        }
        eth[sizeof(ethg)]: <default(hasCutthroughSwitching ? "EthernetCutthroughInterface" : "EthernetInterface")> like IEthernetInterface {
            @display("p=250,750,row,150;q=txQueue");
        }
    connections:
        ethernet.lowerLayerOut --> li.in++ if exists(ethernet);
        li.out++ --> ethernet.lowerLayerIn if exists(ethernet);

        ieee8021q.lowerLayerOut --> li.in++ if exists(ieee8021q);
        li.out++ --> ieee8021q.lowerLayerIn if exists(ieee8021q);

        ieee8021r.lowerLayerOut --> li.in++ if exists(ieee8021r);
        li.out++ --> ieee8021r.lowerLayerIn if exists(ieee8021r);

        bl.out++ --> li.in++;
        li.out++ --> bl.in++;

        llc.upperLayerOut --> sc.in++ if exists(llc);
        sc.out++ --> llc.upperLayerIn if exists(llc);

        sc.out++ --> cb.in++;
        cb.out++ --> sc.in++;

        cb.out++ --> llc.lowerLayerIn if exists(llc);
        llc.lowerLayerOut --> cb.in++ if exists(llc);

        cb.out++ --> bridging.upperLayerIn;
        bridging.upperLayerOut --> cb.in++;

        ieee8021q.upperLayerOut --> bl.in++ if exists(ieee8021q);
        bl.out++ --> ieee8021q.upperLayerIn if exists(ieee8021q);

        ieee8021r.upperLayerOut --> bl.in++ if exists(ieee8021r);
        bl.out++ --> ieee8021r.upperLayerIn if exists(ieee8021r);

        bl.out++ --> ethernet.upperLayerIn if exists(ethernet);
        ethernet.upperLayerOut --> bl.in++ if exists(ethernet);

        bridging.lowerLayerOut --> bl.in++;
        bl.out++ --> bridging.lowerLayerIn;

        for i=0..sizeof(ethg)-1 {
            li.out++ --> eth[i].upperLayerIn;
            li.in++ <-- eth[i].upperLayerOut;
            eth[i].phys <--> { @display("m=s"); } <--> ethg[i];
        }

        if hasMrp {
            mrp.relayIn <-- sc.out++;
            mrp.relayOut --> sc.in++;
        }

        if hasStp {
            stp.relayIn <-- sc.out++;
            stp.relayOut --> sc.in++;
        }

        if hasGptp {
            gptp.socketOut --> sc.in++;
            sc.out++ --> gptp.socketIn;
        }

        for i=0..numApps-1 {
            app[i].socketOut --> sc.in++;
            app[i].socketIn <-- sc.out++;
        }
}

