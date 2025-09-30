/*
 * Simulation model for RPL (Routing Protocol for Low-Power and Lossy Networks)
 *
 * Copyright (C) 2021  Institute of Communication Networks (ComNets),
 *                     Hamburg University of Technology (TUHH)
 *           (C) 2021  Yevhenii Shudrenko
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <deque>
#include <numeric>
#include <string>
#include <random>
#include <algorithm>
#include <regex>
#include <math.h>
#include "inet/wirelesspan/routing/rpl/Rpl.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/SignalTag_m.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/IRadio.h"
#include "inet/physicallayer/wireless/common/base/packetlevel/NarrowbandReceiverBase.h"

namespace inet {
namespace wirelesspan {
namespace routing {

Define_Module(Rpl);

// statistic signals
simsignal_t Rpl::dioReceivedSignal = registerSignal("dioReceived");
simsignal_t Rpl::daoReceivedSignal = registerSignal("daoReceived");
simsignal_t Rpl::parentUnreachableSignal = registerSignal("parentUnreachable");
simsignal_t Rpl::parentChangedSignal = registerSignal("parentChanged");
simsignal_t Rpl::rankUpdatedSignal = registerSignal("rankUpdated");
simsignal_t Rpl::childJoinedSignal = registerSignal("childJoined");


std::ostream& operator<<(std::ostream& os, std::vector<Ipv6Address> &list)
{
    for (auto const &addr: list) {
        os << addr << ", " << endl;
    }

    return os;
}

std::ostream& operator<<(std::ostream& os, std::map<Ipv6Address, std::pair<cMessage *, uint8_t>> acks)
{
    for (auto a : acks)
        os << a.first << " (" << a.second.second << " attempts), timeout event - " << a.second.first << endl;
    return os;
}

double Rpl::currentFrequency()
{
    if (receiver) {
        return receiver->getCenterFrequency().get();
    }
    return -1;
}

// TODO: Refactor utility functions out

// The vairables are not initialized in the declaration
//Rpl::Rpl() :
//    dodagId(Ipv6Address::UNSPECIFIED_ADDRESS),
//    daoDelay(DEFAULT_DAO_DELAY),
//    daoAckTimeout(10),
//    daoRtxCtn(0),
//    isRoot(false),
//    hasStarted(false),
//    preferredParent(nullptr),
//    pUnreachabilityDetectionEnabled(false),
//    floating(false),
//    numDaoDropped(0),
//    udpPacketsRecv(0),
//    isLeaf(false),
//    isMobile(false),
//    detachedTimeout(2), // manually suppressing previous DODAG info [RFC 6550, 8.2.2.1]
//    prefixLength(128),
//    numParentUpdates(0),
//    numDaoForwarded(0),
//    uplinkSlotOffset(0),
//    apps({}),
//    pJoinAtSinkAllowed(false),
//    daoSeqNum(0),
//    objectiveFunctionType("hopCount"),
//    dodagColor(cFigure::BLACK),
//    prefParentConnector(nullptr)
//{}

Rpl::Rpl()
{}


Rpl::~Rpl()
{
    stop();
}

void Rpl::initialize(int stage)
{
    EV_DETAIL << "Initialization stage: " << stage << endl;

    RoutingProtocolBase::initialize(stage);

    if (stage == INITSTAGE_LOCAL) {


        routingTable.reference(this, "routingTableModule", true);
        interfaceTable.reference(this, "interfaceTableModule", true);
        networkProtocol.reference(this, "networkProtocolModule", true);

        EV_DETAIL << "got interface table - " << interfaceTable << endl;

        nd = check_and_cast<Ipv6NeighbourDiscovery*>(getModuleByPath("^.ipv6.neighbourDiscovery"));


//        mac = getModuleByPath("^.wlan[0].mac.mac");
//
//        if (mac)
//            mac->subscribe("currentFrequency", this);

//        EV_DETAIL << "Found MAC module - " << mac << endl;
        trickleTimer = check_and_cast<TrickleTimer*>(getModuleByPath("^.trickleTimer"));

        daoEnabled = par("daoEnabled").boolValue();
        host = getContainingNode(this);
        hostName = host->getFullName();
        EV_DETAIL << "My full name: " << hostName << endl;

        objectiveFunction = new ObjectiveFunction(par("objectiveFunctionType").stdstringValue());
        objectiveFunction->setMinHopRankIncrease(par("minHopRankIncrease").intValue());
        daoRtxThresh = par("numDaoRetransmitAttempts").intValue();
        allowDodagSwitching = par("allowDodagSwitching").boolValue();
        pDaoAckEnabled = par("daoAckEnabled").boolValue();
        pUseWarmup = par("useWarmup").boolValue();
        pUnreachabilityDetectionEnabled = par("unreachabilityDetectionEnabled").boolValue();
        pShowBackupParents = par("showBackupParents").boolValue();
        pAllowDaoForwarding = par("allowDaoForwarding").boolValue();
        pJoinAtSinkAllowed = par("allowJoinAtSink").boolValue() || (uniform(0, 1) < par("joinAtSinkProbability").doubleValue());

        startDelay = par("startDelay").doubleValue();

        if (par("layoutConfigurator").boolValue())
            generateLayout(host->getParentModule()); // generate layout using the topmost simulation module, TODO: refactor into mobility extension module

        //dagInfo = *(new DodagInfo());
        dagInfo = DodagInfo();



        WATCH(numParentUpdates);
        WATCH_MAP(candidateParents);
        WATCH_MAP(backupParents);
        WATCH_PTRMAP(pendingDaoAcks);
//        WATCH_OBJ(dagInfo); TODO: figure out why this doesn't work! the object IS shown in the GUI, but without any fields
        WATCH(dagInfo.prefParent);
        WATCH(dagInfo.prefParentRank);
        WATCH(rank);
        WATCH(selfAddr);
        WATCH(selfId);
        WATCH(isMobile);
        WATCH(uplinkSlotOffset);
    }
    else if (stage == INITSTAGE_NETWORK_CONFIGURATION) {
        // list of interfaces.
        const char *auxChar = par("interfaceList").stringValue();
        std::vector<std::string> interfaceListStr = cStringTokenizer(auxChar).asVector();
        for (const auto &elem : interfaceListStr) {
            auto ie = interfaceTable->findInterfaceByName(elem.c_str());
            if (ie) {
                auto mod = ie->getSubmodule("radio");
                if (mod != nullptr) {
                    auto radio = dynamic_cast<physicallayer::IRadio *>(mod);
                    if (radio) {
                        auto rec = dynamic_cast<const physicallayer::NarrowbandReceiverBase *> (radio->getReceiver());
                        if (rec)
                            receiver = const_cast<physicallayer::NarrowbandReceiverBase *>(rec);
                    }
                }
                interfaceList.push_back(ie);
            }
        }
        if (interfaceList.empty())
            throw cRuntimeError("Not found any interface");
        if (interfaceList.size() > 1)
            throw cRuntimeError("this RPL implementation only support 1 interface");
    }
    else if (stage == INITSTAGE_ROUTING_PROTOCOLS) {
            // low-latency mode settings
#if LOWLATENCYSUPPORT
        if (par("lowLatencyMode").boolValue()) {
            auto tschSf = getModuleByPath("^.wlan[0].mac.sixtischInterface.sf");
            uplinkSlotOffsetSignal = registerSignal("uplinkSlotOffset");
            tschSf->subscribe("uplinkScheduled", this);
        }
#endif
        registerProtocol(Protocol::manet, gate("ipOut"), gate("ipIn"));
        host->subscribe(linkBrokenSignal, this);
        networkProtocol->registerHook(0, this);
    }
}

void Rpl::finish() {
    recordScalar("rank", rank);
}

void Rpl::generateLayout(cModule *net) {
    auto sink = net->getSubmodule("sink", 0);
    auto numSinks = net->par("numSinks").intValue();
    if (!sink)
        throw cRuntimeError("Couldn't find sink[0] module during layout configuration");

    auto sinkPos = sink->getSubmodule("mobility");

    Coord anchor(sinkPos->par("initialX").doubleValue(), sinkPos->par("initialY").doubleValue());
    double padX = par("padX").doubleValue();
    double padY = par("padY").doubleValue();
    double gap = par("gridColumnsGapMultiplier").doubleValue();

    // optional
    bool bLayout = par("branchesLayout").boolValue();
    int numBranches = par("numBranches").intValue();
    int numHosts = net->par("numHosts").intValue();

    configureTopologyLayout(anchor, padX, padY, gap, net, bLayout, numBranches, numHosts, numSinks);
}

void Rpl::deleteManualRoutes() {
    auto numRoutes = routingTable->getNumRoutes();
    vector<Ipv6Route*> deleteableRts;

    for (int i = 0; i < numRoutes; i++) {
        auto ri = routingTable->getRoute(i);
        string routeTypeAbbr(ri->getSourceTypeAbbreviation());

        if (ri->getSourceType() == Ipv6Route::MANUAL || routeTypeAbbr.find("???") != string::npos)
            deleteableRts.push_back(ri);
    }

    for (auto rt: deleteableRts)
        routingTable->deleteRoute(rt);
}

void Rpl::configureTopologyLayout(Coord anchorPoint, double padX, double padY, double columnGapMultiplier,
        cModule *network, bool branchLayout, int numBranches, int numHosts, int numSinks)
{
    EV_DETAIL << "Configuring topology for " << boolStr(branchLayout, "branch", "seatbelt") << " layout" << endl;

    int i = 1;
    int k = 1;
    int l = 0;
    int m = 0;
    int hopCtn = 0;
    int middleBranchId = (int) floor(numBranches / 2);
    int nodesPerBranch = branchLayout ? (int) ceil(numHosts / numBranches) : 0;

    double skew = numBranches % 2 == 0 ? 1.5 : 1.2;
    int numHops = par("numHops").intValue();

    double currentX = anchorPoint.x - (branchLayout ? (numBranches * skew) : columnGapMultiplier) * padX;
    double defaultY = branchLayout ? (anchorPoint.y + padY) : anchorPoint.y;
    double currentY = defaultY;

    // Shift starting location if we have an even number of sinks
    double bias = (!branchLayout && numSinks % 2 == 0) ? padY * numHosts / 16 : 0;

    for (cModule::SubmoduleIterator it(network); !it.end(); ++it) {
        cModule *subm = *it;
        std::string sname(subm->getFullName());
        auto nodeId = getNodeId(sname);
        auto subMobility = subm->getSubmodule("mobility");

        if (branchLayout) {
            if (nodeId <= 0)
                continue;

            subMobility->par("initialX") = currentX;
            subMobility->par("initialY") = currentY;

            if (currentY == defaultY) {
                // allow branch roots join the sink directly, since they are closest to it
                subm->getSubmodule("rpl")->par("allowJoinAtSink") = true;
                hopCtn = 1;
                if (m != (int) ceil(numBranches / 2)) {
                    auto drift = ((m < middleBranchId ? padX : -padX) * pow((m - middleBranchId + 0.0001) / 1.3, 2));
                    subMobility->par("initialX") = currentX + drift;
                } else
                    k = numBranches % 2;
                m++;
            } else {
                try {
                    subm->getSubmodule("rpl")->par("allowJoinAtSink") = false;
                    subm->getSubmodule("rpl")->par("assignParentManual") = true;
                } catch (...) {}
            }

            currentX += pow(-1, k++);
            // switching to next branch
            if (nodeId % nodesPerBranch == 0) {
                currentX += padX * columnGapMultiplier;
                EV_DETAIL << "Switching to next branch, updated currentX to " << currentX << endl;
                currentY = defaultY;
            } else
                currentY += padY;

            if (numHops > 0) {
                subm->par("numApps") = hopCtn == numHops ? 1 : 0;
                hopCtn++;
            }

        } else {
            if (sname.find("sink") != std::string::npos) {
                double initialY = anchorPoint.y + pow(-1, k++) * padY * l * numHosts / (numSinks * 2.5) - bias;
                if (l == 0 || k % 2 == 0)
                    l++;
                subMobility->par("initialY") = initialY;
                subMobility->par("initialX") = anchorPoint.x;
            } else {
                if (nodeId <= 0)
                    continue;

                subMobility->par("initialX") = currentX;
                subMobility->par("initialY") = currentY;
                // switching to next row
                if (nodeId % 6 == 0) {
                    currentX = anchorPoint.x - columnGapMultiplier * padX;
                    currentY = anchorPoint.y + pow(-1, i) * padY * i;
                    i++;
                    // 'jumping' over the aircraft central hallway
                } else if (nodeId % 3 == 0)
                    currentX += columnGapMultiplier * padX;
                else
                    currentX += padX;
            }
        }
    }
}


int Rpl::getNodeId(std::string nodeName) {
    const std::regex base_regex("host\\[([0-9]+)\\]");
    std::smatch base_match;

    if (std::regex_match(nodeName, base_match, base_regex)) {
        // The first sub_match is the whole string; the next
        // sub_match is the first parenthesized expression.
        if (base_match.size() == 2) {
            std::ssub_match base_sub_match = base_match[1];
            std::string base = base_sub_match.str();
            return std::stoi(base);
        }
    }
    return -1;
}

cFigure::Color Rpl::pickRandomColor() {
    auto palette = selfId % 2 == 0 ? cFigure::GOOD_DARK_COLORS : cFigure::GOOD_LIGHT_COLORS;
    auto numColors = selfId % 2 == 0 ? cFigure::NUM_GOOD_DARK_COLORS : cFigure::NUM_GOOD_LIGHT_COLORS;

    try {
        return host->getParentModule()->par("numSinks").intValue() == 1 ? cFigure::BLACK : palette[intrand(numColors, 0) - 1];
    } catch (...) {
        return cFigure::BLACK;
    }
}

cModule* Rpl::findSubmodule(std::string sname, cModule *host) {
    EV_DETAIL << "Looking for module " << sname << " in " << host << endl;

    for (cModule::SubmoduleIterator it(host); !it.end(); ++it) {
        cModule *subm = *it;
        std::string s(subm->getFullName());

        if (s.find(sname) != std::string::npos)
            return *it;
    }
    return nullptr;
}

//
// Lifecycle operations
//

void Rpl::start()
{
    if (startDelay > 0) {
        auto startMsg = new cMessage("", RPL_START);
        scheduleAt(simTime() + startDelay, startMsg);
        pendingTimers.insert(startMsg);
        return;
    }
    hasStarted = true;

    isRoot = par("isRoot").boolValue(); // Initialization of this parameter should be here to ensure
                                        // multi-gateway configurator will have time to assign 'root' roles
                                        // to randomly chosen nodes
    //position = *(new Coord());
    position = Coord::NIL;
    interfaceEntryPtr = interfaceList.front();


    selfId = interfaceEntryPtr->getMacAddress().getInt(); //interfaceTable->getInterface(1)->getMacAddress().getInt();
    mobility = check_and_cast<IMobility*> (getParentModule()->getSubmodule("mobility"));
    isMobile = mobility->getMaxSpeed() > 0;
    position = mobility->getCurrentPosition();

    EV_DETAIL << "RPL detected node location: " << position << endl;

    auto numApps = host->par("numApps").intValue();
    EV_DETAIL << "Detected " << numApps << " apps" << endl;

    for (auto i = 0; i < numApps; i++)
        apps.push_back(host->getSubmodule("app", i));

    rank = INF_RANK; // TODO: was INF_RANK - 1, why?
    detachedTimeoutEvent = new cMessage("", DETACHED_TIMEOUT);
    selfAddr = getSelfAddress();

    deleteManualRoutes();

    if (isRoot && !par("disabled").boolValue()) {
        trickleTimer->setMinInterval(DEFAULT_DIO_INTERVAL_MIN);
        trickleTimer->setNumDoublings(DEFAULT_DIO_INTERVAL_DOUBLINGS);
        trickleTimer->setCurrentInterval(DEFAULT_DIO_INTERVAL_MIN);
        trickleTimer->setRedundancyConst(DEFAULT_DIO_REDUNDANCY_CONST);

        trickleTimer->start(pUseWarmup, par("numSkipTrickleIntervalUpdates").intValue());
        dodagColor = pickRandomColor();
        rank = ROOT_RANK;
        dodagVersion = DEFAULT_INIT_DODAG_VERSION;
        instanceId = RPL_DEFAULT_INSTANCE;
        dtsn = 0;
        storing = par("storing").boolValue();
        for (auto app : apps)
            app->subscribe("packetReceived", this);

    }

    // For demo purposes
    if (par("scheduleEthernetPkt").boolValue()) {
        auto sendEth = new cMessage("", SEND_ETHER);
        scheduleAt(simTime() + 20, new cMessage("", SEND_ETHER));
        pendingTimers.insert(sendEth);
    }
}

void Rpl::refreshDisplay() const {
    if (preferredParent && preferredParent->isMobile() && prefParentConnector && parentMobilityMod) {
        auto parentLoc = parentMobilityMod->getCurrentPosition();

        prefParentConnector->setEnd(cFigure::Point(parentLoc.x, parentLoc.y));
    }

    if (isMobile && prefParentConnector) {
        auto currentCoord = mobility->getCurrentPosition();
        prefParentConnector->setStart(cFigure::Point(currentCoord.x, currentCoord.y));

        if (!preferredParent)
            prefParentConnector->setEnd(cFigure::Point(currentCoord.x, currentCoord.y));
    }

    if (isRoot) {
        host->getDisplayString().setTagArg("t", 0, std::string(" num rcvd: " + std::to_string(udpPacketsRecv)).c_str());
        host->getDisplayString().setTagArg("t", 1, "l"); // set display text position to 'left'
    }
}

void Rpl::stop()
{
    if (detachedTimeoutEvent)
        cancelAndDelete(detachedTimeoutEvent);
    if (daoAckTimeoutEvent)
        cancelAndDelete(daoAckTimeoutEvent);
    daoAckTimeoutEvent = nullptr;
    detachedTimeoutEvent = nullptr;
    for (auto elem : pendingDaoAcks) {
        auto pending = elem.second;
        cancelAndDelete(pending->timeoutPtr);
        delete pending;
    }
    for (auto elem : pendingTimers)
        cancelAndDelete(elem);
    pendingTimers.clear();
    pendingDaoAcks.clear();
}

void Rpl::handleMessageWhenUp(cMessage *message)
{
    if (message->isSelfMessage())
        processSelfMessage(message);
    else
        processMessage(message);
}


void Rpl::processSelfMessage(cMessage *message)
{
    auto type = message->getKind();
    switch (type) {
        case DETACHED_TIMEOUT: {
            floating = false;
            EV_DETAIL << "Detached state ended, processing new incoming RPL packets" << endl;
            break;
        }
        case RPL_START: {
            startDelay = 0;
            start();
            break;
        }
        case SEND_ETHER: {
            EV_DETAIL << "Preparing to send packet over ethernet" << endl;
            sendRplPacket(createDao(getSelfAddress()), DAO, preferredParent->getSrcAddress(), daoDelay * uniform(1, 2), "eth0");
            break;
        }
        case DAO_ACK_TIMEOUT: {
            Ipv6Address *advDest = (Ipv6Address*) message->getContextPointer();
            if (pendingDaoAcks.find(*advDest) == pendingDaoAcks.end())
                EV_WARN << "Received DAO_ACK timeout for deleted entry, ignoring" << endl;
            else
                retransmitDao(*advDest);
            break;
        }
        default:
            throw cRuntimeError("Unknown self-message received - %s", message->str().c_str());
            //EV_WARN << "Unknown self-message received - " << message << endl;
    }
    pendingTimers.erase(message);
    if (detachedTimeoutEvent == message)
        detachedTimeoutEvent = nullptr;
    if (daoAckTimeoutEvent == message)
        daoAckTimeoutEvent = nullptr;
    if (type != DAO_ACK_TIMEOUT)
        delete message;
}

void Rpl::clearDaoAckTimer(Ipv6Address daoDest) {
    auto it = pendingDaoAcks.find(daoDest);
    if (it != pendingDaoAcks.end()) {
        if (it->second->timeoutPtr)
            cancelAndDelete(it->second->timeoutPtr);
        delete it->second;
        pendingDaoAcks.erase(it);
    }
    return;
}

void Rpl::clearAllDaoAckTimers() {
    for (auto t: pendingDaoAcks) {
        if (t.second->timeoutPtr)
            cancelAndDelete(t.second->timeoutPtr);
        delete t.second;
    }
    pendingDaoAcks.erase(pendingDaoAcks.begin(), pendingDaoAcks.end());
}

void Rpl::retransmitDao(Ipv6Address advDest) {
    EV_DETAIL << "DAO_ACK for " << advDest << " timed out, attempting retransmit" << endl;

    if (!preferredParent) {
        EV_WARN << "Preferred parent not set, cannot retransmit DAO"
                << "erasing entry from pendingDaoAcks " << endl;
        clearDaoAckTimer(advDest);
        return;
    }

    auto rtxCtn = pendingDaoAcks[advDest]->numRetries++;
    if (rtxCtn > daoRtxThresh) {
        EV_DETAIL << "Retransmission threshold (" << std::to_string(daoRtxThresh)
            << ") exceeded, erasing corresponding entry from pending ACKs" << endl;
        clearDaoAckTimer(advDest);
        numDaoDropped++;
        return;
    }

    EV_DETAIL << "(" << std::to_string(rtxCtn) << " attempt)" << endl;

    sendRplPacket(createDao(advDest), DAO, preferredParent->getSrcAddress(), daoDelay);

}

void Rpl::detachFromDodag() {
    /**
     * If parent set of a node turns empty, it is no longer associated
     * with a DODAG and should suppress previous RPL state info by
     * clearing dodagId, neighbor sets and setting it's rank to INFINITE_RANK [RFC6560, 8.2.2.1]
     */
    eraseBackupParentList(backupParents);

    /** Delete all routes associated with DAO destinations of the former DODAG */
    purgeDaoRoutes();
    clearAllDaoAckTimers();
    rank = INF_RANK;
    trickleTimer->suspend(); // TODO: re-think this part of TT lifecycle, possibly replace with stop
    if (par("poisoning").boolValue())
        poisonSubDodag();
    dodagId = Ipv6Address::UNSPECIFIED_ADDRESS;
    floating = true;
    EV_DETAIL << "Detached state enabled, no RPL packets will be processed for "
            << (int)detachedTimeout << "s" << endl;
    // TODO: this was causing runtime crash, figure out how can this message not be a self-msg?
    if (detachedTimeoutEvent && detachedTimeoutEvent->isSelfMessage())
        cancelEvent(detachedTimeoutEvent);
    else
        detachedTimeoutEvent = new cMessage("Detachment from DODAG timeout", DETACHED_TIMEOUT);

    if (!isMobile)
        drawConnector(position, cFigure::BLACK);
    scheduleAt(simTime() + detachedTimeout, detachedTimeoutEvent);
}

