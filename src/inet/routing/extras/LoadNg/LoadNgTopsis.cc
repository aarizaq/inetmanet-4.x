//
// Copyright (C) 2014 OpenSim Ltd.
// Author: Benjamin Seregi
// Copyright (C) 2019 Universidad de Malaga
// Author: Alfonso Ariza
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


#include <algorithm>
#include <math.h>
#include "inet/common/IProtocolRegistrationListener.h"
#include "inet/common/ModuleAccess.h"
#include "inet/common/ProtocolTag_m.h"
#include "inet/common/packet/Packet.h"
#include "inet/linklayer/common/InterfaceTag_m.h"
#include "inet/networklayer/common/HopLimitTag_m.h"
#include "inet/networklayer/common/L3AddressTag_m.h"
#include "inet/networklayer/common/L3Tools.h"
#include "inet/networklayer/ipv4/IcmpHeader.h"
#include "inet/networklayer/ipv4/Ipv4Header_m.h"
#include "inet/networklayer/ipv4/Ipv4Route.h"
#include "inet/routing/extras/LoadNg/LoadNgTopsis.h"
#include "inet/routing/extras/LoadNg/DeepFirstForwardTag_m.h"
#include "inet/linklayer/common/MacAddressTag_m.h"
#include "inet/networklayer/common/L3Tools.h"
#include "inet/common/packet/dissector/PacketDissector.h"
#include "inet/mobility/single/RandomWaypointMobility2.h"
#include "inet/mobility/base/StationaryMobilityBase.h"

// DONE: actualize routes using hello information,
// DONE: Fill the routing tables using the routes computes by Dijkstra
// DONE: Modify the link layer to force that a percentage of links could be only unidirectional
// DONE: Compute k-shortest paths for using with DFF
// DONE: Measure ETX, DONE: include fields

// TODO: Calculate the route to the sink using Hellos, TODO: Propagate distance to root, DONE: define TLV for this
// DONE: Solve unidirectional problem, find a route when not bidirectional path exist, double RREQ.
// TODO: the protocol LoadNg must handle the packets in this case, the final destination address must be included in a header and the Ip address must be broadcast.
// TODO: Review the RRER handle packet.

// DONE: when the entry is recorded in the routing data base check that is consistent with the data stored in topsis.

