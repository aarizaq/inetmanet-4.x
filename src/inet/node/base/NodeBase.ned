//
// Copyright (C) 2020 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


package inet.node.base;

import inet.clock.contract.IClock;
import inet.common.ExternalEnvironment;
import inet.common.IMeasurer;
import inet.common.lifecycle.NodeStatus;
import inet.mobility.contract.IMobility;
import inet.node.contract.INetworkNode;
import inet.power.contract.IEnergyGenerator;
import inet.power.contract.IEnergyManagement;
import inet.power.contract.IEnergyStorage;

//
// This module contains the most basic infrastructure for network nodes that are
// not strictly communication protocol-related.
//
// See derived modules: ~StandardHost, ~WirelessHost, ~AdhocHost, ~Router, etc.
//
module NodeBase like INetworkNode
{
    parameters:
        @networkNode;
        @labels(node,ethernet-node,wireless-node);
        bool hasStatus = default(false);
        bool hasEnvironment = default(false);
        string osgModel = default(""); // 3D model for OSG visualization, no 3D model by default
        string osgModelColor = default(""); // Tint color, no colorization by default
        string canvasImage = default(""); // Image for canvas visualization, no image by default
        string canvasImageColor = default(""); // Tint color, no colorization by default
        mobility.subjectModule = default("^");
        *.mobilityModule = default(exists(mobility) ? absPath(".mobility") : "");
        *.energySourceModule = default(exists(energyStorage) ? absPath(".energyStorage") : "");
        *.energyGeneratorModule = default(exists(energyGenerator) ? absPath(".energyGenerator") : "");
        *.clockModule = default(exists(clock) ? absPath(".clock") : "");
        @class(NodeBase);
        @display("bgb=1256,1125");
    submodules:
        status: NodeStatus if hasStatus {
            @display("p=125,80;is=s");
        }
        clock: <default("")> like IClock if typename != "" {
            @display("p=125,320;is=s");
        }
        energyStorage: <default("")> like IEnergyStorage if typename != "" {
            @display("p=125,400;is=s");
        }
        energyManagement: <default("")> like IEnergyManagement if typename != "" {
            @display("p=125,480;is=s");
        }
        energyGenerator: <default("")> like IEnergyGenerator if typename != "" {
            @display("p=125,560;is=s");
        }
        // TODO move mobility right after the status to have it in the same order as their positions, it changes fingerprints
        mobility: <default("")> like IMobility if typename != "" {
            @display("p=125,160;is=s");
        }
        measurer: <default("")> like IMeasurer if typename != "" {
            @display("p=125,640;is=s");
        }
        environment: ExternalEnvironment if hasEnvironment {
            @display("p=125,800;is=s");
        }
}

