//
// Copyright (C) 2013 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//
#include "inet/wirelesspan/wakeup/packetlevelradio/WakeUpRadioBase.h"

#include "inet/common/Simsignals.h"
#include "inet/common/ProtocolTag_m.h"
#include "inet/common/packet/chunk/BitCountChunk.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/RadioControlInfo_m.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/SignalTag_m.h"
#include "inet/physicallayer/wireless/common/medium/RadioMedium.h"
#include "inet/physicallayer/wireless/common/base/packetlevel/FlatReceiverBase.h"
#include "inet/physicallayer/wireless/common/base/packetlevel/FlatTransmitterBase.h"
#include "inet/wirelesspan/wakeup/packetlevelradio/WakeUpPreamble_m.h"
#include "inet/common/stlutils.h"
namespace inet {
namespace wirelesspan {
namespace physicallayer {

using namespace inet::physicallayer;

Define_Module(WakeUpRadioBase);

WakeUpRadioBase::WakeUpRadioBase() :
    NarrowbandRadioBase()
{
}

void WakeUpRadioBase::parseControllerRadioModeSwitchingTimes()
{
    const char *times = controlledRadio->par("switchingTimes");

    char prefix[3];
    unsigned int count = sscanf(times, "%s", prefix);

    if (count > 2)
        throw cRuntimeError("Metric prefix should be no more than two characters long");

    double metric = 1;

    if (strcmp("s", prefix) == 0)
        metric = 1;
    else if (strcmp("ms", prefix) == 0)
        metric = 0.001;
    else if (strcmp("ns", prefix) == 0)
        metric = 0.000000001;
    else
        throw cRuntimeError("Undefined or missed metric prefix for switchingTimes parameter");

    cStringTokenizer tok(times + count + 1);
    unsigned int idx = 0;
    while (tok.hasMoreTokens()) {
        controlledSwitchingTimes[idx / RADIO_MODE_SWITCHING][idx % RADIO_MODE_SWITCHING] = atof(tok.nextToken()) * metric;
        idx++;
    }
    if (idx != RADIO_MODE_SWITCHING * RADIO_MODE_SWITCHING)
        throw cRuntimeError("Check your switchingTimes parameter! Some parameters may be missed");
}


void WakeUpRadioBase::setWakeUpMode()
{
    Enter_Method("setWakeUpMode");
    setModeControlled(IRadio::RADIO_MODE_SLEEP);
    if (interval > simtime_t::ZERO) {
        this->setState(IRadio::RADIO_MODE_SLEEP);
        rescheduleAfter(interval, awake);
    }
    else
        this->setState(IRadio::RADIO_MODE_RECEIVER);

    if (scanning->isScheduled())
        cancelEvent(scanning);
}

IRadio::RadioMode WakeUpRadioBase::getRadioMode() const
{
    // TODO: emulate, if the radio is sleeping, the upper should perceive it like in receiving mode
    if (this->radioMode == IRadio::RADIO_MODE_TRANSMITTER || this->radioMode == IRadio::RADIO_MODE_TRANSCEIVER)
        return this->radioMode;
    if (controlledRadio->Radio::getRadioMode() == IRadio::RADIO_MODE_SLEEP || controlledRadio->Radio::getRadioMode() == IRadio::RADIO_MODE_OFF)
        return IRadio::RADIO_MODE_RECEIVER;
    return controlledRadio->Radio::getRadioMode();
}

IRadio::ReceptionState WakeUpRadioBase::getReceptionState() const
{
    auto stateControled = controlledRadio->Radio::getReceptionState();
    auto state = this->receptionState;
    if (state == IRadio::RECEPTION_STATE_BUSY || state == IRadio::RECEPTION_STATE_RECEIVING)
        return state;
    if (this->radioMode == IRadio::RADIO_MODE_TRANSMITTER || this->radioMode == IRadio::RADIO_MODE_TRANSCEIVER)
        return IRadio::RECEPTION_STATE_UNDEFINED;
    if (controlledRadio->Radio::getRadioMode() == IRadio::RADIO_MODE_SLEEP)
        return IRadio::RECEPTION_STATE_IDLE;
    return stateControled;
}


void WakeUpRadioBase::initialize(int stage)
{
    Radio::initialize(stage);
    if (stage == INITSTAGE_LOCAL) {
        toControlled = gate("toControlled");

        controlledSistemGateId = findGate("fromControlled");


        auto mod = toControlled->getPathEndGate()->getOwner();
        cModule *modAux = this->getParentModule()->getSubmodule("controlledRadio");
        controlledRadio = check_and_cast<inet::physicallayer::FlatRadioBase *>(modAux);
        if (mod != controlledRadio)
            throw cRuntimeError("check gate and radio controlled module");

        awake = new cMessage("awakeRadio");
        scanning = new cMessage("scanTime");

        interval = par("interval");
        scanInterval = par("scanTime");
        // the channel 0 is always present and must awake all the nodes in the coverage area
        listeningCodes.insert(0);
        auto listCodes = cStringTokenizer(par("listeningCodes").stringValue()).asIntVector();
        if (!listCodes.empty()) {
            for (const auto &elem: listCodes)
                listeningCodes.insert(elem);
        }
    }
    else if  (stage == INITSTAGE_PHYSICAL_LAYER) {
        // initialize
        parseControllerRadioModeSwitchingTimes();
        controlledRadio->subscribe(radioModeChangedSignal, this);
        controlledRadio->subscribe(transmissionStartedSignal, this);
        controlledRadio->subscribe(receptionStartedSignal, this);
        controlledRadio->subscribe(transmissionStateChangedSignal, this);
        controlledRadio->subscribe(receptionStateChangedSignal, this);
        controlledRadio->subscribe(receivedSignalPartChangedSignal, this);
        controlledRadio->subscribe(transmittedSignalPartChangedSignal, this);

        setWakeUpMode();
    }
}

void WakeUpRadioBase::cancelScanning()
{
    if (awake->isScheduled())
        cancelEvent(awake);
    if (scanning->isScheduled())
        cancelEvent(scanning);
    if (interval > simtime_t::ZERO)
        this->setState(IRadio::RADIO_MODE_SLEEP);
    else
        this->setState(IRadio::RADIO_MODE_RECEIVER);
}

void WakeUpRadioBase::setState(RadioMode newRadioMode)
{
    Enter_Method("setState");
    Radio::setRadioMode(newRadioMode);
}

void WakeUpRadioBase::handleSelfMessage(cMessage *message)
{
    if (message == awake) {
        if (radioMode == IRadio::RADIO_MODE_OFF || radioMode == IRadio::RADIO_MODE_SLEEP)
            setState(IRadio::RADIO_MODE_RECEIVER);
        if (attemptedReceptionTimers.size() > 0 && !inmediateAwake) {
            // energy detected
            controllerRadioAwakeReceiver();
            cancelScanning();
        }
        else {
            if (interval > simtime_t::ZERO) {
                scheduleAfter(interval, awake);
                rescheduleAfter(scanInterval, scanning);
            }
        }
    }
    else if (message == scanning) {
        if (radioMode == IRadio::RADIO_MODE_RECEIVER) {

            if (getReceptionState () == RECEPTION_STATE_IDLE || getReceptionState () == RECEPTION_STATE_UNDEFINED) {
                if (interval > simtime_t::ZERO)
                    setState(IRadio::RADIO_MODE_SLEEP);
                if (controlledRadio->Radio::getRadioMode() == RADIO_MODE_RECEIVER && controlledRadio->getReceptionState () == RECEPTION_STATE_IDLE){
                    controlledRadio->setRadioMode(IRadio::RADIO_MODE_SLEEP);
                }
            }
            else if (getReceptionState () == RECEPTION_STATE_BUSY ) {
                controlledRadio->setRadioMode(IRadio::RADIO_MODE_RECEIVER);
            }
        }
    }
    else {
        Radio::handleSelfMessage(message);
    }
}


void WakeUpRadioBase::receiveSignal(cComponent *source, simsignal_t signalID, intval_t value, cObject *details)
{
    Enter_Method("%s", cComponent::getSignalName(signalID));
    emit(signalID, value, details);
}

void WakeUpRadioBase::receiveSignal(cComponent *source, simsignal_t signalID, cObject *obj, cObject *details)
{
    Enter_Method("%s", cComponent::getSignalName(signalID));
    emit(signalID, obj, details);
}

void WakeUpRadioBase::updateTransceiverState()
{
    auto receptionTimer = attemptedReceptionTimers.size() > 0;
    // reception state
    ReceptionState newRadioReceptionState;
    if (radioMode == RADIO_MODE_OFF || radioMode == RADIO_MODE_SWITCHING || radioMode == RADIO_MODE_SLEEP || radioMode == RADIO_MODE_TRANSMITTER)
        newRadioReceptionState = RECEPTION_STATE_UNDEFINED;
    else if (receptionTimer)
        newRadioReceptionState = RECEPTION_STATE_RECEIVING;
    else if (isListeningPossible())
        newRadioReceptionState = RECEPTION_STATE_BUSY;
    else
        newRadioReceptionState = RECEPTION_STATE_IDLE;
    if (receptionState != newRadioReceptionState) {
        EV_INFO << "Changing radio reception state from \x1b[1m" << getRadioReceptionStateName(receptionState) << "\x1b[0m to \x1b[1m" << getRadioReceptionStateName(newRadioReceptionState) << "\x1b[0m." << endl;
        receptionState = newRadioReceptionState;
        emit(receptionStateChangedSignal, newRadioReceptionState);
    }
    // transmission state
    TransmissionState newRadioTransmissionState;
    auto transmissionTimer = transmissionTimers.size() > 0;

    if (radioMode == RADIO_MODE_OFF || radioMode == RADIO_MODE_SWITCHING || radioMode == RADIO_MODE_SLEEP || radioMode == RADIO_MODE_RECEIVER)
        newRadioTransmissionState = TRANSMISSION_STATE_UNDEFINED;
    else if (transmissionTimer)
        newRadioTransmissionState = TRANSMISSION_STATE_TRANSMITTING;
    else
        newRadioTransmissionState = TRANSMISSION_STATE_IDLE;
    if (transmissionState != newRadioTransmissionState) {
        EV_INFO << "Changing radio transmission state from \x1b[1m" << getRadioTransmissionStateName(transmissionState) << "\x1b[0m to \x1b[1m" << getRadioTransmissionStateName(newRadioTransmissionState) << "\x1b[0m." << endl;
        transmissionState = newRadioTransmissionState;
        emit(transmissionStateChangedSignal, newRadioTransmissionState);
    }
}

void WakeUpRadioBase::startReception(cMessage *timer, IRadioSignal::SignalPart part)
{
    auto signal = static_cast<WirelessSignal *>(timer->getControlInfo());
    auto arrival = signal->getArrival();
    auto reception = signal->getReception();
    auto packet = medium->receivePacket(this, signal);

    if (packet->getTag<PacketProtocolTag>()->getProtocol() == &Protocol::wakeUpOnRadio) {
        auto preamble = packet->peekAtFront<WakeUpPreamble>();
        // ignore channels
        auto it = listeningCodes.find(preamble->getListeningCode());
        if (it != listeningCodes.end()) {
            // Wake on radio, check timer
            //simtime_t arrivalTime = arrival->getStartTime(IRadioSignal::SignalPart::SIGNAL_PART_WHOLE);
            simtime_t endArrivalTime = arrival->getEndTime(IRadioSignal::SignalPart::SIGNAL_PART_WHOLE);
            if (isReceiverMode(radioMode) || (awake->isScheduled() && awake->getArrivalTime() <= endArrivalTime)) {
                // awakening possible
                auto transmission = signal->getTransmission();
                auto isReceptionAttempted = medium->isReceptionAttempted(this, transmission, part);
                EV_INFO << "Reception started: " << (isReceptionAttempted ? "\x1b[1mattempting\x1b[0m" : "\x1b[1mnot attempting\x1b[0m") << " " << (IWirelessSignal*) signal << " " << IRadioSignal::getSignalPartName(part)  << " as " << reception << endl;
                if (isReceptionAttempted) {
                    attemptedReceptionTimers.push_back(timer);
                    emit(receptionStartedSignal, check_and_cast<const cObject*>(reception));
                    if (inmediateAwake) {
                        controllerRadioAwakeReceiver();
                        cancelScanning();
                    }
                }
            }
            else
                EV_INFO << "Radio will be off all the period, reception started: \x1b[1mignoring\x1b[0m " << (IWirelessSignal *)signal << " " << IRadioSignal::getSignalPartName(part) << " as " << reception << endl;
        }
        else
            EV_INFO << "Channel ignored, Reception started: \x1b[1mignoring\x1b[0m " << (IWirelessSignal *)signal << " " << IRadioSignal::getSignalPartName(part) << " as " << reception << endl;
    }
    else {
        // TODO should be this, but it breaks fingerprints: if (receptionTimer == nullptr && isReceiverMode(radioMode) && arrival->getStartTime(part) == simTime()) {
        if (isReceiverMode(radioMode) && arrival->getStartTime(part) == simTime()) {
            auto transmission = signal->getTransmission();
            auto isReceptionAttempted = medium->isReceptionAttempted(this, transmission, part);
            EV_INFO << "Reception started: " << (isReceptionAttempted ? "\x1b[1mattempting\x1b[0m" : "\x1b[1mnot attempting\x1b[0m") << " " << (IWirelessSignal *)signal << " " << IRadioSignal::getSignalPartName(part) << " as " << reception << endl;
            if (isReceptionAttempted) {
                attemptedReceptionTimers.push_back(timer);
                emit(receptionStartedSignal, check_and_cast<const cObject *>(reception));
            }
        }
        else
            EV_INFO << "Reception started: \x1b[1mignoring\x1b[0m " << (IWirelessSignal *)signal << " " << IRadioSignal::getSignalPartName(part) << " as " << reception << endl;
    }
    timer->setKind(part);
    scheduleAt(arrival->getEndTime(part), timer);
    updateTransceiverState();
    updateTransceiverPart();
    // TODO move to radio medium
    check_and_cast<RadioMedium *>(medium.get())->emit(IRadioMedium::signalArrivalStartedSignal, check_and_cast<const cObject *>(reception));
}

void WakeUpRadioBase::sendBeacon(int code)
{
    cancelScanning();
    if (transmissionTimers.size() > 0)
        return;
    Packet *packet = new Packet();
    auto preamble = makeShared<WakeUpPreamble>();
    //preamble->setChannel(channel);
    preamble->setListeningCode(code);

    packet->insertAtBack(preamble);
    packet->addTagIfAbsent<PacketProtocolTag>()->setProtocol(&Protocol::wakeUpOnRadio);
    packet->addTagIfAbsent<SignalBitrateReq>()->setDataBitrate(bps((int)std::floor(16/interval.dbl())));
    encapsulate(packet);
    WirelessSignal *signal = check_and_cast<WirelessSignal *>(medium->transmitPacket(this, packet));
    auto transmission = signal->getTransmission();
    auto transmissionTimer = getIdleTransmissionTimer();

    transmissionTimer->setKind(IRadioSignal::SignalPart::SIGNAL_PART_WHOLE);
    transmissionTimer->setContextPointer(const_cast<WirelessSignal *>(signal));

    scheduleAt(transmission->getEndTime(IRadioSignal::SignalPart::SIGNAL_PART_WHOLE), transmissionTimer);
    EV_INFO << "Transmission started: " << (IWirelessSignal *)signal << " " << IRadioSignal::getSignalPartName(IRadioSignal::SignalPart::SIGNAL_PART_WHOLE) << " as " << transmission << endl;
    updateTransceiverState();
    updateTransceiverPart();
    emit(transmissionStartedSignal, check_and_cast<const cObject *>(transmission));
    // TODO move to radio medium
    check_and_cast<RadioMedium *>(medium.get())->emit(IRadioMedium::signalDepartureStartedSignal, check_and_cast<const cObject *>(transmission));
}

void WakeUpRadioBase::setRadioModeWithCode(RadioMode newRadioMode, int code)
{
    Enter_Method("setRadioMode");
    if (newRadioMode == IRadio::RADIO_MODE_TRANSMITTER) {
        setState(IRadio::RADIO_MODE_TRANSMITTER);
        sendBeacon(code);
        setModeControlled(newRadioMode);
    }
    else if (newRadioMode == IRadio::RADIO_MODE_RECEIVER) {
        setWakeUpMode();
        //controlledRadio->setRadioMode(IRadio::RADIO_MODE_SLEEP);
    }
}

void WakeUpRadioBase::setModeControlled(RadioMode newRadioMode)
{
    Enter_Method("setModeControlled");
    cancelScanning();
    controlledRadio->setRadioMode(newRadioMode);
}

void WakeUpRadioBase::setRadioModeNoBeacon(RadioMode newRadioMode)
{
    if (newRadioMode == IRadio::RADIO_MODE_TRANSMITTER) {
        setState(IRadio::RADIO_MODE_TRANSMITTER);
        controlledRadio->setRadioMode(newRadioMode);
    }
    else if (newRadioMode == IRadio::RADIO_MODE_RECEIVER) {
        controlledRadio->setRadioMode(IRadio::RADIO_MODE_SLEEP);
    }
}

bool WakeUpRadioBase::isFromControlled(cMessage *message) const
{
    return message->getArrivalGateId() == controlledSistemGateId;
}

void WakeUpRadioBase::handleMessageWhenUp(cMessage *message)
{
    if (message->isSelfMessage())
        handleSelfMessage(message);
    else if (isUpperMessage(message))
        handleUpperMessage(message);
    else if (isLowerMessage(message))
        handleLowerMessage(message);
    else if (isFromControlled(message))
        send(message, "upperLayerOut");
    else
        throw cRuntimeError("Message '%s' received on unexpected gate '%s'", message->getName(), message->getArrivalGate()->getFullName());
}


void WakeUpRadioBase::handleUpperPacket(Packet *packet)
{
    // TODO: Control the delay of the receiver, it is necessary to delay the message, with the objective that the other radio could be awake
    if (controlledRadio->getRadioMode() == IRadio::RADIO_MODE_TRANSMITTER || controlledRadio->getRadioMode() == IRadio::RADIO_MODE_TRANSCEIVER) {
        send(packet, toControlled);
        return;
    }
    controlledRadio->setRadioMode(IRadio::RADIO_MODE_TRANSMITTER);
    if (getRadioMode() == IRadio::RADIO_MODE_TRANSMITTER) {
        send(packet, toControlled);
    }
    else {
        simtime_t switchingTime = controlledSwitchingTimes[controlledRadio->getRadioMode()][RADIO_MODE_TRANSMITTER];

        // The mac already delay it
        sendDelayed(packet, switchingTime, toControlled);

//        if (inmediateAwake)
//            sendDelayed(packet, switchingTime, toControlled);
//        else
//            sendDelayed(packet, switchingTime + interval, toControlled);

    }
}

void WakeUpRadioBase::endReception(cMessage *timer)
{
    auto part = (IRadioSignal::SignalPart)timer->getKind();
    auto signal = static_cast<WirelessSignal *>(timer->getControlInfo());
    auto arrival = signal->getArrival();
    auto reception = signal->getReception();

    if (contains(attemptedReceptionTimers, timer) && isReceiverMode(radioMode) && arrival->getEndTime() == simTime()) {
        auto transmission = signal->getTransmission();
// TODO: this would draw twice from the random number generator in isReceptionSuccessful: auto isReceptionSuccessful = medium->isReceptionSuccessful(this, transmission, part);
        auto isReceptionSuccessful = medium->getReceptionDecision(this, signal->getListening(), transmission, part)->isReceptionSuccessful();
        EV_INFO << "Reception ended: " << (isReceptionSuccessful ? "\x1b[1msuccessfully\x1b[0m" : "\x1b[1munsuccessfully\x1b[0m") << " for " << (IWirelessSignal *)signal << " " << IRadioSignal::getSignalPartName(part) << " as " << reception << endl;
        auto macFrame = medium->receivePacket(this, signal);
        // TODO: FIXME: see handling packets with incorrect PHY headers in the TODO file
        delete macFrame;
        emit(receptionEndedSignal, check_and_cast<const cObject *>(reception));
    }
    else
        EV_INFO << "Reception ended: \x1b[1mignoring\x1b[0m " << (IWirelessSignal *)signal << " " << IRadioSignal::getSignalPartName(part) << " as " << reception << endl;
    updateTransceiverState();
    updateTransceiverPart();

    remove(allReceptionTimers, timer);
    delete timer;

    // TODO: move to radio medium
    check_and_cast<RadioMedium *>(medium.get())->emit(IRadioMedium::signalArrivalEndedSignal, check_and_cast<const cObject *>(reception));
}



void WakeUpRadioBase::handleUpperCommand(cMessage *message)
{
    send(message, "toControlled");
}

void WakeUpRadioBase::setPower(W newPower)
{
    FlatTransmitterBase *flatTransmitter = const_cast<FlatTransmitterBase *>(check_and_cast<const FlatTransmitterBase *>(transmitter));
    flatTransmitter->setPower(newPower);
}

void WakeUpRadioBase::setPowerControlled(W newPower)
{
    controlledRadio->setPower(newPower);
}

void WakeUpRadioBase::setBitrate(bps newBitrate)
{
    controlledRadio->setBitrate(newBitrate);
}

void WakeUpRadioBase::controllerRadioAwakeReceiver()
{
    if (controlledRadio->getRadioMode() == Radio::RADIO_MODE_OFF || controlledRadio->getRadioMode() == Radio::RADIO_MODE_SLEEP) {
        controlledRadio->setRadioMode(RADIO_MODE_RECEIVER);
    }
}


void WakeUpRadioBase::controllerRadioAwakeTransmitter()
{
    if (controlledRadio->getRadioMode() == Radio::RADIO_MODE_OFF || controlledRadio->getRadioMode() == Radio::RADIO_MODE_SLEEP) {
        controlledRadio->setRadioMode(IRadio::RADIO_MODE_TRANSMITTER);
    }
}

// handle listening codes
std::vector<int> WakeUpRadioBase::getListeningCodes() const
{
    std::vector<int> codes;
    for (const auto &elem : listeningCodes) {
        codes.push_back(elem);
    }
    return codes;
}

void WakeUpRadioBase::addListeningCode(const int &code)
{
    listeningCodes.insert(code);
}

bool WakeUpRadioBase::removeListeningCode(const int & code)
{
    auto it = listeningCodes.find(code);
    if (it != listeningCodes.end()) {
        listeningCodes.erase(it);
        return true;
    }
    return false;
}


} // namespace physicallayer
}
} // namespace inet

