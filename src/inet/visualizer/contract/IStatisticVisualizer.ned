//
// Copyright (C) 2020 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


package inet.visualizer.contract;

//
// Interface allows configuring different ~IStatisticVisualizer
// submodules from INI files. Such a visualizer displays a statistic for
// multiple network nodes.
//
// @see ~StatisticCanvasVisualizer, ~StatisticOsgVisualizer, ~StatisticVisualizer, ~StatisticVisualizerBase
//
moduleinterface IStatisticVisualizer extends IVisualizer
{
    parameters:
        @display("i=block/app");
}

