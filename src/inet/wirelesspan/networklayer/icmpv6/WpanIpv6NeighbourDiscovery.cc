/**
 * Copyright (C) 2005 Andras Varga
 * Copyright (C) 2005 Wei Yang, Ng
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "inet/common/ModuleAccess.h"
#include "inet/common/ProtocolTag_m.h"
#include "inet/common/lifecycle/NodeStatus.h"
#include "inet/linklayer/common/InterfaceTag_m.h"
#include "inet/networklayer/common/HopLimitTag_m.h"
#include "inet/networklayer/common/L3AddressTag_m.h"
#include "inet/networklayer/contract/IInterfaceTable.h"
#include "inet/networklayer/icmpv6/Icmpv6.h"
#include "inet/wirelesspan/networklayer/icmpv6/WpanIpv6NeighbourDiscovery.h"
#include "inet/networklayer/ipv6/Ipv6Header.h"
#include "inet/networklayer/ipv6/Ipv6InterfaceData.h"
#include "inet/networklayer/ipv6/Ipv6RoutingTable.h"


namespace inet {
namespace wirelesspan {

#define MK_ASSIGN_LINKLOCAL_ADDRESS    0
#define MK_SEND_PERIODIC_RTRADV        1
#define MK_SEND_SOL_RTRADV             2
#define MK_INITIATE_RTRDIS             3
#define MK_DAD_TIMEOUT                 4
#define MK_RD_TIMEOUT                  5
#define MK_NUD_TIMEOUT                 6
#define MK_AR_TIMEOUT                  7
#define WIND_SEND_DELAYED              8 // CUSTOM msg kind for WIND (wireless neighbor discovery)


Define_Module(WpanIpv6NeighbourDiscovery);


WpanIpv6NeighbourDiscovery::WpanIpv6NeighbourDiscovery()
{
}

WpanIpv6NeighbourDiscovery::~WpanIpv6NeighbourDiscovery()
{
    // FIXME delete the following data structures, cancelAndDelete timers in them etc.
    // Deleting the data structures my become unnecessary if the lists store the
    // structs themselves and not pointers.

    //   RaTimerList raTimerList;
    for (const auto & elem : raTimerList) {
        cancelAndDelete(elem);
        delete (elem);
    }

    //   DadList dadList;
    for (const auto & elem : dadList) {
        cancelAndDelete((elem)->timeoutMsg);
        delete (elem);
    }

    //   RdList rdList;
    for (const auto & elem : rdList) {
        cancelAndDelete((elem)->timeoutMsg);
        delete (elem);
    }

    //   AdvIfList advIfList;
    for (const auto & elem : advIfList) {
        cancelAndDelete((elem)->raTimeoutMsg);
        delete (elem);
    }
}

void WpanIpv6NeighbourDiscovery::initialize(int stage)
{
    Ipv6NeighbourDiscovery::initialize(stage);

    if (stage == INITSTAGE_LOCAL) {
        nsFwdDelay = par("nsForwardingDelay").doubleValue();
        EV_DETAIL << "ns forwarding delay: " << nsFwdDelay << endl;

        pAddRandomDelays = par("addRandomDelays").boolValue();
        pRandomDelayMin = par("randomDelayMin").doubleValue();
        pRandomDelayMax = par("randomDelayMax").doubleValue();
        EV_DETAIL << "pAddRandomDelays enabled: " << pAddRandomDelays << " in range " << pRandomDelayMin << "-" << pRandomDelayMax << endl;
    }
}

void WpanIpv6NeighbourDiscovery::handleMessage(cMessage *msg)
{
    if (msg->isSelfMessage() && msg->getKind() == WIND_SEND_DELAYED) {
        auto ci = check_and_cast<Ipv6NdPacketInfo*> (msg->getControlInfo());
        sendPacketToIpv6Module(ci->getMsgPtr(), ci->getDestAddr(), ci->getSrcAddr(), ci->getInterfaceId());
        delete msg;
    }
    else
        Ipv6NeighbourDiscovery::handleMessage(msg);
}

void WpanIpv6NeighbourDiscovery::initiateNeighbourUnreachabilityDetection(Neighbour *nce)
{
    // CUSTOM WiND PART
    if (par("disableNud").boolValue())
        return;
    // END CUSTOM PART
    Ipv6NeighbourDiscovery::initiateNeighbourUnreachabilityDetection(nce);
}


void WpanIpv6NeighbourDiscovery::initiateAddressResolution(const Ipv6Address& dgSrcAddr,
        Neighbour *nce)
{
    const Key *nceKey = nce->nceKey;
    auto ie = ift->getInterfaceById(nceKey->interfaceID);
    Ipv6Address neighbourAddr = nceKey->address;
    int ifID = nceKey->interfaceID;

    //RFC2461: Section 7.2.2
    //When a node has a unicast packet to send to a neighbor, but does not
    //know the neighbor's link-layer address, it performs address
    //resolution.  For multicast-capable interfaces this entails creating a
    //Neighbor Cache entry in the INCOMPLETE state(already created if not done yet)
    //WEI-If entry already exists, we still have to ensure that its state is INCOMPLETE.
    nce->reachabilityState = Ipv6NeighbourCache::INCOMPLETE;

    //and transmitting a Neighbor Solicitation message targeted at the
    //neighbor.  The solicitation is sent to the solicited-node multicast
    //address "corresponding to"(or "derived from") the target address.
    //(in this case, the target address is the address we are trying to resolve)
    EV_INFO << "Preparing to send NS to solicited-node multicast group\n";
    EV_INFO << "on the next hop interface\n";
    Ipv6Address nsDestAddr = neighbourAddr.formSolicitedNodeMulticastAddress();    //for NS datagram
    Ipv6Address nsTargetAddr = neighbourAddr;    //for the field within the NS
    Ipv6Address nsSrcAddr;

    /*If the source address of the packet prompting the solicitation is the
       same as one of the addresses assigned to the outgoing interface,*/
    if (ie->getProtocolData<Ipv6InterfaceData>()->hasAddress(dgSrcAddr))
        /*that address SHOULD be placed in the IP Source Address of the outgoing
           solicitation.*/
        nsSrcAddr = dgSrcAddr;
    else
        /*Otherwise, any one of the addresses assigned to the interface
           should be used.*/
        nsSrcAddr = ie->getProtocolData<Ipv6InterfaceData>()->getPreferredAddress();
    ASSERT(ifID != -1);
    //Sending NS on specified interface.
    createAndSendNsPacket(nsTargetAddr, nsDestAddr, nsSrcAddr, ie);
    nce->numOfARNSSent = 1;
    nce->nsSrcAddr = nsSrcAddr;

    /*While awaiting a response, the sender SHOULD retransmit Neighbor Solicitation
       messages approximately every RetransTimer milliseconds, even in the absence
       of additional traffic to the neighbor. Retransmissions MUST be rate-limited
       to at most one solicitation per neighbor every RetransTimer milliseconds.*/
    cMessage *msg = new cMessage("arTimeout", MK_AR_TIMEOUT);    //AR msg timer
    nce->arTimer = msg;
    msg->setContextPointer(nce);