void Rpl::eraseBackupParentList(map <Ipv6Address, Ptr<Dio> > &backupParents) {

    if (!backupParents.size())
        return;

    // First clear any drawn connector lines
    cCanvas *canvas = getParentModule()->getParentModule()->getCanvas();
    for (auto entry : backupConnectors)
        canvas->removeFigure(entry.second);

    backupConnectors.erase(backupConnectors.begin(), backupConnectors.end());
    backupParents.erase(backupParents.begin(), backupParents.end());
    EV_DETAIL << "Backup parents list erased" << endl;
}

void Rpl::purgeDaoRoutes() {
    if (!routingTable) {
        EV_WARN << "No routing table module found, can't purge routes learned from DAOs" << endl;
        return;
    }

    std::list<Ipv6Route *> purgedRoutes;
    EV_DETAIL << "Purging DAO routes from the routing table: " << endl;
    auto numRoutes = routingTable->getNumRoutes();
    for (int i = 0; i < numRoutes; i++) {
        auto ri = routingTable->getRoute(i);
        auto routeData = dynamic_cast<RplRouteData *> (ri->getProtocolData());
        if (routeData && routeData->getDodagId() == dodagId && routeData->getInstanceId() == instanceId)
            purgedRoutes.push_front(ri);
    }
    if (!purgedRoutes.empty()) {
        for (auto route : purgedRoutes)
            EV_DETAIL << route->getDestPrefix() << " via " << route->getNextHop()
                    << " : " << boolStr(routingTable->deleteRoute(route), "Success", "Fail");
    } else
        EV_DETAIL << "No DAO-associated routes found" << endl;
}

