//
// Copyright (C) 2011 Andras Varga
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

#include "inet/applications/udpapp/UdpForwardApp.h"

#include "inet/common/ModuleAccess.h"
#include "inet/networklayer/common/L3AddressTag_m.h"
#include "inet/transportlayer/common/L4PortTag_m.h"
#include "inet/transportlayer/contract/udp/UdpControlInfo_m.h"

namespace inet {

Define_Module(UdpForwardApp);

void UdpForwardApp::initialize(int stage)
{
    ApplicationBase::initialize(stage);

    if (stage == INITSTAGE_LOCAL) {
        // init statistics
        numForwarded = 0;
        WATCH(numForwarded);

        destAddress = L3Address(par("destAddress").stringValue());
        destPort = par("destPort").longValue();
    }
}

void UdpForwardApp::handleMessageWhenUp(cMessage *msg)
{
    socket.processMessage(msg);
}

void UdpForwardApp::socketDataArrived(UdpSocket *socket, Packet *pk)
{
    pk->clearTags();
    pk->trim();

    // statistics
    numForwarded++;
    emit(packetSentSignal, pk);
    // send back
    socket->sendTo(pk, destAddress, destPort);
}

void UdpForwardApp::socketErrorArrived(UdpSocket *socket, Indication *indication)
{
    EV_WARN << "Ignoring UDP error report " << indication->getName() << endl;
    delete indication;
}

void UdpForwardApp::refreshDisplay() const
{
    char buf[40];
    sprintf(buf, "forwarded: %d pks", numForwarded);
    getDisplayString().setTagArg("t", 0, buf);
}

void UdpForwardApp::finish()
{
    ApplicationBase::finish();
}

bool UdpForwardApp::handleNodeStart(IDoneCallback *doneCallback)
{
    socket.setOutputGate(gate("socketOut"));
    int localPort = par("localPort");
    socket.bind(localPort);
    MulticastGroupList mgl = getModuleFromPar<IInterfaceTable>(par("interfaceTableModule"), this)->collectMulticastGroups();
    socket.joinLocalMulticastGroups(mgl);
    socket.setCallback(this);
    return true;
}

bool UdpForwardApp::handleNodeShutdown(IDoneCallback *doneCallback)
{
    //TODO if(socket.isOpened()) socket.close();
    socket.setCallback(nullptr);
    return true;
}

void UdpForwardApp::handleNodeCrash()
{
}

} // namespace inet