//    scheduleAt(simTime() + ie->getProtocolData<Ipv6InterfaceData>()->_getRetransTimer(), msg); ORIGINAL

    // CUSTOM WiND PART - retry NS messages after slightly randomized timeouts to avoid deadlocks
    // when cells overlap in TSCH
    scheduleAt(simTime() + ie->getProtocolData<Ipv6InterfaceData>()->getRetransTimer() + uniform(0, nsFwdDelay), msg);
}

void WpanIpv6NeighbourDiscovery::processArTimeout(cMessage *arTimeoutMsg)
{
    //AR timeouts are cancelled when a valid solicited NA is received.
    Neighbour *nce = (Neighbour *)arTimeoutMsg->getContextPointer();
    const Key *nceKey = nce->nceKey;
    Ipv6Address nsTargetAddr = nceKey->address;
    auto ie = ift->getInterfaceById(nceKey->interfaceID);
    EV_INFO << "Num Of NS Sent:" << nce->numOfARNSSent << endl;
    EV_INFO << "Max Multicast Solicitation:" << ie->getProtocolData<Ipv6InterfaceData>()->_getMaxMulticastSolicit() << endl;

    if (nce->numOfARNSSent < ie->getProtocolData<Ipv6InterfaceData>()->_getMaxMulticastSolicit()) {
        EV_INFO << "Sending another Address Resolution NS message" << endl;
        Ipv6Address nsDestAddr = nsTargetAddr.formSolicitedNodeMulticastAddress();
        createAndSendNsPacket(nsTargetAddr, nsDestAddr, nce->nsSrcAddr, ie);
        nce->numOfARNSSent++;
        // CUSTOM WIND PART - retry NS messages after slightly randomized timeouts to avoid deadlocks with overlapping cells
        scheduleAt(simTime() + ie->getProtocolData<Ipv6InterfaceData>()->_getRetransTimer() + uniform(0, nsFwdDelay), arTimeoutMsg);
        // OG
//        scheduleAt(simTime() + ie->getProtocolData<Ipv6InterfaceData>()->_getRetransTimer(), arTimeoutMsg);
        return;
    }

    EV_WARN << "Address Resolution has failed." << endl;
    dropQueuedPacketsAwaitingAr(nce);
    EV_INFO << "Deleting AR timeout msg\n";
    delete arTimeoutMsg;
}