void Rpl::poisonSubDodag() {
    ASSERT(rank == INF_RANK);
    EV_DETAIL << "Poisoning sub-dodag by advertising INF_RANK " << endl;
    sendRplPacket(createDio(), DIO, Ipv6Address::ALL_NODES_1, uniform(1, 2));
}

//
// Handling generic packets
//

void Rpl::processMessage(cMessage *message)
{
    if (!hasStarted || par("disabled").boolValue())
        return;
    std::string arrivalGateName = std::string(message->getArrivalGate()->getBaseName());
    if (arrivalGateName.compare("ttModule") == 0)
        processTrickleTimerMsg(message);
    else if (Packet *fp = dynamic_cast<Packet *>(message)) {
        try {
            processPacket(fp);
        }
        catch (std::exception &e) {
            EV_WARN << "Error occured during packet processing: " << e.what() << endl;
        }
    }
}

void Rpl::processTrickleTimerMsg(cMessage *message)
{
    /**
     * Process signal from trickle timer module,
     * indicating DIO broadcast event [RFC6560, 8.3]
     */
    EV_DETAIL << "Processing msg from trickle timer" << endl;
    switch (message->getKind()) {
        case TRICKLE_TRIGGER_EVENT: {
            /**
             * Broadcast DIO only if number of DIOs heard
             * from other nodes <= redundancyConstant (k) [RFC6206, 4.2]
             */
            if (trickleTimer->checkRedundancyConst()) {
                EV_DETAIL << "Redundancy OK, broadcasting DIO" << endl;
                sendRplPacket(createDio(), DIO, Ipv6Address::ALL_NODES_1, uniform(0, 1));
            }
            break;
        }
        default: throw cRuntimeError("Unknown TrickleTimer message");
    }
    delete message;
}

bool Rpl::isRplPacket(Packet *packet) {
    auto fullname = std::string(packet->getFullName());
    return !(fullname.find("DIO") == std::string::npos && fullname.find("DAO") == std::string::npos);
}

void Rpl::processPacket(Packet *packet)
{
    if (floating || !isRplPacket(packet)) {
        EV_DETAIL << "Not processing packet " << packet
                << "\n due to floating (detached) state or because of unsupported packet type" << endl;
        delete packet;
        return;
    }

    RplHeader* rplHeader;
    EV_DETAIL << "Processing packet -" << packet << endl;

    try {
        rplHeader = const_cast<RplHeader*> ((packet->popAtFront<RplHeader>()).get());
    } catch (...) {
        EV_WARN << "Error trying to pop RPL header from " << packet << endl;
        return;
    }

    auto rssiTag = packet->findTag<SignalPowerInd>();
    double receivedPower = 0;
    if (rssiTag) {
        EV_DETAIL << "Received packet RSSI: " << rssiTag->getPower() << endl;
        receivedPower = rssiTag->getPower().get() * pow(10, 12); // in picowatts
    }

    // in non-storing mode check for RPL Target, Transit Information options
    if (!storing && dodagId != Ipv6Address::UNSPECIFIED_ADDRESS)
        extractSourceRoutingData(packet);

    auto rplBody = packet->peekData<RplPacket>();
    switch (rplHeader->getIcmpv6Code()) {
        case DIO: {
            processDio(dynamicPtrCast<const Dio>(rplBody), receivedPower);
            break;
        }
        case DAO: {
            processDao(dynamicPtrCast<const Dao>(rplBody));
            break;
        }
        case DAO_ACK: {
            processDaoAck(dynamicPtrCast<const Dao>(rplBody));
            break;
        }
        default: EV_WARN << "Unknown Rpl packet" << endl;
    }

    delete packet;
}

int Rpl::getNumDownlinks() {
    EV_DETAIL << "Calculating number of downlinks" << endl;
    auto numRts = routingTable->getNumRoutes();
    int downlinkRoutes = 0;
    for (int i = 0; i < numRts; i++) {
        auto ri = routingTable->getRoute(i);
        auto dest = ri->getDestinationAsGeneric().toIpv6();

        if (dest.isUnicast() && ri->getPrefixLength() == prefixLength)
            downlinkRoutes++;
    }


    return downlinkRoutes - 1; // minus uplink route through the preferred parent
}

void Rpl::sendPacket(cPacket *packet, double delay)
{
    if (delay == 0)
        send(packet, "ipOut");
    else
        sendDelayed(packet, delay, "ipOut");
}




//
// Handling RPL packets
//

void Rpl::sendRplPacket(const Ptr<RplPacket>& body, RplPacketCode code,
        const L3Address& nextHop, double delay, std::string interfaceName)
{
    sendRplPacket(body, code, nextHop, delay, Ipv6Address::UNSPECIFIED_ADDRESS, Ipv6Address::UNSPECIFIED_ADDRESS, interfaceName);
}

