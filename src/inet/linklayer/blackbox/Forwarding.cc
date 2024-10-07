//
// This file is part of an OMNeT++/OMNEST simulation example.
//
// Copyright (C) 1992-2015 Andras Varga
//
// This file is distributed WITHOUT ANY WARRANTY. See the file
// `license' for details on this and other legal matters.
//

#include "Forwarding.h"
#include "inet/common/ModuleAccess.h"
#include "inet/common/DijktraKShortest.h"
#include "inet/networklayer/common/L3AddressResolver.h"
#include "BlackBoxLabel_m.h"
#include "inet/networklayer/common/L3Tools.h"
#include "inet/linklayer/common/InterfaceTag_m.h"
#include "inet/linklayer/ppp/Ppp.h"
#include "inet/networklayer/contract/IInterfaceTable.h"
#include "inet/networklayer/ipv4/Ipv4Header_m.h"
#include "inet/common/IProtocolRegistrationListener.h"

/**
 * Demonstrates static routing, utilizing the cTopology class.
 */



namespace inet {
namespace blackbox {

Forwarding::Topology Forwarding::topology;
Forwarding::LogicalTopology Forwarding::logicalTopology;
Forwarding::LabelMap Forwarding::labelMap;
Forwarding::LabelMapInverse Forwarding::labelMapInverse;
Forwarding::ConnectedL3Address Forwarding::connectedL3Address;
std::map<L3Address, MacAddress> Forwarding::l3AddressDestination;
uint64_t Forwarding::labelCount = 0;
simsignal_t Forwarding::updateTopology = registerSignal("blackBoxUpdateTopology");

Define_Module(Forwarding);

void Forwarding::initialize(int stage)
{
    if (stage == INITSTAGE_LOCAL) {
        myAddress = MacAddress(getParentModule()->par("address").intValue());
        if (myAddress.isUnspecified()) {
            throw cRuntimeError("Invalid address %s", getParentModule()->par("address").stringValue());
        }
    }
    else if (stage == INITSTAGE_ROUTING_PROTOCOLS) {
        registerProtocol(Protocol::ipv4, gate("outPpp"), gate("inPpp"));
        IInterfaceTable *ift = getModuleFromPar<IInterfaceTable>(par("interfaceTableModule"), this);
        //
        // Brute force approach -- every node does topology discovery on its own,
        // and finds routes to all other nodes independently, at the beginning
        // of the simulation. This could be improved: (1) central routing database,
        // (2) on-demand route calculation
        //
        cTopology *topo = new cTopology("topo");
        std::vector<std::string> nedTypes;
        nedTypes.push_back(getParentModule()->getNedTypeName());
        topo->extractByProperty("networkNode");
        //topo->extractByNedTypeName(nedTypes);
        EV << "cTopology found " << topo->getNumNodes() << " nodes\n";
        auto moduleNode = getContainingNode(this);

        cTopology::Node *thisNode = topo->getNodeFor(moduleNode);
        for (int i = 0 ; i < thisNode->getNumOutLinks(); i++) {
            auto link = thisNode->getLinkOut(i);
            auto remote = link->getRemoteGate();
            //int outIndex = link->getLocalGate()->getIndex();
            auto remoteNode = findContainingNode(remote->getOwnerModule());
            auto props = remoteNode->getProperties();
            auto channel = dynamic_cast<cDatarateChannel *>(link->getLocalGate()->getChannel());

            bool isBlackBox = props && props->getAsBool("backBoxNode");
            if (isBlackBox) {
                auto address = MacAddress(remoteNode->par("address").intValue());
                linkToNeigborTable.push_back(address);
                neigborTable[address] = i;
                // add the link to the topology data.
                auto phyLink = std::make_shared<PhysicakLink>();
                phyLink->capacity = bps(channel->getDatarate());
                cGate *g = this->gate("outGate", i);
                auto modQueue = g->getPathEndGate()->getOwnerModule();
                phyLink->l2Queue = check_and_cast<L2Queue *>(modQueue);
                phyLink->origin = myAddress;
                phyLink->destination = address;
                topology[std::make_pair(phyLink->origin, phyLink->destination)]= phyLink;
            }
            else {
                linkToNeigborTable.push_back(MacAddress::UNSPECIFIED_ADDRESS);
                auto addr = L3AddressResolver().addressOf(remoteNode);
                OutNode node;
                node.addr = addr;
                // get interface
                auto link = thisNode->getLinkIn(i);
                auto gate = link->getLocalGate();
                auto gateMod = gate->getPathEndGate();
                auto mod = gateMod->getOwnerModule();
                auto ppp = dynamic_cast<Ppp*>(mod);
                if (ppp) {
                    auto ni = ift->findInterfaceByInterfaceModule(ppp);
                    node.ifaceId = ni->getInterfaceId();
                    node.iface = ni;
                }
                outNodesTable[i] = node;
            }
        }
        delete topo;
        if (!outNodesTable.empty()) {
            ConnectedAddress v;
            for (const auto &elem: outNodesTable) {
                v.push_back(elem.second.addr);
                l3AddressDestination[elem.second.addr] = myAddress;
            }
            if (!v.empty()) {
                connectedL3Address[myAddress] = v;
            }
        }
        djkVirtual.setRoot(myAddress);
        djkReal.setRoot(myAddress);
        // first, add the labels for the physical

        for (auto &elem : topology) {
            auto l = std::make_shared<LogicalLinksLink>();
            auto label = getNextLabel();
            l->capacity = elem.second->capacity;
            l->destination = elem.second->destination;
            l->origin = elem.second->origin;
            l->label = label;

            l->links.push_back(elem.second);
            l->nodes.push_back(elem.second->origin);
            l->nodes.push_back(elem.second->destination);
            logicalTopology[std::make_pair(l->origin, l->destination)] = l;
            elem.second->l2Queue->addQueue(label, elem.second->capacity);
            labelMap.insert(std::make_pair(label, l));
            labelMapInverse.insert(std::make_pair(std::make_pair(l->origin, l->destination),label));
        }
        moduleNode->subscribe(updateTopology, this);
    }
}

void Forwarding::computeRoutes()
{
    Dijkstra dj;
    for (const auto &elem : logicalTopology) {
        // add the logical topology
        double cost = elem.second->cost;
        dj.addEdge(elem.first.first, elem.first.second, cost, 1);
    }
    dj.setRoot(myAddress);
    dj.run();
}

MacAddress  Forwarding::searchL3Address(const L3Address &addr)
{
    auto it = l3AddressDestination.find(addr);
    if (it != l3AddressDestination.end())
        return it->second;
    return MacAddress::UNSPECIFIED_ADDRESS;
}

void Forwarding::handleMessage(cMessage *msg)
{
    Packet *pkt = check_and_cast<Packet *>(msg);
    // first check the the header
    auto chunk = pkt->peekAtFront<Chunk>();
    auto labelHeader = dynamicPtrCast<const BlackBoxLabel>(chunk);

    if (labelHeader != nullptr) {
        // internal packet. check the labels and forward.
        auto label = labelHeader->getLabels().front();
        // search label in the list
        auto it = labelMap.find(label);
        if (it == labelMap.end())
            throw cRuntimeError("Label not found");
        if (it->second->nodes.back() != myAddress) {
            // last node in the list, it is necessary to remove the label
            auto labelHeaderAux = pkt->removeAtFront<BlackBoxLabel>();
            labelHeaderAux->getLabelsForUpdate().pop_front();
            if (!labelHeaderAux->getLabelsForUpdate().empty()) {
                // last node, check destination address and sends it
                pkt->insertAtFront(labelHeaderAux);
                labelHeader = pkt->peekAtFront<BlackBoxLabel>();
                label = labelHeader->getLabels().front();
                // search label in the list
                it = labelMap.find(label);
                if (it == labelMap.end())
                    throw cRuntimeError("Label not found");
            }
            else {
                // this is the end path, search the final destination
                const auto& ipv4Header = pkt->peekAtFront<Ipv4Header>();
                pkt->addTagIfAbsent<NetworkProtocolInd>()->setNetworkProtocolHeader(ipv4Header);
                pkt->addTagIfAbsent<NetworkProtocolInd>()->setProtocol(&Protocol::ipv4);
                const auto& networkHeader = getNetworkProtocolHeader(pkt);
                auto destAddr = networkHeader->getDestinationAddress();
                auto it = l3AddressDestination.find(destAddr);
                if (it == l3AddressDestination.end()) {
                    throw cRuntimeError("destination not found not found");
                }
                for (const auto &elem :outNodesTable) {
                    if (elem.second.addr == destAddr) {
                        auto ifaceId = elem.second.ifaceId;
                        pkt->addTagIfAbsent<InterfaceReq>()->setInterfaceId(ifaceId);
                        send(pkt, "outPpp");
                        return;
                    }
                }
                // search out link
                throw cRuntimeError("Cannot send the packet ");
            }
        }
        else {
            // this is the end path, search the final destination
            const auto& ipv4Header = pkt->peekAtFront<Ipv4Header>();
            pkt->addTagIfAbsent<NetworkProtocolInd>()->setNetworkProtocolHeader(ipv4Header);
            pkt->addTagIfAbsent<NetworkProtocolInd>()->setProtocol(&Protocol::ipv4);
            const auto& networkHeader = getNetworkProtocolHeader(pkt);
            auto destAddr = networkHeader->getDestinationAddress();
            auto it = l3AddressDestination.find(destAddr);
            if (it == l3AddressDestination.end()) {
                throw cRuntimeError("destination not found not found");
            }
            for (const auto &elem :outNodesTable) {
                if (elem.second.addr == destAddr) {
                    auto ifaceId = elem.second.ifaceId;
                    pkt->addTagIfAbsent<InterfaceReq>()->setInterfaceId(ifaceId);
                    send(pkt, "outPpp");
                    return;
                }
            }
            // search out link
            throw cRuntimeError("Cannot send the packet ");
        }
        // extract next hop and forward.
        for (const auto &elem : it->second->links) {
            // search the correct link
            if (elem->origin == myAddress) {
                auto nextHop = elem->destination;
                for (int i = 0; i < linkToNeigborTable.size(); i++) {
                    if (linkToNeigborTable[i] == nextHop) {
                        send(pkt, "outGate", i);
                        return;
                    }
                }
            }
        }
        throw cRuntimeError("next hop not found");
    }
    else {
        // external packet
        // extract the path, labels, add the header and forward it
        const auto& networkHeader = getNetworkProtocolHeader(pkt);
        auto destAddr = networkHeader->getDestinationAddress();
        auto it = l3AddressDestination.find(destAddr);
        if (it == l3AddressDestination.end()) {
            throw cRuntimeError("destination not found not found");
        }
        // search the path and create the header
        auto header = makeShared<BlackBoxLabel>();
        std::vector<L3Address> pathNode;
        std::vector<L3Address> pathNodePhy;
        djkVirtual.getRoute(L3Address(it->second), pathNode);
        djkReal.getRoute(L3Address(it->second), pathNodePhy);
        if (pathNodePhy.empty())
            throw cRuntimeError("Path doesn't exist");
        if (!pathNode.empty()) {
            // use this path extract the labels

            for (const auto &e : pathNode)
                header->getNodesVirtualForUpdate().push_back(e.toMac());

            for (int i = 0; i < pathNode.size()-1; i++) {
                auto aux = std::make_pair(pathNode[i].toMac(), pathNode[i+1].toMac());
                if (labelMapInverse.empty()) {
                    for (const auto &e : labelMap) {
                        auto aux = std::make_pair(e.second->origin, e.second->destination);
                        labelMapInverse[aux] = e.first;
                    }
                }
                auto itAux = labelMapInverse.find(aux);
                if (itAux == labelMapInverse.end())
                    throw cRuntimeError("Label not found");
                header->getLabelsForUpdate().push_back(itAux->second);
            }
        }
    }
    throw cRuntimeError("Unable to process");
}

void Forwarding::receiveSignal(cComponent *source, simsignal_t signal, cObject *object, cObject *details)
{
    if (updateTopology == signal) {
        // changes, force to recompute and modify the logical network
    }
}

}
}