void WpanIpv6NeighbourDiscovery::sendPacketToIpv6Module(Packet *msg, const Ipv6Address& destAddr, const Ipv6Address& srcAddr, int interfaceId, double delay)
{

    if (delay <= 0)
        Ipv6NeighbourDiscovery::sendPacketToIpv6Module(msg, destAddr, srcAddr, interfaceId);
    else
    {
        auto selfmsg = new cMessage("WiND delayed packet");
        auto controlInfo = new Ipv6NdPacketInfo(msg, destAddr, srcAddr, interfaceId);
        selfmsg->setControlInfo(controlInfo);
        selfmsg->setKind(WIND_SEND_DELAYED);

        scheduleAt(simTime() + delay, selfmsg);
    }
}



void WpanIpv6NeighbourDiscovery::assignLinkLocalAddress(cMessage *timerMsg)
{
    //Node has booted up. Start assigning a link-local address for each
    //interface in this node.
    for (int i = 0; i < ift->getNumInterfaces(); i++) {
        auto ie = ift->getInterface(i);

        //Skip the loopback interface.
        if (ie->isLoopback())
            continue;

        Ipv6Address linkLocalAddr = ie->getProtocolData<Ipv6InterfaceData>()->getLinkLocalAddress();
        if (linkLocalAddr.isUnspecified()) {
            //if no link local address exists for this interface, we assign one to it.
            EV_INFO << "No link local address exists. Forming one" << endl;
            linkLocalAddr = Ipv6Address().formLinkLocalAddress(ie->getInterfaceToken());
            EV_DETAIL << "Formed link local address - " << linkLocalAddr << endl;
            ie->getProtocolDataForUpdate<Ipv6InterfaceData>()->assignAddress(linkLocalAddr, true, SIMTIME_ZERO, SIMTIME_ZERO);
            // CUSTOM WiND PART
            if (par("skipDad").boolValue()) {
                makeTentativeAddressPermanent(linkLocalAddr, ie);
                EV_DETAIL << "and assigned permanent" << endl;
            }

            // END CUSTOM PART
        }

        //Before we can use this address, we MUST initiate DAD first.
        if (ie->getProtocolData<Ipv6InterfaceData>()->isTentativeAddress(linkLocalAddr)) {

            // CUSTOM WiND PART
            if (par("skipDad").boolValue())
                makeTentativeAddressPermanent(linkLocalAddr, ie);
            // END CUSTOM PART
            else
            {
                if (ie->getProtocolData<Ipv6InterfaceData>()->getDupAddrDetectTransmits() > 0)
                    initiateDad(linkLocalAddr, ie);
                else
                    makeTentativeAddressPermanent(linkLocalAddr, ie);
            }

        }
    }
    delete timerMsg;
}


void WpanIpv6NeighbourDiscovery::createRaTimer(NetworkInterface *ie)
{
    // CUSTOM PART FOR WiND
    if (!par("raEnabled").boolValue())
        return;
    Ipv6NeighbourDiscovery::createRaTimer(ie);
}


