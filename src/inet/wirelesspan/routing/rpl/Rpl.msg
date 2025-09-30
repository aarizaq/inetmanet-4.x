//
//  Simulation model for RPL (Routing Protocol for Low-Power and Lossy Networks)
// 
//  Copyright (C) 2021  Institute of Communication Networks (ComNets),
//                      Hamburg University of Technology (TUHH)
//            (C) 2021  Yevhenii Shudrenko
// 
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
// 
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
// 
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <https://www.gnu.org/licenses/>.
//

import inet.common.INETDefs;
import inet.common.packet.chunk.Chunk;
import inet.wirelesspan.routing.rpl.RplDefs;
import inet.networklayer.common.L3Address;
import inet.networklayer.contract.ipv6.Ipv6Address;
import inet.networklayer.icmpv6.Icmpv6Header;
import inet.common.geometry.Geometry;

namespace inet::wirelesspan::routing;

// Objective Code Point - defines objective function of RPL instance
//enum Ocp {
//    ETX = 0;
//    HOP_COUNT = 1;
//    ENERGY = 2;
//};

enum Ocp {
        STATE_ATRIBUTE = 3;
        ENERGY = 2;
        HOP_COUNT = 1;
        THROUGHPUT = 4;
        LATENCY = 5;
        LQL = 6;
        ETX = 0;
        LINK_COLOR = 7;
        OPERATION_MODE = 8;
};

enum RplPacketCode {
    DIS = 0;
    DIO = 1;
    DAO = 2;
    DAO_ACK = 3;
    PING = 4;
    PING_ACK = 5;
};
//
// RPL packets
//

class RplHeader extends Icmpv6Header {
	RplPacketCode icmpv6Code;
	const uint8_t icmpv6Type = 155;
}

class RplPacket extends FieldsChunk {
    Ipv6Address destAddress;   		
    Ipv6Address srcAddress;
    uint8_t instanceId;        	  	// RPL instance ID
    Ipv6Address dodagId;            // IPv6 address of the DODAG root
	uint64_t nodeId; 				// Node's MAC (temp. workaround for cross-layer 6TiSCH)
}

// DODAG Information Option [RFC 6550, 6.3]
class Dio extends RplPacket {
    // DODAG version number, may be incremented by the root to trigger topology refresh 
    uint8_t dodagVersion;       	
	uint16_t rank;            	 	// Node's rank within the DODAG 
    bool storing;					// Mode of operation: storing / non-storing                
    bool grounded;              	// DODAG grounded flag, indicates whether sink is connected to backbone	   
    
    // Destination Advertisement Trigger Sequence Number, 
    // may be incremented to trigger downward routes update
    uint8_t dtsn;
   
	// Trickle timer parameters (DAG Configuration Option)
    int minInterval = -1;           
    int dioRedundancyConst = -1;              
    int dioNumDoublings = -1;						
    Ocp ocp;                
    
    // Non-RFC fields, misc
    string nodeName; // name of the sender node, e.g. host[0], needed for dynamic update of connectiviy arrows in GUI
    bool isMobile;
    Coord position; // DIO sender node location to draw directed parent-child connectors on canvas
    cFigure::Color color; // Color of the parent-child connector line, per DODAG
	int colorId; // DODAG color id (from static palette) for multi-GW scenario
	
	// Low-latency (LL) mode fields
	long slotOffset; // ideally we're able to schedule a slot offset at this value - 1 to our preferred parent
}

cplusplus (Dio) {{
    friend std::ostream& operator<<(std::ostream& os, Dio* dio)
    {
        os << "rank: " << (int) dio->getRank();
        return os;
    }
}}

// Destination Advertisement Object [RFC 6550, 6.4] 
class Dao extends RplPacket {
    uint8_t seqNum;				// ID for each unique DAO sent by a node
    bool daoAckRequired;		// indicates whether DAO-ACK is expected by the sender 
    Ipv6Address reachableDest;	// advertised reachable destination		
}

cplusplus (Dao) {{
	std::vector<Ipv6Address> knownTargets; 
	
	std::vector<Ipv6Address> getKnownTargets() { return this->knownTargets; }
	void setKnownTargets(std::vector<Ipv6Address> knownTargets) { handleChange(); this->knownTargets = knownTargets; }
}}

// DODAG Information Solicitation
class Dis extends RplPacket {

}

// RPL Information Packet header [RFC 6550 11.2]
class RplPacketInfo extends RplPacket {
    bool down;
    bool rankError;
    bool fwdError;
    uint16_t senderRank;
}


class RplTargetInfo extends RplPacket {
    Ipv6Address target;
    // tbd fields according to 
}

class RplTransitInfo extends RplPacket {
    Ipv6Address transit;
    // tbd fields
}


class SourceRoutingHeader extends FieldsChunk {	
}

cplusplus (SourceRoutingHeader) {{
    std::deque<Ipv6Address> addresses;
    
    std::deque<Ipv6Address> getAddresses() { return this->addresses; }
    void setAddresses(std::deque<Ipv6Address> hopList) { handleChange(); this->addresses = hopList; }	
}}



