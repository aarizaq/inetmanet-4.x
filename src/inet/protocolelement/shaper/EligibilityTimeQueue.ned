//
// Copyright (C) 2020 OpenSim Ltd.
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
// along with this program.  If not, see <https://www.gnu.org/licenses/>.
//

package inet.protocolelement.shaper;

import inet.queueing.queue.PacketQueue;

//
// This module is a packet queue that keeps the packets in ascending order based
// on the eligibility time in the attached ~EligibilityTimeTag of the packets.
//
// This module is part of the asynchronous shaper infrastructure.
//
// @see ~EligibilityTimeGate, ~EligibilityTimeFilter, ~EligibilityTimeMeter, ~EligibilityTimeTag
//
simple EligibilityTimeQueue extends PacketQueue
{
    parameters:
        comparatorClass = default("inet::PacketEligibilityTimeComparator"); // specifies the class that compares the eligibility time in the ~EligibilityTimeTag of two packets
}