namespace inet {
namespace inetmanet {

simsignal_t LoadNgTopsis::recomputeSignal = registerSignal("LoadNgRecomputeSignal");
simsignal_t LoadNgTopsis::nextRecSignal = registerSignal("LoadNgnextRecSignal");
simtime_t LoadNgTopsis::nextRecTime;



Define_Module(LoadNgTopsis);


inline std::ostream& operator<<(std::ostream& out, const NeigborElement& d)
{
    double metric = d.metricToNeig;
    out << "Bidir="<< d.isBidirectional << " Pending " << d.pendingConfirmation << " Metric= "<< metric << " Neig list ";
    for (auto elem : d.listNeigbours)
    {
        int metric = elem.second.metric;
        out << "add = " << elem.first << " bidir = " <<  elem.second.isBidirectional << "Pending " << elem.second.pendingConfirmation
        << " metric = "<< metric << " <> ";
    }
    return out;
}

inline std::ostream& operator<<(std::ostream& out, const TopsisLoadNgNeigbors::Cost& d)
{
    out << "Addr: " << d.address << " Next: " << d.nextAddress;
    out << " Etx: " << d.etx << " Delay: " << d.delay << " powerRec: " << d.recPower << " Neig: " << d.neighbors;
    out << " Hops: " << d.numHops << " commonAddr: " << d.commonAddress;
    return out;
}

inline std::ostream& operator<<(std::ostream& out, const int64_t& d)
{
    out << "Seq num = " << std::to_string(d);
    return out;
}


void LoadNgTopsis::initialize(int stage)
{
    if (stage == INITSTAGE_ROUTING_PROTOCOLS)
        addressType = getSelfIPAddress().getAddressType();  // needed for handleStartOperation()

    RoutingProtocolBase::initialize(stage);

    if (stage == INITSTAGE_LOCAL) {
        lastBroadcastTime = SIMTIME_ZERO;
        rebootTime = SIMTIME_ZERO;
        rreqId = sequenceNum = 0;
        rreqCount = rerrCount = 0;
        host = getContainingNode(this);
        routingTable.reference(this, "routingTableModule", true);
        interfaceTable.reference(this, "interfaceTableModule", true);
        networkProtocol.reference(this, "networkProtocolModule", true);


        aodvUDPPort = par("udpPort");
        useHelloMessages = par("useHelloMessages");
        activeRouteTimeout = par("activeRouteTimeout");
        helloInterval = par("helloInterval");
        allowedHelloLoss = par("allowedHelloLoss");
        netDiameter = par("netDiameter");
        nodeTraversalTime = par("nodeTraversalTime");
        rerrRatelimit = par("rerrRatelimit");
        rreqRetries = par("rreqRetries");
        rreqRatelimit = par("rreqRatelimit");
        timeoutBuffer = par("timeoutBuffer");
        ttlStart = par("ttlStart");
        ttlIncrement = par("ttlIncrement");
        ttlThreshold = par("ttlThreshold");
        localAddTTL = par("localAddTTL");
        jitterPar = &par("jitter");
        periodicJitter = &par("periodicJitter");
        periodicRreqPropagation = par("periodicRreqPropagation");

        myRouteTimeout = par("myRouteTimeout");
        deletePeriod = par("deletePeriod");
        blacklistTimeout = par("blacklistTimeout");
        netTraversalTime = par("netTraversalTime");
        nextHopWait = par("nextHopWait");
        pathDiscoveryTime = par("pathDiscoveryTime");
        expungeTimer = new cMessage("ExpungeTimer");
        counterTimer = new cMessage("CounterTimer");
        rrepAckTimer = new cMessage("RrepAckTimer");
        blacklistTimer = new cMessage("BlackListTimer");
        recomputeTopsis = new cMessage("RecomputeTopsisTimer");
        if (useHelloMessages)
            helloMsgTimer = new cMessage("HelloMsgTimer");

        measureEtx = par("measureEtx");

        minHelloInterval = par("minHelloInterval");
        maxHelloInterval = par("maxHelloInterval");
        threeMessagesMode = par("threeMessagesMode");

        WATCH_MAP(neighbors);
        WATCH(lastTopsisCompute);
        WATCH_MAP(topsisResults);
        WATCH_MAP(seqNumbers);

        if (par("checkStationary")) {
            auto node = getContainingNode(this);
            auto mob = node->getSubmodule("mobility");
            auto rw = dynamic_cast<RandomWaypointMobility2 *>(mob);
            auto st = dynamic_cast<StationaryMobilityBase *>(mob);

            if ((rw != nullptr && strcmp(rw->par("Active").stringValue(), "OFF") == 0) || st != nullptr)  {
                stationary = true;
            }
            etxPenalty = par("etxPenalty");
            if (etxPenalty < 1) etxPenalty = 1;
        }
        else if (par("mobileNodesNoPropagateRreq")) {
            auto node = getContainingNode(this);
            auto mob = node->getSubmodule("mobility");
            auto rw = dynamic_cast<RandomWaypointMobility2 *>(mob);
            if (strcmp(rw->par("Active").stringValue(), "ON") == 0)  {
                noPropagateRreq = true;
            }
        }

    }
    else if (stage == INITSTAGE_ROUTING_PROTOCOLS) {
        registerProtocol(Protocol::manet, gate("ipOut"), gate("ipIn"));
        networkProtocol->registerHook(0, this);
        host->subscribe(linkBrokenSignal, this);
        cModule *en = host->getSubmodule("energyStorage");
        if (en != nullptr)
            energyStorage = check_and_cast<power::IEpEnergyStorage *>(host->getSubmodule("energyStorage"));
    }
}

void LoadNgTopsis::actualizeDelayed(Packet *pkt) {
    const auto header = pkt->peekAtFront<FieldsChunk>();
    const auto rreqSt = dynamicPtrCast<const Rreq>(header);
    if (rreqSt) {
        bool sendRrep = rreqSt->getBuild() && rreqSt->getRrepRequest();
        // check if better metric has been received.
        IRoute * route = routingTable->findBestMatchingRoute(rreqSt->getOriginatorAddr());
        LoadNgRouteData *routeData = route ? dynamic_cast<LoadNgRouteData *>(route->getProtocolData()) : nullptr;
        double metric = -1;
        if (routeData) {
            auto rreq = pkt->removeAtFront<Rreq>();
            if (getMetric(rreq, metric)) {
                if (routeData->getMetric() < metric || (routeData->getMetric() == metric && (int)rreqSt->getHopCount() < route->getMetric())) {
                    setMetric(rreq, metric);
                    rreq->setHopCount(route->getMetric());
                }
            }
            pkt->insertAtFront(rreq);
        }

        if (sendRrep) {
            auto rreq = pkt->removeAtFront<Rreq>();
            auto rrep = createRREP(rreq);
            rrep->setHopLimit(par("maxHopLimit"));
            // send the rrep with a small jitter to avoid collisions.
            sendRREP(rrep, rreq->getOriginatorAddr(), 255, -1, *jitterPar);
            pkt->insertAtFront(rreq);
        }
    }
}

void LoadNgTopsis::handleMessageWhenUp(cMessage *msg)
{
    checkNeigList(msg);
    if (msg->isSelfMessage()) {
        if (msg->isScheduled())
            cancelEvent(msg);
        if (recomputeTimer == msg) {
            regenerateLinkMap();
            return;
        }
        if (auto waitForRrep = dynamic_cast<WaitForRrep *>(msg))
            handleWaitForRREP(waitForRrep);
        else if (msg == helloMsgTimer)
            sendHelloMessagesIfNeeded();
        else if (msg == periodicRreqTimer)
            periodicRreqTigger();
        else if (msg == expungeTimer)
            expungeRoutes();
        else if (msg == counterTimer) {
            rreqCount = rerrCount = 0;
            scheduleAt(simTime() + 1, counterTimer);
        }
        else if (msg == rrepAckTimer)
            handleRREPACKTimer();
        else if (msg == blacklistTimer)
            handleBlackListTimer();
        else if (msg->getKind() == KIND_DELAYEDSEND) {
            auto timer = check_and_cast<PacketHolderMessage*>(msg);
            auto pkt = timer->removeOwnedPacket();
            actualizeDelayed(pkt);
            send(pkt, "ipOut");
            auto it = pendingSend.find(timer);
            if (it != pendingSend.end())
                pendingSend.erase(it);
            delete timer;
        }
        else if (msg->getKind() == KIND_SMALLDELAY) {
            auto timer = check_and_cast<SmallDelayPacket*>(msg);
            auto target = timer->getDestAddress();
            auto lt = targetAddressToDelayedPackets.lower_bound(target);
            auto ut = targetAddressToDelayedPackets.upper_bound(target);
            // reinject the delayed datagrams
            for (auto it = lt; it != ut; it++) {
                Packet *datagram = it->second;
                const auto& networkHeader = getNetworkProtocolHeader(datagram);
                EV_DETAIL << "Sending queued datagram: source "
                                 << networkHeader->getSourceAddress()
                                 << ", destination "
                                 << networkHeader->getDestinationAddress()
                                 << endl;
                networkProtocol->reinjectQueuedDatagram(datagram);
            }
            // clear the multimap
            targetAddressToDelayedPackets.erase(lt, ut);
            delete msg;
        }
        // we have a route for the destination, thus we must cancel the WaitForRREPTimer events

        else
            throw cRuntimeError("Unknown self message");
    }
    else {

        auto packet = check_and_cast<Packet *>(msg);
        auto protocol = packet->getTag<PacketProtocolTag>()->getProtocol();
        auto signalPowerInd = packet->findTag<SignalPowerInd>();
        auto snirInd = packet->findTag<SnirInd>();

        if (protocol == &Protocol::icmpv4) {
            //auto icmpPacket = mspcheck_and_cast<IcmpHeader *>(msg);
            auto icmpPacket = packet->peekAtFront<IcmpHeader>();
            // ICMP packet arrived, dropped
            delete msg;
        }
        else if (protocol == &Protocol::icmpv6) {
            auto icmpPacket = packet->peekAtFront<IcmpHeader>();
            // ICMP packet arrived, dropped
            delete msg;
        }
        else if (true) {  //FIXME protocol == ???
            Packet *udpPacket = check_and_cast<Packet *>(msg);
            //udpPacket->popAtFront<UdpHeader>();
            L3Address sourceAddr = udpPacket->getTag<L3AddressInd>()->getSrcAddress();
            L3Address nextHopAddr = udpPacket->getTag<L3AddressInd>()->getDestAddress();
            MacAddress macSenderAddr = udpPacket->getTag<MacAddressInd>()->getSrcAddress();
            unsigned int arrivalPacketTTL = udpPacket->getTag<HopLimitInd>()->getHopLimit() - 1;

            const auto& ctrlPacket = udpPacket->popAtFront<LoadNgControlPacket>();
//            ctrlPacket->copyTags(*msg);

            switch (ctrlPacket->getPacketType()) {
                case RREQ:
                    handleRREQ(dynamicPtrCast<Rreq>(ctrlPacket->dupShared()), sourceAddr, nextHopAddr, arrivalPacketTTL, macSenderAddr);
                    break;

                case RREP:
                    handleRREP(dynamicPtrCast<Rrep>(ctrlPacket->dupShared()), sourceAddr, macSenderAddr);
                    break;

                case RERR:
                    handleRERR(dynamicPtrCast<const Rerr>(ctrlPacket), sourceAddr);
                    break;

                case RREPACK:
                    handleRREPACK(dynamicPtrCast<const RrepAck>(ctrlPacket), sourceAddr);
                    break;

                case HELLO:
                    handleHelloMessage(dynamicPtrCast<const Hello>(ctrlPacket), signalPowerInd, snirInd, macSenderAddr);
                    break;

                default:
                    throw cRuntimeError("AODV Control Packet arrived with undefined packet type: %d", ctrlPacket->getPacketType());
            }
            delete udpPacket;
        }
    }
}

INetfilter::IHook::Result LoadNgTopsis::ensureRouteForDatagram(Packet *datagram)
{
    const auto& networkHeader = getNetworkProtocolHeader(datagram);
    const L3Address& destAddr = networkHeader->getDestinationAddress();
    const L3Address& sourceAddr = networkHeader->getSourceAddress();

    if (destAddr.isBroadcast() || routingTable->isLocalAddress(destAddr) || destAddr.isMulticast())
        return ACCEPT;
    else {
        EV_INFO << "Finding route for source " << sourceAddr << " with destination " << destAddr << endl;
        // refresh route to origin

        if (!sourceAddr.isUnspecified() && !routingTable->isLocalAddress(sourceAddr)) {
            IRoute *routeOrigin = routingTable->findBestMatchingRoute(sourceAddr);
            if (routeOrigin) {
                std::vector<L3Address> pathNode;

                auto macSender = datagram->getTag<MacAddressInd>()->getSrcAddress();
                auto it = macToIpAddress.find(macSender);
                if (it != macToIpAddress.end()) {
                    auto senderAddr = it->second;
                    std::vector<L3Address> pathNode;
                    //if (dijkstra && dijkstra->getRoute(sourceAddr, pathNode)) {
                    if (false) { // check topsis route
                        // adapt the route
                        if (pathNode[1] != routeOrigin->getNextHopAsGeneric()) {
                            routeOrigin->setNextHop(pathNode[1]);
                        }
                    }
                    else {
                        auto itNeig = neighbors.find(senderAddr);
                        if (itNeig != neighbors.end() && itNeig->second.isBidirectional && !itNeig->second.pendingConfirmation) {
                            if (routeOrigin->getNextHopAsGeneric() != senderAddr) {
                                routeOrigin->setNextHop(senderAddr);
                            }
                        }
                    }
                    updateValidRouteLifeTime(senderAddr, simTime() + activeRouteTimeout);
                    updateValidRouteLifeTime(sourceAddr, simTime() + activeRouteTimeout);
                }
            }
        }

        IRoute *route = routingTable->findBestMatchingRoute(destAddr);
        LoadNgRouteData *routeData = route ? dynamic_cast<LoadNgRouteData *>(route->getProtocolData()) : nullptr;
        bool isActive = routeData && routeData->isActive();
        if (isActive && !route->getNextHopAsGeneric().isUnspecified()) {
            EV_INFO << "Active route found: " << route << endl;

            // Each time a route is used to forward a data packet, its Active Route
            // Lifetime field of the source, destination and the next hop on the
            // path to the destination is updated to be no less than the current
            // time plus ACTIVE_ROUTE_TIMEOUT.

            updateValidRouteLifeTime(destAddr, simTime() + activeRouteTimeout);
            updateValidRouteLifeTime(route->getNextHopAsGeneric(), simTime() + activeRouteTimeout);

            auto it = specialPaths.find(destAddr);
            if (it != specialPaths.end())
                it->second.lifetime = simTime() + activeRouteTimeout;

            return ACCEPT;
        }
        else {
            bool isInactive = routeData && !routeData->isActive();

            // TODO: check in the neighbor list if you have an alternative

            if (!sourceAddr.isUnspecified() && !routingTable->isLocalAddress(sourceAddr)) {
                sendRERRWhenNoRouteToForward(destAddr, sourceAddr);
                return DROP;
            }

            auto it = specialPaths.find(destAddr);
            if (it != specialPaths.end()) {

                if (it->second.lifetime < simTime())
                    specialPaths.erase(it);
                else {
                    // force special RREP
                    it->second.lifetime = simTime() + activeRouteTimeout;
                    // delay the datadram
                    delayDatagram(datagram);
                    auto rrep = createRREPSp(it->first, it->second);
                    rrep->setHopLimit(par("maxHopLimit"));
                    sendRREP(rrep, it->first, 255, -1);
                    // small delay
                    auto msgDelay = new SmallDelayPacket();
                    msgDelay->setDestAddress(it->first);
                    scheduleAt(simTime()+0.0001,msgDelay);

                    return QUEUE;
                }
            }


            // A node disseminates a RREQ when it determines that it needs a route
            // to a destination and does not have one available.  This can happen if
            // the destination is previously unknown to the node, or if a previously
            // valid route to the destination expires or is marked as invalid.

            EV_INFO << (isInactive ? "Inactive" : "Missing") << " route for destination " << destAddr << endl;

            delayDatagram(datagram);

            if (!hasOngoingRouteDiscovery(destAddr)) {
                // When a new route to the same destination is required at a later time
                // (e.g., upon route loss), the TTL in the RREQ IP header is initially
                // set to the Hop Count plus TTL_INCREMENT.
                if (isInactive)
                    startRouteDiscovery(destAddr, route->getMetric() + ttlIncrement);
                else
                    startRouteDiscovery(destAddr);
            }
            else
                EV_DETAIL << "Route discovery is in progress, originator " << getSelfIPAddress() << " target " << destAddr << endl;

            return QUEUE;
        }
    }
}

LoadNgTopsis::LoadNgTopsis()
{
}

bool LoadNgTopsis::hasOngoingRouteDiscovery(const L3Address& target)
{
    return waitForRREPTimers.find(target) != waitForRREPTimers.end();
}


void LoadNgTopsis::periodicRreqTigger()
{
    EV_INFO << "Sending periodic RREQ  "<< endl;
    auto rreqBl = createRREQ(this->getSelfIPAddress());
    rreqBl->setHopLimit(periodicRreqPropagation);
    rreqBl->setBuild(true);
    rreqCount++;
    sendLoadNgPacket(rreqBl, addressType->getBroadcastAddress(), par("periodicRreqPropagation").intValue(), *jitterPar);
    scheduleAt(simTime() + par("PeriodicRreq"), periodicRreqTimer);
    EV_INFO << "Sending periodic RREQ  "<< endl;
}

void LoadNgTopsis::startRouteDiscovery(const L3Address& target, unsigned timeToLive)
{
    EV_INFO << "Starting route discovery with originator " << getSelfIPAddress() << " and destination " << target << endl;
    ASSERT(!hasOngoingRouteDiscovery(target));
    auto rreq = createRREQ(target);
    addressToRreqRetries[target] = 0;
    if (par("Force3Messages").boolValue())
        rreq->setAccumulate(true);
    sendRREQ(rreq, addressType->getBroadcastAddress(), timeToLive);
}

L3Address LoadNgTopsis::getSelfIPAddress() const
{
    return routingTable->getRouterIdAsGeneric();
}

void LoadNgTopsis::delayDatagram(Packet *datagram)
{
    const auto& networkHeader = getNetworkProtocolHeader(datagram);
    EV_DETAIL << "Queuing datagram, source " << networkHeader->getSourceAddress() << ", destination " << networkHeader->getDestinationAddress() << endl;
    const L3Address& target = networkHeader->getDestinationAddress();
    targetAddressToDelayedPackets.insert(std::pair<L3Address, Packet *>(target, datagram));
}

void LoadNgTopsis::sendRREQ(const Ptr<Rreq>& rreq, const L3Address& destAddr, unsigned int timeToLive)
{
    // In an expanding ring search, the originating node initially uses a TTL =
    // TTL_START in the RREQ packet IP header and sets the timeout for
    // receiving a RREP to RING_TRAVERSAL_TIME milliseconds.
    // RING_TRAVERSAL_TIME is calculated as described in section 10.  The
    // TTL_VALUE used in calculating RING_TRAVERSAL_TIME is set equal to the
    // value of the TTL field in the IP header.  If the RREQ times out
    // without a corresponding RREP, the originator broadcasts the RREQ
    // again with the TTL incremented by TTL_INCREMENT.  This continues
    // until the TTL set in the RREQ reaches TTL_THRESHOLD, beyond which a
    // TTL = NET_DIAMETER is used for each attempt.

    if (rreqCount >= rreqRatelimit) {
        EV_WARN << "A node should not originate more than RREQ_RATELIMIT RREQ messages per second. Canceling sending RREQ" << endl;
        return;
    }

    auto rrepTimer = waitForRREPTimers.find(rreq->getDestAddr());
    WaitForRrep *rrepTimerMsg = nullptr;
    if (rrepTimer != waitForRREPTimers.end()) {
        rrepTimerMsg = rrepTimer->second;
        unsigned int lastTTL = rrepTimerMsg->getLastTTL();
        rrepTimerMsg->setDestAddr(rreq->getDestAddr());

        // The Hop Count stored in an invalid routing table entry indicates the
        // last known hop count to that destination in the routing table.  When
        // a new route to the same destination is required at a later time
        // (e.g., upon route loss), the TTL in the RREQ IP header is initially
        // set to the Hop Count plus TTL_INCREMENT.  Thereafter, following each
        // timeout the TTL is incremented by TTL_INCREMENT until TTL =
        // TTL_THRESHOLD is reached.  Beyond this TTL = NET_DIAMETER is used.
        // Once TTL = NET_DIAMETER, the timeout for waiting for the RREP is set
        // to NET_TRAVERSAL_TIME, as specified in section 6.3.

        if (timeToLive != 0) {
            rrepTimerMsg->setLastTTL(timeToLive);
            rrepTimerMsg->setFromInvalidEntry(true);
            cancelEvent(rrepTimerMsg);
        }
        else if (lastTTL + ttlIncrement < ttlThreshold) {
            ASSERT(!rrepTimerMsg->isScheduled());
            timeToLive = lastTTL + ttlIncrement;
            rrepTimerMsg->setLastTTL(lastTTL + ttlIncrement);
        }
        else {
            ASSERT(!rrepTimerMsg->isScheduled());
            timeToLive = netDiameter;
            rrepTimerMsg->setLastTTL(netDiameter);
        }
    }
    else {
        rrepTimerMsg = new WaitForRrep();
        waitForRREPTimers[rreq->getDestAddr()] = rrepTimerMsg;
        ASSERT(hasOngoingRouteDiscovery(rreq->getDestAddr()));

        timeToLive = ttlStart;
        rrepTimerMsg->setLastTTL(ttlStart);
        rrepTimerMsg->setFromInvalidEntry(false);
        rrepTimerMsg->setDestAddr(rreq->getDestAddr());
    }

    rreq->setHopLimit(timeToLive);

    if (rreq->getHopLimit() == 0) {
        EV_WARN << "Hop limit 0. Canceling sending RERR" << endl;
        return;
    }


    // Each time, the timeout for receiving a RREP is RING_TRAVERSAL_TIME.
    simtime_t ringTraversalTime = 2.0 * nodeTraversalTime * (timeToLive + timeoutBuffer);
    scheduleAt(simTime() + ringTraversalTime, rrepTimerMsg);

    EV_INFO << "Sending a Route Request with target " << rreq->getDestAddr() << " and TTL= " << timeToLive << endl;
    if (destAddr.isUnicast())
        sendLoadNgPacket(rreq, destAddr, timeToLive, 0);
    else
        sendLoadNgPacket(rreq, destAddr, timeToLive, *jitterPar);
   // sendLoadNgPacket(rreq, destAddr, timeToLive, *jitterPar);
    rreqCount++;
}

void LoadNgTopsis::sendRREP(const Ptr<Rrep>& rrep, const L3Address& destAddr, unsigned int timeToLive, const double &cost, const double &delay)
{
    EV_INFO << "Sending Route Reply to " << destAddr << endl;

    // When any node transmits a RREP, the precursor list for the
    // corresponding destination node is updated by adding to it
    // the next hop node to which the RREP is forwarded.

    if (rrep->getHopLimit() == 0) {
        EV_WARN << "Hop limit 0. Canceling sending RREP" << endl;
        return;
    }

    rrep->setPathAddressArraySize(rrep->getPathAddressArraySize() + 1);
    rrep->setPathAddress(rrep->getPathAddressArraySize() - 1, this->getSelfIPAddress());
    if (rrep->getAccumulate()) {
        // special packet, send
        // first check the origin
        L3Address nextHop;
        L3Address destAddress = rrep->getDestAddr();
        if (rrep->getAccumulateAddressArraySize() == 0)
            nextHop = destAddress;
        else {
            if (routingTable->isLocalAddress(rrep->getOriginatorAddr())) {
                // Fist
                nextHop = rrep->getAccumulateAddress(0);
            }
            else {
                // Extract next hop
                for (auto i = 0; i < (int)rrep->getAccumulateAddressArraySize(); i++) {
                    if (routingTable->isLocalAddress(rrep->getOriginatorAddr())) {
                        if (i == (int)rrep->getAccumulateAddressArraySize() - 1)
                            nextHop = destAddress;
                        else
                            nextHop = rrep->getAccumulateAddress(i+1);
                    }
                }
            }
        }
        // modify the routing table and send
        IRoute *destRoute = routingTable->findBestMatchingRoute(destAddr);
        IRoute *nextHopRoute = routingTable->findBestMatchingRoute(nextHop);
        int hops = rrep->getAccumulateAddressArraySize() + 1;

        // extract the metric to actualize the cost to the next hop
        if (!destRoute || destRoute->getSource() != this) {
            // create without valid sequence number
            destRoute = createRoute(destAddr, nextHop, hops, rrep->getSeqNumDest(), true, simTime() + activeRouteTimeout, HOPCOUNT, hops);
        }
        else {
            if (destRoute) {
               updateRoutingTable(destRoute, nextHop, hops, rrep->getSeqNumDest(), true, simTime() + activeRouteTimeout, HOPCOUNT, hops);
            }
        }

        if (!nextHopRoute || nextHopRoute->getSource() != this) {
                   // create without valid sequence number
            nextHopRoute = createRoute(destAddr, nextHop, 1, -1, true, simTime() + activeRouteTimeout, HOPCOUNT, hops);
        }
        sendLoadNgPacket(rrep, nextHop, timeToLive, delay);
        return;
    }

    IRoute *destRoute = routingTable->findBestMatchingRoute(destAddr);
    const L3Address& nextHop = destRoute->getNextHopAsGeneric();
    //LoadNgRouteData *destRouteData = check_and_cast<LoadNgRouteData *>(destRoute->getProtocolData());

    IRoute *nextHopRoute = routingTable->findBestMatchingRoute(nextHop);
    LoadNgRouteData *destRouteDataNextHop = check_and_cast<LoadNgRouteData *>(nextHopRoute->getProtocolData());


    // The node we received the Route Request for is our neighbor,
    // it is probably an unidirectional link
    if (destRoute->getMetric() == 1 || !destRouteDataNextHop->getIsBidirectiona()) {
        // It is possible that a RREP transmission may fail, especially if the
        // RREQ transmission triggering the RREP occurs over a unidirectional
        // link.

        auto ackReq = new LoadNgAckRrepReq();
        rrep->getTlvOptionsForUpdate().appendTlvOption(ackReq);
        //rrep->setAckRequiredFlag(true);
        // when a node detects that its transmission of a RREP message has failed,
        // it remembers the next-hop of the failed RREP in a "blacklist" set.

        failedNextHop = nextHop;

        if (rrepAckTimer->isScheduled())
            cancelEvent(rrepAckTimer);

        scheduleAt(simTime() + nextHopWait, rrepAckTimer);
    }
    sendLoadNgPacket(rrep, nextHop, timeToLive, delay);
}

const Ptr<Rreq> LoadNgTopsis::createRREQ(const L3Address& destAddr)
{
    auto rreqPacket = makeShared<Rreq>();
    //IRoute *lastKnownRoute = routingTable->findBestMatchingRoute(destAddr);

    rreqPacket->setPacketType(RREQ);

    // The Originator Sequence Number in the RREQ message is the
    // node's own sequence number, which is incremented prior to
    // insertion in a RREQ.
    sequenceNum++;

    rreqPacket->setHopLimit(par("maxHopLimit"));
    rreqPacket->setSeqNum(sequenceNum);

    rreqPacket->setOriginatorAddr(getSelfIPAddress());
    rreqPacket->setDestAddr(destAddr);

    if (rreqPacket->getOriginatorAddr().getType() == L3Address::IPv4)
        rreqPacket->setAddrLen(3);
    else if (rreqPacket->getOriginatorAddr().getType() == L3Address::IPv6)
        rreqPacket->setAddrLen(15);
    else if (rreqPacket->getOriginatorAddr().getType() == L3Address::MAC)
        rreqPacket->setAddrLen(5);

    // The Hop Count field is set to zero.
    rreqPacket->setHopCount(0);

/// HERE is propagate the metric.
// metric data
    auto metric2 = new LoadNgMetric2Option();
    metric2->setValue(0);
    rreqPacket->getTlvOptionsForUpdate().appendTlvOption(metric2);


    // Before broadcasting the RREQ, the originating node buffers the RREQ
    // ID and the Originator IP address (its own address) of the RREQ for
    // PATH_DISCOVERY_TIME.
    // In this way, when the node receives the packet again from its neighbors,
    // it will not reprocess and re-forward the packet.

    RreqIdentifier rreqIdentifier(getSelfIPAddress(), rreqId);
    rreqsArrivalTime[rreqIdentifier] = simTime();
    if (rreqPacket->getAddrLen() == 3)
        rreqPacket->setChunkLength(B(30));
    else if (rreqPacket->getAddrLen() == 15)
        rreqPacket->setChunkLength(B(54));
    else if (rreqPacket->getAddrLen() == 5)
        rreqPacket->setChunkLength(B(34));
    return rreqPacket;
}

const Ptr<Rrep> LoadNgTopsis::createRREPSp(const L3Address &dest, const SpecialPath &path)
{

    auto rrep = makeShared<Rrep>();
    rrep->setPacketType(RREP);

    // When generating a RREP message, a node copies the Destination IP
    // Address and the Originator Sequence Number from the RREQ message into
    // the corresponding fields in the RREP message.

    rrep->setDestAddr(dest);
    rrep->setOriginatorAddr(this->getSelfIPAddress());
    rrep->setSeqNumDest(-1);

    sequenceNum++;
    rrep->setSeqNum(sequenceNum);
    rrep->setHopLimit(par("maxHopLimit"));

    if (rrep->getOriginatorAddr().getType() == L3Address::IPv4)
        rrep->setAddrLen(3);
    else if (rrep->getOriginatorAddr().getType() == L3Address::IPv6)
        rrep->setAddrLen(15);
    else if (rrep->getOriginatorAddr().getType() == L3Address::MAC)
        rrep->setAddrLen(5);
    else
        throw cRuntimeError("Error size incorrect");
    // OriginatorAddr = The IP address of the node which originated the RREQ
    // for which the route is supplied.
    rrep->setHopCount(0);

    // The destination node copies the value MY_ROUTE_TIMEOUT
    // into the Lifetime field of the RREP.
   // rrep->setLifeTime(myRouteTimeout);

// include metrict.
    // metric data
    auto metric2 = new LoadNgMetric2Option();
    metric2->setValue(0);
    rrep->getTlvOptionsForUpdate().appendTlvOption(metric2);

    if (rrep->getAddrLen() == 3)
        rrep->setChunkLength(B(34));
    else if (rrep->getAddrLen() == 15)
        rrep->setChunkLength(B(58));
    else if (rrep->getAddrLen() == 5)
        rrep->setChunkLength(B(38));

    rrep->setAccumulate(true);
    rrep->setAccumulateAddressArraySize(path.path.size());
    for (int i = 0; i < (int)path.path.size(); i++)
        rrep->setAccumulateAddress(i,path.path[i]);
    return rrep;
}


const Ptr<Rrep> LoadNgTopsis::createRREP(const Ptr<Rreq>& rreq)
{
    if (rreq->getDestAddr() != getSelfIPAddress())
        return Ptr<Rrep>();
    auto rrep = makeShared<Rrep>();
    rrep->setPacketType(RREP);

    // When generating a RREP message, a node copies the Destination IP
    // Address and the Originator Sequence Number from the RREQ message into
    // the corresponding fields in the RREP message.

    rrep->setDestAddr(rreq->getOriginatorAddr());
    rrep->setOriginatorAddr(this->getSelfIPAddress());

    sequenceNum++;
    rrep->setSeqNum(sequenceNum);
    rrep->setHopLimit(par("maxHopLimit"));

    if (rrep->getOriginatorAddr().getType() == L3Address::IPv4)
        rrep->setAddrLen(3);
    else if (rrep->getOriginatorAddr().getType() == L3Address::IPv6)
        rrep->setAddrLen(15);
    else if (rrep->getOriginatorAddr().getType() == L3Address::MAC)
        rrep->setAddrLen(5);
    else
        throw cRuntimeError("Error size incorrect");
    // OriginatorAddr = The IP address of the node which originated the RREQ
    // for which the route is supplied.
    rrep->setHopCount(0);

    // The destination node copies the value MY_ROUTE_TIMEOUT
    // into the Lifetime field of the RREP.
   // rrep->setLifeTime(myRouteTimeout);

// include metrict.
    // metric data
    auto metric2 = new LoadNgMetric2Option();
    metric2->setValue(0);
    rrep->getTlvOptionsForUpdate().appendTlvOption(metric2);

    if (rrep->getAddrLen() == 3)
        rrep->setChunkLength(B(34));
    else if (rrep->getAddrLen() == 15)
        rrep->setChunkLength(B(58));
    else if (rrep->getAddrLen() == 5)
        rrep->setChunkLength(B(38));
    return rrep;
}

// TODO: Modify the RREP to actualize cost to destination
void LoadNgTopsis::handleRREP(const Ptr<Rrep>& rrep, const L3Address& sourceAddr, const MacAddress &macSenderAddress)
{
    EV_INFO << "LoadNgTopsis Route Reply arrived with source addr: " << sourceAddr << " originator addr: " << rrep->getOriginatorAddr()
            << " destination addr: " << rrep->getDestAddr() << endl;

    if (!macSenderAddress.isUnspecified() && !sourceAddr.isUnspecified()) {
        auto itMac = macToIpAddress.find(macSenderAddress);
        if (itMac == macToIpAddress.end()) {
            macToIpAddress[macSenderAddress] = sourceAddr;
        }
    }

    if (!rrep->getSeqField()) // bad format
        return;

    if (rrep->getHopLimit() != 0) // actualize
        rrep->setHopLimit(rrep->getHopLimit()-1);

    // check sequence number
    auto itSeq = seqNumbers.find(rrep->getOriginatorAddr());

    if (itSeq != seqNumbers.end() && itSeq->second > rrep->getSeqNum())
        return;

    //int64_t oldSeqNum = -1;
    //if (itSeq != seqNumbers.end())
    //    oldSeqNum = itSeq->second;

    if (itSeq == seqNumbers.end() || (itSeq != seqNumbers.end() && itSeq->second < rrep->getSeqNum())) {
        seqNumbers[rrep->getOriginatorAddr()] = rrep->getSeqNum();
    }

    if (rrep->getHopCount() > 0 && !routingTable->isLocalAddress(rrep->getDestAddr())) {
            // modify the routing table and send
        IRoute *destRoute = routingTable->findBestMatchingRoute(rrep->getDestAddr());

        if (destRoute) {
            bool sendError = false;
            if (destRoute->getNextHopAsGeneric() == rrep->getOriginatorAddr()) {
                sendError = true;
            }
            for (auto i = 0; i < (int) rrep->getPathAddressArraySize(); i++) {
                if (rrep->getPathAddress(i) == destRoute->getNextHopAsGeneric() ||
                        rrep->getPathAddress(i) == this->getSelfIPAddress()) {
                    sendError = true;
                }
            }
            if (sendError) {
                sendRERRWhenNoRouteToForward(rrep->getDestAddr(), rrep->getOriginatorAddr());
                return;
            }
        }
    }

    // Special RREP packet, generate with the double search RREQ
    if (rrep->getAccumulate()) {
        // special packet, send
        // first check the origin
        L3Address nextHop;
        L3Address destAddress = rrep->getDestAddr();
        int hops = rrep->getAccumulateAddressArraySize() + 1;
        if (!routingTable->isLocalAddress(destAddress)) {
            if (rrep->getAccumulateAddressArraySize() == 0)
                nextHop = destAddress;
            else {
                if (routingTable->isLocalAddress(rrep->getOriginatorAddr())) {
                    // Fist
                    nextHop = rrep->getAccumulateAddress(0);
                }
                else {
                    // Extract next hop
                    for (auto i = 0; i < (int)rrep->getAccumulateAddressArraySize(); i++) {
                        if (routingTable->isLocalAddress(rrep->getAccumulateAddress(i))) {
                            if (i == (int) rrep->getAccumulateAddressArraySize() - 1)
                                nextHop = destAddress;
                            else
                                nextHop = rrep->getAccumulateAddress(i + 1);
                            hops -= (i+1);
                            break;
                        }
                    }
                }
            }
        }
        if (!routingTable->isLocalAddress(destAddress)) {
            // modify the routing table and send
            IRoute *destRoute = routingTable->findBestMatchingRoute(destAddress);

            if (nextHop.isUnspecified())
                throw cRuntimeError("Next hop address unspecified");

            // extract the metric to actualize the cost to the next hop
            if (!destRoute || destRoute->getSource() != this) {
                // create without valid sequence number
                destRoute = createRoute(destAddress, nextHop, hops, rrep->getSeqNumDest(), true, simTime() + activeRouteTimeout, HOPCOUNT, hops);
            }
            else {
                auto seqNum = rrep->getSeqNumDest();
                auto routeData = check_and_cast<LoadNgRouteData *>(destRoute->getProtocolData());
                auto regSeqNum = routeData->getDestSeqNum();
                if (seqNum !=-1 && seqNum < regSeqNum)
                    throw cRuntimeError("Check seq numbers");
                if (destRoute) {
                    updateRoutingTable(destRoute, nextHop, hops, rrep->getSeqNumDest(), true, simTime() + activeRouteTimeout, HOPCOUNT, hops);
                }
            }
        }
        else {
            if(rrep->getIncludeRoute()) {
                SpecialPath path;
                path.lifetime = simTime() + 2 * netTraversalTime - 2 * (rrep->getRouteArraySize()+1) * nodeTraversalTime;
                for (auto i = 0; i < (int)rrep->getRouteArraySize(); i ++) {
                    path.path.push_back(rrep->getRoute(i));
                }
                specialPaths[rrep->getOriginatorAddr()] = path;
            }
        }
        if (rrep->getHopLimit() > 0 && this->getSelfIPAddress() != rrep->getDestAddr()) {
            auto outgoingRREP = dynamicPtrCast<Rrep>(rrep->dupShared());
            forwardRREP(outgoingRREP, nextHop);
        }
        if (hasOngoingRouteDiscovery(rrep->getOriginatorAddr())) {
              EV_INFO << "The Route Reply has arrived for our Route Request to node " << rrep->getOriginatorAddr() << endl;
              completeRouteDiscovery(rrep->getOriginatorAddr());
        }
        return;
    }

    // the hello is now a sparate type.
   /*if (rrep->getDestAddr().isUnspecified()) {
        EV_INFO << "This Route Reply is a Hello Message" << endl;
        handleHelloMessage(rrep);
        return;
    }
    */

    int metricType = -1;
    unsigned int metric = 255;
    double metricNeg = 1;
    auto metricPos = rrep->getTlvOptionsForUpdate().findByType(METRIC);
    if (metricPos != -1) {
        auto metricTlv = check_and_cast<LoadNgMetricOption *>(rrep->getTlvOptionsForUpdate().getTlvOptionForUpdate(metricPos));
        if (metricTlv->getExtensionFlag())
            metricType = metricTlv->getExtension();
        if (metricTlv->getValueFlag())
            metric = metricTlv->getValue();
        if (metricType == HOPCOUNT) {
            if (metricType == HOPCOUNT) {
                if (metric < 255)
                    metric++;
                metricTlv->setValue(metric);
                metricNeg = 1;
            }
            else {
                // unknow metric set to hopcount
                metricTlv->setType(HOPCOUNT);
                metricTlv->setValue(255);
            }
            metricNeg = 1;
        }
    }


    // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    // HERE:::::::: ACTUALIZE METRIC
    // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

    auto itNeig = neighbors.find(sourceAddr);
    if (useHelloMessages && (itNeig == neighbors.end() || !itNeig->second.isBidirectional || itNeig->second.pendingConfirmation)) {
        EV_INFO << "The sender node " << sourceAddr << " is not in the neighbors list or is not bi-directional. Ignoring the Route Request" << endl;
        // throw cRuntimeError("");
    }

    //double val = metricNeg;
    double metricToOrigin = 255;
    double etx = NaN;
    double delay = NaN;
    double snir = NaN;
    double energy = NaN;
    double recpower = NaN;
    int numNeigh = -1;

    if (itNeig != neighbors.end()) {
        if (!itNeig->second.powerList.empty())
            recpower = itNeig->second.powerList.back();
        if (!itNeig->second.delayList.empty()) {
            simtime_t val;
            for (auto e : itNeig->second.delayList)
                val += e;
            val /= itNeig->second.delayList.size();
            delay = val.dbl();
        }
        if (!itNeig->second.snirList.empty()) {
            double val = 0;
            for (auto e : itNeig->second.snirList)
                val += e;
            val /= itNeig->second.snirList.size();
            snir = val;
        }
        etx = itNeig->second.metricToNeig;
        //val = itNeig->second.metricToNeig;
        energy = itNeig->second.energy;
        numNeigh = itNeig->second.listNeigbours.size();
    }

    // TODO: Compare the new route with this route
    auto prevNodeOrigin = sourceAddr;
    auto itTopsisOrg = topsisResults.find(rrep->getOriginatorAddr());
    if (itTopsisOrg != topsisResults.end() && itTopsisOrg->second.nextAddress != sourceAddr) {
        //double time = simTime().dbl();
        delay = itTopsisOrg->second.delay;
        snir = itTopsisOrg->second.snir;
        recpower = itTopsisOrg->second.recPower;
        energy = itTopsisOrg->second.energy;
        etx = itTopsisOrg->second.etx;
        metricToOrigin = etx;
        prevNodeOrigin = itTopsisOrg->second.nextAddress;
        // actualize in the packet with the correct metrics.
        rrep->setHopCount(1);
        if (!setMetric(rrep, metricToOrigin))
            metricToOrigin = 255;
        //if (originRoute && originRoute->getNextHopAsGeneric() == itTopsisOrg->second.nextAddress)
        //    checkCost = false; // don't check the costs.
    }


    if (!isnan(etx)) {
        if (isnan(rrep->getEtx()))
            rrep->setEtx(etx);
        else
            rrep->setEtx(etx + rrep->getEtx());
    }
    if (!isnan(delay)) {
        if (isnan(rrep->getDelay()))
            rrep->setDelay(delay);
        else
            rrep->setDelay(delay + rrep->getDelay());
    }
    if (!isnan(energy)) {
        if (isnan(rrep->getEnergy()))
            rrep->setEnergy(energy);
        else if (rrep->getEnergy() > energy)
            rrep->setEnergy(energy);
    }

    if (!isnan(recpower)) {
        if (isnan(rrep->getRecPower()))
            rrep->setRecPower(recpower);
        else if (rrep->getRecPower() > recpower)
            rrep->setRecPower(recpower);
    }

    if (!isnan(snir)) {
        if (isnan(rrep->getSnir()))
            rrep->setSnir(snir);
        else if (rrep->getSnir() > snir)
            rrep->setSnir(snir);
    }

    if (numNeigh != -1) {
        if (rrep->getNumNeig() == -1)
            rrep->setNumNeig(numNeigh);
        else if (rrep->getNumNeig() < numNeigh)
            rrep->setNumNeig(numNeigh);
    }

    if (useHelloMessages && measureEtx)
          metricNeg = itNeig->second.metricToNeig;

    if (!actualizeMetric(rrep, metricNeg, metricToOrigin))
        metricToOrigin = 255;

    IRoute *previousHopRoute = routingTable->findBestMatchingRoute(sourceAddr);

    // HERE,
    // TODO : actualize the entry costs
    if (!previousHopRoute || previousHopRoute->getSource() != this) {
        // create without valid sequence number
        previousHopRoute = createRoute(sourceAddr, sourceAddr, 1, -1, true, simTime() + activeRouteTimeout, metricType, metricNeg);
    }
    else {
        auto loadNgRouteData = check_and_cast<LoadNgRouteData *> (previousHopRoute->getProtocolData());
        // check first topsis
        auto itTopsis = topsisResults.find(sourceAddr);
        if (itTopsis != topsisResults.end()) {

            if (itTopsis->second.nextAddress == sourceAddr)
                updateRoutingTable(previousHopRoute, sourceAddr, 1, loadNgRouteData->getDestSeqNum(), true, simTime() + activeRouteTimeout, metricType, metric);
            else
                updateRoutingTable(previousHopRoute, itTopsis->second.nextAddress, itTopsis->second.numHops, loadNgRouteData->getDestSeqNum(), true, simTime() + activeRouteTimeout, metricType, itTopsis->second.etx);
        }
        else
            updateRoutingTable(previousHopRoute, sourceAddr, 1, loadNgRouteData->getDestSeqNum(), true, simTime() + activeRouteTimeout, metricType, metric);
    }

    // Next, the node then increments the hop count value in the RREP by one,
    // to account for the new hop through the intermediate node
    unsigned int newHopCount = rrep->getHopCount() + 1;
    rrep->setHopCount(newHopCount);

    int64_t seqNum = rrep->getSeqNum();

    // TODO: Include Topsis comparation,
    TopsisLoadNgNeigbors topsis;
    topsis.setExperiment(par("matrixExperiment"));
    auto itPrevRrep = listPrevRrepCost.find(rrep->getOriginatorAddr());
    auto topsisCost = topsis.addCost(
                        rrep->getOriginatorAddr(), prevNodeOrigin,
                        rrep->getEtx(), rrep->getNumNeig(),
                        rrep->getSnir(), rrep->getRecPower(), rrep->getEnergy(),
                        rrep->getHopCount(), rrep->getDelay());

    if (itPrevRrep == listPrevRrepCost.end()) {
        CostInfo cost;
        cost.seqNum = seqNum;
        cost.index = -1;
        cost.list.push_back(topsisCost);
        listPrevRrepCost[rrep->getOriginatorAddr()] = cost;
        itPrevRrep = listPrevRrepCost.find(rrep->getOriginatorAddr());
    }
    else if (itPrevRrep->second.seqNum < seqNum) {
        itPrevRrep->second.seqNum = seqNum;
        itPrevRrep->second.index = -1;
        itPrevRrep->second.list.clear();
        itPrevRrep->second.list.push_back(topsisCost);
    }
    else if (itPrevRrep->second.seqNum == seqNum) {
        // TODO: Multiples RREP
        auto it = std::find(itPrevRrep->second.list.begin(), itPrevRrep->second.list.end(), topsisCost);
        if (it == itPrevRrep->second.list.end())
            itPrevRrep->second.list.push_back(topsisCost);
        else
            (*it) = topsisCost;
    }
    else {
        throw cRuntimeError("Sequence error problem");
    }

    std::deque<TopsisLoadNgNeigbors::CostAndRange> results;
    topsis.clear();

    for (auto elem : itPrevRrep->second.list) {
        topsis.addCost(elem);
    }

    IRoute *originRoute = routingTable->findBestMatchingRoute(rrep->getOriginatorAddr());
    LoadNgRouteData *orgRouteData = nullptr;

    if (originRoute && originRoute->getSource() == this) {    // already exists
        orgRouteData = check_and_cast<LoadNgRouteData *>(originRoute->getProtocolData());
        // 11.1.  Identifying Invalid RREQ or RREP Messages,
        int64_t oldSeqNum = orgRouteData->getDestSeqNum();
        if (oldSeqNum != -1 && seqNum < oldSeqNum) // invalid message
            return;

        // orgRouteData->setIsBidirectiona(true);

        if (orgRouteData->getDestSeqNum() == -1 || seqNum > orgRouteData->getDestSeqNum()) {
            actualizeCostRrep(rrep, orgRouteData);
            updateRoutingTable(originRoute, prevNodeOrigin, newHopCount, seqNum, true, simTime() + activeRouteTimeout, metricType, metric);
        }
        else {
            // (iii) the sequence numbers are the same, but the route is
            //       marked as inactive, or
            topsis.runTopsisRoutes(results);
            if (seqNum == orgRouteData->getDestSeqNum() && !orgRouteData->isActive()) {
                actualizeCostRrep(rrep, orgRouteData);
                updateRoutingTable(originRoute, sourceAddr, newHopCount, seqNum, true, simTime() + activeRouteTimeout, metricType, metric);
            }
            // (iv) the sequence numbers are the same, and the New Hop Count is
            //      smaller than the hop count in route table entry.
            else if (seqNum == orgRouteData->getDestSeqNum()) {
                if (!results.empty()) {
                    if (results.front().index == (int)results.size() - 1) { // it is the information of this RREQ that is the latest in to be included
                        actualizeCostRrep(rrep, orgRouteData);
                        updateRoutingTable(originRoute, sourceAddr, newHopCount, seqNum, true, simTime() + activeRouteTimeout, metricType, metric, TOPSIS_UPDATE);
                    }
                }
            }
        }
    }
    else {
        // create forward route for the destination: this path will be used by the originator to send data packets
       // orgRouteData->setIsBidirectiona(true);
        originRoute = createRoute(rrep->getOriginatorAddr(), sourceAddr, newHopCount, seqNum, true, simTime() + activeRouteTimeout, metricType, metric, TOPSIS_UPDATE);
        orgRouteData = check_and_cast<LoadNgRouteData *>(originRoute->getProtocolData());
        actualizeCostRrep(rrep, orgRouteData);
     }

    // check ack req, Tlv
    auto flagPos = rrep->getTlvOptionsForUpdate().findByType(FLAGS);
    if (flagPos != -1) {
        auto flag = check_and_cast<LoadNgAckRrepReq *>(rrep->getTlvOptionsForUpdate().getTlvOptionForUpdate(flagPos));
        if (flag->getValue() == REQACK) {
            auto rrepACK = createRREPACK();
            sendRREPACK(rrepACK, sourceAddr);
            // check if must delete ACK option

            delete rrep->getTlvOptionsForUpdate().dropTlvOption(flag);
            // rrep->setAckRequiredFlag(false);
        }
    }

    if (hasOngoingRouteDiscovery(rrep->getOriginatorAddr())) {
        EV_INFO << "The Route Reply has arrived for our Route Request to node " << rrep->getOriginatorAddr() << endl;
        completeRouteDiscovery(rrep->getOriginatorAddr());
    }


    // Then the forward route for this destination is created if it does not
    // already exist.

    IRoute *destRoute = routingTable->findBestMatchingRoute(rrep->getDestAddr());
    if (this->getSelfIPAddress() == rrep->getDestAddr()) {
       // nothing to do

    }
    else if (destRoute && destRoute->getSource() == this) {    // already exists
        // Upon comparison, the existing entry is updated only in the following circumstances:
        //updateRoutingTable(destRoute, destRoute->getNextHopAsGeneric(), destRoute->getMetric(), destRouteData->getDestSeqNum(), true, simTime() + activeRouteTimeout, destRouteData->getMetric(), destRouteData->getMetric());
        updateValidRouteLifeTime(rrep->getDestAddr(), simTime() + activeRouteTimeout);
        // When any node transmits a RREP, the precursor list for the
        // corresponding destination node is updated by adding to it
        // the next hop node to which the RREP is forwarded.

        if (rrep->getHopCount() > 0) {
            // auto it = topsisResults.find(rrep->getOriginatorAddr());
            if (destRoute->getNextHopAsGeneric() == rrep->getOriginatorAddr()) {
                sendRERRWhenNoRouteToForward(rrep->getDestAddr(), rrep->getOriginatorAddr());
                return;
                std::string infoData = simTime().str() + " Origin: " + rrep->getOriginatorAddr().str() +  " Dest: " + rrep->getDestAddr().str() + " Route : ";
                for (auto i = 0; i < (int)rrep->getPathAddressArraySize(); i++)
                    infoData += rrep->getPathAddress(i).str() + "-";
                throw cRuntimeError("%s",infoData.c_str());
            }

            for (auto i = 0; i < (int)rrep->getPathAddressArraySize(); i++) {
                if (rrep->getPathAddress(i) == destRoute->getNextHopAsGeneric() ||
                        rrep->getPathAddress(i) == this->getSelfIPAddress()) {
                    sendRERRWhenNoRouteToForward(rrep->getDestAddr(), rrep->getOriginatorAddr());
                    return;
                    std::string infoData = simTime().str() + " Origin: " + rrep->getOriginatorAddr().str() +  " Dest: " + rrep->getDestAddr().str() + " Route : ";
                    for (auto j = 0; j < (int)rrep->getPathAddressArraySize(); j++) {
                        infoData += rrep->getPathAddress(j).str() + "-";
                    }
                    throw cRuntimeError("%s",infoData.c_str());
                }
            }
            auto outgoingRREP = dynamicPtrCast<Rrep>(rrep->dupShared());
            outgoingRREP->appendAccumulateAddress(this->getSelfIPAddress());
            forwardRREP(outgoingRREP, destRoute->getNextHopAsGeneric());
        }
    }
    else {    // create forward route for the destination: this path will be used by the originator to send data packets
        /** received routing message is an RREP and no routing entry was found **/
        sendRERRWhenNoRouteToForward(rrep->getDestAddr(), rrep->getOriginatorAddr());
    }
}

IRoute *LoadNgTopsis::createRoute(const L3Address& destAddr, const L3Address& next,
        unsigned int hopCount, int64_t destSeqNum,
        bool isActive, simtime_t lifeTime, int metricType, const double & m, UpdateBehaviour check)
{
    auto nextHop = next;
    auto metric = m;
    if (destAddr == this->getSelfIPAddress())
        throw cRuntimeError("never ");
    auto it = neighbors.find(nextHop);
    if (it == neighbors.end()) {
        // include in the list
        NeigborElement elem;
        // elem.lastNotification = simTime();
        if (nextHop == destAddr)
            elem.seqNumber = destSeqNum;
        neighbors[nextHop] = elem;
    }
    else {
        // it->second.lastNotification = simTime();
        if (nextHop == destAddr)
            it->second.seqNumber = destSeqNum;
    }

    // create a new route
    // first check if the route exist
    IRoute *route = routingTable->findBestMatchingRoute(destAddr);
    if (route != nullptr) {
        if (route->getSource() == this)
            throw cRuntimeError("route->getSource() == this");
        else
            routingTable->deleteRoute(route);
    }
    do {
        route = routingTable->findBestMatchingRoute(destAddr);
        if (route) routingTable->deleteRoute(route);
    } while(route != nullptr);

    // check if the entry is the same that the data in topsis
    if (check == CHECK_UPDATE) {
        auto itTopsis = topsisResults.find(destAddr);
        if (itTopsis != topsisResults.end()) {
            if (itTopsis->second.nextAddress != nextHop)
                throw cRuntimeError("itTopsis->second.nextAddress != nextHop"); // Change the value to the data in topsisData base, temporal solution
        }
    }
    else if (check == TOPSIS_UPDATE) {
        auto itTopsis = topsisResults.find(destAddr);
        if (itTopsis != topsisResults.end()) {
            if (itTopsis->second.nextAddress != nextHop) {
               nextHop = itTopsis->second.nextAddress;
               hopCount = itTopsis->second.numHops;
               metric = itTopsis->second.etx;
            }
        }
    }
    IRoute *newRoute = routingTable->createRoute();

    // adding generic fields
    newRoute->setDestination(destAddr);
    newRoute->setNextHop(nextHop);
    newRoute->setPrefixLength(addressType->getMaxPrefixLength());    // TODO:
    newRoute->setMetric(hopCount);
    NetworkInterface *ifEntry = interfaceTable->findInterfaceByName("wlan0");    // TODO: IMPLEMENT: multiple interfaces
    if (ifEntry)
        newRoute->setInterface(ifEntry);
    newRoute->setSourceType(IRoute::MANET2);
    newRoute->setSource(this);

    // A route towards a destination that has a routing table entry
    // that is marked as valid.  Only active routes can be used to
    // forward data packets.

    // adding protocol-specific fields
    LoadNgRouteData *newProtocolData = new LoadNgRouteData();
    newProtocolData->setIsActive(isActive);
    newProtocolData->setDestSeqNum(destSeqNum);
    newProtocolData->setLifeTime(lifeTime);
    newProtocolData->setMetricType(metricType);
    newProtocolData->setMetric(metric);
    newProtocolData->setHopCount(hopCount);
    newProtocolData->setActualizeTime();

    newRoute->setProtocolData(newProtocolData);

    EV_DETAIL << "Adding new route " << newRoute << endl;
    routingTable->addRoute(newRoute);

    scheduleExpungeRoutes();
    return newRoute;
}

void LoadNgTopsis::updateRoutingTable(IRoute *route, const L3Address& next, unsigned int hopCount, int64_t destSeqNum, bool isActive, simtime_t lifeTime, int metricType, const double & m, UpdateBehaviour check)
{
    EV_DETAIL << "Updating existing route: " << route << endl;

    auto nextHop = next;
    auto metric = m;
    auto it = neighbors.find(nextHop);
    if (it == neighbors.end()) {
        // include in the list
        NeigborElement elem;
//        elem.lastNotification = simTime();
        if (nextHop == route->getDestinationAsGeneric()) {
            elem.seqNumber = destSeqNum;
        }
        neighbors[nextHop] = elem;
    }
    else {
//        it->second.lastNotification = simTime();
        if (nextHop == route->getDestinationAsGeneric()) {
            it->second.seqNumber = destSeqNum;
         }
    }
    // check if the entry is the same that the data in topsis
    if (check == CHECK_UPDATE) {
        auto itTopsis = topsisResults.find(route->getDestinationAsGeneric());
        if (itTopsis != topsisResults.end()) {
            if (itTopsis->second.nextAddress != nextHop)
                throw cRuntimeError("itTopsis->second.nextAddress != nextHop"); // Change the value to the data in topsisData base, temporal solution
        }
    }
    else if (check == TOPSIS_UPDATE) {
        auto itTopsis = topsisResults.find(route->getDestinationAsGeneric());
        if (itTopsis != topsisResults.end()) {
            if (itTopsis->second.nextAddress != nextHop) {
               nextHop = itTopsis->second.nextAddress;
               hopCount = itTopsis->second.numHops;
               metric = itTopsis->second.etx;
            }
        }
    }


    route->setNextHop(nextHop);
    route->setMetric(hopCount);
    LoadNgRouteData *routingData = check_and_cast<LoadNgRouteData *>(route->getProtocolData());
    ASSERT(routingData != nullptr);

    routingData->setLifeTime(lifeTime);
    routingData->setDestSeqNum(destSeqNum);
    routingData->setIsActive(isActive);
    routingData->setActualizeTime();


    if (metricType != -1) {
        routingData->setMetricType(metricType);
        routingData->setMetric(metric);
    }
    routingData->setHopCount(hopCount);

    EV_DETAIL << "Route updated: " << route << endl;

    scheduleExpungeRoutes();
}

void LoadNgTopsis::sendLoadNgPacket(const Ptr<LoadNgControlPacket>& packet, const L3Address& destAddr, unsigned int timeToLive, double delay)
{
    ASSERT(timeToLive != 0);

    if (packet->getHopLimits()) {
        auto rreq = dynamicPtrCast<const Rreq>(packet);
        if (rreq && rreq->getHopLimit() == 0) {
            EV_WARN << "Hop limit 0. Canceling sending Packet Rreq" << endl;
            return;
        }
        else {
            auto rrep = dynamicPtrCast<const Rrep>(packet);
            if (rrep && rrep->getHopLimit() == 0) {
                EV_WARN << "Hop limit 0. Canceling sending Packet Rrep" << endl;
                return;
            }
            else {
                auto rerr = dynamicPtrCast<const Rerr>(packet);
                if (rerr && rerr->getHopLimit() == 0) {
                    EV_WARN << "Hop limit 0. Canceling sending Packet Rerr" << endl;
                    return;
                }
            }
        }
    }
    // TODO: Implement: support for multiple interfaces
    NetworkInterface *ifEntry = interfaceTable->findInterfaceByName("wlan0");

    auto className = packet->getClassName();
    Packet *udpPacket = new Packet(!strncmp("inet::", className, 6) ? className + 6 : className);
    udpPacket->insertAtFront(packet);
    // TODO: was udpPacket->copyTags(*packet);
    udpPacket->addTagIfAbsent<PacketProtocolTag>()->setProtocol(&Protocol::manet);
    udpPacket->addTagIfAbsent<DispatchProtocolReq>()->setProtocol(addressType->getNetworkProtocol());
    udpPacket->addTagIfAbsent<InterfaceReq>()->setInterfaceId(ifEntry->getInterfaceId());
    auto addresses = udpPacket->addTagIfAbsent<L3AddressReq>();
    addresses->setSrcAddress(getSelfIPAddress());
    addresses->setDestAddress(destAddr);
    udpPacket->addTagIfAbsent<HopLimitReq>()->setHopLimit(timeToLive);

    if (destAddr.isBroadcast())
        lastBroadcastTime = simTime();
    if (delay == 0)
        send(udpPacket, "ipOut");
    else {
        auto *timer = new PacketHolderMessage("LoadNg-send-jitter", KIND_DELAYEDSEND);
        timer->setOwnedPacket(udpPacket);
        pendingSend.insert(timer);
        scheduleAt(simTime()+delay, timer);
    }
}

bool LoadNgTopsis::actualizeMetric(const Ptr<LoadNgControlPacketWithTlv> &pkt, const double &val, double &newMetric) {
    auto metricPos = pkt->getTlvOptionsForUpdate().findByType(METRIC2);
    int metricType = -1;
    if (metricPos != -1) {
        auto metricTlv = check_and_cast<LoadNgMetric2Option *>(
                pkt->getTlvOptionsForUpdate().getTlvOptionForUpdate(
                        metricPos));
        if (metricTlv->getExtensionFlag())
            metricType = metricTlv->getExtension();
        if (!metricTlv->getValueFlag())
            return false;
        if (metricType == HOPCOUNT) {
            return false;
        } else if (metricType == DIMENSIONLESS ){
            // HERE ACTUALIZE THE METRIC
            metricTlv->setValue(metricTlv->getValue() + val);
            newMetric = metricTlv->getValue();
            return true;
        }
    }

    return false;
}


bool LoadNgTopsis::setMetric(const Ptr<LoadNgControlPacketWithTlv> &pkt, const double & val) {
    auto metricPos = pkt->getTlvOptionsForUpdate().findByType(METRIC2);
    int metricType = -1;
    if (metricPos  != -1) {
        auto metricTlv = check_and_cast<LoadNgMetric2Option *>(pkt->getTlvOptionsForUpdate().getTlvOptionForUpdate(metricPos));
        if (metricTlv->getExtensionFlag())
            metricType = metricTlv->getExtension();
        if (!metricTlv->getValueFlag())
            return false;
        if (metricType == HOPCOUNT) {
           return false;
        } else if (metricType == DIMENSIONLESS ){
            // HERE ACTUALIZE THE METRIC
            metricTlv->setValue(val);
            return true;
        }
    }
    return false;
}


bool LoadNgTopsis::getMetric(const Ptr<LoadNgControlPacketWithTlv> &pkt, double &newMetric) {
    auto metricPos = pkt->getTlvOptionsForUpdate().findByType(METRIC2);
    int metricType = -1;
    if (metricPos != -1) {
        auto metricTlv = check_and_cast<LoadNgMetric2Option *>(pkt->getTlvOptionsForUpdate().getTlvOptionForUpdate(metricPos));
        if (metricTlv->getExtensionFlag())
            metricType = metricTlv->getExtension();
        if (!metricTlv->getValueFlag())
            return false;
        if (metricType == HOPCOUNT) {
            return false;
        } else if (metricType == DIMENSIONLESS ){
            // HERE ACTUALIZE THE METRIC
            newMetric = metricTlv->getValue();
            return true;
        }
    }

    return false;
}

bool LoadNgTopsis::checkCost(const Ptr<Rreq>& rreq, LoadNgRouteData * loadNgRouteData) {

    if (!isnan(rreq->getEtx())) {
        if (isnan(loadNgRouteData->getEtx()) || loadNgRouteData->getEtx() >  rreq->getEtx()) {
            return true;
        }
    }

    if (!isnan(rreq->getEnergy())) {
        if (isnan(loadNgRouteData->getEnergy()) || loadNgRouteData->getEnergy() <  rreq->getEnergy()) {
            return true;
        }
    }

    if (!isnan(rreq->getDelay())) {
        if (isnan(loadNgRouteData->getDelay()) || loadNgRouteData->getDelay() >  rreq->getDelay()) {
            return true;
        }
    }
    if (!isnan(rreq->getSnir())) {
        if (isnan(loadNgRouteData->getSnir()) || loadNgRouteData->getSnir() <  rreq->getSnir()) {
            return true;
        }
    }
    return false;
}


void LoadNgTopsis::actualizeCostUsingStatus(const NodeStatus &status, LoadNgRouteData * loadNgRouteData) {
    loadNgRouteData->setEnergy(status.energy);
    loadNgRouteData->setRecPower(status.recPower);
    loadNgRouteData->setNumNeig(status.numNeig);
    loadNgRouteData->setDelay(status.delay.dbl());
    loadNgRouteData->setSnir(status.snir);
    loadNgRouteData->setEtx(status.metric);
}

void LoadNgTopsis::actualizeCostUsingNeig(const NeigborElement &status, LoadNgRouteData * loadNgRouteData) {
    loadNgRouteData->setEnergy(status.energy);
    loadNgRouteData->setRecPower(status.powerList.back());
    loadNgRouteData->setNumNeig(status.listNeigbours.size());
    loadNgRouteData->setDelay(status.delayCost);
    loadNgRouteData->setSnir(status.snirCost);
    loadNgRouteData->setEtx(status.metricToNeig);
}


void LoadNgTopsis::actualizeCost(const Ptr<Rreq>& rreq, LoadNgRouteData * loadNgRouteData) {
    loadNgRouteData->setEnergy(rreq->getEnergy());
    loadNgRouteData->setRecPower(rreq->getRecPower());
    loadNgRouteData->setNumNeig(rreq->getNumNeig());
    loadNgRouteData->setDelay(rreq->getDelay());
    loadNgRouteData->setSnir(rreq->getSnir());
    loadNgRouteData->setEtx(rreq->getEtx());
}

void LoadNgTopsis::actualizeCostRrep(const Ptr<Rrep>& rrep, LoadNgRouteData * loadNgRouteData) {
    loadNgRouteData->setEnergy(rrep->getEnergy());
    loadNgRouteData->setRecPower(rrep->getRecPower());
    loadNgRouteData->setNumNeig(rrep->getNumNeig());
    loadNgRouteData->setDelay(rrep->getDelay());
    loadNgRouteData->setSnir(rrep->getSnir());
    loadNgRouteData->setEtx(rrep->getEtx());
}


void LoadNgTopsis::handleRREQ(const Ptr<Rreq>& rreq, const L3Address& sourceAddr, const L3Address& nextHopAddr, unsigned int timeToLive, const MacAddress &macSenderAddress)
{

    // TODO: Adapt to multiple RREQ sends by intermediate nodes

    EV_INFO << "LoadNgTopsis Route Request arrived with source addr: " << sourceAddr << " originator addr: " << rreq->getOriginatorAddr()
            << " destination addr: " << rreq->getDestAddr() << " Hop limit :" << rreq->getHopLimit() << endl;

    // A node ignores all RREQs received from any node in its blacklist set.

    if (!macSenderAddress.isUnspecified() && !sourceAddr.isUnspecified()) {
        auto itMac = macToIpAddress.find(macSenderAddress);
        if (itMac == macToIpAddress.end()) {
            macToIpAddress[macSenderAddress] = sourceAddr;
        }
    }


    auto itNeig = neighbors.find(sourceAddr);
    if (useHelloMessages && (itNeig == neighbors.end() || !itNeig->second.isBidirectional || itNeig->second.pendingConfirmation) && !rreq->getAccumulate()) {
        EV_INFO << "The sender node " << sourceAddr << " is not in the neighbors list or is not bi-directional. Ignoring the Route Request" << endl;
        return;
    }

    auto blackListIt = blacklist.find(sourceAddr);
    if (blackListIt != blacklist.end() && !rreq->getAccumulate()) {
        EV_INFO << "The sender node " << sourceAddr << " is in our blacklist. Ignoring the Route Request" << endl;
        return;
    }

    // check unidirectional RREQ
    if (nextHopAddr == getSelfIPAddress() && rreq->getDestAddr() != getSelfIPAddress() && RREQUNI_RERR) {
        // unicast, check if exist the hop to the destination
        auto it = topsisResults.find(rreq->getDestAddr());
        if (it == topsisResults.end()) {
            // send RERR and return.
            std::vector<UnreachableNode> unreachableNodes;
            UnreachableNode node;
            node.addr = rreq->getDestAddr();

            IRoute *unreachableRoute = routingTable->findBestMatchingRoute(rreq->getDestAddr());
            LoadNgRouteData *unreachableRouteData = unreachableRoute ? dynamic_cast<LoadNgRouteData *>(unreachableRoute->getProtocolData()) : nullptr;
           if (unreachableRouteData && unreachableRouteData->hasValidDestNum())
               node.seqNum = unreachableRouteData->getDestSeqNum();
           else
               node.seqNum = 0;
           unreachableNodes.push_back(node);
           auto rerr = createRERR(unreachableNodes);
           rerr->setHopLimit(1);
           rerr->setOriginatorAddr(this->getSelfIPAddress());
           rerr->setDestAddr(rreq->getOriginatorAddr());
           rerr->setUniRreq(true);
           rerrCount++;
           // broadcast
           sendLoadNgPacket(rerr, sourceAddr, 1, 0);
           return;
        }
    }

    auto itSeq = seqNumbers.find(rreq->getOriginatorAddr());
    if (itSeq != seqNumbers.end() && itSeq->second > rreq->getSeqNum())
        return;

    int64_t oldSeqNum = -1;

    if (itSeq != seqNumbers.end())
        oldSeqNum = itSeq->second;

    if (itSeq == seqNumbers.end() || (itSeq != seqNumbers.end() && itSeq->second < rreq->getSeqNum())) {
        seqNumbers[rreq->getOriginatorAddr()] = rreq->getSeqNum();
    }

    // When a node receives a RREQ, it first creates or updates a route to
    // the previous hop without a valid sequence number (see section 6.2).

    int metricType = -1;
    double metricNeg = 1;

    auto metricPos = rreq->getTlvOptionsForUpdate().findByType(METRIC);

    if (metricPos != -1) {
        auto metricTlv = check_and_cast<LoadNgMetricOption *>(rreq->getTlvOptionsForUpdate().getTlvOptionForUpdate(metricPos));
        if (metricTlv->getExtensionFlag())
            metricType = metricTlv->getExtension();
        uint8_t metric = 255;
        if (metricTlv->getValueFlag())
            metric = metricTlv->getValue();
        if (metricType == HOPCOUNT) {
            if (metric < 255)
                metric++;
            metricTlv->setValue(metric);
            metricNeg = 1;
        }
        else {
            // unknow metricToNeig set to hopcount
            metricTlv->setType(HOPCOUNT);
            metricTlv->setValue(255);
        }
    }

    // HERE; Metric to neighbor
    if (useHelloMessages && measureEtx)
        metricNeg = itNeig->second.metricToNeig;

    // HERE:::::::: ACTUALIZE METRIC
    // HERE it is necessary to set the value of VAL,

    double val = metricNeg;
    double metricToOrigin = 255;
    double etx = NaN;
    double delay = NaN;
    double snir = NaN;
    double energy = NaN;
    double recpower = NaN;
    int numNeigh = -1;

    auto itTopsis = topsisResults.find(sourceAddr);
    if (itTopsis != topsisResults.end() && itTopsis->second.nextAddress != sourceAddr) {
        delay = itTopsis->second.delay;
        snir = itTopsis->second.snir;
        energy = itTopsis->second.energy;
        recpower = itTopsis->second.recPower;
        etx = itTopsis->second.etx;
        numNeigh = itTopsis->second.neighbors;
        metricNeg = etx;
    }
    else {
        if (itNeig != neighbors.end()) {
            if (!itNeig->second.powerList.empty())
                recpower = itNeig->second.powerList.back();
            if (!itNeig->second.delayList.empty()) {
                simtime_t val;
                for (auto e : itNeig->second.delayList)
                    val += e;
                val /= itNeig->second.delayList.size();
                delay = val.dbl();
            }
            if (!itNeig->second.snirList.empty()) {
                double val = 0;
                for (auto e : itNeig->second.snirList)
                    val += e;
                val /= itNeig->second.snirList.size();
                snir = val;
            }
            etx = itNeig->second.metricToNeig;
            val = itNeig->second.metricToNeig;
            energy = itNeig->second.energy;
            numNeigh = itNeig->second.listNeigbours.size();
        }
    }
    // End actualize metrics. Now they sould select the best alternative.
    if (!actualizeMetric(rreq, val, metricToOrigin))
        metricToOrigin = 255;

    if (!isnan(etx)) {
        if (isnan(rreq->getEtx()))
            rreq->setEtx(etx);
        else
            rreq->setEtx(etx + rreq->getEtx());
    }
    if (!isnan(delay)) {
        if (isnan(rreq->getDelay()))
            rreq->setDelay(delay);
        else
            rreq->setDelay(delay + rreq->getDelay());
    }
    if (!isnan(energy)) {
        if (isnan(rreq->getEnergy()))
            rreq->setEnergy(energy);
        else if (rreq->getEnergy() > energy)
            rreq->setEnergy(energy);
    }

    if (!isnan(recpower)) {
        if (isnan(rreq->getRecPower()))
            rreq->setRecPower(recpower);
        else if (rreq->getRecPower() > recpower)
            rreq->setRecPower(recpower);
    }

    if (!isnan(snir)) {
        if (isnan(rreq->getSnir()))
            rreq->setSnir(snir);
        else if (rreq->getSnir() > snir)
            rreq->setSnir(snir);
    }

    if (numNeigh != -1) {
        if (rreq->getNumNeig() == -1)
            rreq->setNumNeig(numNeigh);
        else if (rreq->getNumNeig() < numNeigh)
            rreq->setNumNeig(numNeigh);
    }


    if (rreq->getHopLimit() != 0) // actualize
        rreq->setHopLimit(rreq->getHopLimit()-1);

    IRoute *previousHopRoute = routingTable->findBestMatchingRoute(sourceAddr);

    if (itTopsis == topsisResults.end() || (itTopsis != topsisResults.end() && itTopsis->second.nextAddress == sourceAddr)) {
        // extract the metric to actualize the cost to the next hop
        auto itNeig = neighbors.find(sourceAddr);
        if (itNeig != neighbors.end()) {
            // extract the data (cost) to the neighbor.
            if ((!previousHopRoute || previousHopRoute->getSource() != this) && !rreq->getAccumulate()) {
                // create without valid sequence number
                previousHopRoute = createRoute(sourceAddr, sourceAddr, 1, -1, true, simTime() + activeRouteTimeout, metricType, metricNeg);
                auto loadNgRouteData = check_and_cast<LoadNgRouteData *> (previousHopRoute->getProtocolData());
                actualizeCostUsingNeig(itNeig->second, loadNgRouteData);
            }
            else {
                if (previousHopRoute) {
                    auto loadNgRouteData = check_and_cast<LoadNgRouteData *> (previousHopRoute->getProtocolData());
                    actualizeCostUsingNeig(itNeig->second, loadNgRouteData);
                    loadNgRouteData->setUsingTopsis(false);
                    updateRoutingTable(previousHopRoute, sourceAddr, 1, loadNgRouteData->getDestSeqNum(), true, simTime() + activeRouteTimeout, metricType, metricNeg);
                }
            }
        }
        else {
            if ((!previousHopRoute || previousHopRoute->getSource() != this) && !rreq->getAccumulate()) {
                // create without valid sequence number
                previousHopRoute = createRoute(sourceAddr, sourceAddr, 1, -1, true, simTime() + activeRouteTimeout, metricType, metricNeg);
                //auto loadNgRouteData = check_and_cast<LoadNgRouteData *> (previousHopRoute->getProtocolData());
                //actualizeCostUsingNeig(itNeig->second, loadNgRouteData);

            }
            else {
                if (previousHopRoute) {
                    auto loadNgRouteData = check_and_cast<LoadNgRouteData *> (previousHopRoute->getProtocolData());
                    loadNgRouteData->setUsingTopsis(false);
                    updateRoutingTable(previousHopRoute, sourceAddr, 1, loadNgRouteData->getDestSeqNum(), true, simTime() + activeRouteTimeout, metricType, metricNeg);
                }
            }
        }
    }
    else {
        // TODO: check this, or should use the data of sourceAddr?
        if ((!previousHopRoute || previousHopRoute->getSource() != this) && !rreq->getAccumulate()) {
            // create without valid sequence number
            previousHopRoute = createRoute(sourceAddr, itTopsis->second.nextAddress, 1, -1, true, simTime() + activeRouteTimeout, metricType, metricNeg);
        }
        else {
            if (previousHopRoute) {
                auto loadNgRouteData = check_and_cast<LoadNgRouteData *> (previousHopRoute->getProtocolData());
                updateRoutingTable(previousHopRoute, itTopsis->second.nextAddress, 1, loadNgRouteData->getDestSeqNum(), true, simTime() + activeRouteTimeout, metricType, metricNeg);
            }
        }
    }

    // First, it first increments the hop count value in the RREQ by one, to
    // account for the new hop through the intermediate node.

    // check 11.1. (1)
    if (rreq->getOriginatorAddr() == getSelfIPAddress())
        return;

    rreq->setHopCount(rreq->getHopCount() + 1);
    if (itTopsis != topsisResults.end() && itTopsis->second.nextAddress != sourceAddr) {
        rreq->setHopCount(rreq->getHopCount() + 1); // increase a hop because we assume that the RREQ has arrive using the alternative node
    }

    // Then the node searches for a reverse route to the Originator IP Address (see
    // section 6.2), using longest-prefix matching.

    IRoute *reverseRoute = routingTable->findBestMatchingRoute(rreq->getOriginatorAddr());

    // If need be, the route is created, or updated using the Originator Sequence Number from the
    // RREQ in its routing table.
    //
    // When the reverse route is created or updated, the following actions on
    // the route are also carried out:
    //
    //   1. the Originator Sequence Number from the RREQ is compared to the
    //      corresponding destination sequence number in the route table entry
    //      and copied if greater than the existing value there
    //
    //   2. the valid sequence number field is set to true;
    //
    //   3. the next hop in the routing table becomes the node from which the
    //      RREQ was received (it is obtained from the source IP address in
    //      the IP header and is often not equal to the Originator IP Address
    //      field in the RREQ message);
    //
    //   4. the hop count is copied from the Hop Count in the RREQ message;
    //
    //   Whenever a RREQ message is received, the Lifetime of the reverse
    //   route entry for the Originator IP address is set to be the maximum of
    //   (ExistingLifetime, MinimalLifetime), where
    //
    //   MinimalLifetime = (current time + 2*NET_TRAVERSAL_TIME - 2*HopCount*NODE_TRAVERSAL_TIME).

    unsigned int hopCount = rreq->getHopCount();
    simtime_t minimalLifeTime = simTime() + 2 * netTraversalTime - 2 * hopCount * nodeTraversalTime;
    simtime_t newLifeTime = std::max(simTime(), minimalLifeTime);
    int rreqSeqNum = rreq->getSeqNum();

    double oldMetric = 255;
    auto prevNodeOrigin = sourceAddr;

    bool checkCost = true;
    std::deque<TopsisLoadNgNeigbors::CostAndRange> results;
    bool modified = false;

    auto itTopsisOrg = topsisResults.find(rreq->getOriginatorAddr());
    if (itTopsisOrg != topsisResults.end() && itTopsisOrg->second.nextAddress != sourceAddr) {
        delay = itTopsisOrg->second.delay;
        snir = itTopsisOrg->second.snir;
        recpower = itTopsisOrg->second.recPower;
        energy = itTopsisOrg->second.energy;
        etx = itTopsisOrg->second.etx;
        modified = true;
        metricToOrigin = etx;
        prevNodeOrigin = itTopsisOrg->second.nextAddress;
        // actualize in the packet with the correct metrics.

        rreq->setHopCount(itTopsisOrg->second.numHops);
        hopCount = itTopsisOrg->second.numHops;
        if (!setMetric(rreq, metricToOrigin))
            metricToOrigin = 255;
        rreq->setEtx(etx);
        rreq->setDelay(delay);
        rreq->setRecPower(recpower);
        rreq->setEnergy(energy);
        rreq->setSnir(snir);
        if (reverseRoute && reverseRoute->getNextHopAsGeneric() == itTopsisOrg->second.nextAddress)
            checkCost = false; // don't check the costs.
    }

    TopsisLoadNgNeigbors topsis;
    topsis.setExperiment(par("matrixExperiment"));
    // include the new data in listPrevRreqCost
    auto itPrevRreq = listPrevRreqCost.find(rreq->getOriginatorAddr());
    auto topsisCost = topsis.addCost(
            rreq->getOriginatorAddr(), prevNodeOrigin,
            rreq->getEtx(), rreq->getNumNeig(),
            rreq->getSnir(), rreq->getRecPower(), rreq->getEnergy(),
            rreq->getHopCount(), rreq->getDelay());
    if (itPrevRreq == listPrevRreqCost.end())  {
        CostInfo costInfo;
        costInfo.list.push_back(topsisCost);
        costInfo.seqNum = rreqSeqNum;
        costInfo.index = -1;
        listPrevRreqCost[rreq->getOriginatorAddr()] = costInfo;
        itPrevRreq = listPrevRreqCost.find(rreq->getOriginatorAddr());
    }
    else if (itPrevRreq->second.seqNum < rreqSeqNum) {
        itPrevRreq->second.index = -1;
        itPrevRreq->second.list.clear();
        itPrevRreq->second.list.push_back(topsisCost);
        itPrevRreq->second.seqNum = rreqSeqNum;
    }
    else if (itPrevRreq->second.seqNum == rreqSeqNum) {
        // search if exist
        // TODO: Adapt to multiple RREQ sends by intermediate nodes
        auto it = std::find(itPrevRreq->second.list.begin(), itPrevRreq->second.list.end(), topsisCost);
        if (it == itPrevRreq->second.list.end())
            itPrevRreq->second.list.push_back(topsisCost);
        else {
            (*it) = topsisCost;
        }
    }
    else {
        throw cRuntimeError("Error in data base, Problems with sequence numbers itPrevRreq->second.seqNum > rreqSeqNum");
    }

    if (nextHopAddr == this->getSelfIPAddress() && RREQUNI_RERR) {
        itPrevRreq->second.unicastAddr = true;
        itPrevRreq->second.prevAddress = sourceAddr;
    }

    if ((!reverseRoute || reverseRoute->getSource() != this) && !rreq->getAccumulate()) {    // create
        // This reverse route will be needed if the node receives a RREP back to the
        // node that originated the RREQ (identified by the Originator IP Address).
        reverseRoute = createRoute(rreq->getOriginatorAddr(), prevNodeOrigin, hopCount, rreqSeqNum, true, newLifeTime,  metricType, metricToOrigin);
        LoadNgRouteData *routeData = check_and_cast<LoadNgRouteData *>(reverseRoute->getProtocolData());
        actualizeCost(rreq, routeData);
    }
    else {
        if (reverseRoute) {
            LoadNgRouteData *routeData = check_and_cast<LoadNgRouteData *>(reverseRoute->getProtocolData());
            int routeSeqNum = routeData->getDestSeqNum();
            oldMetric = routeData->getMetric();
            oldSeqNum = routeSeqNum;

            int newSeqNum = std::max(routeSeqNum, rreqSeqNum);
          //  int newHopCount = rreq->getHopCount(); // Note: already incremented by 1.
          //  int routeHopCount = reverseRoute->getMetric();

            // The route is only updated if the new sequence number is either
            //
            //   (i)       higher than the destination sequence number in the route
            //             table, or
            //
            //   (ii)      the sequence numbers are equal, but the hop count (of the
            //             new information) plus one, is smaller than the existing hop
            //             count in the routing table, or
            //
            //   (iii)     the sequence number is unknown.

            topsis.clear();

            if (newSeqNum <= rreqSeqNum) {
                // the sequence number in the
                for (auto elem : itPrevRreq->second.list) {
                    topsis.addCost(elem);
                }
            }

            if (topsis.numElements() > 1)
                topsis.runTopsisRoutes(results);
            else
                topsis.runTopsisRoutes(results);

            if (rreqSeqNum > routeSeqNum) {
                actualizeCost(rreq, routeData);
                routeData->setUsingTopsis(false);
                updateRoutingTable(reverseRoute, prevNodeOrigin, hopCount, newSeqNum, true, newLifeTime, metricType, metricToOrigin);
            }
            else if (rreqSeqNum == routeSeqNum) {
                // Analyze the topsis results
                if (!results.empty() && results.size() > 1) {
                    if (results.front().index == (int)results.size() - 1) { // it is the information of this RREQ that is the latest in to be included
                        actualizeCost(rreq, routeData);
                        routeData->setUsingTopsis(false);
                        updateRoutingTable(reverseRoute, prevNodeOrigin, hopCount, newSeqNum, true, newLifeTime, metricType, metricToOrigin);
                    }
                }
            }
            /*
            if (rreqSeqNum > routeSeqNum || (rreqSeqNum == routeSeqNum && metricToOrigin < oldMetric)
                    || (rreqSeqNum == routeSeqNum && metricToOrigin == oldMetric && newHopCount < routeHopCount) || checkCost(rreq, routeData)) {
                actualizeCost(rreq, routeData);
                routeData->setUsingTopsis(false);
                updateRoutingTable(reverseRoute, prevNodeOrigin, hopCount, newSeqNum, true, newLifeTime, metricType, metricToOrigin);
            }
           */
        }
    }

    bool sendRreq = false;
    bool sendRreqOportunistic = false;
    if (results.size() > 1) {
        // check the range
        if (results.front().index == (int)results.size() - 1) {
            auto itPrevRreq = listPrevRreqCost.find(rreq->getOriginatorAddr());
            // first time
            for (auto elem : results) {
                if ((-1 == itPrevRreq->second.index && 0 == elem.index) || (itPrevRreq->second.index == elem.index)) {
                    // check range difference
                    if ((std::abs(double(results.front().range - elem.range))/elem.range) > par("rangeDiference").doubleValue()) {
                        sendRreq = true;
                        itPrevRreq->second.index = results.front().index;
                    }
                    sendRreqOportunistic = true;
                    break;
                }
            }
            if (sendRreqOportunistic) {
                sendRreqOportunistic = false;
                for (auto &elem : pendingSend) {
                    auto pkt = elem->getOwnedPacketForUpdate();
                    const auto header = pkt->peekAtFront<FieldsChunk>();
                    const auto rreqAux = dynamicPtrCast<const Rreq>(header);
                    if (rreqAux) {
                        if (rreqAux->getOriginatorAddr() ==  rreq->getOriginatorAddr() &&  rreqAux->getSeqNum() == rreq->getSeqNum()) {
                            sendRreqOportunistic = true;
                            sendRreq = false;
                            break;
                        }
                    }
                }
            }
        }
    }

    // A node generates a RREP if either:
    //
    // (i)       it is itself the destination, or
    //

    // check (i)
    if (rreq->getDestAddr() == getSelfIPAddress() && (oldSeqNum < rreq->getSeqNum() ||
            (oldSeqNum == rreq->getSeqNum() && sendRreq && par("multipleRrep").boolValue()))) {
        EV_INFO << "I am the destination node for which the route was requested" << endl;

        if (rreq->getAccumulate() && !rreq->getIncludeRoute()) {

            auto rreqR = createRREQ(rreq->getOriginatorAddr());
            rreqR->setAccumulate(true);
            rreqR->setIncludeRoute(true);
            rreqR->setAccumulateAddressArraySize(rreq->getAccumulateAddressArraySize());
            for (int i = 0; i < (int)rreq->getAccumulateAddressArraySize(); i ++){
                rreqR->setAccumulateAddress(i, rreq->getAccumulateAddress(i));
            }
            rreqR->setChunkLength(rreq->getChunkLength()+B(rreq->getAccumulateAddressArraySize()));
            rreqR->setHopLimit(netDiameter);
            forwardRREQ(rreqR);
            //sendRREQ(rreqR, addressType->getBroadcastAddress(), 0);
        }
        else if (rreq->getAccumulate() && rreq->getIncludeRoute()) {
            // Special RREP
            auto rrep = createRREP(rreq);
            rrep->setAccumulate(true);
            rrep->setIncludeRoute(true);
            rrep->setHopLimit(par("maxHopLimit"));
            rrep->setAccumulateAddressArraySize(rreq->getAccumulateAddressArraySize());
            for (auto i = 0; i < (int)rreq->getAccumulateAddressArraySize(); i ++){
                rrep->setAccumulateAddress(i, rreq->getAccumulateAddress(i));
            }
            rrep->setRouteArraySize(rreq->getRouteArraySize());
            for (auto i = 0; i < (int)rreq->getRouteArraySize(); i ++){
                rrep->setRoute(i, rreq->getRoute(i));
            }
            rrep->setChunkLength(rreq->getChunkLength()+B(rreq->getAccumulateAddressArraySize())+B(rreq->getRouteArraySize()));
            rrep->setSeqNumDest(rreq->getSeqNum());
            sendRREP(rrep, rreq->getOriginatorAddr(), 255, -1);

            //Store the data  in memory, introduce a small delay before send the stored packets.
            SpecialPath path;
            path.lifetime = simTime() + 2 * netTraversalTime - 2 * (rreq->getAccumulateAddressArraySize()+1) * nodeTraversalTime;
            for (auto i = 0; i < (int)rreq->getAccumulateAddressArraySize(); i ++){
                path.path.push_back(rrep->getAccumulateAddress(i));
            }

            specialPaths[rreq->getOriginatorAddr()] = path;
            if (hasOngoingRouteDiscovery(rreq->getOriginatorAddr())) {
                EV_INFO << "The Route Reply has arrived for our Route Request to node " << rreq->getOriginatorAddr() << endl;
                completeRouteDiscoveryDelayed(rreq->getOriginatorAddr());
            }
        }
        else {
            // create RREP
            auto rrep = createRREP(rreq);
            rrep->setHopLimit(par("maxHopLimit"));
            //LoadNgRouteData *routeData = check_and_cast<LoadNgRouteData *>(reverseRoute->getProtocolData());
            //rrep->setRecPower(routeData->getRecPower());
            //rrep->setEnergy(routeData->getEnergy());
            //rrep->setDelay(routeData->getDelay());
            //rrep->setSnir(routeData->getSnir());
            //rrep->setEtx(routeData->getEtx());
            //rrep->setNumNeig(routeData->getNumNeig());
            // send to the originator
            sendRREP(rrep, rreq->getOriginatorAddr(), 255, -1);
        }

        return;    // discard RREQ, in this case, we do not forward it.
    }
    else if (oldSeqNum == rreq->getSeqNum() && sendRreq) {
        // Propagate if the new path is better?, now it don't propagate it.
        // Search the packet in the delayed list to actualize it
        // No, it won't work
        // search if it is possible to replace the packet in the delay list
        double time = simTime().dbl();
        EV_WARN << time << "Send grat RREQ : " <<  rreq->getOriginatorAddr() << endl;
        return;
        /*
        for (auto &elem : pendingSend) {
            auto pkt = elem->getOwnedPacketForUpdate();
            const auto header = pkt->peekAtFront<FieldsChunk>();
            const auto rreqAux = dynamicPtrCast<const Rreq>(header);
            if (rreqAux) {
                if (rreqAux->getOriginatorAddr() ==  rreq->getOriginatorAddr() &&  rreqAux->getSeqNum() == rreq->getSeqNum()) {
                    // modify
                    pkt->removeAtFront<Rreq>();
                    pkt->insertAtFront(rreq->dupShared());
                    return;
                }
            }
        }
        */
        // Opportunistic failed, must send the new RREQ.
    }
    else if (oldSeqNum == rreq->getSeqNum() && sendRreqOportunistic) {
        double time = simTime().dbl();
        EV_WARN << time << "Opportunistic RREQ : " <<  rreq->getOriginatorAddr() << endl;
        for (auto &elem : pendingSend) {
            auto pkt = elem->getOwnedPacketForUpdate();
            const auto header = pkt->peekAtFront<FieldsChunk>();
            const auto rreqAux = dynamicPtrCast<const Rreq>(header);
            if (rreqAux) {
                if (rreqAux->getOriginatorAddr() ==  rreq->getOriginatorAddr() &&  rreqAux->getSeqNum() == rreq->getSeqNum()){
                    // modify
                    pkt->removeAtFront<Rreq>();
                    pkt->insertAtFront(rreq->dupShared());
                    break;
                }
            }
        }
        return; // don't propagate,
    }
    else if (oldSeqNum >= rreq->getSeqNum()) {
        return;
    }

    if (noPropagateRreq) {
        return;
    // no propagate the RREQ, only should be acive if checkStationary = false, mobileNodesNoPropagateRreq = true,
    // mobility module of type RandomWaypointMobility2 and Active = "ON"
    }

    // If a node does not generate a RREP (following the processing rules in
    // section 6.6), and if the incoming IP header has TTL larger than 1,
    // the node updates and broadcasts the RREQ to address 255.255.255.255
    // on each of its configured interfaces (see section 6.14).  To update
    // the RREQ, the TTL or hop limit field in the outgoing IP header is
    // decreased by one, and the Hop Count field in the RREQ message is
    // incremented by one, to account for the new hop through the
    // intermediate node. (!) Lastly, the Destination Sequence number for the
    // requested destination is set to the maximum of the corresponding
    // value received in the RREQ message, and the destination sequence
    // value currently maintained by the node for the requested destination.
    // However, the forwarding node MUST NOT modify its maintained value for
    // the destination sequence number, even if the value received in the
    // incoming RREQ is larger than the value currently maintained by the
    // forwarding node.

    if (simTime() > rebootTime + deletePeriod || rebootTime == 0) {
        auto outgoingRREQ = dynamicPtrCast<Rreq>(rreq->dupShared());
        forwardRREQ(outgoingRREQ);
    }
    else
        EV_WARN << " LoadNg reboot has not completed yet" << endl;
}




#if 0
void LoadNgTopsis::actualizeBreakRoutes() {
    for (int i = 0; i < routingTable->getNumRoutes(); i++) {
        IRoute *route = routingTable->getRoute(i);
        LoadNgRouteData *routeData = dynamic_cast<LoadNgRouteData *>(route->getProtocolData());
        if (routeData && routeData->isActive() && route->getNextHopAsGeneric() == unreachableAddr) {
            if (routeData->hasValidDestNum())
                routeData->setDestSeqNum(routeData->getDestSeqNum() + 1);
            EV_DETAIL << "Marking route to " << route->getDestinationAsGeneric() << " as inactive" << endl;
            routeData->setIsActive(false);
            routeData->setLifeTime(simTime() + deletePeriod);
            scheduleExpungeRoutes();
            UnreachableNode node;
            node.addr = route->getDestinationAsGeneric();
            node.seqNum = routeData->getDestSeqNum();
            unreachableNodes.push_back(node);
        }
    }
}
#endif

void LoadNgTopsis::inectPacket(Packet *datagram)
{
    const auto& networkHeader = findNetworkProtocolHeader(datagram);
    if (networkHeader == nullptr)
        return;
    auto pkt = datagram->dup();

    //pkt->copyTags(*datagram);
    PacketDissector::PduTreeBuilder pduTreeBuilder;
    auto packetProtocolTag = datagram->findTag<PacketProtocolTag>();
    auto protocol = packetProtocolTag != nullptr ? packetProtocolTag->getProtocol() : nullptr;
    PacketDissector packetDissector(ProtocolDissectorRegistry::getInstance(), pduTreeBuilder);
    packetDissector.dissectPacket(const_cast<Packet *>(datagram), protocol);
    auto& protocolDataUnit = pduTreeBuilder.getTopLevelPdu();
    auto chunkList = protocolDataUnit->getChunks();

    if (chunkList.front() != networkHeader) {
        pkt->eraseAll();
        for (const auto &chunk : chunkList) {
            if (auto childLevel = dynamicPtrCast<const PacketDissector::ProtocolDataUnit>(chunk)) {
                auto chunkListAux = childLevel->getChunks();
                for (const auto &chunkAux : chunkListAux) {
                    if (chunkAux->getChunkType() == Chunk::CT_SEQUENCE) {
                        // remove previous headers to ipv4
                        bool removed = false;
                        for (const auto &elementChunk : staticPtrCast<const SequenceChunk>(chunkAux)->getChunks()) {
                            if (elementChunk == networkHeader) {
                                removed = true;
                                auto header = networkHeader->dupShared();
                                header->addTagIfAbsent<RetransmitTag>()->setRetransmitNode(this->getSelfIPAddress());
                                insertNetworkProtocolHeader(pkt, Protocol::ipv4, dynamicPtrCast<NetworkHeaderBase>(header));
                            }
                            else if (removed)
                                pkt->insertAtBack(elementChunk->dupShared());
                        }
                    }
                }
            }
            else if (chunk->getChunkType() == Chunk::CT_SEQUENCE) {
                if (staticPtrCast<const SequenceChunk>(chunk)->getChunks().front() != networkHeader) {
                    delete pkt;
                    return;
                }
                for (const auto &elementChunk : staticPtrCast<const SequenceChunk>(chunk)->getChunks()) {
                    if (elementChunk == networkHeader) {
                        insertNetworkProtocolHeader(pkt, Protocol::ipv4, dynamicPtrCast<NetworkHeaderBase>(networkHeader->dupShared()));
                    }
                    else
                        pkt->insertAtBack(elementChunk->dupShared());
                }
            }
        }
    }
    pkt->addTagIfAbsent<PacketProtocolTag>()->setProtocol(&Protocol::ipv4);
    NetworkInterface *ifEntry = interfaceTable->findInterfaceByName("wlan0");
    pkt->addTagIfAbsent<InterfaceInd>()->setInterfaceId(ifEntry->getInterfaceId());
    networkProtocol->enqueueRoutingHook(pkt, IHook::Type::PREROUTING);
    networkProtocol->reinjectQueuedDatagram(pkt);
}


void LoadNgTopsis::receiveSignal(cComponent *source, simsignal_t signalID, cObject *obj, cObject *details)
{
    Enter_Method("receiveChangeNotification");
    if (signalID == recomputeSignal) {
        if (!recomputeTimer->isScheduled()) {
            scheduleAt(simTime(), recomputeTimer);
        }
    }
    else if (signalID == linkBrokenSignal) {

        Packet *datagram = check_and_cast<Packet *>(obj);
        EV_DETAIL << "Received link break signal" << datagram << endl;

        const auto& networkHeader = findNetworkProtocolHeader(datagram);
        if (networkHeader != nullptr) {
            L3Address unreachableAddr = networkHeader->getDestinationAddress();
            L3Address sourceAddress = networkHeader->getSourceAddress();
            if (unreachableAddr.getAddressType() == addressType) {
                // A node initiates processing for a RERR message in three situations:
                //
                //   (i)     if it detects a link break for the next hop of an active
                //           route in its routing table while transmitting data (and
                //           route repair, if attempted, was unsuccessful), or

                // TODO: Implement: local repair
                // check if the node is accessible using Dijkstra

                IRoute *route = routingTable->findBestMatchingRoute(unreachableAddr);
                if (route == nullptr) {
                    //if (!routingTable->isLocalAddress(sourceAddress))
                    //     sendRERRWhenNoRouteToForward(unreachableAddr, sourceAddress);
                    return;
                }
                // TODO: Check Topsis route
                bool isStaticLink = false;
                if (par("stationaryNetwork").boolValue())
                    isStaticLink = true;
                else {
                    if (stationary && route) {
                        auto itAux = neighbors.find(route->getNextHopAsGeneric());
                        if (itAux != neighbors.end() && itAux->second.stationary)
                            isStaticLink =true;
                    }
                }

                if (!isStaticLink) {
                    if (route && route->getSource() == this) {
                        // set the neig to error
                        auto it = neighbors.find(route->getNextHopAsGeneric());
                        if (it != neighbors.end()) {
                            it->second.pendingHello = true;
                            it->second.pendingConfirmation = true;
                        }
                        // invalidate the next hop route.
                        auto nextAddress = route->getNextHopAsGeneric();

                        std::set<L3Address> unreachableAddresses;

                        for (int i = 0; i < routingTable->getNumRoutes(); i++) {
                            auto rt = routingTable->getRoute(i);
                            if (rt->getSource() == this && rt->getNextHopAsGeneric() == nextAddress) {
                                unreachableAddresses.insert(rt->getDestinationAsGeneric());
                            }
                        }

                        // compute possible new route
                        runTopsisNeigborsNow();
                        // check if there is now a valid address
                        route = routingTable->findBestMatchingRoute(unreachableAddr);
                        sendGratHelloMessages();
                        //rebuildDijkstraWihtHellos();
                        // std::vector<L3Address> pathNode;
                        auto itTopsis = topsisResults.find(unreachableAddr);
                        //if (dijkstra->getRoute(unreachableAddr, pathNode)) {

                        // try to reinject the packet
                        if (itTopsis != topsisResults.end()) {
                            auto it = neighbors.find(itTopsis->second.nextAddress);
                            if (it != neighbors.end() && it->second.isBidirectional && !it->second.pendingConfirmation) {

                                // remove this address from the unreachable address list
                                auto itAuxSet = unreachableAddresses.find(unreachableAddr);
                                if (itAuxSet != unreachableAddresses.end())
                                    unreachableAddresses.erase(itAuxSet);

                                // alternative route
                                if (route->getNextHopAsGeneric() != itTopsis->second.nextAddress)
                                    throw cRuntimeError("Check run dijkstra");
                                // re-inject the packet
                                // Now remove all the headers that are before the network header
                                // first check the first header
                                auto chuckFirst = datagram->peekAtFront<Chunk>();
                                bool isIpv4 = false;
                                bool sendError = false;
                               // bool ipFirst = false;
                                if (dynamic_cast<const Ipv4Header *>(chuckFirst.get()) != nullptr) {
                                    isIpv4 = true;
                                   // ipFirst = true;
                                }
                                else {
                                    PacketDissector::PduTreeBuilder pduTreeBuilder;
                                    auto packetProtocolTag = datagram->findTag<PacketProtocolTag>();
                                    auto protocol = packetProtocolTag != nullptr ? packetProtocolTag->getProtocol() : nullptr;
                                    PacketDissector packetDissector(ProtocolDissectorRegistry::getInstance(), pduTreeBuilder);
                                    packetDissector.dissectPacket(const_cast<Packet *>(datagram), protocol);
                                    auto& protocolDataUnit = pduTreeBuilder.getTopLevelPdu();
                                    for (const auto& chunk : protocolDataUnit->getChunks()) {
                                        if (auto childLevel = dynamicPtrCast<const PacketDissector::ProtocolDataUnit>(chunk)) {
                                            for (const auto& chunkAux : childLevel->getChunks()) {
                                                if (chunkAux->getChunkType() == Chunk::CT_SEQUENCE) {
                                                    for (const auto& elementChunk : staticPtrCast<const SequenceChunk>(chunkAux)->getChunks()) {
                                                        if (dynamic_cast<const Ipv4Header *>(elementChunk.get())) {
                                                            isIpv4 = true;
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                        else if (chunk->getChunkType()== Chunk::CT_SEQUENCE) {
                                            for (const auto& elementChunk : staticPtrCast<const SequenceChunk>(chunk)->getChunks()) {
                                                if (dynamic_cast<const Ipv4Header *>(elementChunk.get())) {
                                                    isIpv4 = true;
                                                }
                                            }
                                        }
                                    }
                                }


                                if (sendError) {
                                    if (route && route->getSource() == this) {
                                        handleLinkBreakSendRERR(route->getNextHopAsGeneric(), sourceAddress, unreachableAddr);
                                    }
                                    return;
                                }

                                if (isIpv4)
                                    inectPacket(datagram);
                            }
                        }
                        if (!routingTable->isLocalAddress(sourceAddress))
                            handleLinkBreakSendRERRSet(unreachableAddresses, sourceAddress);
                    }
                }
                else {
                    // Don't modify the network using signal, only hellos
                    // create a copy of this packet, but only with the network headers and below.
                    if (route) {
                        auto it = neighbors.find(route->getNextHopAsGeneric());
                        if (it != neighbors.end()) {
                            auto it2 = lastInjection.find(route->getNextHopAsGeneric());
                            if (it2 == lastInjection.end() || (simTime() - it2->second) > 0.0001) {
                                lastInjection[route->getNextHopAsGeneric()]= simTime();
                                inectPacket(datagram);
                            }
                            return;
                        }
                    }
                }
            }
        }
    }
}

void LoadNgTopsis::handleLinkBreakSendRERRSet(std::set<L3Address>& unreachableAddr, const L3Address& source)
{
    // If local address Destination or source ignore
    if (routingTable->isLocalAddress(source))
        return;

    for (auto it = unreachableAddr.begin(); it != unreachableAddr.begin();) {
        auto rt = routingTable->findBestMatchingRoute(*it);

        if (rt != nullptr) {
            LoadNgRouteData *routeData = dynamic_cast<LoadNgRouteData *>(rt->getProtocolData());
            if (routeData->isActive()) {
                auto itAux = neighbors.find(rt->getNextHopAsGeneric());
                if (itAux != neighbors.end() && itAux->second.isBidirectional && !itAux->second.pendingConfirmation) {
                    unreachableAddr.erase(it++);
                    continue;
                }
                else {
                    routeData->setIsActive(false);
                    routeData->setLifeTime(simTime() + deletePeriod);
                    scheduleExpungeRoutes();
                }
            }
        }
        ++it;
    }

    if (unreachableAddr.empty())
        return;

    std::vector<UnreachableNode> unreachableNodes;
    for (auto elem: unreachableAddr) {
        UnreachableNode node;
        node.addr = elem;
        auto itAux = seqNumbers.find(elem);
        if (itAux != seqNumbers.end())
            node.seqNum = itAux->second;

    }

    // For case (i), the node first makes a list of unreachable destinations
    // consisting of the unreachable neighbor and any additional
    // destinations (or subnets, see section 7) in the local routing table
    // that use the unreachable neighbor as the next hop.

    // Just before transmitting the RERR, certain updates are made on the
    // routing table that may affect the destination sequence numbers for
    // the unreachable destinations.  For each one of these destinations,
    // the corresponding routing table entry is updated as follows:
    //
    // 1. The destination sequence number of this routing entry, if it
    //    exists and is valid, is incremented for cases (i) and (ii) above,
    //    and copied from the incoming RERR in case (iii) above.
    //
    // 2. The entry is invalidated by marking the route entry as invalid
    //
    // 3. The Lifetime field is updated to current time plus DELETE_PERIOD.
    //    Before this time, the entry SHOULD NOT be deleted.

    if (rerrCount >= rerrRatelimit) {
        EV_WARN << "A node should not generate more than RERR_RATELIMIT RERR messages per second. Canceling sending RERR" << endl;
        return;
    }

    if (unreachableNodes.empty())
        return;

    // extract the route to the previous node
    auto nextAddress = addressType->getBroadcastAddress();
    IRoute *route = routingTable->findBestMatchingRoute(source);
    int ttl = 1;
     if (route && route->getSource() == this) {
         LoadNgRouteData *routeData = check_and_cast<LoadNgRouteData *>(route->getProtocolData());
         if (routeData->isActive()) {
             nextAddress = route->getNextHopAsGeneric();
             ttl = route->getMetric();
         }
     }
     if (nextAddress.isBroadcast()) // Only unicast
         return;

     auto rerr = createRERR(unreachableNodes);
     rerr->setOriginatorAddr(this->getSelfIPAddress());
     rerr->setDestAddr(source);
     rerrCount++;
     EV_INFO << " Route Error message to " << nextAddress  << endl;
     sendLoadNgPacket(rerr, nextAddress, ttl, 0);
}


void LoadNgTopsis::handleLinkBreakSendRERR(const L3Address& unreachableAddr, const L3Address& source, const L3Address& destination)
{

    // If local address Destination or source ignore
    if (routingTable->isLocalAddress(source) || routingTable->isLocalAddress(destination))
        return;

    // For case (i), the node first makes a list of unreachable destinations
    // consisting of the unreachable neighbor and any additional
    // destinations (or subnets, see section 7) in the local routing table
    // that use the unreachable neighbor as the next hop.

    // Just before transmitting the RERR, certain updates are made on the
    // routing table that may affect the destination sequence numbers for
    // the unreachable destinations.  For each one of these destinations,
    // the corresponding routing table entry is updated as follows:
    //
    // 1. The destination sequence number of this routing entry, if it
    //    exists and is valid, is incremented for cases (i) and (ii) above,
    //    and copied from the incoming RERR in case (iii) above.
    //
    // 2. The entry is invalidated by marking the route entry as invalid
    //
    // 3. The Lifetime field is updated to current time plus DELETE_PERIOD.
    //    Before this time, the entry SHOULD NOT be deleted.

    IRoute *unreachableRoute = routingTable->findBestMatchingRoute(unreachableAddr);

    if (!unreachableRoute || unreachableRoute->getSource() != this)
        return;

    std::vector<UnreachableNode> unreachableNodes;
    LoadNgRouteData *unreachableRouteData = check_and_cast<LoadNgRouteData *>(unreachableRoute->getProtocolData());

    if (unreachableRouteData->isActive()) {
        UnreachableNode node;
        node.addr = unreachableAddr;
        node.seqNum = unreachableRouteData->getDestSeqNum();
        unreachableNodes.push_back(node);
    }

    // For case (i), the node first makes a list of unreachable destinations
    // consisting of the unreachable neighbor and any additional destinations
    // (or subnets, see section 7) in the local routing table that use the
    // unreachable neighbor as the next hop.

    for (int i = 0; i < routingTable->getNumRoutes(); i++) {
        IRoute *route = routingTable->getRoute(i);

        LoadNgRouteData *routeData = dynamic_cast<LoadNgRouteData *>(route->getProtocolData());
        if (routeData && routeData->isActive() && route->getNextHopAsGeneric() == unreachableAddr) {
         //   if (routeData->hasValidDestNum())
         //       routeData->setDestSeqNum(routeData->getDestSeqNum() + 1);

            EV_DETAIL << "Marking route to " << route->getDestinationAsGeneric() << " as inactive" << endl;

            routeData->setIsActive(false);
            routeData->setLifeTime(simTime() + deletePeriod);
            scheduleExpungeRoutes();

            UnreachableNode node;
            node.addr = route->getDestinationAsGeneric();
            node.seqNum = routeData->getDestSeqNum();
            unreachableNodes.push_back(node);
        }
    }

    // The neighboring node(s) that should receive the RERR are all those
    // that belong to a precursor list of at least one of the unreachable
    // destination(s) in the newly created RERR.  In case there is only one
    // unique neighbor that needs to receive the RERR, the RERR SHOULD be
    // unicast toward that neighbor.  Otherwise the RERR is typically sent
    // to the local broadcast address (Destination IP == 255.255.255.255,
    // TTL == 1) with the unreachable destinations, and their corresponding
    // destination sequence numbers, included in the packet.

    if (rerrCount >= rerrRatelimit) {
        EV_WARN << "A node should not generate more than RERR_RATELIMIT RERR messages per second. Canceling sending RERR" << endl;
        return;
    }

    if (unreachableNodes.empty())
        return;

    // extract the route to the previous node
    auto nextAddress = addressType->getBroadcastAddress();
    IRoute *route = routingTable->findBestMatchingRoute(source);
    int ttl = 1;
     if (route && route->getSource() == this) {
         LoadNgRouteData *routeData = check_and_cast<LoadNgRouteData *>(route->getProtocolData());
         if (routeData->isActive()) {
             nextAddress = route->getNextHopAsGeneric();
             ttl = route->getMetric();
         }
     }
     if (nextAddress.isBroadcast()) // Only unicast
         return;

     auto rerr = createRERR(unreachableNodes);
     rerr->setOriginatorAddr(this->getSelfIPAddress());
     rerr->setDestAddr(source);

    rerrCount++;

    EV_INFO << " Route Error message to " << nextAddress  << endl;
    sendLoadNgPacket(rerr, nextAddress, ttl, 0);
}

const Ptr<Rerr> LoadNgTopsis::createRERR(const std::vector<UnreachableNode>& unreachableNodes)
{
    auto rerr = makeShared<Rerr>(); // TODO: "AODV-RERR");
    unsigned int destCount = unreachableNodes.size();

    rerr->setPacketType(RERR);
    rerr->setUnreachableNodesArraySize(destCount);
    rerr->setHopLimit(par("maxHopLimit"));

    for (unsigned int i = 0; i < destCount; i++) {
        UnreachableNode node;
        node.addr = unreachableNodes[i].addr;
        node.seqNum = unreachableNodes[i].seqNum;
        rerr->setUnreachableNodes(i, node);
    }

    rerr->setChunkLength(B(4 + 4 * 2 * destCount));
    return rerr;
}

void LoadNgTopsis::handleRERR(const Ptr<const Rerr>& rerr, const L3Address& sourceAddr)
{
    EV_INFO << "LoadNg Route Error arrived with source addr: " << sourceAddr << endl;

    // A node initiates processing for a RERR message in three situations:
    // (iii)   if it receives a RERR from a neighbor for one or more
    //         active routes.

    auto originator = rerr->getOriginatorAddr();

    unsigned int unreachableArraySize = rerr->getUnreachableNodesArraySize();

    std::vector<UnreachableNode> unreachableNeighbors;
    auto itNeig = neighbors.find(originator);
    bool runTopsis = false;
    if (itNeig != neighbors.end()) {
        for (unsigned int j = 0; j < unreachableArraySize; j++) {
            auto itAux = itNeig->second.listNeigbours.find(rerr->getUnreachableNodes(j).addr);
            if (itAux != itNeig->second.listNeigbours.end()) {
                itNeig->second.listNeigbours.erase(itAux);
                runTopsis = true;
            }
        }
    }

    for (int i = 0; i < routingTable->getNumRoutes(); i++) {
        IRoute *route = routingTable->getRoute(i);
        LoadNgRouteData *routeData = route ? dynamic_cast<LoadNgRouteData *>(route->getProtocolData()) : nullptr;

        if (!routeData)
            continue;

        // For case (iii), the list should consist of those destinations in the RERR
        // for which there exists a corresponding entry in the local routing
        // table that has the transmitter of the received RERR as the next hop.

        if (routeData->isActive() && route->getNextHopAsGeneric() == sourceAddr) {
            for (unsigned int j = 0; j < unreachableArraySize; j++) {
                if (route->getDestinationAsGeneric() == rerr->getUnreachableNodes(j).addr) {
                    // 1. The destination sequence number of this routing entry, if it
                    // exists and is valid, is incremented for cases (i) and (ii) above,
                    // ! and copied from the incoming RERR in case (iii) above.

                    auto it = seqNumbers.find(route->getDestinationAsGeneric());
                    if (it != seqNumbers.end() && rerr->getUnreachableNodes(j).seqNum < it->second)
                        continue; // ignore older seq num

                    routeData->setDestSeqNum(rerr->getUnreachableNodes(j).seqNum);
                    routeData->setIsActive(false);    // it means invalid, see 3. AODV Terminology p.3. in RFC 3561
                    routeData->setLifeTime(simTime() + deletePeriod);

                    // The RERR should contain those destinations that are part of
                    // the created list of unreachable destinations and have a non-empty
                    // precursor list.

                    //if (routeData->getPrecursorList().size() > 0) {
                    //    UnreachableNode node;
                    //    node.addr = route->getDestinationAsGeneric();
                    //    node.seqNum = routeData->getDestSeqNum();
                    //    unreachableNeighbors.push_back(node);
                    //}
                    // Delete the link from the list.

                    scheduleExpungeRoutes();
                }
            }
        }
    }

    if (runTopsis)
        runTopsisNeigborsNow();

    if (rerrCount >= rerrRatelimit) {
        EV_WARN << "A node should not generate more than RERR_RATELIMIT RERR messages per second. Canceling sending RERR" << endl;
        return;
    }

    if (routingTable->isLocalAddress(rerr->getDestAddr())) // No propagate more
        return;

    if (unreachableNeighbors.size() > 0
            && (simTime() > rebootTime + deletePeriod || rebootTime == 0)) {
        EV_INFO << "Sending RERR to inform our neighbors about link breaks."
                       << endl;
        if (rerr->getHopLimit() > 1) {


            // extract the route to the previous node
            auto nextAddress = addressType->getBroadcastAddress();
            IRoute *route = routingTable->findBestMatchingRoute(rerr->getDestAddr());
            int ttl = 1;
            if (route && route->getSource() == this) {
                LoadNgRouteData *routeData = check_and_cast<LoadNgRouteData *>(
                        route->getProtocolData());
                if (routeData->isActive()) {
                    nextAddress = route->getNextHopAsGeneric();
                    ttl = route->getMetric();
                }
            }

            if (nextAddress.isBroadcast()) // Only unicast
                return;
            if (!rerr->getDestAddr().isBroadcast()
                    && routingTable->isLocalAddress(rerr->getDestAddr()))
                return;

            auto newRERR = createRERR(unreachableNeighbors);
            newRERR->setOriginatorAddr(rerr->getOriginatorAddr());
            newRERR->setDestAddr(rerr->getDestAddr());

            if (rerr->getHopLimit() != 0)
                newRERR->setHopLimit(rerr->getHopLimit() - 1);

            sendLoadNgPacket(newRERR, nextAddress, ttl, 0);
            rerrCount++;
        }
    }
}

void LoadNgTopsis::handleStartOperation(LifecycleOperation *operation)
{
    rebootTime = simTime();

    // RFC 5148:
    // Jitter SHOULD be applied by reducing this delay by a random amount, so that
    // the delay between consecutive transmissions of messages of the same type is
    // equal to (MESSAGE_INTERVAL - jitter), where jitter is the random value.
    if (useHelloMessages) {
        simtime_t nextHello = simTime() + helloInterval - *periodicJitter;
        /*
        if (nextHello+0.001 > nextRecTime) {
            nextRecTime = nextHello + 0.001;
            emit(nextRecSignal, nextRecTime);
            scheduleAt(nextRecTime, recomputeTopsis);
        }
        */
        scheduleAt(nextHello, helloMsgTimer);
    }

    if (par("IsRoot").boolValue()) {
        isRoot = true;
        periodicRreqTimer = new cMessage("periodicRreqTimer");
        scheduleAt(simTime() + par("PeriodicRreq"), periodicRreqTimer);
    }

    if (measureEtx) {
        helloInterval = maxHelloInterval = minHelloInterval;
        //
        if (helloMsgTimer->isScheduled())
            cancelEvent(helloMsgTimer);
       // sendHelloMessagesIfNeeded();
        scheduleAt(simTime() + *periodicJitter, helloMsgTimer);
    }
    scheduleAt(simTime() + 1, counterTimer);
}

void LoadNgTopsis::handleStopOperation(LifecycleOperation *operation)
{
    clearState();
}

void LoadNgTopsis::handleCrashOperation(LifecycleOperation *operation)
{
    clearState();
}

void LoadNgTopsis::clearState()
{
    rerrCount = rreqCount = rreqId = sequenceNum = 0;
    addressToRreqRetries.clear();
    for (auto & elem : waitForRREPTimers)
        cancelAndDelete(elem.second);

    // FIXME: Drop the queued datagrams.
    //for (auto it = targetAddressToDelayedPackets.begin(); it != targetAddressToDelayedPackets.end(); it++)
    //    networkProtocol->dropQueuedDatagram(it->second);

    targetAddressToDelayedPackets.clear();

    waitForRREPTimers.clear();
    rreqsArrivalTime.clear();

    if (useHelloMessages)
        cancelEvent(helloMsgTimer);
    if (expungeTimer)
        cancelEvent(expungeTimer);
    if (counterTimer)
        cancelEvent(counterTimer);
    if (blacklistTimer)
        cancelEvent(blacklistTimer);
    if (rrepAckTimer)
        cancelEvent(rrepAckTimer);
}

void LoadNgTopsis::handleWaitForRREP(WaitForRrep *rrepTimer)
{
    EV_INFO << "We didn't get any Route Reply within RREP timeout" << endl;
    L3Address destAddr = rrepTimer->getDestAddr();

    ASSERT(addressToRreqRetries.find(destAddr) != addressToRreqRetries.end());
    if (addressToRreqRetries[destAddr] == rreqRetries) {
        cancelRouteDiscovery(destAddr);
        EV_WARN << "Re-discovery attempts for node " << destAddr << " reached RREQ_RETRIES= " << rreqRetries << " limit. Stop sending RREQ." << endl;
        return;
    }

    auto rreq = createRREQ(destAddr);

    // the node MAY try again to discover a route by broadcasting another
    // RREQ, up to a maximum of RREQ_RETRIES times at the maximum TTL value.
    if (rrepTimer->getLastTTL() == netDiameter) // netDiameter is the maximum TTL value
        addressToRreqRetries[destAddr]++;

    if (addressToRreqRetries[destAddr] == rreqRetries && threeMessagesMode) {
        // Try 3 messages
        rreq->setAccumulate(true);
    }

    sendRREQ(rreq, addressType->getBroadcastAddress(), 0);
}

void LoadNgTopsis::forwardRREP(const Ptr<Rrep>& rrep, const L3Address& destAddr)
{
    EV_INFO << "Forwarding the Route Reply to the node " << rrep->getOriginatorAddr() << " which originated the Route Request" << endl;

    if (rrep->getHopLimit() == 0) {
        EV_WARN << "Hop limit 0. Canceling sending RREP" << endl;
        return;
    }

    IRoute *destRoute = routingTable->findBestMatchingRoute(destAddr);
    LoadNgRouteData *destRouteData = check_and_cast<LoadNgRouteData *>(destRoute->getProtocolData());

/*
    char myAddress[30];
    char dest[30];
    strcpy(myAddress, this->getSelfIPAddress().str().c_str());
    strcpy(dest, destAddr.str().c_str());
    */

    bool isNeigBiDir = false;
    auto it = neighbors.find(destAddr);
    if (it != neighbors.end())
        isNeigBiDir = it->second.isBidirectional;

    if (!isNeigBiDir && (!destRouteData->getIsBidirectiona() && destRoute->getNextHopAsGeneric() == destAddr)) {
        // It is possible that a RREP transmission may fail, especially if the
        // RREQ transmission triggering the RREP occurs over a unidirectional
        // link.
        auto ackReq = new LoadNgAckRrepReq();
        rrep->getTlvOptionsForUpdate().appendTlvOption(ackReq);
        //rrep->setAckRequiredFlag(true);
        // when a node detects that its transmission of a RREP message has failed,
        // it remembers the next-hop of the failed RREP in a "blacklist" set.
        failedNextHop = destAddr;
        if (rrepAckTimer->isScheduled())
            cancelEvent(rrepAckTimer);
        scheduleAt(simTime() + nextHopWait, rrepAckTimer);
    }
    if (destAddr.isUnicast())
        sendLoadNgPacket(rrep, destAddr, rrep->getHopLimit(), 0);
    else
        sendLoadNgPacket(rrep, destAddr, rrep->getHopLimit(), *jitterPar);
}

void LoadNgTopsis::forwardRREQ(const Ptr<Rreq>& rreq)
{
    EV_INFO << "Forwarding the Route Request message with TTL= " << rreq->getHopLimit() << endl;
    IRoute *next = nullptr;

    if (!rreq->getTrigger() && !rreq->getBuild()) {
        if (par("smartRREQ"))
            next = routingTable->findBestMatchingRoute(rreq->getDestAddr());
        auto itTopsis = topsisResults.find(rreq->getDestAddr());
        if (itTopsis != topsisResults.end()) {
            if (next == nullptr)
                next = routingTable->findBestMatchingRoute(rreq->getDestAddr());
            if (next->getNextHopAsGeneric() != itTopsis->second.nextAddress) {
                // actualize
                int hops = itTopsis->second.numHops;
                L3Address nextAddr = itTopsis->second.nextAddress;
                auto itAux = seqNumbers.find(itTopsis->first);
                if (itAux == seqNumbers.end())
                    throw cRuntimeError("Impossible");
                auto seq = itAux->second;
                updateRoutingTable(next, nextAddr, hops, seq, true, simTime() + activeRouteTimeout, HOPCOUNT, hops);
            }
            if (RREQUNI_RERR) {
                auto itPrevRreq = listPrevRreqCost.find(rreq->getOriginatorAddr());
                if (itPrevRreq != listPrevRreqCost.end())
                    itPrevRreq->second.nextAddress = next->getNextHopAsGeneric();
            }
        }
    }
    LoadNgRouteData *destRouteData = nullptr;
    if (next && next->getSource() == this) {
        destRouteData = check_and_cast<LoadNgRouteData *>(next->getProtocolData());
        if (!destRouteData->isActive())
            next = nullptr;
        // it is possible unicast RREQ, propagate at least one more hop
        if (next && rreq->getHopLimit() == 0) {
            rreq->setHopLimit(1);
        }
    }

    if (rreq->getHopLimit() == 0) {
        EV_WARN << "Hop limit 0. Canceling sending RREQ" << endl;
        return;
    }

    if (rreq->getOriginatorAddr() != this->getSelfIPAddress()) {
        if (rreq->getAccumulate() && !rreq->getIncludeRoute()) {
            rreq->setAccumulateAddressArraySize(rreq->getAccumulateAddressArraySize() + 1);
            rreq->setAccumulateAddress(rreq->getAccumulateAddressArraySize() - 1, this->getSelfIPAddress());
            rreq->setChunkLength(rreq->getChunkLength() + B(1));
        }
        else if (rreq->getAccumulate() && rreq->getIncludeRoute()) {
            rreq->setRouteArraySize(rreq->getRouteArraySize() + 1);
            rreq->setRoute(rreq->getRouteArraySize() - 1, this->getSelfIPAddress());
            rreq->setChunkLength(rreq->getChunkLength() + B(1));
        }
    }


    if (!next)
        sendLoadNgPacket(rreq, addressType->getBroadcastAddress(), rreq->getHopLimit(), *jitterPar);
    else
        sendLoadNgPacket(rreq, next->getNextHopAsGeneric(), rreq->getHopLimit(), *jitterPar);
}

void LoadNgTopsis::completeRouteDiscoveryDelayed(const L3Address& target)
{
    EV_DETAIL << "Completing route discovery, originator " << getSelfIPAddress() << ", target " << target << endl;
    ASSERT(hasOngoingRouteDiscovery(target));


    auto route = routingTable->findBestMatchingRoute(target);
    if (route == nullptr)
        throw cRuntimeError("Error no valid route");

    LoadNgRouteData *routeDestData = check_and_cast<LoadNgRouteData *>(route->getProtocolData());

    if (!routeDestData->isActive())
        throw cRuntimeError("Error no valid route");

    auto msgDelay = new SmallDelayPacket();
    msgDelay->setDestAddress(target);
    scheduleAt(simTime()+0.0001,msgDelay);
    auto waitRREPIter = waitForRREPTimers.find(target);
    ASSERT(waitRREPIter != waitForRREPTimers.end());
    cancelAndDelete(waitRREPIter->second);
    waitForRREPTimers.erase(waitRREPIter);
}


void LoadNgTopsis::completeRouteDiscovery(const L3Address& target)
{
    EV_DETAIL << "Completing route discovery, originator " << getSelfIPAddress() << ", target " << target << endl;
    ASSERT(hasOngoingRouteDiscovery(target));

    auto lt = targetAddressToDelayedPackets.lower_bound(target);
    auto ut = targetAddressToDelayedPackets.upper_bound(target);

    auto route = routingTable->findBestMatchingRoute(target);
    if (route == nullptr)
        throw cRuntimeError("Error no valid route");

    LoadNgRouteData *routeDestData = check_and_cast<LoadNgRouteData *>(route->getProtocolData());

    if (!routeDestData->isActive())
        throw cRuntimeError("Error no valid route");

    // reinject the delayed datagrams
    for (auto it = lt; it != ut; it++) {
        Packet *datagram = it->second;
        const auto& networkHeader = getNetworkProtocolHeader(datagram);
        EV_DETAIL << "Sending queued datagram: source " << networkHeader->getSourceAddress() << ", destination " << networkHeader->getDestinationAddress() << endl;
        networkProtocol->reinjectQueuedDatagram(datagram);
    }

    // clear the multimap
    targetAddressToDelayedPackets.erase(lt, ut);

    // we have a route for the destination, thus we must cancel the WaitForRREPTimer events
    auto waitRREPIter = waitForRREPTimers.find(target);
    ASSERT(waitRREPIter != waitForRREPTimers.end());
    cancelAndDelete(waitRREPIter->second);
    waitForRREPTimers.erase(waitRREPIter);
}

void LoadNgTopsis::sendGRREP(const Ptr<Rrep>& grrep, const L3Address& destAddr, unsigned int timeToLive)
{
    EV_INFO << "Sending gratuitous Route Reply to " << destAddr << endl;

    IRoute *destRoute = routingTable->findBestMatchingRoute(destAddr);
    const L3Address& nextHop = destRoute->getNextHopAsGeneric();
    if (grrep->getHopLimit() == 0) {
        EV_WARN << "Hop limit 0. Canceling sending RREQ" << endl;
        return;
    }

    sendLoadNgPacket(grrep, nextHop, timeToLive, 0);
}

void LoadNgTopsis::regenerateLinkMap() {
   return;
}



void LoadNgTopsis::checkNeigList(cMessage *timer)
{
    bool recompute = false;
    for (auto it = neighbors.begin(); it != neighbors.end();) {
        if (simTime() - it->second.lastNotification >= helloInterval * par("allowedHelloLoss")) {
            std::vector<IRoute *> routesToDelete;
            for (int i = 0; i< routingTable->getNumRoutes(); i++) {
                IRoute * route = routingTable->getRoute(i);
                if (route->getNextHopAsGeneric() == it->first) {
                    routesToDelete.push_back(route);
                }
            }
            while (!routesToDelete.empty()) {
                routingTable->deleteRoute(routesToDelete.back());
                routesToDelete.pop_back();
            }
            // delete it
            recompute = true;
            neighbors.erase(it++);
        }
        else {
            if (simTime() - it->second.lastNotification >= helloInterval * 2) {
                helloInterval = minHelloInterval;
                it->second.pendingConfirmation = true;
                if (timer != helloMsgTimer) {
                    if (!helloMsgTimer->isScheduled()) {
                        // schedule immediately
                        scheduleAt(simTime(), helloMsgTimer);
                    }
                    else {
                        simtime_t schduled = helloMsgTimer->getSendingTime();
                        simtime_t arrival = helloMsgTimer->getArrivalTime();
                        simtime_t interval = arrival - schduled;
                        if (interval > minHelloInterval) {
                            // Schedule immediately
                            cancelEvent(helloMsgTimer);
                            scheduleAt(simTime(), helloMsgTimer);
                        }
                    }
                }
            }
            ++it;
        }
    }

    if (recompute) {
        std::vector<IRoute *> routesToDelete;
        for (int i = 0; i< routingTable->getNumRoutes(); i++) {
            IRoute * route = routingTable->getRoute(i);
            auto it = neighbors.find(route->getNextHopAsGeneric());
            if (it == neighbors.end())
                routesToDelete.push_back(route);
        }
        while (!routesToDelete.empty()) {
            routingTable->deleteRoute(routesToDelete.back());
            routesToDelete.pop_back();
        }
        runTopsisNeigborsNow();
        if (timer != helloMsgTimer)
            sendGratHelloMessages();
    }
}


const Ptr<Hello> LoadNgTopsis::createHelloMessage()
{
    // called a Hello message, with the RREP
    // message fields set as follows:
    //
    //    Destination IP Address         The node's IP address.
    //
    //    Destination Sequence Number    The node's latest sequence number.
    //
    //    Hop Count                      0
    //
    //    Lifetime                       ALLOWED_HELLO_LOSS *HELLO_INTERVAL

    auto helloMessage = makeShared<Hello>();
    helloMessage->setPacketType(HELLO);
    helloMessage->setSeqNum(sequenceNum);
    helloMessage->setOriginatorAddr(getSelfIPAddress());
    helloMessage->setHopCount(0);
    helloMessage->setHelloIdentifier(helloIdentifier++);
    helloMessage->setHopLimit(1);
    helloMessage->setChunkLength(B(34));
    helloMessage->setStationary(stationary);

    helloMessage->setCreationTime(simTime());
    if (energyStorage)
        helloMessage->setEnergy(energyStorage->getResidualEnergyCapacity().get());

    // include neighbors list


    if (!(neighbors.empty())) {
        int s = (neighbors.size()-1)*1 + std::ceil(double(neighbors.size()*2)/8.0) + 6;
        // it is necessary to check the hello size
        // int s = (neighbors.size()-1)*1 + std::ceil(double(neighbors.size())*8.0) + 6;
        if (this->getSelfIPAddress().getType() == L3Address::IPv4)
            s += 4;
        else if (this->getSelfIPAddress().getType() == L3Address::IPv6)
            s += 16;
        else if (this->getSelfIPAddress().getType() == L3Address::MAC)
            s += 6;
        else
            throw cRuntimeError("Unknown address type");

        if (s < 47)
            helloMessage->setNeighAddrsArraySize(neighbors.size());
        else {
            s = 6 + (30-1)*1 + std::ceil(double(30*2)/8.0);
            helloMessage->setNeighAddrsArraySize(30);
            if (this->getSelfIPAddress().getType() == L3Address::IPv4)
                s += 4;
            else if (this->getSelfIPAddress().getType() == L3Address::IPv6)
                s += 16;
            else if (this->getSelfIPAddress().getType() == L3Address::MAC)
                s += 6;
        }
        int k = 0;


        lastNeig.clear();

        for (auto &elem : neighbors){
            NeigborData nData;
            nData.setAddr(elem.first);
            nData.setIsBidir(elem.second.isBidirectional);
            nData.setPendingConfirmation(elem.second.pendingConfirmation);
            nData.setSeqNum(elem.second.seqNumber);
            nData.setRecPower(elem.second.powerRecCost);
            nData.setEnergy(elem.second.energy);
            nData.setSnir(elem.second.snirCost);
            nData.setDelay(elem.second.delayCost);
            nData.setNumNeig(elem.second.listNeigbours.size());
            nData.setNumNeig(elem.second.listNeigbours.size());
            nData.setStationary(elem.second.stationary);

            if (elem.second.isBidirectional && !elem.second.pendingConfirmation)
                lastNeig.insert(elem.first);



            // HERE inform to the neighbors about the metricToNeig.
            if (measureEtx) {
                // delete old entries.
                while (!elem.second.helloTime.empty() && (simTime() - elem.second.helloTime.front()) > numHellosEtx * minHelloInterval)
                    elem.second.helloTime.pop_front();
                nData.setNumHelloRec(elem.second.helloTime.size());

                if (elem.second.helloTime.size() == 0 || elem.second.numHelloRec == 0)
                    elem.second.metricToNeig = 255;
                else {
                    double val = (double)(elem.second.numHelloRec * elem.second.helloTime.size())/(double)(numHellosEtx*numHellosEtx);
                    if (val > 1) val = 1;
                    val = 1/val;
                    if (val > 255) elem.second.metricToNeig = 255;
                    else elem.second.metricToNeig = val;
                }
            }

            //
            nData.setMetric(elem.second.metricToNeig);
            helloMessage->setNeighAddrs(k, nData);

            k++;
            if (k >= (int)helloMessage->getNeighAddrsArraySize())
                break;
        }
        // size
        helloMessage->setChunkLength(helloMessage->getChunkLength() + B(s));
    }
   // include metrict.
    /*
    auto metric = new LoadNgMetricOption();
    metric->setValue(0);
    helloMessage->getTlvOptionsForUpdate().appendTlvOption(metric);

    */

    return helloMessage;
}

void LoadNgTopsis::sendHelloMessagesIfNeeded()
{
    ASSERT(useHelloMessages);
    // Every HELLO_INTERVAL milliseconds, the node checks whether it has
    // sent a broadcast (e.g., a RREQ or an appropriate layer 2 message)
    // within the last HELLO_INTERVAL.  If it has not, it MAY broadcast
    // a RREP with TTL = 1


    simtime_t nextHello = simTime() + helloInterval - *periodicJitter;
/*    if (nextHello+0.001 > nextRecTime) {
        nextRecTime = nextHello + 0.001;
        emit(nextRecSignal, nextRecTime);
        scheduleAt(nextRecTime, recomputeTopsis);
    }
*/
    if (measureEtx) {
        // new to scheduleHello
        auto helloMessage = createHelloMessage();
        helloMessage->setLifetime(nextHello);
        sendLoadNgPacket(helloMessage, addressType->getBroadcastAddress(), 1, 0);
        scheduleAt(nextHello, helloMsgTimer);
        return;
    }

    if (!par("useIntelligentHello").boolValue()) {
        // send hello
        scheduleAt(nextHello, helloMsgTimer);
        auto helloMessage = createHelloMessage();
        helloMessage->setLifetime(nextHello);
        sendLoadNgPacket(helloMessage, addressType->getBroadcastAddress(), 1, 0);
        scheduleAt(nextHello, helloMsgTimer);
        return;
    }

    // A node SHOULD only use hello messages if it is part of an
    // active route.
    bool hasActiveRoute = false;

    for (int i = 0; i < routingTable->getNumRoutes(); i++) {
        IRoute *route = routingTable->getRoute(i);
        if (route->getSource() == this) {
            LoadNgRouteData *routeData = check_and_cast<LoadNgRouteData *>(route->getProtocolData());
            if (routeData->isActive()) {
                hasActiveRoute = true;
                break;
            }
        }
    }

    if (hasActiveRoute && (lastBroadcastTime == 0 || simTime() - lastBroadcastTime > helloInterval)) {
        EV_INFO << "It is hello time, broadcasting Hello Messages with TTL=1" << endl;
        // sequenceNum++;
        auto helloMessage = createHelloMessage();
        helloMessage->setLifetime(nextHello);
        sendLoadNgPacket(helloMessage, addressType->getBroadcastAddress(), 1, 0);
    }

    scheduleAt(nextHello, helloMsgTimer);
}

void LoadNgTopsis::sendGratHelloMessages()
{
    if (!GRAT_HELLO)
        return;

    std::set<L3Address> tempSet;
    for (auto &elem : neighbors){
        if (elem.second.isBidirectional && !elem.second.pendingConfirmation)
            tempSet.insert(elem.first);
    }

    if (tempSet == lastNeig)
        return;

    if(helloMsgTimer->isScheduled() && helloMsgTimer->getArrivalTime() - simTime() < 0.001)
        return;

    if (simTime() - lastGratHello < 0.0005)
        return;

    lastGratHello = simTime();

    ASSERT(useHelloMessages);
    auto helloMessage = createHelloMessage();
    helloMessage->setLifetime(0);
    sendLoadNgPacket(helloMessage, addressType->getBroadcastAddress(), 1, 0);
}


bool LoadNgTopsis::runTopsisNeigbors()
{
    double time = simTime().dbl();
    double last = lastTopsisCompute.dbl();
    if (time - last < minTopsisIntervalCompute)
        return false;
    return runTopsisNeigborsNow();
    //runTopsisNeigborsNowTwoHops();
}


bool LoadNgTopsis::runTopsisNeigborsNow()
{
    TopsisLoadNgNeigbors topsis;
    topsis.setExperiment(par("matrixExperiment"));
    lastTopsisCompute = simTime();
   // if (abs(simTime().dbl() - 86.5) < 0.5 && this->getSelfIPAddress() == L3Address(Ipv4Address("145.236.0.12")))
   //     printf("");

    bool erased = false;
    for (auto it = neighbors.begin(); it != neighbors.end();) {
        if (simTime() - it->second.lastNotification >= helloInterval * par("allowedHelloLoss")) {
            neighbors.erase(it++);
            erased = true;
            continue;
        }
        if (it->second.isBidirectional && !it->second.pendingConfirmation) {
            // add first this node
            topsis.addCost(it->first, // destination
                    it->first, //next hop
                    it->second.metricToNeig, // Etx
                    it->second.listNeigbours.size(), // num neighbors
                    it->second.snirCost, // Snir
                    it->second.powerRecCost, // power rec
                    1.0, // battery power
                    1, // hops
                    it->second.delayCost); // delay
            // add the neighbors of the node
            for (const auto &elem2: it->second.listNeigbours) {
                double baseEtx = it->second.metricToNeig + elem2.second.metric;
                double basePowerRec = std::min(it->second.powerRecCost, elem2.second.recPower);
                int baseNeig = std::max((int)it->second.listNeigbours.size(), elem2.second.numNeig);
                double baseSnir = std::min(it->second.snirCost, elem2.second.snir);
                double baseDelay =  it->second.delayCost + elem2.second.delay.dbl();
                double basePower = 1;

                if (elem2.second.isBidirectional && !elem2.second.pendingConfirmation) {
                    topsis.addCost(elem2.first, it->first, baseEtx,
                            baseNeig,
                            baseSnir,
                            basePowerRec,
                            basePower,
                            2,
                            baseDelay);
                    // include alternative paths to this element if it is possible
                    // Check if this node is also neighbor
                    auto itAlternate = neighbors.find(elem2.first);
                    if (itAlternate != neighbors.end()) {
                        // first check validity
                        if (simTime() - itAlternate->second.lastNotification >= helloInterval * par("allowedHelloLoss")) {
                            std::vector<IRoute *> routesToDelete;
                            for (int i = 0; i< routingTable->getNumRoutes(); i++) {
                                IRoute * route = routingTable->getRoute(i);
                                if (route->getNextHopAsGeneric() == itAlternate->first)
                                    routesToDelete.push_back(route);
                            }
                            while (!routesToDelete.empty()) {
                                routingTable->deleteRoute(routesToDelete.back());
                                routesToDelete.pop_back();
                            }
                            neighbors.erase(itAlternate);
                        }
                        else if (itAlternate->second.isBidirectional && !itAlternate->second.pendingConfirmation) {
                            // include this node in the list
                            for (const auto &elem3 : itAlternate->second.listNeigbours) {
                                if (elem3.first != it->first  && elem3.second.isBidirectional && !elem3.second.pendingConfirmation)
                                    topsis.addCost(elem3.first, it->first, baseEtx + elem3.second.metric,
                                        std::max(baseNeig, elem3.second.numNeig),
                                        std::min(baseSnir, elem3.second.snir),
                                        std::min(basePowerRec, elem3.second.recPower),
                                        1.0,
                                        3,
                                        it->second.delayCost + elem3.second.delay.dbl(),
                                        elem2.first);
                                else {
                                    // invalidate the route
                                    auto e = routingTable->findBestMatchingRoute(elem3.first);
                                    // check in topsis results
                                    auto itTopsis = topsisResults.find(elem3.first);
                                    if (e != nullptr && e->getSource() == this) {
                                        if (e->getNextHopAsGeneric() == it->first && itTopsis != topsisResults.end()) {// invalidate this route
                                            routingTable->deleteRoute(e);
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
                else {
                    // delete entry
                    auto e = routingTable->findBestMatchingRoute(elem2.first);
                    if (e != nullptr && e->getSource() == this) {
                        if (e->getNextHopAsGeneric() == it->first) {// invalidate this route
                            routingTable->deleteRoute(e);
                        }
                    }
                }
            }
        }
        ++it;
    }

    // Guarantee that the next hop exist in the neighbor table
    std::vector<IRoute *> routesToDelete;
    for (int i = 0; i< routingTable->getNumRoutes(); i++) {
        IRoute * route = routingTable->getRoute(i);
        if (route->getSource() != this)
            continue;
        auto it = neighbors.find(route->getNextHopAsGeneric());
        if (it == neighbors.end())
            routesToDelete.push_back(route);
    }

    while (!routesToDelete.empty()) {
        routingTable->deleteRoute(routesToDelete.back());
        routesToDelete.pop_back();
    }

    topsis.runTopsisRoutes(topsisResults);
    // modify the routing table.

    for (const auto &elem : topsisResults) {
        IRoute *route = routingTable->findBestMatchingRoute(elem.first);
        int hops = elem.second.numHops;

        int64_t seqNum;
        auto it = neighbors.find(elem.second.nextAddress);
        if (it == neighbors.end())
            throw cRuntimeError("neighbors error");
        if (elem.second.address == elem.second.nextAddress)
            seqNum = it->second.seqNumber;
        else {
            auto it2 = it->second.listNeigbours.find(elem.second.address);
            if (it2 == it->second.listNeigbours.end()) {
                // possible 3 hops
                if (elem.second.commonAddress.isUnspecified())
                    throw cRuntimeError("neighbors error");

                auto neig = neighbors.find(elem.second.commonAddress);
                auto it3 = neig->second.listNeigbours.find(elem.second.address);
                if (it3 == neig->second.listNeigbours.end())
                    throw cRuntimeError("neighbors error");
            }
            else
                seqNum = it2->second.seqNum;
        }

        auto costEtx = elem.second.etx;

        simtime_t newLifeTime = it->second.lastNotification + par("allowedHelloLoss") * minHelloInterval;

        if (!route || route->getSource() != this) {
            route = createRoute(elem.first, elem.second.nextAddress, hops, seqNum, true, newLifeTime, HOPCOUNT, costEtx);
            LoadNgRouteData *routeData = check_and_cast<LoadNgRouteData *>(route->getProtocolData());
            routeData->setIsBidirectiona(true);
            routeData->setEtx(elem.second.etx);
            routeData->setDelay(elem.second.delay);
            routeData->setSnir(elem.second.snir);
            routeData->setNumNeig(elem.second.neighbors);
            routeData->setEnergy(elem.second.energy);
            routeData->setRecPower(elem.second.recPower);
            routeData->setUsingTopsis(true);
        }
        else {
            LoadNgRouteData *routeData = check_and_cast<LoadNgRouteData *>(route->getProtocolData());
            simtime_t lifeTime = routeData->getLifeTime();
            routeData->setIsBidirectiona(true);

            routeData->setEtx(elem.second.etx);
            routeData->setDelay(elem.second.delay);
            routeData->setSnir(elem.second.snir);
            routeData->setNumNeig(elem.second.neighbors);
            routeData->setEnergy(elem.second.energy);
            routeData->setRecPower(elem.second.recPower);
            routeData->setUsingTopsis(true);

            updateRoutingTable(route, elem.second.nextAddress, hops, seqNum, true, std::max(lifeTime, newLifeTime), HOPCOUNT, costEtx);
        }
    }
    return erased;
}


void LoadNgTopsis::runTopsisNeigborsNowCheckTopsis()
{
    TopsisLoadNgNeigbors topsis;
    topsis.setExperiment(par("matrixExperiment"));
    lastTopsisCompute = simTime();
    for (auto it = neighbors.begin(); it != neighbors.end();) {
        if (it->second.isBidirectional && !it->second.pendingConfirmation) {
            // add first this node
            topsis.addCost(it->first, // destination
                    it->first, //next hop
                    it->second.metricToNeig, // Etx
                    it->second.listNeigbours.size(), // num neighbors
                    it->second.snirCost, // Snir
                    it->second.powerRecCost, // power rec
                    1.0, // battery power
                    1, // hops
                    it->second.delayCost); // delay
            // add the neighbors of the node
            for (const auto &elem2: it->second.listNeigbours) {
                double baseEtx = it->second.metricToNeig + elem2.second.metric;
                double basePowerRec = std::min(it->second.powerRecCost, elem2.second.recPower);
                int baseNeig = std::max((int)it->second.listNeigbours.size(), elem2.second.numNeig);
                double baseSnir = std::min(it->second.snirCost, elem2.second.snir);
                double baseDelay =  it->second.delayCost + elem2.second.delay.dbl();
                double basePower = 1;

                if (elem2.second.isBidirectional && !elem2.second.pendingConfirmation) {
                    topsis.addCost(elem2.first, it->first, baseEtx,
                            baseNeig,
                            baseSnir,
                            basePowerRec,
                            basePower,
                            2,
                            baseDelay);
                    // include alternative paths to this element if it is possible
                    // Check if this node is also neighbor
                    auto itAlternate = neighbors.find(elem2.first);
                    if (itAlternate != neighbors.end()) {
                        // first check validity
                        if (itAlternate->second.isBidirectional && !itAlternate->second.pendingConfirmation) {
                            // include this node in the list
                            for (const auto &elem3 : itAlternate->second.listNeigbours) {
                                if (elem3.first != it->first  && elem3.second.isBidirectional && !elem3.second.pendingConfirmation)
                                    topsis.addCost(elem3.first, it->first, baseEtx + elem3.second.metric,
                                        std::max(baseNeig, elem3.second.numNeig),
                                        std::min(baseSnir, elem3.second.snir),
                                        std::min(basePowerRec, elem3.second.recPower),
                                        1.0,
                                        3,
                                        it->second.delayCost + elem3.second.delay.dbl(),
                                        elem2.first);
                            }
                        }
                    }
                }
            }
        }
        ++it;
    }
    topsis.runTopsisRoutes(topsisResultsCheck, true);
    // modify the routing table.
}

bool LoadNgTopsis::runTopsisNeigborsNowTwoHops()
{
    TopsisLoadNgNeigbors topsis;
    topsis.setExperiment(par("matrixExperiment"));
    lastTopsisCompute = simTime();
   // if (abs(simTime().dbl() - 86.5) < 0.5 && this->getSelfIPAddress() == L3Address(Ipv4Address("145.236.0.12")))
   //     printf("");
    bool erased = false;
    for (auto it = neighbors.begin(); it != neighbors.end();) {
        if (simTime() - it->second.lastNotification >= helloInterval * par("allowedHelloLoss")) {
            std::vector<IRoute *> routesToDelete;
            for (int i = 0; i< routingTable->getNumRoutes(); i++) {
                IRoute * route = routingTable->getRoute(i);
                if (route->getNextHopAsGeneric() == it->first)
                    routesToDelete.push_back(route);
            }
            while (!routesToDelete.empty()) {
                routingTable->deleteRoute(routesToDelete.back());
                routesToDelete.pop_back();
            }
            neighbors.erase(it++);
            erased = true;
            continue;
        }
        if (it->second.isBidirectional && !it->second.pendingConfirmation) {
            // add first this node
            topsis.addCost(it->first, // destination
                    it->first, //next hop
                    it->second.metricToNeig, // Etx
                    it->second.listNeigbours.size(), // num neighbors
                    it->second.snirCost, // Snir
                    it->second.powerRecCost, // power rec
                    1.0, // battery power
                    1, // hops
                    it->second.delayCost); // delay
            // add the neighbors of the node
            for (const auto &elem2: it->second.listNeigbours) {

                double baseEtx = it->second.metricToNeig + elem2.second.metric;
                double basePowerRec = std::min(it->second.powerRecCost, elem2.second.recPower);
                int baseNeig = std::max((int)it->second.listNeigbours.size(), elem2.second.numNeig);
                double baseSnir = std::min(it->second.snirCost, elem2.second.snir);
                double baseDelay =  it->second.delayCost + elem2.second.delay.dbl();
                double basePower = 1;

                if (elem2.second.isBidirectional && !elem2.second.pendingConfirmation) {
                    topsis.addCost(elem2.first, it->first, baseEtx,
                            baseNeig,
                            baseSnir,
                            basePowerRec,
                            basePower,
                            2,
                            baseDelay);
                }
            }
        }
        ++it;
    }

    topsis.runTopsisRoutes(topsisResultsTwoHops);
    // modify the routing table.

    for (const auto &elem : topsisResultsTwoHops) {

        // check routing tables.
        auto it = topsisResults.find(elem.first);
        if (it == topsisResults.end())
            throw cRuntimeError("Impossible");

        if (it->second.etx > elem.second.etx) {
            // re run
            topsis.runTopsisRoutes(topsisResultsTwoHops,true);
            runTopsisNeigborsNowCheckTopsis();
        }
    }
    return erased;
}


void LoadNgTopsis::handleHelloMessage(const Ptr<const Hello>& helloMessage, const Ptr<const SignalPowerInd>& powerInd, const Ptr<const SnirInd> &snirInd, const MacAddress &macSenderAddress)
{
    if (helloMessage->getLifetime() == 0) {
        // handle special hello, notification of changes in the connectivity
        const L3Address& helloOriginatorAddr = helloMessage->getOriginatorAddr();

        if (!macSenderAddress.isUnspecified() && !helloOriginatorAddr.isUnspecified()) {
            auto itMac = macToIpAddress.find(macSenderAddress);
            if (itMac == macToIpAddress.end()) {
                macToIpAddress[macSenderAddress] = helloOriginatorAddr;
            }
        }
        std::set<L3Address> addrList;
        for (auto k = 0 ; k < (int) helloMessage->getNeighAddrsArraySize(); k++)
            addrList.insert(helloMessage->getNeighAddrs(k).getAddr());

        auto itNeigAux = neighbors.find(helloOriginatorAddr);
        if (itNeigAux == neighbors.end())
            return;
        bool recompute = false;
        for (auto it = itNeigAux->second.listNeigbours.begin();  it != itNeigAux->second.listNeigbours.end();) {
            auto itAux = addrList.find(it->first);
            if (itAux == addrList.end()) {
                itNeigAux->second.listNeigbours.erase(it++);
                recompute = true;
            }
            else
                ++it;
        }
        if (recompute)
            runTopsisNeigborsNow();
        return;
    }

    double power = NaN;
    double snir = NaN;
    bool forceRecompute = false;
    if (powerInd) {
        power = powerInd->getPower().get();
        // this values are for uni disk.
        double max = 1.0/100.0;
        double min = 1.0/10000;
        double interval = max-min;
        double step = interval / 511;
        if (power > max)
            power = max;
        power = std::round(power/step);
        if (power > 511)
            power = 511;
    }

    if (snirInd) {
        snir = snirInd->getMaximumSnir();

        double max = 1.0/100.0;
        double min = 1.0/10000;
        double interval = max-min;
        double step = interval / 511;
        if (snir > max)
            snir = max;
        snir = std::round(snir/step);
        if (snir > 511)
            snir = 511;
    }

    auto delay = simTime() - helloMessage->getCreationTime();
    const L3Address& helloOriginatorAddr = helloMessage->getOriginatorAddr();

    if (!macSenderAddress.isUnspecified() && !helloOriginatorAddr.isUnspecified()) {
        auto itMac = macToIpAddress.find(macSenderAddress);
        if (itMac == macToIpAddress.end()) {
            macToIpAddress[macSenderAddress] = helloOriginatorAddr;
        }
    }

    auto it = neighbors.find(helloOriginatorAddr);
    bool changeTimerNotification = false;
    if (it != neighbors.end()) {
        // refresh
        if (helloMessage->getSeqNum() < it->second.seqNumber)
            return;
    }
    else {
        // add
        NeigborElement elem;
        elem.isBidirectional = false;
        elem.creationTime = simTime();
        neighbors[helloOriginatorAddr] = elem;
        it = neighbors.find(helloOriginatorAddr);
        changeTimerNotification = true;
    }

    auto itSeq = seqNumbers.find(helloOriginatorAddr);
    if (itSeq == seqNumbers.end() || (itSeq != seqNumbers.end() && itSeq->second < helloMessage->getSeqNum())) {
        seqNumbers[helloOriginatorAddr] = helloMessage->getSeqNum();
    }

    it->second.lifeTime = helloMessage->getLifetime();
    it->second.lastNotification = simTime();
    it->second.pendingConfirmation = false;
    it->second.pendingHello = false;
    it->second.seqNumber = helloMessage->getSeqNum();
    it->second.energy = helloMessage->getEnergy();
    it->second.stationary = helloMessage->getStationary();

    if (!isnan(power))
        it->second.powerList.push_back(power);
    if (!isnan(snir))
        it->second.snirList.push_back(snir);
    it->second.delayList.push_back(delay);
    while (it->second.powerList.size() > 10)
        it->second.powerList.pop_front();
    while (it->second.snirList.size() > 10)
        it->second.snirList.pop_front();
    while (it->second.delayList.size() > 10)
        it->second.delayList.pop_front();

    if (measureEtx) {
        it->second.helloTime.push_back(simTime());
        // delete old entries.
        while (!it->second.helloTime.empty() && (simTime() - it->second.helloTime.front()) > numHellosEtx * minHelloInterval)
            it->second.helloTime.pop_front();
    }

    if (!it->second.snirList.empty()) {
        double val = 0;
        for (auto e : it->second.snirList)
            val += e;
        val /= it->second.snirList.size();
        it->second.snirCost = std::round(val);
    }

    if (!it->second.powerList.empty())
        it->second.powerRecCost = it->second.powerList.back();

    if (!it->second.delayList.empty()) {
        simtime_t val;
        for (auto e : it->second.delayList)
            val += e;
        val /= it->second.delayList.size();
        it->second.delayCost = val.dbl();
    }

    int metricType = -1;
    unsigned int metric = 255;
    unsigned int metricNeg = 255;
    auto metricPos = helloMessage->getTlvOptions().findByType(METRIC);

    if (metricPos != -1) {
        auto metricTlv = check_and_cast<const LoadNgMetricOption *>(helloMessage->getTlvOptions().getTlvOption(metricPos));
        if (metricTlv->getExtensionFlag())
            metricType = metricTlv->getExtension();
        if (metricTlv->getValueFlag())
            metric = metricTlv->getValue();
        if (metricType == HOPCOUNT) {
            if (metric < 255)
                metric++;
            metricNeg = 1;
        }
        else {
            // unknow metricToNeig set to hopcount
            metric = 255;
        }
    }

    std::map<L3Address, NodeStatus> nodeStatus;
    for (auto k = 0 ; k < (int)helloMessage->getNeighAddrsArraySize(); k++) {
        auto nData = helloMessage->getNeighAddrs(k);
        NodeStatus status;
        auto itSeq = seqNumbers.find(nData.getAddr());
        if (itSeq != seqNumbers.end() && nData.getSeqNum() < itSeq->second)
            continue; // ignore it
        status.isBidirectional = nData.isBidir();
        status.pendingConfirmation = nData.getPendingConfirmation();
        status.seqNum = nData.getSeqNum();
        status.metric = nData.getMetric();
        status.numHelloRec = nData.getNumHelloRec();
        status.delay = nData.getDelay();
        status.energy = nData.getEnergy();
        status.snir = nData.getSnir();
        status.numNeig = nData.getNumNeig();
        status.recPower = nData.getRecPower();
        status.stationary = nData.getStationary();

        nodeStatus[nData.getAddr()] = status;

        if (itSeq == seqNumbers.end() || (itSeq != seqNumbers.end() && itSeq->second < nData.getSeqNum())) {
            seqNumbers[nData.getAddr()] = nData.getSeqNum();
        }

        // check common neighbor
        auto itNeigAux = neighbors.find(nData.getAddr());
        if (itNeigAux != neighbors.end() && itNeigAux->second.seqNumber < nData.getSeqNum()) {
            itNeigAux->second.pendingConfirmation = true;
        }
    }

//    if (it->second.listNeigbours.size() != nodeStatus.size())
//        forceRecompute = true;

    if (!forceRecompute) {
        for (const auto &elem : it->second.listNeigbours) {
            auto it = nodeStatus.find(elem.first);
            if (it == nodeStatus.end())
                forceRecompute = true;
        }
    }

    it->second.listNeigbours.clear();
    for (const auto &elem : nodeStatus) {
        if (this->getSelfIPAddress() != elem.first)
            it->second.listNeigbours[elem.first] = elem.second;
    }

    // First check nodes that are in the list, remove nodes that are not in the new list, and actualize the others
    // Check if the list of neighbors of this node has change

    auto itCheckBiDir = nodeStatus.find(this->getSelfIPAddress());
    if (itCheckBiDir == nodeStatus.end()) {
        if (it->second.isBidirectional ) {
            changeTimerNotification = true;
            it->second.isBidirectional = false;
            it->second.metricToNeig = 255;
        }
    }
    else {
        it->second.isBidirectional = true;
        // Compute ETX if is needed. HERE IT IS POSSIBLE TO COMPUTE THE METRICS
        it->second.numHelloRec = itCheckBiDir->second.numHelloRec;
        if (measureEtx) {
            if (itCheckBiDir->second.numHelloRec == 0)
                it->second.metricToNeig = 255;
            else {
                double val = 0;
                if (simTime() - it->second.creationTime >= numHellosEtx * minHelloInterval) {
                    val = (double)(itCheckBiDir->second.numHelloRec * it->second.helloTime.size());
                    val /= (double)(numHellosEtx*numHellosEtx);
                }
                else {
                    int hellos = ((simTime() - it->second.creationTime)/minHelloInterval)+1;
                    if (hellos != 0) {
                        val =  (double) (itCheckBiDir->second.numHelloRec * it->second.helloTime.size());
                        val /= (double) (hellos * hellos);
                    }
                    else
                        val = 1;
                }
                if (val > 1) val = 1;
                val = 1/val;
                if (!stationary || !it->second.stationary)
                    val *= etxPenalty;
                if (val > 255)
                    it->second.metricToNeig = 255;
                else
                    it->second.metricToNeig = val;
            }
        }
            // now should actualize the routing table
        // bi-dir
        if (it->second.isBidirectional)
            it->second.pendingConfirmation = false;
    }


    if (it->second.isBidirectional && !it->second.pendingConfirmation) {
        // if the link is biDirectional, delete from blacklist
        auto blackListIt = blacklist.find(it->first);
        if (blackListIt != blacklist.end())
            blacklist.erase(blackListIt);
    }
    else {
        blacklist[it->first] = simTime() + blacklistTimeout;
    }

    // now include the information in dijkstra
    // rebuildDijkstraWihtHellos();


   // int numRoutes = routingTable->getNumRoutes();
     // Whenever a node receives a Hello message from a neighbor, the node
    // SHOULD make sure that it has an active route to the neighbor, and
    // create one if necessary.  If a route already exists, then the
    // Lifetime for the route should be increased, if necessary, to be at
    // least ALLOWED_HELLO_LOSS * HELLO_INTERVAL.  The route to the
    // neighbor, if it exists, MUST subsequently contain the latest
    // Destination Sequence Number from the Hello message.  The current node
    // can now begin using this route to forward data packets.  Routes that
    // are created by hello messages and not used by any other active routes
    // will have empty precursor lists and would not trigger a RERR message
    // if the neighbor moves away and a neighbor timeout occurs.

    if (it->second.pendingConfirmation == false && it->second.isBidirectional) {
        IRoute *routeHelloOriginator = routingTable->findBestMatchingRoute(helloOriginatorAddr);
        unsigned int latestDestSeqNum = helloMessage->getSeqNum();
        simtime_t newLifeTime = helloMessage->getLifetime() + par("allowedHelloLoss") * minHelloInterval;
        if (!routeHelloOriginator || routeHelloOriginator->getSource() != this) {
            routeHelloOriginator = createRoute(helloOriginatorAddr, helloOriginatorAddr, 1, latestDestSeqNum, true, newLifeTime, metricType, metricNeg, TOPSIS_UPDATE);
            LoadNgRouteData *routeData = check_and_cast<LoadNgRouteData *>(routeHelloOriginator->getProtocolData());
            routeData->setIsBidirectiona(true);
        }
        else {
            LoadNgRouteData *routeData = check_and_cast<LoadNgRouteData *>(routeHelloOriginator->getProtocolData());
            simtime_t lifeTime = routeData->getLifeTime();
            routeData->setIsBidirectiona(true);
            updateRoutingTable(routeHelloOriginator, helloOriginatorAddr, 1, latestDestSeqNum, true, std::max(lifeTime, newLifeTime), metricType, metricNeg, TOPSIS_UPDATE);
        }
    }

    /*
    LoadNgRouteData *routeData = check_and_cast<LoadNgRouteData *>(routeHelloOriginator->getProtocolData());
    routeData->setPower(rreq->getPower());
    routeData->setDelay(rreq->getDelay());
    routeData->setSnir(rreq->getSnir());
    routeData->setEtx(rreq->getEtx());
    */

    // run topsis with the new information
    if (forceRecompute)
        runTopsisNeigborsNow();
    else
        runTopsisNeigbors();

    changeTimerNotification = true;

    // add the information of the hello to the routing table
    if (changeTimerNotification) {
        helloInterval = minHelloInterval;
        if (!helloMsgTimer->isScheduled()) {
            // schedule immediately
            scheduleAt(simTime(), helloMsgTimer);
        }
        else {
            simtime_t schduled = helloMsgTimer->getSendingTime();
            simtime_t arrival = helloMsgTimer->getArrivalTime();
            simtime_t interval = arrival - schduled;
            if (interval > minHelloInterval) {
                // Schedule immediately
                cancelEvent(helloMsgTimer);
                scheduleAt(simTime(), helloMsgTimer);
            }
        }
    }
    else {
        helloInterval += 2.0;
        if (helloInterval > maxHelloInterval) {
            helloInterval = maxHelloInterval;
        }
    }
}

void LoadNgTopsis::expungeRoutes()
{
    std::vector<IRoute *> deletedRoutes;
    for (int i = 0; i < routingTable->getNumRoutes(); i++) {
        IRoute *route = routingTable->getRoute(i);
        if (route->getSource() == this) {
            LoadNgRouteData *routeData = check_and_cast<LoadNgRouteData *>(route->getProtocolData());
            ASSERT(routeData != nullptr);
            if (routeData->getLifeTime() <= simTime()) {
                if (routeData->isActive()) {
                    EV_DETAIL << "Route to " << route->getDestinationAsGeneric() << " expired and set to inactive. It will be deleted after DELETE_PERIOD time" << endl;
                    // An expired routing table entry SHOULD NOT be expunged before
                    // (current_time + DELETE_PERIOD) (see section 6.11).  Otherwise, the
                    // soft state corresponding to the route (e.g., last known hop count)
                    // will be lost.
                    routeData->setIsActive(false);
                    routeData->setLifeTime(simTime() + deletePeriod);
                }
                else {
                    // Any routing table entry waiting for a RREP SHOULD NOT be expunged
                    // before (current_time + 2 * NET_TRAVERSAL_TIME).
                    if (hasOngoingRouteDiscovery(route->getDestinationAsGeneric())) {
                        EV_DETAIL << "Route to " << route->getDestinationAsGeneric() << " expired and is inactive, but we are waiting for a RREP to this destination, so we extend its lifetime with 2 * NET_TRAVERSAL_TIME" << endl;
                        routeData->setLifeTime(simTime() + 2 * netTraversalTime);
                    }
                    else {
                        EV_DETAIL << "Route to " << route->getDestinationAsGeneric() << " expired and is inactive and we are not expecting any RREP to this destination, so we delete this route" << endl;
                        deletedRoutes.push_back(route);
                        //routingTable->deleteRoute(route);
                    }
                }
            }
        }
    }
    while(!deletedRoutes.empty()) {
        routingTable->deleteRoute(deletedRoutes.back());
        deletedRoutes.pop_back();
    }
    scheduleExpungeRoutes();
}

void LoadNgTopsis::scheduleExpungeRoutes()
{
    simtime_t nextExpungeTime = SimTime::getMaxTime();
    std::vector<IRoute *> deletedRoutes;
    for (int i = 0; i < routingTable->getNumRoutes(); i++) {
        IRoute *route = routingTable->getRoute(i);
        if (route->getSource() == this) {
            LoadNgRouteData *routeData = check_and_cast<LoadNgRouteData *>(route->getProtocolData());
            ASSERT(routeData != nullptr);
            if (routeData->getLifeTime() <= simTime()) {
                if (routeData->isActive()) {
                    routeData->setIsActive(false);
                    routeData->setLifeTime(simTime() + deletePeriod);
                }
                else {
                    if (hasOngoingRouteDiscovery(route->getDestinationAsGeneric())) {
                        EV_DETAIL << "Route to " << route->getDestinationAsGeneric() << " expired and is inactive, but we are waiting for a RREP to this destination, so we extend its lifetime with 2 * NET_TRAVERSAL_TIME" << endl;
                        routeData->setLifeTime(simTime() + 2 * netTraversalTime);
                    }
                    else {
                        EV_DETAIL << "Route to " << route->getDestinationAsGeneric() << " expired and is inactive and we are not expecting any RREP to this destination, so we delete this route" << endl;
                        deletedRoutes.push_back(route);
                        //routingTable->deleteRoute(route);
                        routeData = nullptr;
                    }
                }
            }
            if (routeData && routeData->getLifeTime() < nextExpungeTime)
                nextExpungeTime = routeData->getLifeTime();
        }
    }

    while(!deletedRoutes.empty()) {
        routingTable->deleteRoute(deletedRoutes.back());
        deletedRoutes.pop_back();
    }

    if (nextExpungeTime == SimTime::getMaxTime()) {
        if (expungeTimer->isScheduled())
            cancelEvent(expungeTimer);
    }
    else {
        if (!expungeTimer->isScheduled())
            scheduleAt(nextExpungeTime, expungeTimer);
        else {
            if (expungeTimer->getArrivalTime() != nextExpungeTime) {
                cancelEvent(expungeTimer);
                scheduleAt(nextExpungeTime, expungeTimer);
            }
        }
    }
}

INetfilter::IHook::Result LoadNgTopsis::datagramForwardHook(Packet *datagram)
{
    // TODO: Implement: Actions After Reboot
    // If the node receives a data packet for some other destination, it SHOULD
    // broadcast a RERR as described in subsection 6.11 and MUST reset the waiting
    // timer to expire after current time plus DELETE_PERIOD.

    Enter_Method("datagramForwardHook");
    const auto& networkHeader = getNetworkProtocolHeader(datagram);
    const L3Address& destAddr = networkHeader->getDestinationAddress();
    const L3Address& sourceAddr = networkHeader->getSourceAddress();
    IRoute *ipSource = routingTable->findBestMatchingRoute(sourceAddr);

    if (destAddr.isBroadcast() || routingTable->isLocalAddress(destAddr) || destAddr.isMulticast()) {
        if (routingTable->isLocalAddress(destAddr) && ipSource && ipSource->getSource() == this)
            updateValidRouteLifeTime(ipSource->getNextHopAsGeneric(), simTime() + activeRouteTimeout);

        return ACCEPT;
    }

    // TODO: IMPLEMENT: check if the datagram is a data packet or we take control packets as data packets

    IRoute *routeDest = routingTable->findBestMatchingRoute(destAddr);
    LoadNgRouteData *routeDestData = routeDest ? dynamic_cast<LoadNgRouteData *>(routeDest->getProtocolData()) : nullptr;

    // Each time a route is used to forward a data packet, its Active Route
    // Lifetime field of the source, destination and the next hop on the
    // path to the destination is updated to be no less than the current
    // time plus ACTIVE_ROUTE_TIMEOUT

    updateValidRouteLifeTime(sourceAddr, simTime() + activeRouteTimeout);
    updateValidRouteLifeTime(destAddr, simTime() + activeRouteTimeout);

    if (routeDest && routeDest->getSource() == this)
        updateValidRouteLifeTime(routeDest->getNextHopAsGeneric(), simTime() + activeRouteTimeout);

    // Since the route between each originator and destination pair is expected
    // to be symmetric, the Active Route Lifetime for the previous hop, along the
    // reverse path back to the IP source, is also updated to be no less than the
    // current time plus ACTIVE_ROUTE_TIMEOUT.

    if (ipSource && ipSource->getSource() == this)
        updateValidRouteLifeTime(ipSource->getNextHopAsGeneric(), simTime() + activeRouteTimeout);

    EV_INFO << "We can't forward datagram because we have no active route for " << destAddr << endl;
    if (routeDest && routeDestData && !routeDestData->isActive()) {    // exists but is not active
        // A node initiates processing for a RERR message in three situations:
        // (ii)      if it gets a data packet destined to a node for which it
        //           does not have an active route and is not repairing (if
        //           using local repair)

        // TODO: check if it is not repairing (if using local repair)

        // 1. The destination sequence number of this routing entry, if it
        // exists and is valid, is incremented for cases (i) and (ii) above,
        // and copied from the incoming RERR in case (iii) above.

        if (routeDestData->hasValidDestNum())
            routeDestData->setDestSeqNum(routeDestData->getDestSeqNum() + 1);

        // 2. The entry is invalidated by marking the route entry as invalid <- it is invalid

        // 3. The Lifetime field is updated to current time plus DELETE_PERIOD.
        //    Before this time, the entry SHOULD NOT be deleted.
        routeDestData->setLifeTime(simTime() + deletePeriod);

        if (!routingTable->isLocalAddress(sourceAddr))
            sendRERRWhenNoRouteToForward(destAddr, sourceAddr);
    }
    else if (!routeDest || routeDest->getSource() != this) // doesn't exist at all
        if (!routingTable->isLocalAddress(sourceAddr))
            sendRERRWhenNoRouteToForward(destAddr, sourceAddr);

    return ACCEPT;
}

void LoadNgTopsis::sendRERRWhenNoRouteToForward(const L3Address& unreachableAddr, const L3Address &destAddr)
{
    if (rerrCount >= rerrRatelimit) {
        EV_WARN << "A node should not generate more than RERR_RATELIMIT RERR messages per second. Canceling sending RERR" << endl;
        return;
    }

    std::vector<UnreachableNode> unreachableNodes;
    UnreachableNode node;
    node.addr = unreachableAddr;

    IRoute *unreachableRoute = routingTable->findBestMatchingRoute(unreachableAddr);
    LoadNgRouteData *unreachableRouteData = unreachableRoute ? dynamic_cast<LoadNgRouteData *>(unreachableRoute->getProtocolData()) : nullptr;

    if (unreachableRouteData && unreachableRouteData->hasValidDestNum())
        node.seqNum = unreachableRouteData->getDestSeqNum();
    else
        node.seqNum = 0;

    unreachableNodes.push_back(node);

    // extract the route to the previous node
    auto nextAddress = addressType->getBroadcastAddress();
    IRoute *route = routingTable->findBestMatchingRoute(destAddr);
    int ttl = 1;
     if (route && route->getSource() == this) {
         LoadNgRouteData *routeData = check_and_cast<LoadNgRouteData *>(route->getProtocolData());
         if (routeData->isActive()) {
             nextAddress = route->getNextHopAsGeneric();
             ttl = route->getMetric();
         }
     }
     if (nextAddress.isBroadcast()) // Only unicast
         return;

     auto rerr = createRERR(unreachableNodes);
     rerr->setOriginatorAddr(this->getSelfIPAddress());
     rerr->setDestAddr(destAddr);


    rerrCount++;
    EV_INFO << "Broadcasting Route Error message with TTL=1" << endl;
    sendLoadNgPacket(rerr, nextAddress, ttl, 0);    // TODO: unicast if there exists a route to the source
}

void LoadNgTopsis::cancelRouteDiscovery(const L3Address& destAddr)
{
    ASSERT(hasOngoingRouteDiscovery(destAddr));
    auto lt = targetAddressToDelayedPackets.lower_bound(destAddr);
    auto ut = targetAddressToDelayedPackets.upper_bound(destAddr);
    for (auto it = lt; it != ut; it++)
        networkProtocol->dropQueuedDatagram(it->second);

    targetAddressToDelayedPackets.erase(lt, ut);

    auto waitRREPIter = waitForRREPTimers.find(destAddr);
    ASSERT(waitRREPIter != waitForRREPTimers.end());
    cancelAndDelete(waitRREPIter->second);
    waitForRREPTimers.erase(waitRREPIter);
}

bool LoadNgTopsis::updateValidRouteLifeTime(const L3Address& destAddr, simtime_t lifetime)
{
    IRoute *route = routingTable->findBestMatchingRoute(destAddr);
    if (route && route->getSource() == this) {
        LoadNgRouteData *routeData = check_and_cast<LoadNgRouteData *>(route->getProtocolData());
        if (routeData->isActive()) {
            simtime_t newLifeTime = std::max(routeData->getLifeTime(), lifetime);
            EV_DETAIL << "Updating " << route << " lifetime to " << newLifeTime << endl;
            routeData->setLifeTime(newLifeTime);
            return true;
        }
    }
    return false;
}

const Ptr<RrepAck> LoadNgTopsis::createRREPACK()
{
    auto rrepACK = makeShared<RrepAck>(); // TODO: "AODV-RREPACK");
    rrepACK->setPacketType(RREPACK);
    rrepACK->setChunkLength(B(2));
    return rrepACK;
}

void LoadNgTopsis::sendRREPACK(const Ptr<RrepAck>& rrepACK, const L3Address& destAddr)
{
    EV_INFO << "Sending Route Reply ACK to " << destAddr << endl;
    sendLoadNgPacket(rrepACK, destAddr, 100, 0);
}

void LoadNgTopsis::handleRREPACK(const Ptr<const RrepAck>& rrepACK, const L3Address& neighborAddr)
{
    // Note that the RREP-ACK packet does not contain any information about
    // which RREP it is acknowledging.  The time at which the RREP-ACK is
    // received will likely come just after the time when the RREP was sent
    // with the 'A' bit.
    if (rrepAckTimer->isScheduled()) {
        EV_INFO << "RREP-ACK arrived from " << neighborAddr << endl;

        IRoute *route = routingTable->findBestMatchingRoute(neighborAddr);
        if (route && route->getSource() == this) {
            EV_DETAIL << "Marking route " << route << " as active" << endl;
            LoadNgRouteData *routeData = check_and_cast<LoadNgRouteData *>(route->getProtocolData());
            routeData->setIsActive(true);
            routeData->setIsBidirectiona(true);
            cancelEvent(rrepAckTimer);
        }
    }
}

void LoadNgTopsis::handleRREPACKTimer()
{
    // when a node detects that its transmission of a RREP message has failed,
    // it remembers the next-hop of the failed RREP in a "blacklist" set.

    EV_INFO << "RREP-ACK didn't arrived within timeout. Adding " << failedNextHop << " to the blacklist" << endl;

    blacklist[failedNextHop] = simTime() + blacklistTimeout;    // lifetime

    if (!blacklistTimer->isScheduled())
        scheduleAt(simTime() + blacklistTimeout, blacklistTimer);
}

void LoadNgTopsis::handleBlackListTimer()
{
    simtime_t nextTime = SimTime::getMaxTime();

    for (auto it = blacklist.begin(); it != blacklist.end(); ) {
        auto current = it++;

        // Nodes are removed from the blacklist set after a BLACKLIST_TIMEOUT period
        if (current->second <= simTime()) {
            EV_DETAIL << "Blacklist lifetime has expired for " << current->first << " removing it from the blacklisted addresses" << endl;
            blacklist.erase(current);
        }
        else if (nextTime > current->second)
            nextTime = current->second;
    }

    if (nextTime != SimTime::getMaxTime())
        scheduleAt(nextTime, blacklistTimer);
}

LoadNgTopsis::~LoadNgTopsis()
{

    for (auto & e : pendingSend) {
        cancelAndDelete(e);
    }
    cancelAndDelete(recomputeTimer);
    pendingSend.clear();

    neighbors.clear();
    clearState();
    cancelAndDelete(recomputeTopsis);
    cancelAndDelete(helloMsgTimer);
    cancelAndDelete(expungeTimer);
    cancelAndDelete(counterTimer);
    cancelAndDelete(rrepAckTimer);
    cancelAndDelete(blacklistTimer);
}

} // namespace aodv
} // namespace inet

