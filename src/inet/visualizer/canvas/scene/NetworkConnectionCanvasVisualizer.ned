//
// Copyright (C) 2020 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


package inet.visualizer.canvas.scene;

import inet.visualizer.base.NetworkConnectionVisualizerBase;
import inet.visualizer.contract.INetworkConnectionVisualizer;

//
// Visualizes multiple network connections on a 2D canvas.
//
// @see ~NetworkConnectionCanvasVisualizer, ~NetworkConnectionVisualizer, ~NetworkConnectionVisualizerBase, ~INetworkConnectionVisualizer
//
simple NetworkConnectionCanvasVisualizer extends NetworkConnectionVisualizerBase like INetworkConnectionVisualizer
{
    parameters:
        double zIndex = default(0); // Determines the drawing order of figures relative to other visualizers
        nodeFilter = default(""); // Disabled because OMNeT++ displays network connections by default
        @class(NetworkConnectionCanvasVisualizer);
}

