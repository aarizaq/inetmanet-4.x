//
// Copyright (C) 2020 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


package inet.visualizer.common;

import inet.visualizer.contract.IRadioVisualizer;

//
// Integrates canvas and osg radio visualizers into a single module.
//
// @see ~RadioCanvasVisualizer, ~RadioOsgVisualizer, ~RadioVisualizerBase, ~IRadioVisualizer
//
module RadioVisualizer like IRadioVisualizer
{
    parameters:
        @display("i=block/app");
        string visualizationTargetModule = default("^");
        string visualizationSubjectModule = default("^");
        *.visualizationTargetModule = default(absPath(this.visualizationTargetModule));
        *.visualizationSubjectModule = default(absPath(this.visualizationSubjectModule));
    submodules:
        canvasVisualizer: <default(firstAvailableOrEmpty("RadioCanvasVisualizer"))> like IRadioVisualizer if typename != "" {
            parameters:
                @display("p=100,100");
        }
        osgVisualizer: <default(firstAvailableOrEmpty("RadioOsgVisualizer"))> like IRadioVisualizer if typename != "" {
            parameters:
                @display("p=100,200");
        }
}