void Rpl::sendRplPacket(const Ptr<RplPacket>& body, RplPacketCode code,
        const L3Address& nextHop, double delay)
{
    sendRplPacket(body, code, nextHop, delay, Ipv6Address::UNSPECIFIED_ADDRESS, Ipv6Address::UNSPECIFIED_ADDRESS, "wlan0");
}

void Rpl::sendRplPacket(const Ptr<RplPacket> &body, RplPacketCode code,
        const L3Address& nextHop, double delay, const Ipv6Address &target, const Ipv6Address &transit, std::string interfaceName)
{
    Packet *pkt = new Packet(std::string("inet::RplPacket::" + rplIcmpCodeToStr(code)).c_str());

    auto header = makeShared<RplHeader>();
    header->setIcmpv6Code(code);
    pkt->addTagIfAbsent<PacketProtocolTag>()->setProtocol(&Protocol::manet);
    pkt->addTagIfAbsent<DispatchProtocolReq>()->setProtocol(&Protocol::ipv6);

    auto addresses = pkt->addTagIfAbsent<L3AddressReq>();
    addresses->setSrcAddress(getSelfAddress());
    addresses->setDestAddress(nextHop);
    pkt->insertAtFront(header);
    pkt->insertAtBack(body);
    // append RPL Target + Transit option headers if corresponding addresses were provided (non-storing mode)
    if (target != Ipv6Address::UNSPECIFIED_ADDRESS && transit != Ipv6Address::UNSPECIFIED_ADDRESS)
        appendDaoTransitOptions(pkt, target, transit);

    if (code == DAO && ((dynamicPtrCast<Dao>) (body))->getDaoAckRequired()) {
        auto outgoingDao = (dynamicPtrCast<Dao>) (body);
        auto advertisedDest = outgoingDao->getReachableDest();
        auto timeout = simTime() + SimTime(daoAckTimeout, SIMTIME_S) * uniform(1, 3); // TODO: Magic numbers

        EV_DETAIL << "Scheduling DAO_ACK timeout at " << timeout << " for advertised dest "
                << advertisedDest << endl;

        cMessage *daoTimeoutMsg = new cMessage("", DAO_ACK_TIMEOUT);
        daoTimeoutMsg->setContextPointer(new Ipv6Address(advertisedDest));
        EV_DETAIL << "Created DAO_ACK timeout msg with context pointer - " <<
                reinterpret_cast<Ipv6Address *>(daoTimeoutMsg->getContextPointer())->str() << endl;

        auto daoAckEntry = pendingDaoAcks.find(advertisedDest);
        if (daoAckEntry != pendingDaoAcks.end()) {
            if (daoAckEntry->second->timeoutPtr)
                cancelAndDelete(daoAckEntry->second->timeoutPtr);
            daoAckEntry->second->timeoutPtr = daoTimeoutMsg;
            EV_DETAIL << "Found existing entry in DAO_ACKs map for dest - " << advertisedDest
                    << " updating timeout event ptr" << endl;
        }
        else {
            pendingDaoAcks[advertisedDest] = new DaoTimeoutInfo(daoTimeoutMsg);
        }

        EV_DETAIL << "Pending DAO_ACKs:" << endl;
        for (auto e : pendingDaoAcks)
            EV_DETAIL << e.first << " (" << std::to_string(e.second->numRetries) << " attempts), timeout msg ptr - " << e.second->timeoutPtr << endl;
        scheduleAt(timeout, daoTimeoutMsg);
    }
    sendPacket(pkt, delay);
}

const Ptr<Dio> Rpl::createDio()
{
   // auto ourMacAddr = interfaceTable->getInterface(1)->getMacAddress();
    selfId = interfaceTable->getInterface(1)->getMacAddress().getInt();
    auto dio = makeShared<Dio>();
    dio->setInstanceId(instanceId);
    dio->setChunkLength(getDioSize());
    dio->setStoring(storing);
    dio->setRank(rank);
    dio->setDtsn(dtsn);
    dio->setNodeId(selfId);
    dio->setDodagVersion(dodagVersion);
    dio->setDodagId(isRoot ? getSelfAddress() : dodagId);
    dio->setSrcAddress(getSelfAddress());
    dio->setPosition(position);
    dio->setNodeName(hostName.c_str());
    dio->setIsMobile(isMobile);
    dio->setSlotOffset(uplinkSlotOffset);
    if (isRoot)
        dio->setColor(dodagColor);
    else
        dio->setColor(preferredParent ? preferredParent->getColor() : cFigure::GREY);

    EV_DETAIL << "DIO created advertising DODAG - " << dio->getDodagId()
                << " and rank " << dio->getRank() << endl;

    if (trickleTimer->getMinInterval() != 0)
        dio->setMinInterval(trickleTimer->getMinInterval());
    if (trickleTimer->getNumDoublings() != 0)
        dio->setDioNumDoublings(trickleTimer->getNumDoublings());
    if (trickleTimer->getRedundancyConst() != 0)
        dio->setDioRedundancyConst(trickleTimer->getRedundancyConst());

    return dio;
}


const Ptr<Dao> Rpl::createDao(const Ipv6Address &reachableDest)
{
    auto dao = makeShared<Dao>();
    dao->setInstanceId(instanceId);
    dao->setChunkLength(b(64));
    dao->setSrcAddress(getSelfAddress());
    dao->setReachableDest(reachableDest);
    dao->setSeqNum(daoSeqNum++);
    dao->setNodeId(selfId);
    dao->setDaoAckRequired(pDaoAckEnabled);
    EV_DETAIL << "Created DAO with seqNum = " << std::to_string(dao->getSeqNum()) << " advertising " << reachableDest << endl;
    return dao;
}

const Ptr<Dao> Rpl::createDao(const Ipv6Address &reachableDest, bool ackRequired)
{
    auto dao = makeShared<Dao>();
    dao->setInstanceId(instanceId);
    dao->setChunkLength(b(64));
    dao->setSrcAddress(getSelfAddress());
    dao->setReachableDest(reachableDest);
    dao->setSeqNum(daoSeqNum++);
    dao->setNodeId(selfId);
    dao->setDaoAckRequired(ackRequired);
    EV_DETAIL << "Created DAO with seqNum = " << std::to_string(dao->getSeqNum()) << " advertising " << reachableDest << endl;
    return dao;
}

bool Rpl::isUdpSink(cModule* app) {
    if (!app)
        return false;

    std::string appName(app->getNedTypeName());
    return appName.find("Sink") != std::string::npos;
}

bool Rpl::isInvalidDio(const Ptr<const Dio> &dio) {
    // If INFINITE_RANK is advertised not from the preferred parent (poisoning), discard the packet

    // 1st clause: if the node is detached from the DODAG and the advertised rank is INFINITE_RANK, discard the packet
    // 2nd clause: if the node has preferred parent but the DIO advertising INFINITE_RANK is NOT coming from the preferred parent,
    // discard the packet
    return (!preferredParent && dio->getRank() == INF_RANK)
            || (preferredParent && preferredParent->getSrcAddress() != dio->getSrcAddress() && dio->getRank() == INF_RANK);
}

void Rpl::processDio(const Ptr<const Dio>& dio, double rxPower) {
    if (isRoot || isInvalidDio(dio))
        return;

    if (dio->getRank() == 1 && rxPower < par("joinAtSinkPowerThresh").doubleValue())
        return;

    processDio(dio);
}

void Rpl::processDio(const Ptr<const Dio>& dio)
{
    if (isRoot || isInvalidDio(dio))
        return;

    EV_DETAIL << "Processing DIO from " << dio->getSrcAddress()
                << ", advertised rank - " << dio->getRank()
                << "\ncurrent freq - " << currentFrequency() << endl;

    emit(dioReceivedSignal, dio.get());

    // Custom low-latency mode part: do not join a DODAG if no slot offset is advertised to daisy-chain to
    if (par("lowLatencyMode").boolValue() && dio->getRank() > 1 && dio->getSlotOffset() == 0)
        return;

    // If node's not a part of any DODAG, join the first one advertised
    if (dodagId == Ipv6Address::UNSPECIFIED_ADDRESS)
    {
        dodagId = dio->getDodagId();
        dodagVersion = dio->getDodagVersion();
        instanceId = dio->getInstanceId();
        storing = dio->getStoring();
        dtsn = dio->getDtsn();
        lastTarget = getSelfAddress();
//        selfAddr = getSelfAddress();
        dodagColor = dio->getColor();
        if (dio->getMinInterval() != 0 && dio->getMinInterval() != -1)
            trickleTimer->setMinInterval(dio->getMinInterval());
        if (dio->getDioRedundancyConst() != 0 && dio->getDioRedundancyConst() != -1)
            trickleTimer->setRedundancyConst(dio->getDioRedundancyConst());
        if (dio->getDioNumDoublings() != 0 && dio->getDioNumDoublings() != -1)
            trickleTimer->setNumDoublings(dio->getDioNumDoublings());
        EV_DETAIL << "Joined DODAG with id - " << dodagId << endl;
        // Start broadcasting DIOs, diffusing DODAG control data, TODO: refactor TT lifecycle
        if (trickleTimer->hasStarted())
            trickleTimer->reset();
        else
            trickleTimer->start(false, par("numSkipTrickleIntervalUpdates").intValue());

        // Avoid overwriting manually set dest address
        for (auto app : apps)
            if (!isUdpSink(app) && app->par("destAddresses").stdstringValue().empty())
                app->par("destAddresses") = dio->getDodagId().str();
    }
    else {
        if (!allowDodagSwitching && dio->getDodagId() != dodagId) {
            EV_DETAIL << "Node already joined a DODAG, skipping DIO advertising other ones" << endl;
            return;
        }
        /**
         * If INFINITE_RANK is advertised from a preferred parent (route poisoning),
         * delete preferred parent and remove it from candidate neighbors
         */
        if (checkPoisonedParent(dio)) {
            EV_DETAIL << "Received poisoned DIO from preferred parent - "
                    << preferredParent->getSrcAddress() << endl;
            deletePrefParent(true);
            updatePreferredParent();
            return;
        }
    }
    trickleTimer->ctrlMsgReceived();

//    // Do not process DIO from unknown DAG/RPL instance, TODO: check with RFC
//    if (checkUnknownDio(dio)) {
//        EV_DETAIL << "Unknown DODAG/InstanceId, or receiver is root - discarding DIO" << endl;
//        return;
//    }
    if (dio->getRank() > rank) {
        EV_DETAIL << "Higher rank advertised, discarding DIO" << endl;
        return;
    }

    addNeighbour(dio);
    updatePreferredParent();
}

