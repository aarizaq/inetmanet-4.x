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

#ifndef _TRICKLETIMER_H
#define _TRICKLETIMER_H

#include <map>
#include <vector>

#include "inet/wirelesspan/routing/rpl/ObjectiveFunction.h"
#include "inet/routing/base/RoutingProtocolBase.h"
#include "inet/common/IProtocolRegistrationListener.h"

namespace inet {
namespace wirelesspan {
namespace routing {

class TrickleTimer : public cSimpleModule
{
  private:
    uint8_t minInterval = 0;
    uint8_t numDoublings = 0;
    int currentInterval = 0;
    int skipIntDoublings = 0;
    int intervalUpdatesCtn = 0;
    int maxInterval = DEFAULT_DIO_INTERVAL_MIN * (pow(2, DEFAULT_DIO_INTERVAL_DOUBLINGS));
    bool started = false;
    cMessage *trickleTriggerEvent = nullptr;
    cMessage *trickleTriggerMsg = nullptr;
    cMessage *intervalTriggerEvent = nullptr;

    uint8_t redundancyConst = 0;//DEFAULT_DIO_REDUNDANCY_CONST;
    uint8_t ctrlMsgReceivedCtn = 0;

  public:
    TrickleTimer();
    ~TrickleTimer();

    /** Lifecycle **/
    void start() { start(false, 0); };
    void start(bool warmupDelay, int skipIntervalDoublings);
    virtual void reset();
    virtual void stop();
    virtual void suspend();

    /**
     * Increment counter of control messages heard during current interval [RFC 6550, 8.3]
     * based on the external routing module events.
     */
    void ctrlMsgReceived() { ctrlMsgReceivedCtn++; }

    /**
     * Check if number of control messages heard in current interval is not
     * greater than the threshold 'redundancy constant' [RFC 6206]
     *
     * @return true if control messages heard less than the redundancy const
     * false otherwise
     */
    bool checkRedundancyConst();

    /**
     * Schedule next trickle trigger event, causing transmission/broadcast of control message
     * from routing module
     */
    void scheduleNext();

    /**
     * Double current interval if no inconsistencies detected
     */
    void updateInterval();

    /** Messsage processing */
    void handleMessage(cMessage *message);
    void processSelfMessage(cMessage *message);
    void handleMessageWhenUp(cMessage *message);

    uint8_t getCtrlMsgReceived() const { return ctrlMsgReceivedCtn;}
    void setCtrlMsgReceived(uint8_t ctrlMsgReceivedCtn) {
        this->ctrlMsgReceivedCtn = ctrlMsgReceivedCtn;
    }

    int getCurrentInterval() const { return currentInterval; }
    void setCurrentInterval(int currentInterval) { this->currentInterval = currentInterval; }

    uint8_t getNumDoublings() const { return numDoublings; }
    void setNumDoublings(uint8_t numDoublings) { this->numDoublings = numDoublings; }

    bool hasStarted();

    uint8_t getRedundancyConst() const { return redundancyConst; }
    void setRedundancyConst(uint8_t redundancyConst) { this->redundancyConst = redundancyConst; }

    int getMaxInterval() const { return maxInterval; }
    void setMaxInterval(int maxInterval) { this->maxInterval = maxInterval; }

    uint8_t getMinInterval() const { return minInterval; }
    void setMinInterval(uint8_t minInterval) { this->minInterval = minInterval; }
};

}
}
} // namespace inet

#endif

