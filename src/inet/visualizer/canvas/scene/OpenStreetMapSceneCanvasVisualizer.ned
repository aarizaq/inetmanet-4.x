//
// Copyright (C) 2020 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


package inet.visualizer.canvas.scene;

import inet.visualizer.base.SceneVisualizerBase;
import inet.visualizer.contract.ISceneVisualizer;

//
// Visualizes a stream map given in an OpenStreetMap (openstreetmap.org) file.
// Other objects of the scene are placed on the map according to the coordinate system module.
//
// @see ~SceneCanvasVisualizer, ~SceneVisualizer, ~SceneVisualizerBase, ~ISceneVisualizer
// @see ~IGeographicCoordinateSystem, ~SimpleGeographicCoordinateSystem, ~OsgGeographicCoordinateSystem
//
simple OpenStreetMapSceneCanvasVisualizer extends SceneVisualizerBase like ISceneVisualizer
{
    parameters:
        @class(OpenStreetMapSceneCanvasVisualizer);
        string coordinateSystemModule = default("coordinateSystem"); // The geographic coordinate system, refers to a top-level module by default

        xml mapFile; // OpenStreetMap map file, e.g. one exported from openstreetmap.org

        double zIndex = default(0); // Determines the drawing order of figures relative to other visualizers
        bool adjustBackgroundBox = default(true); // If true, sets the background box (bgb tag) to match map bounds
}

