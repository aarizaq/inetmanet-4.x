

#ifndef __INET_DSDV_H
#define __INET_DSDV_H

#include <list>
#include <map>
#include <stdio.h>
#include <string.h>
#include <vector>
#include <algorithm>

#include "inet/wirelesspan/routing/leach/LeachPkts_m.h"
#include "inet/common/INETDefs.h"
#include "inet/common/packet/Packet.h"
#include "inet/networklayer/contract/IInterfaceTable.h"
#include "inet/networklayer/contract/ipv4/Ipv4Address.h"
#include "inet/networklayer/ipv4/IIpv4RoutingTable.h"
#include "inet/networklayer/ipv4/Ipv4Header_m.h"
#include "inet/networklayer/ipv4/Ipv4InterfaceData.h"
#include "inet/networklayer/ipv4/Ipv4RoutingTable.h"
#include "inet/routing/base/RoutingProtocolBase.h"
using namespace std;


namespace inet {
namespace wirelesspan {
namespace routing {
/**
 * DSDV protocol implementation.
 */
class INET_API Leach : public RoutingProtocolBase
{

  private:
    struct ForwardEntry
    {
        cMessage *event = nullptr;
        Packet *hello = nullptr;

        ForwardEntry() {}
        ~ForwardEntry();
    };

    bool isForwardHello = false;
    cMessage *event = nullptr;
    cPar *broadcastDelay = nullptr;
    //std::list<ForwardEntry *> *forwardList  = nullptr;
    NetworkInterface *interface80211ptr = nullptr;
    int interfaceId = -1;
    unsigned int sequencenumber = 0;
    simtime_t routeLifetime;
    cModule *host = nullptr;

    Ipv4Address idealCH;

    // simsignal_t controlPktSendSignal;
    // simsignal_t dataPktSendSignal;
    // simsignal_t bsPktSendSignal;
    // simsignal_t controlPktReceiveSignal;
    // simsignal_t dataPktReceiveSignal;
    //simsignal_t subIntervalTot;

    int dataPktSent;
    int dataPktReceived;
    int controlPktSent;
    int controlPktReceived;
    int bsPktSent;
    int round;

  protected:
    simtime_t helloInterval;
    IInterfaceTable *ift = nullptr;
    IIpv4RoutingTable *rt = nullptr;

    simtime_t dataPktSendDelay;
    simtime_t CHPktSendDelay;
    simtime_t roundDuration;

    int numNodes = 0;
    double clusterHeadPercentage = 0.0;


  public:
    Leach();
    ~Leach();
    enum LeachState { nch, ch }; // written in lower caps as it conflicts with enum in LeachPkts.msg file

  protected:
    virtual int numInitStages() const override { return NUM_INIT_STAGES; }
    virtual void initialize(int stage) override;
    virtual void handleMessageWhenUp(cMessage *msg) override;

    void handleSelfMessage(cMessage *msg);

    // lifecycle
    virtual void handleStartOperation(LifecycleOperation *operation) override { start(); }
    virtual void handleStopOperation(LifecycleOperation *operation) override { stop(); }
    virtual void handleCrashOperation(LifecycleOperation *operation) override  { stop(); }
    void start();
    void stop();
    virtual void refreshDisplay() const override;
    virtual void finish() override;

    double generateThresholdValue(int subInterval);
    void sendData2CH(Ipv4Address destAddr, Ipv4Address nodeAddr);
    void sendData2BS(Ipv4Address CHAddr);
    void addToNodeList(Ipv4Address nodeAddr, Ipv4Address CHAddr, double energy);
    virtual void setLeachState(LeachState ls);
    bool isNodeCH(Ipv4Address nodeAddr);
    Ipv4Address getIdealCH(Ipv4Address nodeAddr);

    LeachState leachState = nch;
};

}
}
} // namespace inet

#endif // ifndef __INET_DSDV_H

