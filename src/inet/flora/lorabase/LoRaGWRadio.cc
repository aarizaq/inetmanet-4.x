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

#include "inet/flora/lorabase/LoRaGWRadio.h"
#include "inet/common/stlutils.h"
#include "inet/flora/lorabase/LoRaTagInfo_m.h"
#include "inet/flora/loraphy/LoRaMedium.h"
#include "inet/flora/loraphy/LoRaPhyPreamble_m.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/SignalTag_m.h"

namespace inet {

namespace flora {

Define_Module(LoRaGWRadio);

void LoRaGWRadio::initialize(int stage)
{
    NarrowbandRadioBase::initialize(stage);
    iAmGateway = par("iAmGateway").boolValue();
    if (stage == INITSTAGE_LAST) {
        setRadioMode(RADIO_MODE_TRANSCEIVER);
        LoRaGWRadioReceptionStarted = registerSignal("LoRaGWRadioReceptionStarted");
        LoRaGWRadioReceptionFinishedCorrect = registerSignal("LoRaGWRadioReceptionFinishedCorrect");
        LoRaGWRadioReceptionStarted_counter = 0;
        LoRaGWRadioReceptionFinishedCorrect_counter = 0;
        iAmTransmiting = false;
    }
}

void LoRaGWRadio::finish()
{
    NarrowbandRadioBase::finish();
    recordScalar("DER - Data Extraction Rate", double(LoRaGWRadioReceptionFinishedCorrect_counter)/LoRaGWRadioReceptionStarted_counter);
}

void LoRaGWRadio::handleSelfMessage(cMessage *message)
{
    if (message == switchTimer)
        handleSwitchTimer(message);
    else if (isTransmissionTimer(message))
        handleTransmissionTimer(message);
    else if (isReceptionTimer(message))
        handleReceptionTimer(message);
    else
        throw cRuntimeError("Unknown self message");
}


bool LoRaGWRadio::isTransmissionTimer(const cMessage *message) const
{
    return !strcmp(message->getName(), "transmissionTimer");
}

void LoRaGWRadio::handleTransmissionTimer(cMessage *message)
{
    if (message->getKind() == IRadioSignal::SIGNAL_PART_WHOLE)
        endTransmission(message);
    else if (message->getKind() == IRadioSignal::SIGNAL_PART_PREAMBLE)
        continueTransmission(message);
    else if (message->getKind() == IRadioSignal::SIGNAL_PART_HEADER)
        continueTransmission(message);
    else if (message->getKind() == IRadioSignal::SIGNAL_PART_DATA)
        endTransmission(message);
    else
        throw cRuntimeError("Unknown self message");
}

void LoRaGWRadio::handleUpperPacket(Packet *packet)
{
    emit(packetReceivedFromUpperSignal, packet);

    EV << packet->getDetailStringRepresentation(evFlags) << endl;
    const auto &frame = packet->peekAtFront<LoRaMacFrame>();

    auto preamble = makeShared<LoRaPhyPreamble>();

    preamble->setBandwidth(frame->getLoRaBW());
    preamble->setCenterFrequency(frame->getLoRaCF());
    preamble->setCodeRendundance(frame->getLoRaCR());
    preamble->setPower(mW(frame->getLoRaTP()));
    preamble->setSpreadFactor(frame->getLoRaSF());
    preamble->setUseHeader(frame->getLoRaUseHeader());
    preamble->setReceiverAddress(frame->getReceiverAddress());
//    const auto & loraHeader =  packet->peekAtFront<LoRaMacFrame>();
//    preamble->setReceiverAddress(loraHeader->getReceiverAddress());
//
    auto signalPowerReq = packet->addTagIfAbsent<SignalPowerReq>();
    signalPowerReq->setPower(mW(frame->getLoRaTP()));
//
    preamble->setChunkLength(b(16));
    packet->insertAtFront(preamble);
    EV << "Wysylam " << preamble->getPower() << " " << preamble->getSpreadFactor() << endl;


    if (separateTransmissionParts)
        startTransmission(packet, IRadioSignal::SIGNAL_PART_PREAMBLE);
    else
        startTransmission(packet, IRadioSignal::SIGNAL_PART_WHOLE);
}

void LoRaGWRadio::startTransmission(Packet *macFrame, IRadioSignal::SignalPart part)
{
    if(iAmTransmiting == false)
    {
        iAmTransmiting = true;
        auto radioFrame = createSignal(macFrame);
        auto transmission = radioFrame->getTransmission();

        cMessage *txTimer = new cMessage("transmissionTimer");
        txTimer->setKind(part);
        txTimer->setContextPointer(radioFrame);
        scheduleAt(transmission->getEndTime(part), txTimer);
        emit(transmissionStartedSignal, check_and_cast<const cObject *>(transmission));
        EV_INFO << "Transmission started: " << (IWirelessSignal *)radioFrame << " " << IRadioSignal::getSignalPartName(part) << " as " << transmission << endl;
        check_and_cast<LoRaMedium *>(medium.get())->emit(IRadioMedium::signalDepartureStartedSignal, check_and_cast<const cObject *>(transmission));
    }
    else delete macFrame;
}

void LoRaGWRadio::continueTransmission(cMessage *timer)
{
    auto previousPart = (IRadioSignal::SignalPart)timer->getKind();
    auto nextPart = (IRadioSignal::SignalPart)(previousPart + 1);
    auto radioFrame = static_cast<IWirelessSignal *>(timer->getContextPointer());
    auto transmission = radioFrame->getTransmission();
    EV_INFO << "Transmission ended: " << (IWirelessSignal *)radioFrame << " " << IRadioSignal::getSignalPartName(previousPart) << " as " << radioFrame->getTransmission() << endl;
    timer->setKind(nextPart);
    scheduleAt(transmission->getEndTime(nextPart), timer);
    EV_INFO << "Transmission started: " << (IWirelessSignal *)radioFrame << " " << IRadioSignal::getSignalPartName(nextPart) << " as " << transmission << endl;
}

void LoRaGWRadio::endTransmission(cMessage *timer)
{
    iAmTransmiting = false;
    auto part = (IRadioSignal::SignalPart)timer->getKind();
    auto signal = static_cast<WirelessSignal *>(timer->getContextPointer());
    auto transmission = signal->getTransmission();
    timer->setContextPointer(nullptr);
//    concurrentTransmissions.remove(timer);
    EV_INFO << "Transmission ended: " << (IWirelessSignal *)signal << " " << IRadioSignal::getSignalPartName(part) << " as " << transmission << endl;
    emit(transmissionEndedSignal, check_and_cast<const cObject *>(transmission));
    check_and_cast<LoRaMedium *>(medium.get())->emit(IRadioMedium::signalDepartureEndedSignal, check_and_cast<const cObject *>(transmission));
    delete(timer);
}

void LoRaGWRadio::handleSignal(WirelessSignal *radioFrame)
{
    auto receptionTimer = createReceptionTimer(radioFrame);
    if (separateReceptionParts)
        startReception(receptionTimer, IRadioSignal::SIGNAL_PART_PREAMBLE);
    else
        startReception(receptionTimer, IRadioSignal::SIGNAL_PART_WHOLE);
}

bool LoRaGWRadio::isReceptionTimer(const cMessage *message) const
{
    return !strcmp(message->getName(), "receptionTimer");
}

void LoRaGWRadio::startReception(cMessage *timer, IRadioSignal::SignalPart part)
{
    auto radioFrame = static_cast<WirelessSignal *>(timer->getControlInfo());
    auto arrival = radioFrame->getArrival();
    auto reception = radioFrame->getReception();
    emit(LoRaGWRadioReceptionStarted, true);
    if (simTime() >= getSimulation()->getWarmupPeriod())
        LoRaGWRadioReceptionStarted_counter++;
    if (isReceiverMode(radioMode) && arrival->getStartTime(part) == simTime() && iAmTransmiting == false) {
        auto transmission = radioFrame->getTransmission();
        auto isReceptionAttempted = medium->isReceptionAttempted(this, transmission, part);
        EV_INFO << "LoRaGWRadio Reception started: " << (isReceptionAttempted ? "attempting" : "not attempting") << " " << (WirelessSignal *)radioFrame << " " << IRadioSignal::getSignalPartName(part) << " as " << reception << endl;
        if (isReceptionAttempted) {
            if(iAmGateway) {
                concurrentReceptions.push_back(timer);
            }
            attemptedReceptionTimers.push_back(timer);
        }
    }
    else
        EV_INFO << "LoRaGWRadio Reception started: ignoring " << (WirelessSignal *)radioFrame << " " << IRadioSignal::getSignalPartName(part) << " as " << reception << endl;
    timer->setKind(part);
    scheduleAt(arrival->getEndTime(part), timer);
    //updateTransceiverState();
    //updateTransceiverPart();
    radioMode = RADIO_MODE_TRANSCEIVER;
    check_and_cast<RadioMedium *>(medium.get())->emit(IRadioMedium::signalArrivalStartedSignal, check_and_cast<const cObject *>(reception));
    //check_and_cast<LoRaMedium *>(medium.get())->emit(IRadioMedium::signalArrivalStartedSignal, check_and_cast<const cObject *>(reception));
    if(iAmGateway) EV << "[MSDebug] start reception, size : " << concurrentReceptions.size() << endl;
}

void LoRaGWRadio::continueReception(cMessage *timer)
{
    auto previousPart = (IRadioSignal::SignalPart)timer->getKind();
    auto nextPart = (IRadioSignal::SignalPart)(previousPart + 1);
    auto radioFrame = static_cast<WirelessSignal *>(timer->getControlInfo());
    auto arrival = radioFrame->getArrival();
    auto reception = radioFrame->getReception();
    auto receptionTimer = contains(attemptedReceptionTimers, timer);

    if (!iAmGateway)
        throw cRuntimeError("This radio must be used only for wateways");

    if (receptionTimer && isReceiverMode(radioMode) && arrival->getEndTime(previousPart) == simTime() && iAmTransmiting == false) {
        auto transmission = radioFrame->getTransmission();
        bool isReceptionSuccessful = medium->isReceptionSuccessful(this, transmission, previousPart);
        EV_INFO << "LoRaGWRadio Reception ended: " << (isReceptionSuccessful ? "successfully" : "unsuccessfully") << " for " << (IWirelessSignal *)radioFrame << " " << IRadioSignal::getSignalPartName(previousPart) << " as " << reception << endl;
        if (!isReceptionSuccessful) {
            remove(attemptedReceptionTimers, timer);
        }
        auto isReceptionAttempted = medium->isReceptionAttempted(this, transmission, nextPart);
        EV_INFO << "LoRaGWRadio Reception started: " << (isReceptionAttempted ? "attempting" : "not attempting") << " " << (IWirelessSignal *)radioFrame << " " << IRadioSignal::getSignalPartName(nextPart) << " as " << reception << endl;
        if (!isReceptionAttempted) {
            remove(attemptedReceptionTimers, timer);
        }
    }
    else {
        EV_INFO << "LoRaGWRadio Reception ended: ignoring " << (IWirelessSignal *)radioFrame << " " << IRadioSignal::getSignalPartName(previousPart) << " as " << reception << endl;
        EV_INFO << "LoRaGWRadio Reception started: ignoring " << (IWirelessSignal *)radioFrame << " " << IRadioSignal::getSignalPartName(nextPart) << " as " << reception << endl;
    }
    timer->setKind(nextPart);
    scheduleAt(arrival->getEndTime(nextPart), timer);
    //updateTransceiverState();
    //updateTransceiverPart();
    radioMode = RADIO_MODE_TRANSCEIVER;
}

void LoRaGWRadio::endReception(cMessage *timer)
{
    auto part = (IRadioSignal::SignalPart)timer->getKind();
    auto radioFrame = static_cast<WirelessSignal *>(timer->getControlInfo());
    auto arrival = radioFrame->getArrival();
    auto reception = radioFrame->getReception();
    if (!iAmGateway)
        throw cRuntimeError("This radio must be used only for wateways");


    auto receptionTimer = contains(attemptedReceptionTimers, timer);

    if (receptionTimer && isReceiverMode(radioMode) && arrival->getEndTime() == simTime() && iAmTransmiting == false) {
        auto transmission = radioFrame->getTransmission();
// TODO: this would draw twice from the random number generator in isReceptionSuccessful: auto isReceptionSuccessful = medium->isReceptionSuccessful(this, transmission, part);
        auto isReceptionSuccessful = medium->getReceptionDecision(this, radioFrame->getListening(), transmission, part)->isReceptionSuccessful();
        EV_INFO << "LoRaGWRadio Reception ended: " << (isReceptionSuccessful ? "successfully" : "unsuccessfully") << " for " << (IWirelessSignal *)radioFrame << " " << IRadioSignal::getSignalPartName(part) << " as " << reception << endl;
        if(isReceptionSuccessful) {
            auto macFrame = medium->receivePacket(this, radioFrame);
            take(macFrame);
            emit(packetSentToUpperSignal, macFrame);
            emit(LoRaGWRadioReceptionFinishedCorrect, true);
            if (simTime() >= getSimulation()->getWarmupPeriod())
                LoRaGWRadioReceptionFinishedCorrect_counter++;
            EV << macFrame->getCompleteStringRepresentation(evFlags) << endl;
            sendUp(macFrame);
        }
    }
    else
        EV_INFO << "LoRaGWRadio Reception ended: ignoring " << (IWirelessSignal *)radioFrame << " " << IRadioSignal::getSignalPartName(part) << " as " << reception << endl;
    //updateTransceiverState();
    //updateTransceiverPart();
    remove(allReceptionTimers, timer);
    radioMode = RADIO_MODE_TRANSCEIVER;
    check_and_cast<RadioMedium *>(medium.get())->emit(IRadioMedium::signalArrivalEndedSignal, check_and_cast<const cObject *>(reception));
    delete timer;
}

void LoRaGWRadio::abortReception(cMessage *timer)
{
    if (!iAmGateway)
        throw cRuntimeError("This radio must be used only for wateways");
    NarrowbandRadioBase::abortReception(timer);
}

void LoRaGWRadio::setPower(W newPower)
{
    LoRaTransmitter *flatTransmitter = const_cast<LoRaTransmitter *>(check_and_cast<const LoRaTransmitter *>(transmitter));
    flatTransmitter->setPower(newPower);
}

void LoRaGWRadio::setBitrate(bps newBitrate)
{
    LoRaTransmitter *flatTransmitter = const_cast<LoRaTransmitter *>(check_and_cast<const LoRaTransmitter *>(transmitter));
    flatTransmitter->setBitrate(newBitrate);
    while (!attemptedReceptionTimers.empty())
        abortReception(attemptedReceptionTimers.front());
}


}

}
