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

#include "inet/wirelesspan/routing/rpl/Rpl.h"

namespace inet {
namespace wirelesspan {
namespace routing {

Define_Module(TrickleTimer);

// initialized in the declaration
//TrickleTimer::TrickleTimer() :
//    trickleTriggerEvent(nullptr),
//    intervalTriggerEvent(nullptr),
//    intervalUpdatesCtn(0),
//    numDoublings(DEFAULT_DIO_INTERVAL_DOUBLINGS),
//    redundancyConst(DEFAULT_DIO_REDUNDANCY_CONST),
//    started(false),
//    ctrlMsgReceivedCtn(0)
//{
//}

TrickleTimer::TrickleTimer()
{
}


TrickleTimer::~TrickleTimer() {
    stop();
}

void TrickleTimer::stop() {
    try {
        cancelAndDelete(trickleTriggerEvent);
        cancelAndDelete(intervalTriggerEvent);
    }
    catch (...) {
        EV_WARN << "Exception while deleting trickle timer's internal events" << endl;
    }

}

void TrickleTimer::start(bool warmupDelay, int skipIntervalDoublings) {
    Enter_Method("TrickleTimer::start()");
    EV_INFO << "Trickle timer started" << endl;
    skipIntDoublings = skipIntervalDoublings;
    started = true;
    if (minInterval == 0)
        minInterval = DEFAULT_DIO_INTERVAL_MIN;
    if (numDoublings == 0)
        numDoublings = DEFAULT_DIO_INTERVAL_DOUBLINGS;
    if (currentInterval == 0)
        currentInterval = DEFAULT_DIO_INTERVAL_MIN;

    if (redundancyConst == 0)
        redundancyConst = DEFAULT_DIO_REDUNDANCY_CONST;

    currentInterval = warmupDelay ? minInterval * 2 : minInterval;
    maxInterval = minInterval * (pow(2, numDoublings));
    ctrlMsgReceivedCtn = 0;

    intervalTriggerEvent = new cMessage("TT interval update", TRICKLE_INTERVAL_UPDATE_EVENT);
    trickleTriggerEvent = new cMessage("TT triggered", TRICKLE_TRIGGER_EVENT);

    scheduleAt(simTime() + currentInterval, intervalTriggerEvent);
    scheduleNext();
}

void TrickleTimer::handleMessageWhenUp(cMessage *message)
{
    if (message->isSelfMessage())
        processSelfMessage(message);
    else
        handleMessage(message);
}

void TrickleTimer::handleMessage(cMessage *message)
{
    if (message->isSelfMessage())
        processSelfMessage(message);
}

void TrickleTimer::processSelfMessage(cMessage *message)
{
    switch (message->getKind()) {
        case TRICKLE_INTERVAL_UPDATE_EVENT: {
            if (skipIntDoublings)
                intervalUpdatesCtn++;

            if (currentInterval < maxInterval) {
                if (intervalUpdatesCtn >= skipIntDoublings) {
                    currentInterval *= 2;
                    EV_INFO << "Trickle interval doubled, current - " << currentInterval << endl;
                }
            }

            ctrlMsgReceivedCtn = 0;

            if (!intervalTriggerEvent)
                intervalTriggerEvent = new cMessage("TT interval update", TRICKLE_INTERVAL_UPDATE_EVENT);

            if (intervalTriggerEvent->isScheduled())
                cancelEvent(intervalTriggerEvent);

            scheduleAt(simTime() + currentInterval, intervalTriggerEvent);
            scheduleNext();

            break;
        }
        case TRICKLE_TRIGGER_EVENT: {
            send(new cMessage("", TRICKLE_TRIGGER_EVENT), "rpModule$o");
            break;
        }
        default: {
            throw cRuntimeError("Unknown kind of trickle timer self message");
        }
    }
}


void TrickleTimer::scheduleNext() {
    // TODO: Make delay float to allow more diverse transmission intervals
    unsigned long delay = currentInterval/2 + intrand(currentInterval/2);
    try {
        if (!trickleTriggerEvent)
            trickleTriggerEvent = new cMessage("TT triggered", TRICKLE_TRIGGER_EVENT);

        if (trickleTriggerEvent->isScheduled())
            cancelEvent(trickleTriggerEvent);

        scheduleAt(simTime() + delay, trickleTriggerEvent);
        EV_DETAIL << "DIO broadcast scheduled with delay - " << delay << endl;
    } catch (std::exception &e) {
        EV_WARN << "Exception while scheduling next DIO: " << e.what() << endl;
    }
}

bool TrickleTimer::hasStarted() {
    Enter_Method_Silent("TrickleTimer::hasStarted()");
    return started;
}

bool TrickleTimer::checkRedundancyConst() {
    Enter_Method_Silent("TrickleTimer::checkRedundancyConst()");
    return ctrlMsgReceivedCtn < redundancyConst;
}

void TrickleTimer::reset() {
    Enter_Method_Silent("TrickleTimer::reset()");

    if (minInterval == 0)
        minInterval = DEFAULT_DIO_INTERVAL_MIN;
    if (numDoublings == 0)
        numDoublings = DEFAULT_DIO_INTERVAL_DOUBLINGS;
    if (currentInterval == 0)
        currentInterval = DEFAULT_DIO_INTERVAL_MIN;

    ctrlMsgReceivedCtn = 0;
    currentInterval = minInterval;
    intervalUpdatesCtn = 0;
    try {
        if (intervalTriggerEvent)
            cancelEvent(intervalTriggerEvent);
        else
            intervalTriggerEvent = new cMessage("TT interval update", TRICKLE_INTERVAL_UPDATE_EVENT);

        if (trickleTriggerEvent)
            cancelEvent(trickleTriggerEvent);

        scheduleAt(simTime() + currentInterval, intervalTriggerEvent);
        EV_DETAIL << "Trickle timer reset" << endl;
    }
    catch (std::exception &e) {
        EV_WARN << "Exception: " << e.what() <<" while resetting trickle timer, currentInterval = "
            << currentInterval << "; \n next interval doubling would be scheduled at "
            << simTime() + currentInterval << endl;
    }
    scheduleNext();
}

void TrickleTimer::suspend() {
    Enter_Method_Silent("TrickleTimer::suspend()");
    if (intervalTriggerEvent)
        cancelEvent(intervalTriggerEvent);
    if (trickleTriggerEvent)
        cancelEvent(trickleTriggerEvent);

    minInterval = 0;
    numDoublings = 0;
    currentInterval = 0;
    redundancyConst = 0;

    EV_DETAIL << "Trickle timer suspended " << endl;
}


}
}
} // namespace inet




