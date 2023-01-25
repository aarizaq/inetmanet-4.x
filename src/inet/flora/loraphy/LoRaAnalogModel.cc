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

#include "inet/flora/loraphy/LoRaAnalogModel.h"

#include "inet/flora/lorabase/LoRaRadio.h"
#include "inet/flora/loraphy/LoRaReceiver.h"
#include "inet/flora/loraphy/LoRaReception.h"
#include "inet/flora/loraphy/LoRaTransmission.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/IRadioMedium.h"
#include "inet/physicallayer/wireless/common/analogmodel/packetlevel/ScalarAnalogModel.h"
#include "inet/physicallayer/wireless/common/analogmodel/packetlevel/ScalarReception.h"

namespace inet {

namespace flora {

Define_Module(LoRaAnalogModel);

std::ostream& LoRaAnalogModel::printToStream(std::ostream& stream, int level, int evFlags) const
{
    return stream << "LoRaAnalogModel";
}

const W LoRaAnalogModel::getBackgroundNoisePower(const LoRaBandListening *listening) const {
    //const LoRaBandListening *loRaListening = check_and_cast<const LoRaBandListening *>(listening);
    //Sensitivity values from Semtech SX1272/73 datasheet, table 10, Rev 3.1, March 2017
    W noisePower = W(math::dBmW2mW(-126.5) / 1000);
    if(listening->getLoRaSF() == 6)
    {
        if(listening->getLoRaBW() == Hz(125000)) noisePower = W(math::dBmW2mW(-121) / 1000);
        if(listening->getLoRaBW() == Hz(250000)) noisePower = W(math::dBmW2mW(-118) / 1000);
        if(listening->getLoRaBW() == Hz(500000)) noisePower = W(math::dBmW2mW(-111) / 1000);
    }

    if (listening->getLoRaSF() == 7)
    {
        if(listening->getLoRaBW() == Hz(125000)) noisePower = W(math::dBmW2mW(-124) / 1000);
        if(listening->getLoRaBW() == Hz(250000)) noisePower = W(math::dBmW2mW(-122) / 1000);
        if(listening->getLoRaBW() == Hz(500000)) noisePower = W(math::dBmW2mW(-116) / 1000);
    }

    if(listening->getLoRaSF() == 8)
    {
        if(listening->getLoRaBW() == Hz(125000)) noisePower = W(math::dBmW2mW(-127) / 1000);
        if(listening->getLoRaBW() == Hz(250000)) noisePower = W(math::dBmW2mW(-125) / 1000);
        if(listening->getLoRaBW() == Hz(500000)) noisePower = W(math::dBmW2mW(-119) / 1000);
    }
    if(listening->getLoRaSF() == 9)
    {
        if(listening->getLoRaBW() == Hz(125000)) noisePower = W(math::dBmW2mW(-130) / 1000);
        if(listening->getLoRaBW() == Hz(250000)) noisePower = W(math::dBmW2mW(-128) / 1000);
        if(listening->getLoRaBW() == Hz(500000)) noisePower = W(math::dBmW2mW(-122) / 1000);
    }
    if(listening->getLoRaSF() == 10)
    {
        if(listening->getLoRaBW() == Hz(125000)) noisePower = W(math::dBmW2mW(-133) / 1000);
        if(listening->getLoRaBW() == Hz(250000)) noisePower = W(math::dBmW2mW(-130) / 1000);
        if(listening->getLoRaBW() == Hz(500000)) noisePower = W(math::dBmW2mW(-125) / 1000);
    }
    if(listening->getLoRaSF() == 11)
    {
        if(listening->getLoRaBW() == Hz(125000)) noisePower = W(math::dBmW2mW(-135) / 1000);
        if(listening->getLoRaBW() == Hz(250000)) noisePower = W(math::dBmW2mW(-132) / 1000);
        if(listening->getLoRaBW() == Hz(500000)) noisePower = W(math::dBmW2mW(-128) / 1000);
    }
    if(listening->getLoRaSF() == 12)
    {
        if(listening->getLoRaBW() == Hz(125000)) noisePower = W(math::dBmW2mW(-137) / 1000);
        if(listening->getLoRaBW() == Hz(250000)) noisePower = W(math::dBmW2mW(-135) / 1000);
        if(listening->getLoRaBW() == Hz(500000)) noisePower = W(math::dBmW2mW(-129) / 1000);
    }
    return noisePower;
}

W LoRaAnalogModel::computeReceptionPower(const IRadio *receiverRadio, const ITransmission *transmission, const IArrival *arrival) const
{
    const IRadioMedium *radioMedium = receiverRadio->getMedium();
//    const IRadio *transmitterRadio = transmission->getTransmitter();
//    const IAntenna *receiverAntenna = receiverRadio->getAntenna();
//    const IAntenna *transmitterAntenna = transmitterRadio->getAntenna();
    const INarrowbandSignal *narrowbandSignalAnalogModel = check_and_cast<const INarrowbandSignal *>(transmission->getAnalogModel());
    const IScalarSignal *scalarSignalAnalogModel = check_and_cast<const IScalarSignal *>(transmission->getAnalogModel());
    const Coord receptionStartPosition = arrival->getStartPosition();
    const Coord receptionEndPosition = arrival->getEndPosition();
//    const Quaternion transmissionDirection = computeTransmissionDirection(transmission, arrival);
//    const Quaternion transmissionAntennaDirection = transmission->getStartOrientation() - transmissionDirection;
//    const Quaternion receptionAntennaDirection = transmissionDirection - arrival->getStartOrientation();
    double transmitterAntennaGain = computeAntennaGain(transmission->getTransmitterAntennaGain(), transmission->getStartPosition(), arrival->getStartPosition(), transmission->getStartOrientation());
    double receiverAntennaGain = computeAntennaGain(receiverRadio->getAntenna()->getGain().get(), arrival->getStartPosition(), transmission->getStartPosition(), arrival->getStartOrientation());
    double pathLoss = radioMedium->getPathLoss()->computePathLoss(transmission, arrival);
    double obstacleLoss = radioMedium->getObstacleLoss() ? radioMedium->getObstacleLoss()->computeObstacleLoss(narrowbandSignalAnalogModel->getCenterFrequency(), transmission->getStartPosition(), receptionStartPosition) : 1;
    W transmissionPower = scalarSignalAnalogModel->getPower();
    return transmissionPower * std::min(1.0, transmitterAntennaGain * receiverAntennaGain * pathLoss * obstacleLoss);
}

const IReception *LoRaAnalogModel::computeReception(const IRadio *receiverRadio, const ITransmission *transmission, const IArrival *arrival) const
{
    const LoRaTransmission *loRaTransmission = check_and_cast<const LoRaTransmission *>(transmission);
    const simtime_t receptionStartTime = arrival->getStartTime();
    const simtime_t receptionEndTime = arrival->getEndTime();
    const Quaternion receptionStartOrientation = arrival->getStartOrientation();
    const Quaternion receptionEndOrientation = arrival->getEndOrientation();
    const Coord receptionStartPosition = arrival->getStartPosition();
    const Coord receptionEndPosition = arrival->getEndPosition();
    W receivedPower = computeReceptionPower(receiverRadio, transmission, arrival);
    Hz LoRaCF = loRaTransmission->getLoRaCF();
    int LoRaSF = loRaTransmission->getLoRaSF();
    Hz LoRaBW = loRaTransmission->getLoRaBW();
    int LoRaCR = loRaTransmission->getLoRaCR();
    return new LoRaReception(receiverRadio, transmission, receptionStartTime, receptionEndTime, receptionStartPosition, receptionEndPosition, receptionStartOrientation, receptionEndOrientation, LoRaCF, LoRaBW, receivedPower, LoRaSF, LoRaCR);
}

const INoise *LoRaAnalogModel::computeNoise(const IListening *listening, const IInterference *interference) const
{
    const LoRaBandListening *bandListening = check_and_cast<const LoRaBandListening *>(listening);
    Hz commonCenterFrequency = bandListening->getLoRaCF();
    Hz commonBandwidth = bandListening->getLoRaBW();
    simtime_t noiseStartTime = SimTime::getMaxTime();
    simtime_t noiseEndTime = 0;
    std::map<simtime_t, W> powerChanges;
    powerChanges[math::getLowerBound<simtime_t>()] = W(0);
    powerChanges[math::getUpperBound<simtime_t>()] = W(0);
    const std::vector<const IReception *> *interferingReceptions = interference->getInterferingReceptions();
    for (auto reception : *interferingReceptions) {
        const ISignalAnalogModel *signalAnalogModel = reception->getAnalogModel();
        const INarrowbandSignal *narrowbandSignalAnalogModel = check_and_cast<const INarrowbandSignal *>(signalAnalogModel);
        const LoRaReception *loRaReception = check_and_cast<const LoRaReception *>(signalAnalogModel);
        Hz signalCenterFrequency = loRaReception->getLoRaCF();
        Hz signalBandwidth = loRaReception->getLoRaBW();
        if (commonCenterFrequency == signalCenterFrequency && commonBandwidth >= signalBandwidth)
            addReception(reception, noiseStartTime, noiseEndTime, powerChanges);
        else if (!ignorePartialInterference && areOverlappingBands(commonCenterFrequency, commonBandwidth, narrowbandSignalAnalogModel->getCenterFrequency(), narrowbandSignalAnalogModel->getBandwidth()))
            throw cRuntimeError("Partially interfering signals are not supported by ScalarAnalogModel, enable ignorePartialInterference to avoid this error!");
    }
    EV_TRACE << "Noise power begin " << endl;
    simtime_t startTime = listening->getStartTime();
    simtime_t endTime = listening->getEndTime();
    std::map<simtime_t, W> backgroundNoisePowerChanges;
    const W noisePower = getBackgroundNoisePower(bandListening);
    backgroundNoisePowerChanges.insert(std::pair<simtime_t, W>(startTime, noisePower));
    backgroundNoisePowerChanges.insert(std::pair<simtime_t, W>(endTime, -noisePower));
    for (const auto & background : backgroundNoisePowerChanges) {
        auto jt = powerChanges.find(background.first);
        if (jt != powerChanges.end())
            jt->second += background.second;
        else
            powerChanges.insert(std::pair<simtime_t, W>(background.first, background.second));
    }
    W power = W(0);
    for (auto & it : powerChanges) {
        power += it.second;
        it.second = power;
        EV_TRACE << "Noise at " << it.first << " = " << power << endl;
    }
    EV_TRACE << "Noise power end" << endl;
    const auto& powerFunction = makeShared<math::Interpolated1DFunction<W, simtime_t>>(powerChanges, &math::LeftInterpolator<simtime_t, W>::singleton);
    return new ScalarNoise(noiseStartTime, noiseEndTime, commonCenterFrequency, commonBandwidth, powerFunction);
}

const ISnir *LoRaAnalogModel::computeSNIR(const IReception *reception, const INoise *noise) const
{
    return new ScalarSnir(reception, noise);
}
} 

} // namespace inet

