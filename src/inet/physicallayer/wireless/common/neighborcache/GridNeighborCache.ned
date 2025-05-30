//
// Copyright (C) 2020 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


package inet.physicallayer.wireless.common.neighborcache;

import inet.common.Module;
import inet.physicallayer.wireless.common.contract.packetlevel.INeighborCache;

//
// This neighbor cache model organizes radios in a three-dimensional grid with
// a constant cell size and updates periodically.
//
module GridNeighborCache extends Module like INeighborCache
{
    parameters:
        string radioMediumModule = default("^");
        double cellSizeX @unit(m) = default(nan m); // NaN means use cell count
        double cellSizeY @unit(m) = default(nan m); // NaN means use cell count
        double cellSizeZ @unit(m) = default(nan m); // NaN means use cell count
        int cellCountX = default(10); // Divide space into so many cells
        int cellCountY = default(10); // Divide space into so many cells
        int cellCountZ = default(10); // Divide space into so many cells
        double refillPeriod @unit(s) = default(1s);
        @display("i=block/table2");
        @class(GridNeighborCache);
}