void WpanIpv6NeighbourDiscovery::createAndSendNsPacket(const Ipv6Address& nsTargetAddr, const Ipv6Address& dgDestAddr,
        const Ipv6Address& dgSrcAddr, NetworkInterface *ie)
{
#ifdef INET_WITH_xMIPv6
    Enter_Method_Silent();
#endif /* WITH_xMIPv6 */

    MacAddress myMacAddr = ie->getMacAddress();

    //Construct a Neighbour Solicitation message
    auto ns = makeShared<Ipv6NeighbourSolicitation>();

    //Neighbour Solicitation Specific Information
    ns->setTargetAddress(nsTargetAddr);

    /*If the solicitation is being sent to a solicited-node multicast
       address, the sender MUST include its link-layer address (if it has
       one) as a Source Link-Layer Address option.*/
    if (dgDestAddr.matches(Ipv6Address("FF02::1:FF00:0"), 104) &&    // FIXME what's this? make constant...
            !dgSrcAddr.isUnspecified()) {
        auto sla = new Ipv6NdSourceLinkLayerAddress();
        sla->setLinkLayerAddress(myMacAddr);
        ns->getOptionsForUpdate().appendOption(sla);
        ns->addChunkLength(IPv6ND_LINK_LAYER_ADDRESS_OPTION_LENGTH);
    }
    auto packet = new Packet("NSpacket");
    Icmpv6::insertChecksum(checksumMode, ns, packet);
    packet->insertAtFront(ns);
//    ORIGINAL
//    sendPacketToIpv6Module(packet, dgDestAddr, dgSrcAddr, ie->getInterfaceId());

    // CUSTOM WiND PART
    auto timeoutValue = nsFwdDelay > 0 ? uniform(0, nsFwdDelay, myMacAddr.getInt() % 10) : 0; // FIXME: how to get num RNGs?
    sendPacketToIpv6Module(packet, dgDestAddr, dgSrcAddr, ie->getInterfaceId(), timeoutValue);
}


void WpanIpv6NeighbourDiscovery::processNaForIncompleteNceState(const Ipv6NeighbourAdvertisement *na, Neighbour *nce)
{
    MacAddress naMacAddr;
    if (auto tla = check_and_cast_nullable<const Ipv6NdTargetLinkLayerAddress *>(na->getOptions().findOption(IPv6ND_TARGET_LINK_LAYER_ADDR_OPTION)))
        naMacAddr = tla->getLinkLayerAddress();
    bool naRouterFlag = na->getRouterFlag();
    bool naSolicitedFlag = na->getSolicitedFlag();
    const Key *nceKey = nce->nceKey;
    NetworkInterface *ie = ift->getInterfaceById(nceKey->interfaceID);

    /*If the target's neighbour Cache entry is in the INCOMPLETE state when the
       advertisement is received, one of two things happens.*/
    if (naMacAddr.isUnspecified()) {
        /*If the link layer has addresses and no Target Link-Layer address option
           is included, the receiving node SHOULD silently discard the received
           advertisement.*/
        EV_INFO << "No MAC Address specified in NA. Ignoring NA\n";
        return;
    }
    else {
        // Otherwise, the receiving node performs the following steps:
        // - It records the link-layer address in the neighbour Cache entry.
        EV_INFO << "ND is updating Neighbour Cache Entry.\n";
        nce->macAddress = naMacAddr;

        // - If the advertisement's Solicited flag is set, the state of the
        // entry is set to REACHABLE, otherwise it is set to STALE.
        if (naSolicitedFlag == true) {
            nce->reachabilityState = Ipv6NeighbourCache::REACHABLE;
            EV_INFO << "Reachability confirmed through successful Addr Resolution.\n";
            nce->reachabilityExpires = simTime() + ie->getProtocolData<Ipv6InterfaceData>()->_getReachableTime()
                    + par("nceExpiryOverride"); // CUSTOM WiND PART
            //nce->reachabilityExpires = simTime() + ie->getProtocolData<Ipv6InterfaceData>()->_getReachableTime();
        }
        else
            nce->reachabilityState = Ipv6NeighbourCache::STALE;

        // - It sets the IsRouter flag in the cache entry based on the Router
        // flag in the received advertisement.
        nce->isRouter = naRouterFlag;
        if (nce->isDefaultRouter() && !nce->isRouter)
            neighbourCache.getDefaultRouterList().remove(*nce);

        // - It sends any packets queued for the neighbour awaiting address
        // resolution.
        sendQueuedPacketsToIpv6Module(nce);
        cancelAndDelete(nce->arTimer);
        nce->arTimer = nullptr;
    }
}