void Rpl::purgeRoutingTable() {
    auto numRoutes = routingTable->getNumRoutes();
    for (auto i = 0; i < numRoutes; i++)
        routingTable->deleteRoute(routingTable->getRoute(i));
}

bool Rpl::checkPoisonedParent(const Ptr<const Dio>& dio) {
    return preferredParent && dio->getRank() == INF_RANK && preferredParent->getSrcAddress() == dio->getSrcAddress();
}

void Rpl::processDao(const Ptr<const Dao>& dao) {
    if (!daoEnabled && !pAllowDaoForwarding) {
        EV_WARN << "DAO processing disabled, discarding packet" << endl;
        return;
    }

    if (!isRoot && !preferredParent) {
        EV_DETAIL << "Node is detached from DODAG, discarding DAO" << endl;
        return;
    }

    emit(daoReceivedSignal, dao.get());
    auto daoSender = dao->getSrcAddress();

    if (!isRoot && daoSender == preferredParent->getSrcAddress())
        throw cRuntimeError("Received DAO from the preferred parent, loop detected!");

    auto advertisedDest = dao->getReachableDest();
    EV_DETAIL << "Processing DAO with seq num " << std::to_string(dao->getSeqNum()) << " from "
            << daoSender << " advertising " << advertisedDest << endl;

    if (dao->getDaoAckRequired()) {
        auto timeout = uniform(1, 3);
        sendRplPacket(createDao(advertisedDest), DAO_ACK, daoSender, timeout);
        EV_DETAIL << "DAO_ACK will be sent back acknowledging advertised destination at " << simTime() + timeout << "s" << endl;
    }

    /**
     * If a node is root or operates in storing mode
     * update routing table with destinations from DAO [RFC6560, 3.3].
     * TODO: Implement DAO aggregation!
     */
    if (storing || isRoot) {
//        if (!checkDestKnown(daoSender, advertisedDest)) {
//            updateRoutingTable(daoSender, advertisedDest, prepRouteData(dao.get()));
//            emit(childJoinedSignal, 1);
//
//            EV_DETAIL << "Destination learned from DAO - " << advertisedDest
//                    << " reachable via " << daoSender << endl;
//        }
//        else
//            return;
        updateRoutingTable(daoSender, advertisedDest, prepRouteData(dao.get()));
        emit(childJoinedSignal, 1);
    }
    /**
     * Forward DAO 'upwards' via preferred parent advertising destination to the root [RFC6560, 6.4]
     */
    if (!isRoot && preferredParent) {
        if (!storing)
            sendRplPacket(createDao(advertisedDest), DAO,
                preferredParent->getSrcAddress(), daoDelay * uniform(1, 2), lastTarget, lastTransit);
        else
            sendRplPacket(createDao(advertisedDest), DAO,
                preferredParent->getSrcAddress(), daoDelay * uniform(1, 2));

        numDaoForwarded++;
        EV_DETAIL << "Forwarding DAO to " << preferredParent->getSrcAddress()
                << " advertising " << advertisedDest << " reachability" << endl;
    }
}

std::vector<Ipv6Address> Rpl::getNearestChildren() {
    auto prefParentAddr = preferredParent ? preferredParent->getSrcAddress() : Ipv6Address::UNSPECIFIED_ADDRESS;
    std::vector<Ipv6Address> neighbrs = {};
    for (auto i = 0; i < routingTable->getNumRoutes(); i++) {
        auto rt = routingTable->getRoute(i);
        auto dest = rt->getDestPrefix();
        auto nextHop = rt->getNextHop();

        if (dest == nextHop && dest != prefParentAddr)
            neighbrs.push_back(nextHop);
    }

    EV_DETAIL << "Found 1-hop neighbors (" << neighbrs.size() << ") - " << neighbrs << endl;
    return neighbrs;
}

void Rpl::processDaoAck(const Ptr<const Dao>& daoAck) {
    auto advDest = daoAck->getReachableDest();

    EV_INFO << "Received DAO_ACK from " << daoAck->getSrcAddress()
            << " for advertised dest - "  << advDest << endl;

    if (pendingDaoAcks.empty()) {
        EV_DETAIL << "No DAO_ACKs were expected!" << endl;
        return;
    }

    clearDaoAckTimer(advDest);

    EV_DETAIL << "Cancelled timeout event and erased entry in the pendingDaoAcks, remaining: " << endl;

    for (auto e : pendingDaoAcks)
        EV_DETAIL << e.first << " retries: " << std::to_string(e.second->numRetries)
            << ", msg ptr - " << e.second->timeoutPtr << endl;
}


void Rpl::setLineProperties(cLineFigure *connector, Coord target, cFigure::Color col, bool dashed) const
{
    connector->setStart(cFigure::Point(position.x, position.y));
    connector->setEnd(cFigure::Point(target.x, target.y));
    connector->setLineWidth(2);
    connector->setLineColor(col);
    connector->setLineOpacity(dashed ? 0.5 : 1);
    if (dashed)
        connector->setLineStyle(cFigure::LineStyle::LINE_DASHED);
    connector->setEndArrowhead(cFigure::ARROW_BARBED);
    connector->setVisible(true);
}


void Rpl::drawConnector(Coord target, cFigure::Color col, Ipv6Address backupParent) const {
    // (0, 0) corresponds to default Coord constructor, meaning no target position was provided
    if ((!target.x && !target.y) || !par("drawConnectors").boolValue())
        return;

    cCanvas *canvas = getParentModule()->getParentModule()->getCanvas();

    // If a valid pointer to backup parent address is provided
    // And there's no line drawn to it, create a new one
    if (backupParent != Ipv6Address::UNSPECIFIED_ADDRESS) {
        if (backupConnectors.find(backupParent) == backupConnectors.end()) {
            backupConnectors[backupParent] = new cLineFigure("backupParentConnector");
            setLineProperties(backupConnectors[backupParent], target, col, true);
            canvas->addFigure(backupConnectors[backupParent]);
        }
        return;
    }

    // If an arrow to preferred parent is already present, we only need to update its properties
    if (prefParentConnector) {
        prefParentConnector->setLineColor(col);
        prefParentConnector->setEnd(cFigure::Point(target.x, target.y));
        return;
    }

    // Otherwise create a new line and add it to canvas
    prefParentConnector = new cLineFigure("preferredParentConnector");
    setLineProperties(prefParentConnector, target, col);
    canvas->addFigure(prefParentConnector);
}

void Rpl::setParentMobility(Ptr<Dio> &prefParent) {
    if (!prefParent || !prefParent->isMobile())
        return;

    auto prefParentModule = findSubmodule(prefParent->getNodeName(), host->getParentModule());

    if (prefParentModule)
        parentMobilityMod = check_and_cast<IMobility*> (prefParentModule->getSubmodule("mobility"));
}

void Rpl::updatePreferredParent()
{
    Ptr<Dio> newPrefParent;
    EV_DETAIL << "Choosing preferred parent from "
            << boolStr(candidateParents.empty() && par("useBackupAsPreferred").boolValue(),
                    "backup", "candidate") << " parent set:" << endl;
    /**
     * Choose parent from candidate neighbour set. If it's empty, leave the DODAG.
     */
    if (candidateParents.empty())
    {
        EV_DETAIL << "Candidate parent list empty" << endl;
        if (par("useBackupAsPreferred").boolValue()) {
            EV_DETAIL << "Selecting preferred parent from backup parents:" << endl;
            newPrefParent = objectiveFunction->getPreferredParent(backupParents, preferredParent);
        }
        else {
            EV_DETAIL << "Leaving DODAG" << endl;
            detachFromDodag();
            return;
        }
    }
    else
        newPrefParent = objectiveFunction->getPreferredParent(candidateParents, preferredParent);

    if (!newPrefParent) {
        EV_WARN << "Objective function couldn't select preferred parent" << endl;
        return;
    }

    auto newPrefParentAddr = newPrefParent->getSrcAddress();
    /**
     * If a better preferred parent is discovered (based on the objective function metric),
     * update default route to DODAG sink with the new nextHop
     */
    if (checkPrefParentChanged(newPrefParentAddr)) {

        setParentMobility(newPrefParent);

        auto newPrefParentDodagId = newPrefParent->getDodagId();
        dagInfo.update(newPrefParent);

        /** Silently join new DODAG and update dest address for application, TODO: Check with RFC */
        dodagId = newPrefParentDodagId;

        // Avoid overwriting manually set dest addresses
        for (auto app : apps)
            if (!isUdpSink(app) && app->par("destAddresses").stdstringValue().empty())
                app->par("destAddresses") = newPrefParentDodagId.str();

        /** Notify 6TiSCH Scheduling Function (if present) about parent change */
        auto rplCtrlInfo = RplGenericControlInfo(newPrefParent->getNodeId());

        if (par("lowLatencyMode").boolValue())
            emit(uplinkSlotOffsetSignal, newPrefParent->getSlotOffset());

        emit(parentChangedSignal, 0, (cObject*) &rplCtrlInfo);
        daoSeqNum = 0;
        clearParentRoutes();
        clearAllDaoAckTimers();

        drawConnector(newPrefParent->getPosition(), newPrefParent->getColor());
        updateRoutingTable(newPrefParentAddr, dodagId, nullptr, true);

        // required for proper nextHop address resolution
        if (newPrefParentAddr != dodagId)
            updateRoutingTable(newPrefParentAddr, newPrefParentAddr, nullptr, false);

        lastTransit = Ipv6Address(newPrefParentAddr);
        EV_DETAIL << "Updated preferred parent to - " << newPrefParentAddr << endl;
        numParentUpdates++;
        /**
         * Reset trickle timer due to inconsistency (preferred parent changed) detected, thus
         * maintaining higher topology reactivity and convergence rate [RFC 6550, 8.3]
         */
        trickleTimer->reset();
        if (daoEnabled) {
            auto timeout = daoDelay * uniform(1, 7);

            if (storing)
                // TODO: magic numbers
                sendRplPacket(createDao(), DAO, newPrefParentAddr, timeout);
            else
                sendRplPacket(createDao(), DAO, newPrefParentAddr, timeout, getSelfAddress(), newPrefParentAddr);

            EV_DETAIL << "Sending DAO to new pref. parent - " << newPrefParentAddr
                    << " advertising " << getSelfAddress() << " reachability at " << simTime() + timeout << "s" << endl;
        }
    }
    preferredParent = dynamicPtrCast<Dio>(newPrefParent->dupShared());


    /** Recalculate rank based on the objective function */
    auto newRank = objectiveFunction->calcRank(preferredParent);
    if (newRank != rank) {
        rank = newRank;
        EV_DETAIL << "Updated rank - " << rank << endl;
        clearObsoleteBackupParents(backupParents);
        emit(rankUpdatedSignal, (long) rank);
    }
}

