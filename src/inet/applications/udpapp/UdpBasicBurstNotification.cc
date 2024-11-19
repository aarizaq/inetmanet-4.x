//
// Copyright (C) 2000 Institut fuer Telematik, Universitaet Karlsruhe
// Copyright (C) 2007 Universidad de Malaga
// Copyright (C) 2011 Zoltan Bojthe
// Copyright (C) 2011 Alfonso Ariza, Universidad de Malaga
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//


#include "inet/applications/base/ApplicationPacket_m.h"
#include "inet/applications/udpapp/UdpBasicBurstNotification.h"
#include "inet/common/ModuleAccess.h"
#include "inet/common/TimeTag_m.h"
#include "inet/common/packet/Packet.h"
#include "inet/networklayer/common/L3AddressResolver.h"
#include "inet/transportlayer/contract/udp/UdpCommand_m.h"
#include "inet/common/Simsignals.h"

namespace inet {

Define_Module(UdpBasicBurstNotification);

UdpBasicBurstNotification::UdpBasicBurstNotification()
{
    addressModule = nullptr;
}

UdpBasicBurstNotification::~UdpBasicBurstNotification()
{
    if (addressModule)
        delete addressModule;
}

void UdpBasicBurstNotification::processStart()
{
    if (!hasPar("registerInInit") || (hasPar("registerInInit") && !par("registerInInit").boolValue())) {
        socket.setOutputGate(gate("socketOut"));
        socket.setCallback(this);
        socket.bind(localPort);
    }

    IInterfaceTable *ift = getModuleFromPar<IInterfaceTable>(par("interfaceTableModule"), this);

    if (par("setBroadcast").boolValue())
        socket.setBroadcast(true);

    if (strcmp(par("outputInterface").stringValue(),"") != 0) {
        NetworkInterface *ie = ift->findInterfaceByName(par("outputInterface").stringValue());
        if (ie == nullptr)
            throw cRuntimeError(this, "Invalid output interface name : %s",par("outputInterface").stringValue());
        outputInterface = ie->getInterfaceId();
    }

    outputInterfaceMulticastBroadcast.clear();
    if (strcmp(par("outputInterfaceMulticastBroadcast").stringValue(),"") != 0) {
        const char *ports = par("outputInterfaceMulticastBroadcast");
        cStringTokenizer tokenizer(ports);
        const char *token;
        while ((token = tokenizer.nextToken()) != nullptr) {
            if (strstr(token, "ALL") != nullptr) {
                for (int i = 0; i < ift->getNumInterfaces(); i++) {
                    NetworkInterface *ie = ift->getInterface(i);
                    if (ie->isLoopback())
                        continue;
                    if (ie == nullptr)
                        throw cRuntimeError(this, "Invalid output interface name : %s", token);
                    outputInterfaceMulticastBroadcast.push_back(ie->getInterfaceId());
                }
            }
            else {
                NetworkInterface *ie = ift->findInterfaceByName(token);
                if (ie == nullptr)
                    throw cRuntimeError(this, "Invalid output interface name : %s", token);
                outputInterfaceMulticastBroadcast.push_back(ie->getInterfaceId());
            }
        }
    }

    addressModule = new AddressModule();
    //addressModule->initModule(par("chooseNewIfDeleted").boolValue());
    addressModule->initModule(true);
    addressModule->setDestAddrRNG(destAddrRNG);

    nextSleep = simTime();
    nextBurst = simTime();
    nextPkt = simTime();
    activeBurst = false;

    std::string destAddresses = par("destAddresses").stdstringValue();
    if (strcmp(destAddresses.c_str(),"") != 0)
        isSource = true;

    if (isSource) {
        if (chooseDestAddrMode == ONCE) {
            destAddr = chooseDestAddr();
            if (destAddr.isUnspecified())
                isSource = false;
        }
        if (isSource)
            activeBurst = true;
    }
    timerNext->setKind(SEND);
    processSend();
    // subscribe changes in the configuration
    /*
    cModule *host = getContainingNode(this);
    host->subscribe(interfaceIpv4ConfigChangedSignal, this);
    host->subscribe(interfaceIpv6ConfigChangedSignal, this);
    */
}


L3Address UdpBasicBurstNotification::chooseDestAddr()
{
    if (addressModule->isInit())
        return addressModule->choseNewAddress();
    else {
        addressModule->initModule(true);
        if (addressModule->isInit())
            return addressModule->choseNewAddress();
        return L3Address();
    }
}

void UdpBasicBurstNotification::receiveSignal(cComponent *source, simsignal_t signalID, intval_t l, cObject *details)
{
    Enter_Method_Silent();
    if (signalID == interfaceIpv4ConfigChangedSignal || signalID == interfaceIpv6ConfigChangedSignal)
        addressModule->rebuildAddressList();

}

}

