//
// Copyright (C) 2016 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#ifndef __INET_LEGACYSEQUENCENUMBERASSIGMENT_H
#define __INET_LEGACYSEQUENCENUMBERASSIGMENT_H

#include "inet/linklayer/ieee80211/mac/common/SequenceControlField.h"
#include "inet/linklayer/ieee80211/mac/contract/ISequenceNumberAssignment.h"

namespace inet {
namespace ieee80211 {

class INET_API LegacySequenceNumberAssigment : public ISequenceNumberAssignment
{
  protected:
    SequenceNumberCyclic lastSeqNum = SequenceNumberCyclic(0);

  public:
    virtual void assignSequenceNumber(const Ptr<Ieee80211DataOrMgmtHeader>& header) override;
};

} /* namespace ieee80211 */
} /* namespace inet */

#endif

