//
// Copyright (C) 2020 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


package inet.linklayer.ieee8021q;

import inet.common.Module;
//
// Implements the module given interface and can be used as an omitted
// optional module that removes itself from the module hierarchy during initialization.
//
module OmittedIeee8021qTagEpdHeaderChecker extends Module like IIeee8021qTagEpdHeaderChecker
{
    parameters:
        string vlanTagType = default("");
        @class(::inet::OmittedModule);
        @display("i=block/checker,black,50");
    gates:
        input in;
        output out;
    connections:
        in --> out;
}

