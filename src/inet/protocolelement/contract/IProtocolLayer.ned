//
// Copyright (C) 2020 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


package inet.protocolelement.contract;

//
// Interface for all protocol layer modules that
// connect to a higher and to a lower protocol layer, both of which are optional.
//
moduleinterface IProtocolLayer
{
    parameters:
        @omittedTypename(OmittedProtocolLayer);
        @display("i=block/layer");
    gates:
        input upperLayerIn;
        output upperLayerOut;
        input lowerLayerIn;
        output lowerLayerOut;
}