void Rpl::clearObsoleteBackupParents(map <Ipv6Address, Ptr<Dio> > &backupParents) {
    cCanvas *canvas = getParentModule()->getParentModule()->getCanvas();

    vector<Ipv6Address> parentsToDelete;

    for (auto bp : backupParents) {
        if (bp.second->getRank() > rank) {
            auto bkConnector = backupConnectors.find(bp.second->getSrcAddress());
            if (bkConnector != backupConnectors.end()) {
                canvas->removeFigure((*bkConnector).second);
                backupConnectors.erase(bkConnector);
            }
            parentsToDelete.push_back(bp.second->getSrcAddress());
        }
    }

    EV_DETAIL << "Erased obsolete (higher rank) backup parents:" << endl;
    for (auto addr: parentsToDelete) {
        auto it = backupParents.find(addr);
        if (it != backupParents.end()) {
            backupParents.erase(it);
        }
        EV_DETAIL << addr << ", ";
    }
    EV_DETAIL << endl;
}

bool Rpl::checkPrefParentChanged(const Ipv6Address &newPrefParentAddr)
{
    return !preferredParent || preferredParent->getSrcAddress() != newPrefParentAddr;
}

//
// Handling routing data
//

RplRouteData* Rpl::prepRouteData(const Dao *dao) {
    auto routeData = new RplRouteData();
    routeData->setDodagId(dao->getDodagId());
    routeData->setInstanceId(dao->getInstanceId());
    routeData->setDtsn(dao->getSeqNum());
    routeData->setExpirationTime(-1);
    return routeData;
}

void Rpl::updateRoutingTable(const Ipv6Address &nextHop, const Ipv6Address &dest, RplRouteData *routeData, bool defaultRoute)
{
    auto route = routingTable->createRoute();
    route->setSourceType(IRoute::MANET);
    route->setPrefixLength(isRoot ? 128 : prefixLength);
    route->setInterface(interfaceEntryPtr);
    route->setDestination(dest);
    route->setNextHop(nextHop);
    /**
     * If a route through preferred parent is being added
     * (i.e. not downward route, learned from DAO), set it as default route
     */
    if (defaultRoute) {
        routingTable->addDefaultRoute(nextHop, interfaceEntryPtr->getInterfaceId(), DEFAULT_PARENT_LIFETIME);
        EV_DETAIL << "Adding default route via " << nextHop << endl;
    }
    if (routeData)
        route->setProtocolData(routeData);

    if (!checkDuplicateRoute((Ipv6Route*) route)) {
        routingTable->addRoute(route);
        EV_DETAIL << "New destination learned - " << dest << " reachable via " << nextHop << endl;
    }
    else
        delete route;
}

bool Rpl::checkDuplicateRoute(Ipv6Route *route) {
    for (auto i = 0; i < routingTable->getNumRoutes(); i++) {
        auto rt = routingTable->getRoute(i);
        auto rtdest = rt->getDestinationAsGeneric().toIpv6();
        auto dest = route->getDestinationAsGeneric().toIpv6();
        if (dest == rtdest) {
            if (rt->getNextHop() != route->getNextHop()) {
                rt->setNextHop(route->getNextHop());
                EV_DETAIL << "Duplicate route, updated next hop to " << rt->getNextHop() << " for dest " << dest << endl;
                if (route->getProtocolData()) {
                    cObject *protocolData = rt->getProtocolData();
                    rt->setProtocolData(route->getProtocolData());
                    if (protocolData)
                        delete protocolData;
                }
            }
            return true;
        }
    }
    return false;
}

void Rpl::appendRplPacketInfo(Packet *datagram) {
    auto rpi = makeShared<RplPacketInfo>();
    rpi->setChunkLength(B(4));
    rpi->setDown(isRoot || isDownlinkPacket(datagram));
    rpi->setRankError(false);
    rpi->setFwdError(false);
    rpi->setInstanceId(instanceId);
    rpi->setSenderRank(rank);
    datagram->insertAtBack(rpi);
    EV_INFO << "Appended RPL Packet Information: \n" << printHeader(rpi.get())
            << "\n to UDP datagram: " << datagram << endl;
}

bool Rpl::isDownlinkPacket(Packet *datagram) {
    auto networkProtocolHeader = findNetworkProtocolHeader(datagram);
    auto dest = networkProtocolHeader->getDestinationAddress().toIpv6();
    EV_DETAIL << "Determining packet forwarding direction:\n destination - " << dest << endl;
    auto ri = routingTable->doLongestPrefixMatch(dest);
    if (ri == nullptr) {
        auto errorMsg = std::string("Error while determining packet forwarding direction"
                + std::string(", couldn't find route to ") + dest.str());
        throw cRuntimeError("%s",errorMsg.c_str());
    }

    EV_DETAIL << " next hop - " << ri->getNextHop() << endl;
    bool res = isSourceRouted(datagram)
            || !(ri->getNextHop().matches(preferredParent->getSrcAddress(), prefixLength));
    EV_DETAIL << " Packet travels " << boolStr(res, "downwards", "upwards");
    return res;
}


bool Rpl::isSourceRouted(Packet *pkt) {
    EV_DETAIL << "Checking if packet is source-routed" << endl;;
    try {
        auto srh = pkt->popAtBack<SourceRoutingHeader>(B(64));
        EV_DETAIL << "Retrieved source-routing header - " << srh << endl;
        return true;
    }
    catch (std::exception &e) { }

    return false;
}


// TODO: update to realistic value or calculate dynamically?
B Rpl::getDaoLength() {
    return B(8);
}

void Rpl::appendDaoTransitOptions(Packet *pkt) {
    appendDaoTransitOptions(pkt, getSelfAddress(), preferredParent->getSrcAddress());
}

void Rpl::appendDaoTransitOptions(Packet *pkt, const Ipv6Address &target, const Ipv6Address &transit) {
    EV_DETAIL << "Appending target, transit options to DAO: " << pkt << endl;
    auto rplTarget = makeShared<RplTargetInfo>();
    auto rplTransit = makeShared<RplTransitInfo>();
    rplTarget->setChunkLength(getTransitOptionsLength());
    rplTransit->setChunkLength(getTransitOptionsLength());
    rplTarget->setTarget(target);
    rplTransit->setTransit(transit);
    pkt->insertAtBack(rplTarget);
    pkt->insertAtBack(rplTransit);
    EV_DETAIL << "transit => target headers appended: "
            << rplTransit->getTransit() << " => " << rplTarget->getTarget() << endl;
}


bool Rpl::checkRplRouteInfo(Packet *datagram) {
    auto dest = findNetworkProtocolHeader(datagram)->getDestinationAddress().toIpv6();
    if (dest.matches(getSelfAddress(), prefixLength)) {
        EV_DETAIL << "Packet reached its destination, no RPI header checking needed" << endl;
        return true;
    }
    EV_DETAIL << "Checking RPI header for packet " << datagram
            << " \n coming from "
            << findNetworkProtocolHeader(datagram)->getSourceAddress().toIpv6()
            << endl;
    RplPacketInfo *rpi;
    try {
        rpi = const_cast<RplPacketInfo *> (datagram->popAtBack<RplPacketInfo>(B(4)).get());
    }
    catch (std::exception &e) {
       EV_WARN << "No RPL Packet Information present in UDP datagram, appending" << endl;
       try {
           appendRplPacketInfo(datagram);
           return true;
       }
       catch (...) {
           return false;
       }
    }
    if (isRoot)
        return true;
    // check for rank inconsistencies
    EV_DETAIL << "Rank error before checking - " << rpi->getRankError() << endl;
    bool rankInconsistency = checkRankError(rpi);
    if (rpi->getRankError() && rankInconsistency) {
        EV_WARN << "Repeated rank error detected for packet "
                << datagram << "\n dropping..." << endl;
        return false;
    }
    if (rankInconsistency)
        rpi->setRankError(rankInconsistency);
    EV_DETAIL << "Rank error after check - " << rpi->getRankError() << endl;
    /**
     * If there's a forwarding error, packet should be returned to parent
     * with 'F' flag set to clear outdated DAO routes if DAO inconsistency detection
     * is enabled [RFC 6550, 11.2.2.3]
     */
    rpi->setFwdError(!isRoot && checkForwardingError(rpi, dest));
    if (rpi->getFwdError()) {
        EV_WARN << "Forwarding error detected for packet " << datagram
                << "\n destined to " << dest << ", dropping" << endl;
        return false;
    }
    // update packet forwarding direction if storing mode is in use
    // e.g. if unicast P2P packet reaches sub-dodag root with 'O' flag cleared,
    // and this root can route packet downwards to destination, 'O' flag has to be set.
    if (storing)
        rpi->setDown(isRoot || isDownlinkPacket(datagram));
    // try make new shared ptr chunk RPI, to be refactored into reusing existing RPI object
    auto rpiCopy = makeShared<RplPacketInfo>();
    rpiCopy->setChunkLength(getRpiHeaderLength());
    rpiCopy->setDown(rpi->getDown());
    rpiCopy->setRankError(rpi->getRankError());
    rpiCopy->setFwdError(rpi->getFwdError());
    rpiCopy->setInstanceId(instanceId);
    rpiCopy->setSenderRank(rank);
    datagram->insertAtBack(rpiCopy);
    return true;
}

