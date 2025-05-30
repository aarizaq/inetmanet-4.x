//
// Copyright (C) 2013 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#include "inet/physicallayer/wireless/ieee802154/bitlevel/Ieee802154UwbIrReceiver.h"

#include "inet/physicallayer/wireless/common/analogmodel/dimensional/DimensionalNoise.h"
#include "inet/physicallayer/wireless/common/analogmodel/dimensional/DimensionalReceptionAnalogModel.h"
#include "inet/physicallayer/wireless/common/radio/packetlevel/BandListening.h"
#include "inet/physicallayer/wireless/common/radio/packetlevel/ListeningDecision.h"
#include "inet/physicallayer/wireless/common/radio/packetlevel/ReceptionDecision.h"

namespace inet {

namespace physicallayer {

Define_Module(Ieee802154UwbIrReceiver);

std::ostream& Ieee802154UwbIrReceiver::printToStream(std::ostream& stream, int level, int evFlags) const
{
    return stream << "Ieee802154UwbIrReceiver";
}

const IListening *Ieee802154UwbIrReceiver::createListening(const IRadio *radio, const simtime_t startTime, const simtime_t endTime, const Coord& startPosition, const Coord& endPosition) const
{
    return new BandListening(radio, startTime, endTime, startPosition, endPosition, cfg.centerFrequency, cfg.bandwidth);
}

const IListeningDecision *Ieee802154UwbIrReceiver::computeListeningDecision(const IListening *listening, const IInterference *interference) const
{
    return new ListeningDecision(listening, true);
}

bool Ieee802154UwbIrReceiver::computeIsReceptionAttempted(const IListening *listening, const IReception *reception, IRadioSignal::SignalPart part, const IInterference *interference) const
{
    return true;
}

bool Ieee802154UwbIrReceiver::computeIsReceptionSuccessful(const IListening *listening, const IReception *reception, IRadioSignal::SignalPart part, const IInterference *interference, const ISnir *snir) const
{
    std::vector<bool> *bits = decode(reception, interference->getInterferingReceptions(), interference->getBackgroundNoise());
    int bitLength = bits->size() - 48 - 8;
    bool isReceptionSuccessful = true;
    for (int i = 0; i < bitLength; i++) {
        bool bitValue = bits->at(i);
        EV_INFO << "Received bit at " << i << " is " << (int)bitValue << endl;
    }
    // KLUDGE check fake FCS
    for (int i = 0; i < 8; i++) {
        for (int j = 0; j + i < bitLength; j += 8)
            bits->at(bitLength + i) = bits->at(bitLength + i) ^ bits->at(j + i);
        isReceptionSuccessful &= !bits->at(bitLength + i);
    }
    delete bits;
    return isReceptionSuccessful;
}

const IReceptionDecision *Ieee802154UwbIrReceiver::computeReceptionDecision(const IListening *listening, const IReception *reception, IRadioSignal::SignalPart part, const IInterference *interference, const ISnir *snir) const
{
    bool isReceptionSuccessful = computeIsReceptionSuccessful(listening, reception, part, interference, snir);
    return new ReceptionDecision(reception, part, true, true, isReceptionSuccessful);
}

std::vector<bool> *Ieee802154UwbIrReceiver::decode(const IReception *reception, const std::vector<const IReception *> *interferingReceptions, const INoise *backgroundNoise) const
{
    simtime_t now, offset;
    simtime_t aSymbol, shift, burst;
    // times are absolute
    offset = reception->getStartTime() + cfg.preambleLength;
    shift = cfg.shift_duration;
    aSymbol = cfg.data_symbol_duration;
    burst = cfg.burst_duration;
    now = offset + cfg.pulse_duration / 2;
    std::pair<double, double> energyZero, energyOne;
    // Loop to decode each bit value
    int symbol;
    double packetSNIR = 0;
    std::vector<bool> *bits = new std::vector<bool>();
    simtime_t duration = reception->getEndTime() - reception->getStartTime();
    for (symbol = 0; cfg.preambleLength + symbol * aSymbol < duration; symbol++) {
        // sample in window zero
        now = now + cfg.getHoppingPos(symbol) * cfg.burst_duration;
        energyZero = integrateWindow(now, burst, reception, interferingReceptions, backgroundNoise);
        // sample in window one
        now = now + shift;
        energyOne = integrateWindow(now, burst, reception, interferingReceptions, backgroundNoise);
        int decodedBit;
        if (energyZero.second > energyOne.second) {
            decodedBit = 0;
            packetSNIR = packetSNIR + energyZero.first;
        }
        else {
            decodedBit = 1;
            packetSNIR = packetSNIR + energyOne.first;
        }
        bits->push_back(decodedBit);
        now = offset + (symbol + 1) * aSymbol + cfg.pulse_duration / 2;
    }
    // TODO review this whole SNR computation, seems to be wrong (from MiXiM)
    packetSNIR = packetSNIR / (symbol + 1);
    // TODO double snirLastPacket = 10 * log10(packetSNIR);  // convert to dB
    // TODO return SNIR?
    return bits;
}

/*
 * @brief Returns a pair with as first value the SNIR (if the signal is not null in this window, and 0 otherwise)
 * and as second value a "score" associated to this window. This score equals to the sum for all
 * 16 pulse peak positions of the voltage measured by the receiver ADC.
 */
// TODO review this code regarding the dimensional API, could it be done simply by simply adding the signal, the interference and the noise and integrating that?
std::pair<double, double> Ieee802154UwbIrReceiver::integrateWindow(simtime_t_cref pNow, simtime_t_cref burst, const IReception *reception, const std::vector<const IReception *> *interferingReceptions, const INoise *backgroundNoise) const
{
    std::pair<double, double> energy = std::make_pair(0.0, 0.0); // first: stores SNIR, second: stores total captured window energy
    simtime_t windowEnd = pNow + burst;
    const double peakPulsePower = 1.3E-3; // 1.3E-3 W peak power of pulse to reach  0dBm during burst; // peak instantaneous power of the transmitted pulse (A=0.6V) : 7E-3 W. But peak limit is 0 dBm
    // Triangular baseband pulses
    // we sample at each pulse peak
    // get the interpolated values of amplitude for each interferer
    // and add these to the peak with a random phase
    // we sample one point per pulse
    // caller has already set our time reference ("now") at the peak of the pulse
    for (simtime_t now = pNow; now < windowEnd; now += cfg.pulse_duration) {
        double signalValue = 0; // electric field from tracked signal [V/m²]
        double resPower = 0; // electric field at antenna = combination of all arriving electric fields [V/m²]
        double vEfield = 0; // voltage at antenna caused by electric field Efield [V]
        double vmeasured = 0; // voltage measured by energy-detector [V], including thermal noise
        double vmeasured_square = 0; // to the square [V²]
        double snir = 0; // burst SNIR estimate
        double vThermalNoise = 0; // thermal noise realization
        // consider signal power
        auto dimensionalSignalReception = check_and_cast<const DimensionalReceptionAnalogModel *>(reception->getAnalogModel());
        const Ptr<const IFunction<WpHz, Domain<simsec, Hz>>>& signalPower = dimensionalSignalReception->getPower();
        Interval<simsec, Hz> interval(Point<simsec, Hz>(simsec(now), Hz(3.1)), Point<simsec, Hz>(simsec(now), Hz(10.6)), 0b10, 0b10, 0b10);
        double measure = signalPower->getMean(interval).get<WpHz>() * peakPulsePower; // TODO de-normalize (peakPulsePower should be in AirFrame or in Signal, to be set at run-time)
        signalValue = measure * 0.5; // we capture half of the maximum possible pulse energy to account for self  interference
        resPower = resPower + signalValue;
        // consider all interferers at this point in time
        for (const auto& interferingReception : *interferingReceptions) {
            auto dimensionalInterferingReception = check_and_cast<const DimensionalReceptionAnalogModel *>(interferingReception->getAnalogModel());
            const Ptr<const IFunction<WpHz, Domain<simsec, Hz>>>& interferingPower = dimensionalInterferingReception->getPower();
            double measure = interferingPower->getMean(interval).get<WpHz>() * peakPulsePower; // TODO de-normalize (peakPulsePower should be in AirFrame or in Signal, to be set at run-time)
            // measure = measure * uniform(0, +1); // random point of Efield at sampling (due to pulse waveform and self interference)
            // take a random point within pulse envelope for interferer
            resPower = resPower + measure * uniform(-1, +1);
        }
//         double attenuatedPower = resPower / 10; // 10 dB = 6 dB implementation loss + 5 dB noise factor
        vEfield = sqrt(50 * resPower); // P=V²/R
        // add thermal noise realization
        const DimensionalNoise *dimensionalBackgroundNoise = check_and_cast<const DimensionalNoise *>(backgroundNoise);
        vThermalNoise = dimensionalBackgroundNoise->getPower()->getMean(interval).get<WpHz>();
        vmeasured = vEfield + vThermalNoise;
        vmeasured_square = pow(vmeasured, 2);
        // signal + interference + noise
        energy.second = energy.second + vmeasured_square; // collect this contribution
        // Now evaluates signal to noise ratio
        // signal converted to antenna voltage squared
        // TODO review this SNIR computation
        snir = signalValue / 2.0217E-12;
        energy.first = energy.first + snir;

    } // consider next point in time
    return energy;
}

} // namespace physicallayer

} // namespace inet

