//
// Copyright (C) 2001 Matthias Oppitz <Matthias.Oppitz@gmx.de>
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public License
// as published by the Free Software Foundation; either version 3
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
//

#include "inet/transportlayer/rtp/Reports_m.h"

namespace inet {

namespace rtp {

//
// SenderReport
//

Register_Class(SenderReport);

std::string SenderReport::str() const
{
    std::stringstream out;
    out << "SenderReport.timeStamp=" << getRTPTimeStamp();
    return out.str();
}

void SenderReport::dump(std::ostream& os) const
{
    os << "SenderReport:" << endl;
    os << "  ntpTimeStamp = " << getNTPTimeStamp() << endl;
    os << "  rtpTimeStamp = " << getRTPTimeStamp() << endl;
    os << "  packetCount = " << getPacketCount() << endl;
    os << "  byteCount = " << getByteCount() << endl;
}

//
// ReceptionReport
//

Register_Class(ReceptionReport);

std::string ReceptionReport::str() const
{
    std::stringstream out;
    out << "ReceptionReport.ssrc=" << getSsrc();
    return out.str();
}

void ReceptionReport::dump(std::ostream& os) const
{
    os << "ReceptionReport:" << endl;
    os << "  ssrc = " << getSsrc() << endl;
    os << "  fractionLost = " << (int)getFractionLost() << endl;
    os << "  packetsLostCumulative = " << getPacketsLostCumulative() << endl;
    os << "  extendedHighestSequenceNumber = " << getSequenceNumber() << endl;
    os << "  jitter = " << getJitter() << endl;
    os << "  lastSR = " << getLastSR() << endl;
    os << "  delaySinceLastSR = " << getDelaySinceLastSR() << endl;
}

} // namespace rtp

} // namespace inet

