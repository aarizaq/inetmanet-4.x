//
// Copyright (C) 2005 Eric Wu
// Copyright (C) 2004 Andras Varga
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program; if not, see <http://www.gnu.org/licenses/>.
//

#include "RplNetConfigurator.h"
#include "inet/networklayer/common/L3AddressResolver.h"
#include "inet/networklayer/contract/IInterfaceTable.h"
#include "inet/networklayer/ipv6/Ipv6InterfaceData.h"
#include "inet/networklayer/ipv6/Ipv6RoutingTable.h"

namespace inet {
namespace wirelesspan {

Define_Module(RplNetConfigurator);

void RplNetConfigurator::initialize(int stage)
{
    EV_DETAIL << "RplNetConfigurator, stage - " << stage << endl;

    cSimpleModule::initialize(stage);

    if (stage == INITSTAGE_NETWORK_CONFIGURATION) {
        EV_DETAIL << "Initializing RplNetConfigurator" << endl;
        Topology topo("topo");

        // extract topology
        topo.extractByProperty("networkNode");
        EV_DETAIL << "cTopology found " << topo.getNumNodes() << " nodes\n";

        configureAdvPrefixes(topo);

//        addOwnAdvPrefixRoutes(topo);
//        addStaticRoutes(topo);
    }
}


void RplNetConfigurator::configureAdvPrefixes(Topology& topo)
{
    EV_DETAIL << "Configuring adv prefixes:" << endl;
    // assign advertised prefixes to all router interfaces
    for (int i = 0; i < topo.getNumNodes(); i++) {
        // skip bus types
        if (!isIPNode(topo.getNode(i)))
            continue;

        int nodeIndex = i;

        // find interface table and assign address to all (non-loopback) interfaces
        cModule *mod = topo.getNode(i)->getModule();
        auto ift = L3AddressResolver().interfaceTableOf(mod);
        auto rt = L3AddressResolver().findIpv6RoutingTableOf(mod);

        // skip non-Ipv6 nodes
        if (!rt)
            continue;

        // skip hosts
//        if (!rt->par("isRouter"))
//            continue;

        // assign prefix to interfaces
        for (int k = 0; k < ift->getNumInterfaces(); k++) {
            EV_DETAIL << "Interface #" << k << " ";
            auto ie = ift->getInterface(k);
            auto ipv6Data = ie->findProtocolDataForUpdate<Ipv6InterfaceData>();
            if (!ipv6Data || ie->isLoopback())
                continue;
            if (ipv6Data->getNumAdvPrefixes() > 0)
                continue; // already has one

            // add a prefix
//            Ipv6Address prefix(0xaaaa0000 + nodeIndex, ie->getInterfaceId() << 16, 0, 0);
            Ipv6Address addr(0, 0, 0, nodeIndex + k + 1); // + 1 to exclude loop-back ::01 address
            ASSERT(addr.isGlobal());

            EV_DETAIL << "assigned prefix - " << addr << endl;

            Ipv6InterfaceData::AdvPrefix p;
            p.prefix = addr;
            p.prefixLength = 64;
            // RFC 2461:6.2.1. Only default values are used in Ipv6FlatNetworkConfigurator
            // Default: 2592000 seconds (30 days), fixed (i.e., stays the same in
            // consecutive advertisements).
            p.advValidLifetime = 2592000;
            // Default: TRUE
            p.advOnLinkFlag = true;
            // Default: 604800 seconds (7 days), fixed (i.e., stays the same in consecutive
            // advertisements).
            p.advPreferredLifetime = 604800;
            // Default: TRUE
            p.advAutonomousFlag = true;

//            ipv6Data->addAdvPrefix(p);

            ipv6Data->assignAddress(addr, false, SIMTIME_ZERO, SIMTIME_ZERO);

            // add a link-local address (tentative) if it doesn't have one
//            if (ipv6Data->getLinkLocalAddress().isUnspecified())
//                ipv6Data->assignAddress(Ipv6Address::formLinkLocalAddress(ie->getInterfaceToken()), true, SIMTIME_ZERO, SIMTIME_ZERO);
        }
    }
}

}
}


