//
// Copyright (C) 2020 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


package inet.visualizer.contract;

//
// Interface allows configuring different ~IMobilityVisualizer
// submodules from INI files. Such a visualizer updates the position (among
// others) of visual representations according to the corresponding mobility
// model.
//
// @see ~MobilityCanvasVisualizer, ~MobilityOsgVisualizer, ~MobilityVisualizer, ~MobilityVisualizerBase
//
moduleinterface IMobilityVisualizer extends IVisualizer
{
    parameters:
        @display("i=block/app");
}

