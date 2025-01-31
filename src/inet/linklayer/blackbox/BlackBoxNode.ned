//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
// 
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/.
// 


package inet.linklayer.blackbox;

import inet.node.base.NodeBase;
import inet.node.contract.INetworkNode;
import inet.mobility.contract.IMobility;
import inet.networklayer.common.InterfaceTable;
import inet.linklayer.blackbox.L2Queue;
import inet.linklayer.blackbox.Forwarding;
import inet.linklayer.contract.IPppInterface;
import inet.common.MessageDispatcher;

module BlackBoxNode extends NodeBase like INetworkNode
{
    parameters:
        int numPppInterfaces = default(0);  // Minimum number of PPP interfaces
        int numBlaclBoxInterfaces = default(0);
        @backBoxNode;
        @display("bgb=1269,915;i=device/accesspoint;is=vs");
    gates:
        inout pppg[numPppInterfaces] @labels(PppFrame-conn) @allowUnconnected;
        inout blackbox[numBlaclBoxInterfaces] @allowUnconnected;
    submodules:
        interfaceTable: InterfaceTable {
            @display("p=125,240;is=s");
        }
        li: MessageDispatcher {
            @display("p=588,395;b=500,5,,,,1");
        }
        forwarding: Forwarding {
            @display("p=541,192");
        }
        ppp[sizeof(pppg)]: <default("PppInterface")> like IPppInterface {
            @display("p=530,596,row,150;q=txQueue");
        }
        l2queue[sizeof(blackbox)]: L2Queue {
            @display("p=1024,294");
        }
    connections allowunconnected:
        for i=0..sizeof(pppg)-1 {
            pppg[i] <--> { @display("m=s"); } <--> ppp[i].phys;
        }
        for i=0..sizeof(pppg)-1 {
            ppp[i].upperLayerOut --> li.in++;
            ppp[i].upperLayerIn <-- li.out++;
        }

        for i=0..sizeof(blackbox)-1 {
            forwarding.outGate[i] --> l2queue[i].in;
            forwarding.inGate[i] <-- l2queue[i].out;
        }

        for i=0..sizeof(blackbox)-1 {
            blackbox++ <--> { @display("m=s"); } <--> l2queue[i].line;
        }

        forwarding.outPpp --> li.in++;
        li.out++ --> forwarding.inPpp;
}