B Rpl::getTransitOptionsLength() {
    return B(16);
}

B Rpl::getRpiHeaderLength() {
    return B(4);
}


void Rpl::extractSourceRoutingData(Packet *pkt) {
    try {
        lastTransit = pkt->popAtBack<RplTransitInfo>(getTransitOptionsLength()).get()->getTransit();
        lastTarget = pkt->popAtBack<RplTargetInfo>(getTransitOptionsLength()).get()->getTarget();
        if (!isRoot)
            return;

        sourceRoutingTable.insert( std::pair<Ipv6Address, Ipv6Address>(lastTarget, lastTransit) );
        EV_DETAIL << "Source routing table updated with new:\n"
                << "target: " << lastTarget << "\n transit: " << lastTransit << "\n"
                << printMap(sourceRoutingTable) << endl;
    }
    catch (std::exception &e) {
        EV_WARN << "Couldn't pop RPL Target, Transit Information options from packet: "
                << pkt << endl;
    }
}

INetfilter::IHook::Result Rpl::checkRplHeaders(Packet *datagram) {
    // skip further checks if node doesn't belong to a DODAG
    auto datagramName = std::string(datagram->getFullName());
    EV_INFO << "packet fullname " << datagramName << endl;
    if (!isRoot && (preferredParent == nullptr || dodagId == Ipv6Address::UNSPECIFIED_ADDRESS))
    {
        EV_DETAIL << "Node is detached from a DODAG, " <<
                " no forwarding/rank error checks will be performed" << endl;
        return ACCEPT;
    }

    if (isUdp(datagram)) {
        // in non-storing MOP source routing header is needed for downwards traffic
        if (!storing) {
            // generate one if the sender is root
            if (isRoot) {
                if (selfGeneratedPkt(datagram)) {
                    appendSrcRoutingHeader(datagram);
                    return ACCEPT;
                }
                else
                    EV_DETAIL << "P2P source-routing not yet supported" << endl;
            }
            // or forward packet further using the routing header
            else {
                if (!destIsRoot(datagram))
                    forwardSourceRoutedPacket(datagram);
                return ACCEPT;
            }
        }
    }

    return ACCEPT;
}


bool Rpl::destIsRoot(Packet *datagram) {
    return findNetworkProtocolHeader(datagram).get()->getDestinationAddress().toIpv6().matches(dodagId, prefixLength);
}

void Rpl::saveDaoTransitOptions(Packet *dao) {
    try {
        lastTransit = dao->popAtBack<RplTransitInfo>(getTransitOptionsLength()).get()->getTransit();
        lastTarget = dao->popAtBack<RplTargetInfo>(getTransitOptionsLength()).get()->getTarget();
        EV_DETAIL << "Updated lastTransit => lastTarget to: " << lastTransit << " => " << lastTarget << endl;
    }
    catch (std::exception &e) {
        EV_DETAIL << "No Target, Transit headers found on packet:\n " << *dao << endl;
        return;
    }
}

void Rpl::constructSrcRoutingHeader(std::deque<Ipv6Address> &addressList, Ipv6Address dest)
{
    Ipv6Address nextHop = sourceRoutingTable[dest];
    addressList.push_front(dest);
    addressList.push_front(nextHop);
    EV_DETAIL << "Constructing routing header for dest - " << dest
            << "\n next hop: " << nextHop << endl;

    // Create sequence of 'next hop' addresses recursively using the source routing table learned from DAOs
    while (sourceRoutingTable.find(nextHop) != sourceRoutingTable.end())
    {
        nextHop = sourceRoutingTable[nextHop];
        addressList.push_front(nextHop);
    }
    // pop redundant link-local next hop, which is already known from the routing table
    addressList.pop_front();
}


void Rpl::appendSrcRoutingHeader(Packet *datagram) {
    Ipv6Address dest = findNetworkProtocolHeader(datagram)->getDestinationAddress().toIpv6();
    std::deque<Ipv6Address> srhAddresses;
    EV_DETAIL << "Appending routing header to datagram " << datagram << endl;
    if ( sourceRoutingTable.find(dest) == sourceRoutingTable.end() ) {
        EV_WARN << "Required destination " << dest << " not yet present in source-routing table: \n"
                << printMap(sourceRoutingTable) << endl;
        return;
    }

    constructSrcRoutingHeader(srhAddresses, dest);

    EV_DETAIL << "Source routing header constructed : " << srhAddresses << endl;

    auto srh = makeShared<SourceRoutingHeader>();
    srh->setAddresses(srhAddresses);
    srh->setChunkLength(getSrhSize());
    datagram->insertAtBack(srh);
}

void Rpl::forwardSourceRoutedPacket(Packet *datagram) {
    EV_DETAIL << "processing source-routed datagram - " << datagram
            << "\n with routing header: " << endl;
    auto srh = const_cast<SourceRoutingHeader *> (datagram->popAtBack<SourceRoutingHeader>(getSrhSize()).get());
    auto srhAddresses = srh->getAddresses();

    if (srhAddresses.back() == getSelfAddress()) {
        EV_DETAIL << "Source-routed destination reached" << endl;
        return;
    }

    for (auto addr : srhAddresses)
        EV_DETAIL << addr << " => ";

    Ipv6Address nextHop;
    for (auto it = srhAddresses.cbegin(); it != srhAddresses.cend(); it++) {
        if (it->matches(getSelfAddress(), prefixLength)) {
            nextHop = *(++it);
            break;
        }
    }
    EV_DETAIL << "\n Forwarding source-routed datagram to " << nextHop << endl;

    updateRoutingTable(nextHop, nextHop, nullptr, false);

    srhAddresses.pop_front();
//
    // re-insert updated routing header
    auto updatedSrh =  makeShared<SourceRoutingHeader>();
    updatedSrh->setChunkLength(getSrhSize());
    updatedSrh->setAddresses(srhAddresses);
    datagram->insertAtBack(updatedSrh);
    (const_cast<NetworkHeaderBase *>(findNetworkProtocolHeader(datagram).get()))->setDestinationAddress(nextHop);
}

bool Rpl::selfGeneratedPkt(Packet *pkt) {
    bool res;
    if (isRoot) {
        auto dest = findNetworkProtocolHeader(pkt)->getDestinationAddress().toIpv6();
        res = !(dest.matches(getSelfAddress(), prefixLength));
        EV_DETAIL << "Checking if packet is self generated by dest: " << dest << endl;
    }
    else {
        auto srcAddr = findNetworkProtocolHeader(pkt)->getSourceAddress().toIpv6();
        res = srcAddr.matches(getSelfAddress(), prefixLength);
        EV_DETAIL << "Checking if packet is self generated by source address: "
                << srcAddr << "; is self-generated: " << boolStr(res) << endl;
    }
    return res;
}

std::string Rpl::rplIcmpCodeToStr(RplPacketCode code) {
    switch (code) {
        case 0:
            return std::string("DIS");
        case 1:
            return std::string("DIO");
        case 2:
            return std::string("DAO");
        case 3:
            return std::string("DAO_ACK");
        default:
            return std::string("Unknown");
    }
}


bool Rpl::checkRankError(RplPacketInfo *rpi) {
    auto senderRank = rpi->getSenderRank();
    EV_DETAIL << "Checking rank consistency: "
            << "\n direction - " << boolStr(rpi->getDown(), "down", "up")
            << "\n senderRank - " << senderRank << "; own rank - " << rank << endl;
    bool res = (!(rpi->getDown()) && (senderRank <= rank))
                    || (rpi->getDown() && (senderRank >= rank));
    EV_DETAIL << "Rank consistency check " << boolStr(res, "failed", "passed") << endl;
    return res;
}

bool Rpl::checkForwardingError(RplPacketInfo *rpi, Ipv6Address &dest) {
    EV_DETAIL << "Checking forwarding error: \n MOP - "
            << boolStr(storing, "storing", "non-storing")
            << "\n dest - " << dest
            << "\n direction - " << boolStr(rpi->getDown(), "down", "up") << endl;
    auto route = routingTable->doLongestPrefixMatch(dest);
    auto parentAddr = preferredParent != nullptr ? preferredParent->getSrcAddress() : Ipv6Address::UNSPECIFIED_ADDRESS;
    bool res = storing && rpi->getDown()
                    && (route == nullptr || route->getNextHop().matches(parentAddr, prefixLength));
    EV_DETAIL << "Forwarding " << boolStr(res, "error detected", "OK") << endl;
    return res;
}

void Rpl::printTags(Packet *packet) {
    EV_DETAIL << "Packet " << packet->str() << "\n Tags: " << endl;
    for (int i = 0; i < packet->getNumTags(); i++)
        EV_DETAIL << "classname: " << packet->getTag(i)->getClassName() << endl;
}

