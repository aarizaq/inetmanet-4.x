//
// Copyright (C) 2013 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#ifndef __INET_SCALARMEDIUMANALOGMODELLOSS_H
#define __INET_SCALARMEDIUMANALOGMODELLOSS_H

#include "inet/physicallayer/wireless/common/analogmodel/scalar/ScalarMediumAnalogModel.h"

namespace inet {

namespace physicallayer {

class INET_API ScalarMediumAnalogModelLoss : public ScalarMediumAnalogModel
{
    typedef std::vector<int> Links;
    static std::map<int, Links> uniLinks;
    static std::map<int, Links> lossLinks;
    m communicationRange;
    bool checkLinkLoss(const IRadio *radio, const ITransmission *transmission) const;
  protected:

    virtual void initialize(int stage) override;

  public:
    virtual std::ostream& printToStream(std::ostream& stream, int level, int evFlags = 0) const override;

    virtual W computeReceptionPower(const IRadio *radio, const ITransmission *transmission, const IArrival *arrival) const override;

};

} // namespace physicallayer

} // namespace inet

#endif

