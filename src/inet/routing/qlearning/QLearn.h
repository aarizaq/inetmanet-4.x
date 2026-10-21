//
// Copyright (C) 2024
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_QLEARN_H
#define __INET_QLEARN_H

#include <map>
#include <set>

#include "inet/common/ModuleRefByPar.h"
#include "inet/networklayer/contract/IInterfaceTable.h"
#include "inet/networklayer/contract/IL3AddressType.h"
#include "inet/networklayer/contract/INetfilter.h"
#include "inet/networklayer/contract/IRoutingTable.h"
#include "inet/routing/base/RoutingProtocolBase.h"
#include "inet/routing/qlearning/QLearningPacket_m.h"
#include "inet/transportlayer/contract/udp/UdpSocket.h"

namespace inet {
namespace qlearning {

/**
 * Q-Learning Routing Protocol Implementation
 * 
 * This class implements a reinforcement learning based routing protocol
 * where each node learns optimal routes through experience using Q-learning.
 */
class INET_API QLearn : public RoutingProtocolBase, public NetfilterBase::HookBase, public UdpSocket::ICallback, public cListener
{
  protected:
    // Q-table entry: maps (destination, next_hop) to Q-value
    struct QTableEntry {
        double qValue;
        simtime_t lastUpdate;
        unsigned int hopCount;
        
        QTableEntry() : qValue(0.0), lastUpdate(SIMTIME_ZERO), hopCount(0) {}
        QTableEntry(double q, simtime_t t, unsigned int hops) 
            : qValue(q), lastUpdate(t), hopCount(hops) {}
    };
    
    // Neighbor information
    struct NeighborInfo {
        L3Address address;
        simtime_t lastSeen;
        bool isActive;
        
        NeighborInfo() : lastSeen(SIMTIME_ZERO), isActive(false) {}
        NeighborInfo(const L3Address& addr, simtime_t t) 
            : address(addr), lastSeen(t), isActive(true) {}
    };
    
    // Q-table: outer map key is destination, inner map key is next hop
    typedef std::map<L3Address, std::map<L3Address, QTableEntry>> QTable;
    
    // Context
    const IL3AddressType *addressType = nullptr;
    
    // Environment
    cModule *host = nullptr;
    ModuleRefByPar<IRoutingTable> routingTable;
    ModuleRefByPar<IInterfaceTable> interfaceTable;
    ModuleRefByPar<INetfilter> networkProtocol;
    UdpSocket socket;
    bool usingIpv6 = false;
    
    // Q-Learning parameters
    double alpha;              // Learning rate
    double gamma;              // Discount factor
    double epsilon;            // Exploration rate
    double initialQValue;      // Initial Q-value
    
    // Protocol parameters
    unsigned int qlearnUDPPort;
    simtime_t helloInterval;
    simtime_t routeTimeout;
    simtime_t qUpdateInterval;
    simtime_t maxJitter;
    cPar *jitterPar = nullptr;
    
    // Reward/penalty values
    double successReward;
    double failurePenalty;
    double hopPenalty;
    
    // Data structures
    QTable qTable;                              // The Q-learning table
    std::map<L3Address, NeighborInfo> neighbors;  // Active neighbors
    unsigned int sequenceNumber;                 // Sequence number for packets
    
    // Timers
    cMessage *helloTimer = nullptr;
    cMessage *qUpdateTimer = nullptr;
    cMessage *neighborCheckTimer = nullptr;
    
    // Statistics
    unsigned int numPacketsSent;
    unsigned int numPacketsReceived;
    unsigned int numPacketsForwarded;
    unsigned int numPacketsDropped;
    
  protected:
    virtual int numInitStages() const override { return NUM_INIT_STAGES; }
    virtual void initialize(int stage) override;
    virtual void handleMessageWhenUp(cMessage *msg) override;
    virtual void finish() override;
    
    // Lifecycle
    virtual void handleStartOperation(LifecycleOperation *operation) override;
    virtual void handleStopOperation(LifecycleOperation *operation) override;
    virtual void handleCrashOperation(LifecycleOperation *operation) override;
    
    // Message handlers
    virtual void processPacket(Packet *packet);
    virtual void handleHelloPacket(Packet *packet);
    virtual void handleUpdatePacket(Packet *packet);
    
    // Timer handlers
    virtual void sendHelloMessage();
    virtual void performQUpdate();
    virtual void checkNeighbors();
    
    // Q-Learning methods
    virtual double getQValue(const L3Address& destination, const L3Address& nextHop);
    virtual void updateQValue(const L3Address& destination, const L3Address& nextHop, 
                             double reward, double maxNextQ);
    virtual L3Address selectNextHop(const L3Address& destination);
    virtual double getMaxQValue(const L3Address& destination);
    
    // Routing methods
    virtual void updateRouteTable(const L3Address& destination, const L3Address& nextHop, unsigned int hopCount);
    virtual void addNeighbor(const L3Address& neighbor);
    virtual void removeNeighbor(const L3Address& neighbor);
    virtual bool isNeighbor(const L3Address& address);
    
    // NetFilter hook
    virtual Result datagramPreRoutingHook(Packet *datagram) override;
    virtual Result datagramForwardHook(Packet *datagram) override;
    virtual Result datagramPostRoutingHook(Packet *datagram) override;
    virtual Result datagramLocalInHook(Packet *datagram) override;
    virtual Result datagramLocalOutHook(Packet *datagram) override;
    
    // UdpSocket callback
    virtual void socketDataArrived(UdpSocket *socket, Packet *packet) override;
    virtual void socketErrorArrived(UdpSocket *socket, Indication *indication) override;
    virtual void socketClosed(UdpSocket *socket) override;
    
    // Signal handling
    virtual void receiveSignal(cComponent *source, simsignal_t signalID, cObject *obj, cObject *details) override;
    
    // Helper methods
    virtual L3Address getSelfIPAddress() const;
    virtual void sendPacket(Packet *packet, const L3Address& destAddr, int ttl = -1);
    
  public:
    QLearn();
    virtual ~QLearn();
};

} // namespace qlearning
} // namespace inet

#endif