std::string Rpl::printHeader(RplPacketInfo *rpi) {
    std::ostringstream out;
    out << " direction: " << boolStr(rpi->getDown(), "down", "up")
        << "\n senderRank:  " << rpi->getSenderRank()
        << "\n rankError: " << boolStr(rpi->getRankError())
        << "\n forwardingError: " << boolStr(rpi->getFwdError());
    return out.str();
}


void Rpl::deletePrefParent(bool poisoned)
{
    if (!preferredParent) {
        EV_WARN << "No preferred parent to delete" << endl;
        return;
    }

    auto prefParentAddr = preferredParent->getSrcAddress();
    EV_DETAIL << "Preferred parent " << prefParentAddr
            << boolStr(poisoned, " detachment from DODAG", " unreachability") << " detected" << endl;
    emit(parentUnreachableSignal, preferredParent.get());
    clearParentRoutes();

    candidateParents.erase(prefParentAddr);
    preferredParent = nullptr;
    EV_DETAIL << "Erased preferred parent from candidate parent set" << endl;
}

void Rpl::clearParentRoutes() {
    if (!preferredParent) {
        EV_WARN << "Pref. parent not set, cannot delete associated routes from routing table " << endl;
        return;
    }

    // Delete routes with prefix length 0 (default routes)
    if (!interfaceEntryPtr) {
        EV_WARN << "No interface entry found, can't clear routes associated with preferred parent" << endl;
        return;
    }

    if (!routingTable) {
        EV_WARN << "No routing table module found, can't clear routes associated with preferred parent" << endl;
        return;
    }

    deleteDefaultRoutes(interfaceEntryPtr->getInterfaceId());

    std::vector<Ipv6Route*> routesToDelete;
    auto totalRoutes = routingTable->getNumRoutes();

    // Collect any remaining routes with preferred parent as the next hop
    for (int i = 0; i < totalRoutes; i++) {
        auto ri = routingTable->getRoute(i);
        if (ri->getNextHop() == preferredParent->getSrcAddress())
            routesToDelete.push_back(ri);
    }

    // And delete them too
    for (auto rt : routesToDelete)
        routingTable->deleteRoute(rt);

    EV_DETAIL << "Deleted " << routesToDelete.size() << " routes through preferred parent " << endl;
    routingTable->purgeDestCache();
}

void Rpl::deleteDefaultRoutes(int interfaceID) {
    if (interfaceID < 0) {
        EV_ERROR << "Invalid interface id provided" << endl;
        return;
    }

    if (!routingTable) {
         EV_WARN << "No routing table info found, cannot clear default routes" << endl;
         return;
    }

    EV_INFO << "/// Removing default routes for interface=" << interfaceID << endl;

    auto numRts = routingTable->getNumRoutes();
    std::vector<Ipv6Route*> rtsForDeletion;

    for (auto i = 0; i < numRts; i++) {
        auto rt = routingTable->getRoute(i);
        // default routes have prefix length 0
        if (rt->getInterface()
                && rt->getInterface()->getInterfaceId() == interfaceID
                && rt->getPrefixLength() == 0)
        {
            rtsForDeletion.push_back(rt);
        }
    }

    for (auto rt: rtsForDeletion)
        routingTable->deleteRoute(rt);
}

//
// Utility methods
//

std::string Rpl::boolStr(bool cond, std::string positive, std::string negative) {
    return cond ? positive : negative;
}

template<typename Map>
std::string Rpl::printMap(const Map& map) {
    std::ostringstream out;
    for (const auto& p : map)
        out << p.first << " => " << p.second << endl;
    return out.str();
}

Ipv6Address Rpl::getSelfAddress() {
    return interfaceEntryPtr->getNetworkAddress().toIpv6();
}

bool Rpl::checkDestKnown(const Ipv6Address &nextHop, const Ipv6Address &dest) {
    Ipv6Route *outdatedRoute = nullptr;
    for (int i = 0; i < routingTable->getNumRoutes(); i++) {
        auto ri = routingTable->getRoute(i);
        if (ri->getDestPrefix() == dest) {
            EV_DETAIL << "Destination " << ri->getDestPrefix() << " already known, ";
            if (ri->getNextHop() == nextHop) {
                EV_DETAIL << "reachable via " << ri->getNextHop() << endl;
                return true;
            }
            else {
                EV_DETAIL << "but next hop has changed to "
                        << nextHop << ", routing table to be updated" << endl;
                outdatedRoute = ri;
                break;
            }
        }
    }
    try {
        if (outdatedRoute)
            EV_DETAIL << "Deleting outdated route to dest " << dest
                    << " via " << outdatedRoute->getNextHop()
                    << " : \n "
                    << boolStr(routingTable->deleteRoute(outdatedRoute), "Success", "Fail");
    }
    catch (std::exception &e) {
        EV_WARN << "Exception while deleting outdated route " << e.what() << endl;
    }

    return false;
}

bool Rpl::checkUnknownDio(const Ptr<const Dio>& dio)
{
    return dio->getDodagId() != dodagId || dio->getInstanceId() != instanceId;
}

void Rpl::addNeighbour(const Ptr<const Dio>& dio)
{
    /**
     * Maintain following sets of link-local nodes:
     *  - backup parents
     *  - candidate parents
     * where preferred parent is chosen from the candidate parent set [RFC6560, 8.2.1]
     *
     * In current implementation, neighbor data is represented by most recent
     * DIO packet received from it.
     */
    auto dioCopy = dynamicPtrCast<Dio>(dio->dupShared());
    auto dioSender = dio->getSrcAddress();
    /** If DIO sender has an equal rank, consider it a backup parent */
    if (dio->getRank() == rank) {
        auto it = backupParents.find(dioSender);
        if (it != backupParents.end())
            EV_DETAIL << "Backup parent entry updated - " << dioSender;
        else
            EV_DETAIL << "New backup parent added - " << dioSender;
        backupParents[dioSender] = dioCopy;
    }
    /** If DIO sender has a lower rank, consider it a candidate parent */
    if (dio->getRank() < rank) {
        auto it = candidateParents.find(dioSender);
        if (it != candidateParents.end())
            EV_DETAIL << "Candidate parent entry updated - " << dioSender;
        else
            EV_DETAIL << "New candidate parent added - " << dioSender;
        candidateParents[dioSender] = dioCopy;
    }

    EV_DETAIL << " (rank " << dio->getRank() << ")" << endl;

    // Highlight backup parents with a dashed line
    if (pShowBackupParents)
        drawConnector(dioSender, dio->getPosition(),  dio->getColor());
}

void Rpl::drawConnector(Ipv6Address neighborAddr, Coord pos, cFigure::Color col) {
    cCanvas *canvas = getParentModule()->getParentModule()->getCanvas();
    EV_DETAIL << "Canvas - " << canvas << endl;
    if (backupConnectors.find(neighborAddr) == backupConnectors.end()) {
        EV_DETAIL << "No connector yet found for this neighbor" << endl;
        auto connector = new cLineFigure("backupParentConnector");
        setLineProperties(connector, pos, col, true);
        canvas->addFigure(connector);
        EV_DETAIL << "Connector added - " << connector << endl;
        backupConnectors[neighborAddr] = connector;
    }
}

//
// Notification handling
//

void Rpl::receiveSignal(cComponent *source, simsignal_t signalID, cObject *obj, cObject *details)
{

//    Enter_Method("receiveChangeNotification");
//
    Enter_Method_Silent();

    EV_DETAIL << "Processing signal - " << signalID << endl;

    if (signalID == packetReceivedSignal)
        udpPacketsRecv++;

    /**
     * Upon receiving broken link signal from MAC layer, check whether
     * preferred parent is unreachable
     */
    if (signalID == linkBrokenSignal) {
        EV_WARN << "Received link break" << endl;
        Packet *datagram = check_and_cast<Packet *>(obj);
        EV_DETAIL << "Packet " << datagram->str() << " lost?" << endl;
        const auto& networkHeader = findNetworkProtocolHeader(datagram);
        if (networkHeader != nullptr) {
            const Ipv6Address& destination = networkHeader->getDestinationAddress().toIpv6();
            auto nextHop = routingTable->getNextHopForDestination(destination);
            EV_DETAIL << "Connection with destination " << destination << " reachable via "
                    << nextHop << " broken?" << endl;
            /**
             * If preferred parent unreachability detected, remove route with it as a
             * next hop from the routing table and select new preferred parent from the
             * candidate set.
             *
             * If candidate parent set is empty, leave current DODAG
             * (becoming either floating DODAG or poison child routes altogether)
             */
            if (preferredParent && nextHop == preferredParent->getSrcAddress()
                    && pUnreachabilityDetectionEnabled)
            {
                deletePrefParent();
                // TODO: Redirect all packets already enqueued for previous pref. parent to the new one
                // or flush the queue completely
                updatePreferredParent();
            }
        }
    }
}
#if LOWLATENCYSUPPORT
// Low-latency mode signals
void Rpl::receiveSignal(cComponent *src, simsignal_t id, long value, cObject *details) {
    std::string signalName = getSignalName(id);

    EV_DETAIL << "RPL processing signal - " << signalName << endl;

    if (std::strcmp(signalName.c_str(), "uplinkScheduled") == 0) {
        EV_DETAIL << "Received notification about uplink cell scheduled at " << value << " slot offset" << endl;
        uplinkSlotOffset = value;
    }
}
#endif
//void Rpl::receiveSignal(cComponent *source, simsignal_t signalID, double value, cObject *details) {
//    std::string signalName = getSignalName(signalID);
//
//    if (std::strcmp(signalName.c_str(), "currentFrequency") == 0) {
////        EV_DETAIL << "RPL received current frequnecy - " << value << endl;
//        currentFrequency = value;
//    }
//
//}

// Getting current freq from MAC

}
}
} // namespace inet