void WpanIpv6NeighbourDiscovery::processNaForOtherNceStates(const Ipv6NeighbourAdvertisement *na, Neighbour *nce)
{
    bool naRouterFlag = na->getRouterFlag();
    bool naSolicitedFlag = na->getSolicitedFlag();
    bool naOverrideFlag = na->getOverrideFlag();
    MacAddress naMacAddr;
    if (auto tla = check_and_cast_nullable<const Ipv6NdTargetLinkLayerAddress *>(na->getOptions().findOption(IPv6ND_TARGET_LINK_LAYER_ADDR_OPTION)))
        naMacAddr = tla->getLinkLayerAddress();
    const Key *nceKey = nce->nceKey;
    NetworkInterface *ie = ift->getInterfaceById(nceKey->interfaceID);

    /*draft-ietf-ipv6-2461bis-04
       Section 7.2.5: Receipt of Neighbour Advertisements
       If the target's Neighbor Cache entry is in any state other than INCOMPLETE
       when the advertisement is received, the following actions take place:*/

    if (naOverrideFlag == false && !(naMacAddr.equals(nce->macAddress))
        && !(naMacAddr.isUnspecified()))
    {
        EV_INFO << "NA override is FALSE and NA MAC addr is different.\n";

        // I. If the Override flag is clear and the supplied link-layer address
        // differs from that in the cache, then one of two actions takes place:
        // (Note: An unspecified MAC should not be compared with the NCE's mac!)
        // a. If the state of the entry is REACHABLE,
        if (nce->reachabilityState == Ipv6NeighbourCache::REACHABLE) {
            EV_INFO << "NA mac is different. Change NCE state from REACHABLE to STALE\n";
            // set it to STALE, but do not update the entry in any other way.
            nce->reachabilityState = Ipv6NeighbourCache::STALE;
        }
        else
            // b. Otherwise, the received advertisement should be ignored and
            // MUST NOT update the cache.
            EV_INFO << "NCE is not in REACHABLE state. Ignore NA.\n";
    }
    else if (naOverrideFlag == true || naMacAddr.equals(nce->macAddress)
             || naMacAddr.isUnspecified())
    {
        EV_INFO << "NA override flag is TRUE. or Advertised MAC is same as NCE's. or"
                << " NA MAC is not specified.\n";
        /*II. If the Override flag is set, or the supplied link-layer address
           is the same as that in the cache, or no Target Link-layer address
           option was supplied, the received advertisement MUST update the
           Neighbor Cache entry as follows:*/

        /*- The link-layer address in the Target Link-Layer Address option
            MUST be inserted in the cache (if one is supplied and is
            Different than the already recorded address).*/
        if (!(naMacAddr.isUnspecified()) && !(naMacAddr.equals(nce->macAddress))) {
            EV_INFO << "Updating NCE's MAC addr with NA's.\n";
            nce->macAddress = naMacAddr;
        }

        // - If the Solicited flag is set,
        if (naSolicitedFlag == true) {
            EV_INFO << "Solicited Flag is TRUE. Set NCE state to REACHABLE.\n";
            // the state of the entry MUST be set to REACHABLE.
            nce->reachabilityState = Ipv6NeighbourCache::REACHABLE;
            // We have to cancel the NUD self timer message if there is one.

            cMessage *msg = nce->nudTimeoutEvent;
            if (msg != nullptr) {
                EV_INFO << "NUD in progress. Cancelling NUD Timer\n";
                bubble("Reachability Confirmed via NUD.");
                nce->reachabilityExpires = simTime() + ie->getProtocolData<Ipv6InterfaceData>()->_getReachableTime()
                        + par("nceExpiryOverride"); // CUSTOM WiND PART
                //nce->reachabilityExpires = simTime() + ie->getProtocolData<Ipv6InterfaceData>()->_getReachableTime();
                cancelAndDelete(msg);
                nce->nudTimeoutEvent = nullptr;
            }
        }
        else {
            // If the Solicited flag is zero
            EV_INFO << "Solicited Flag is FALSE.\n";
            // and the link layer address was updated with a different address

            if (!(naMacAddr.equals(nce->macAddress))) {
                EV_INFO << "NA's MAC is different from NCE's.Set NCE state to STALE\n";
                // the state MUST be set to STALE.
                nce->reachabilityState = Ipv6NeighbourCache::STALE;
            }
            else
                // Otherwise, the entry's state remains unchanged.
                EV_INFO << "NA's MAC is the same as NCE's. State remains unchanged.\n";
        }
        // (Next paragraph with explanation is omitted.-WEI)

        /*- The IsRouter flag in the cache entry MUST be set based on the
           Router flag in the received advertisement.*/
        EV_INFO << "Updating NCE's router flag to " << naRouterFlag << endl;
        nce->isRouter = naRouterFlag;

        /*In those cases where the IsRouter flag changes from TRUE to FALSE as a
           result of this update, the node MUST remove that router from the Default
           Router List and update the Destination Cache entries for all destinations
           using that neighbor as a router as specified in Section 7.3.3. This is
           needed to detect when a node that is used as a router stops forwarding
           packets due to being configured as a host.*/
        if (nce->isDefaultRouter() && !nce->isRouter)
            neighbourCache.getDefaultRouterList().remove(*nce);

        // TODO remove destination cache entries
    }
}



}
} // namespace inet

