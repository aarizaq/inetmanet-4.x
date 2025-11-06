//
// Copyright (C) 2024
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/routing/qlearning/QLearn.h"

#include "inet/common/IProtocolRegistrationListener.h"
#include "inet/common/ModuleAccess.h"
#include "inet/common/ProtocolTag_m.h"
#include "inet/common/packet/Packet.h"
#include "inet/linklayer/common/InterfaceTag_m.h"
#include "inet/networklayer/common/HopLimitTag_m.h"
#include "inet/networklayer/common/L3AddressResolver.h"
#include "inet/networklayer/common/L3AddressTag_m.h"
#include "inet/networklayer/common/L3Tools.h"
#include "inet/networklayer/ipv4/Ipv4Header_m.h"
#include "inet/networklayer/ipv4/Ipv4Route.h"
#include "inet/transportlayer/common/L4PortTag_m.h"
#include "inet/transportlayer/contract/udp/UdpCommand_m.h"

namespace inet {
namespace qlearning {

Define_Module(QLearn);

simsignal_t linkBrokenSignal = cComponent::registerSignal("linkBroken");

QLearn::QLearn()
{
    sequenceNumber = 0;
    numPacketsSent = 0;
    numPacketsReceived = 0;
    numPacketsForwarded = 0;
    numPacketsDropped = 0;
}

QLearn::~QLearn()
{
    cancelAndDelete(helloTimer);
    cancelAndDelete(qUpdateTimer);
    cancelAndDelete(neighborCheckTimer);
}

void QLearn::initialize(int stage)
{
    if (stage == INITSTAGE_ROUTING_PROTOCOLS)
        addressType = getSelfIPAddress().getAddressType();

    RoutingProtocolBase::initialize(stage);

    if (stage == INITSTAGE_LOCAL) {
        host = getContainingNode(this);
        routingTable.reference(this, "routingTableModule", true);
        interfaceTable.reference(this, "interfaceTableModule", true);
        networkProtocol.reference(this, "networkProtocolModule", true);

        // Q-Learning parameters
        alpha = par("learningRate");
        gamma = par("discountFactor");
        epsilon = par("explorationRate");
        initialQValue = par("initialQValue");

        // Protocol parameters
        qlearnUDPPort = par("udpPort");
        helloInterval = par("helloInterval");
        routeTimeout = par("routeTimeout");
        qUpdateInterval = par("qUpdateInterval");
        maxJitter = par("maxJitter");
        jitterPar = &par("jitter");

        // Reward/penalty values
        successReward = par("successReward");
        failurePenalty = par("failurePenalty");
        hopPenalty = par("hopPenalty");

        // Initialize timers
        helloTimer = new cMessage("HelloTimer");
        qUpdateTimer = new cMessage("QUpdateTimer");
        neighborCheckTimer = new cMessage("NeighborCheckTimer");
        
        sequenceNumber = 0;
        usingIpv6 = false;
    }
    else if (stage == INITSTAGE_ROUTING_PROTOCOLS) {
        networkProtocol->registerHook(0, this);
        host->subscribe(linkBrokenSignal, this);
        
        // Determine IP version
        try {
            usingIpv6 = (routingTable->getRouterIdAsGeneric().getType() == L3Address::IPv6);
        } catch (...) {
            usingIpv6 = false;
        }
    }
}

void QLearn::handleStartOperation(LifecycleOperation *operation)
{
    socket.setOutputGate(gate("socketOut"));
    socket.bind(qlearnUDPPort);
    socket.setBroadcast(true);
    socket.setCallback(this);

    // Schedule initial timers
    scheduleAfter(uniform(0, par("maxJitter").doubleValue()), helloTimer);
    scheduleAfter(qUpdateInterval, qUpdateTimer);
    scheduleAfter(routeTimeout / 2, neighborCheckTimer);
}

void QLearn::handleStopOperation(LifecycleOperation *operation)
{
    cancelEvent(helloTimer);
    cancelEvent(qUpdateTimer);
    cancelEvent(neighborCheckTimer);
    socket.close();
    
    qTable.clear();
    neighbors.clear();
}

void QLearn::handleCrashOperation(LifecycleOperation *operation)
{
    cancelEvent(helloTimer);
    cancelEvent(qUpdateTimer);
    cancelEvent(neighborCheckTimer);
    socket.destroy();
    
    qTable.clear();
    neighbors.clear();
}

void QLearn::handleMessageWhenUp(cMessage *msg)
{
    if (msg->isSelfMessage()) {
        if (msg == helloTimer) {
            sendHelloMessage();
            scheduleAfter(helloInterval, helloTimer);
        }
        else if (msg == qUpdateTimer) {
            performQUpdate();
            scheduleAfter(qUpdateInterval, qUpdateTimer);
        }
        else if (msg == neighborCheckTimer) {
            checkNeighbors();
            scheduleAfter(routeTimeout / 2, neighborCheckTimer);
        }
    }
    else {
        socket.processMessage(msg);
    }
}

void QLearn::finish()
{
    RoutingProtocolBase::finish();
    
    // Record statistics
    recordScalar("numPacketsSent", numPacketsSent);
    recordScalar("numPacketsReceived", numPacketsReceived);
    recordScalar("numPacketsForwarded", numPacketsForwarded);
    recordScalar("numPacketsDropped", numPacketsDropped);
    recordScalar("qTableSize", (long)qTable.size());
    recordScalar("numNeighbors", (long)neighbors.size());
}

void QLearn::sendHelloMessage()
{
    L3Address selfAddr = getSelfIPAddress();
    if (selfAddr.isUnspecified())
        return;

    auto hello = makeShared<QLearningHello>();
    hello->setPacketType(QLEARN_HELLO);
    hello->setSourceAddr(selfAddr.toIpv4());
    hello->setSequenceNumber(sequenceNumber++);
    hello->setQValue(0.0);
    hello->setChunkLength(B(17));  // 1 + 4 + 4 + 8 = 17 bytes

    auto packet = new Packet("QLearningHello");
    packet->insertAtBack(hello);

    // Broadcast to all neighbors
    L3Address destAddr = addressType->getBroadcastAddress();
    sendPacket(packet, destAddr, 1);
    
    numPacketsSent++;
}

void QLearn::performQUpdate()
{
    // Update Q-values based on route quality
    simtime_t now = simTime();
    
    // Age old entries
    for (auto& destEntry : qTable) {
        for (auto& nhEntry : destEntry.second) {
            if (now - nhEntry.second.lastUpdate > routeTimeout) {
                // Apply decay to old Q-values
                nhEntry.second.qValue *= 0.9;
            }
        }
    }
}

void QLearn::checkNeighbors()
{
    simtime_t now = simTime();
    std::vector<L3Address> toRemove;
    
    // Check for inactive neighbors
    for (auto& neighbor : neighbors) {
        if (now - neighbor.second.lastSeen > routeTimeout) {
            neighbor.second.isActive = false;
            toRemove.push_back(neighbor.first);
        }
    }
    
    // Remove inactive neighbors
    for (const auto& addr : toRemove) {
        removeNeighbor(addr);
    }
}

void QLearn::processPacket(Packet *packet)
{
    auto qlearnPacket = packet->peekAtFront<QLearningHello>();
    
    if (qlearnPacket->getPacketType() == QLEARN_HELLO) {
        handleHelloPacket(packet);
    }
    else if (qlearnPacket->getPacketType() == QLEARN_UPDATE) {
        handleUpdatePacket(packet);
    }
    
    delete packet;
}

void QLearn::handleHelloPacket(Packet *packet)
{
    const auto& hello = packet->peekAtFront<QLearningHello>();
    L3Address sourceAddr = L3Address(hello->getSourceAddr());
    
    // Update neighbor information
    addNeighbor(sourceAddr);
    
    numPacketsReceived++;
}

void QLearn::handleUpdatePacket(Packet *packet)
{
    const auto& update = packet->peekAtFront<QLearningUpdate>();
    L3Address sourceAddr = L3Address(update->getSourceAddr());
    L3Address destAddr = L3Address(update->getDestAddr());
    
    // Update Q-table based on received information
    double receivedQValue = update->getQValue();
    unsigned int hopCount = update->getHopCount();
    
    // Update Q-value for this route
    QTableEntry entry(receivedQValue, simTime(), hopCount);
    qTable[destAddr][sourceAddr] = entry;
    
    numPacketsReceived++;
}

double QLearn::getQValue(const L3Address& destination, const L3Address& nextHop)
{
    auto destIt = qTable.find(destination);
    if (destIt != qTable.end()) {
        auto nhIt = destIt->second.find(nextHop);
        if (nhIt != destIt->second.end()) {
            return nhIt->second.qValue;
        }
    }
    return initialQValue;
}

void QLearn::updateQValue(const L3Address& destination, const L3Address& nextHop,
                          double reward, double maxNextQ)
{
    double oldQ = getQValue(destination, nextHop);
    double newQ = oldQ + alpha * (reward + gamma * maxNextQ - oldQ);
    
    QTableEntry entry(newQ, simTime(), 0);
    qTable[destination][nextHop] = entry;
}

L3Address QLearn::selectNextHop(const L3Address& destination)
{
    // Epsilon-greedy selection
    if (uniform(0, 1) < epsilon) {
        // Exploration: random neighbor
        if (!neighbors.empty()) {
            int idx = intuniform(0, neighbors.size() - 1);
            auto it = neighbors.begin();
            std::advance(it, idx);
            return it->first;
        }
    }
    else {
        // Exploitation: best Q-value
        double maxQ = -INFINITY;
        L3Address bestNextHop;
        
        auto destIt = qTable.find(destination);
        if (destIt != qTable.end()) {
            for (const auto& nhEntry : destIt->second) {
                if (isNeighbor(nhEntry.first) && nhEntry.second.qValue > maxQ) {
                    maxQ = nhEntry.second.qValue;
                    bestNextHop = nhEntry.first;
                }
            }
        }
        
        if (!bestNextHop.isUnspecified()) {
            return bestNextHop;
        }
    }
    
    return L3Address();
}

double QLearn::getMaxQValue(const L3Address& destination)
{
    double maxQ = initialQValue;
    
    auto destIt = qTable.find(destination);
    if (destIt != qTable.end()) {
        for (const auto& nhEntry : destIt->second) {
            if (isNeighbor(nhEntry.first) && nhEntry.second.qValue > maxQ) {
                maxQ = nhEntry.second.qValue;
            }
        }
    }
    
    return maxQ;
}

void QLearn::updateRouteTable(const L3Address& destination, const L3Address& nextHop, unsigned int hopCount)
{
    if (destination.isUnspecified() || nextHop.isUnspecified())
        return;
        
    // This is a simplified version - in a full implementation, you would
    // update the actual routing table here
    EV_INFO << "Route update: dest=" << destination << " nextHop=" << nextHop 
            << " hops=" << hopCount << endl;
}

void QLearn::addNeighbor(const L3Address& neighbor)
{
    if (neighbor.isUnspecified() || neighbor == getSelfIPAddress())
        return;
        
    neighbors[neighbor] = NeighborInfo(neighbor, simTime());
    EV_DEBUG << "Added neighbor: " << neighbor << endl;
}

void QLearn::removeNeighbor(const L3Address& neighbor)
{
    neighbors.erase(neighbor);
    
    // Remove from Q-table
    for (auto& destEntry : qTable) {
        destEntry.second.erase(neighbor);
    }
    
    EV_DEBUG << "Removed neighbor: " << neighbor << endl;
}

bool QLearn::isNeighbor(const L3Address& address)
{
    auto it = neighbors.find(address);
    return (it != neighbors.end() && it->second.isActive);
}

// NetFilter hooks
INetfilter::IHook::Result QLearn::datagramPreRoutingHook(Packet *datagram)
{
    return ACCEPT;
}

INetfilter::IHook::Result QLearn::datagramForwardHook(Packet *datagram)
{
    numPacketsForwarded++;
    return ACCEPT;
}

INetfilter::IHook::Result QLearn::datagramPostRoutingHook(Packet *datagram)
{
    return ACCEPT;
}

INetfilter::IHook::Result QLearn::datagramLocalInHook(Packet *datagram)
{
    return ACCEPT;
}

INetfilter::IHook::Result QLearn::datagramLocalOutHook(Packet *datagram)
{
    return ACCEPT;
}

// UdpSocket callbacks
void QLearn::socketDataArrived(UdpSocket *socket, Packet *packet)
{
    processPacket(packet);
}

void QLearn::socketErrorArrived(UdpSocket *socket, Indication *indication)
{
    EV_WARN << "Socket error arrived" << endl;
    delete indication;
}

void QLearn::socketClosed(UdpSocket *socket)
{
    EV_INFO << "Socket closed" << endl;
}

void QLearn::receiveSignal(cComponent *source, simsignal_t signalID, cObject *obj, cObject *details)
{
    if (signalID == linkBrokenSignal) {
        EV_INFO << "Link broken signal received" << endl;
        // Handle link breaks - could update Q-values with penalties
    }
}

L3Address QLearn::getSelfIPAddress() const
{
    return routingTable->getRouterIdAsGeneric();
}

void QLearn::sendPacket(Packet *packet, const L3Address& destAddr, int ttl)
{
    ASSERT(packet != nullptr);
    
    packet->addTagIfAbsent<PacketProtocolTag>()->setProtocol(&Protocol::manet);
    packet->addTagIfAbsent<DispatchProtocolReq>()->setProtocol(addressType->getNetworkProtocol());
    packet->addTagIfAbsent<L3AddressReq>()->setDestAddress(destAddr);
    
    if (ttl != -1)
        packet->addTagIfAbsent<HopLimitReq>()->setHopLimit(ttl);
    
    auto udpHeader = makeShared<UdpHeader>();
    udpHeader->setSourcePort(qlearnUDPPort);
    udpHeader->setDestinationPort(qlearnUDPPort);
    udpHeader->setCrcMode(CRC_COMPUTED);
    insertTransportProtocolHeader(packet, Protocol::udp, udpHeader);
    
    socket.send(packet);
}

} // namespace qlearning
} // namespace inet
