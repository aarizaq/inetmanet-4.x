//
// Copyright (C) 2020 OpenSim Ltd.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.
//

package inet.visualizer.osg.mobility;

import inet.visualizer.base.MobilityVisualizerBase;
import inet.visualizer.contract.IMobilityVisualizer;

//
// This module visualizes multiple mobilities on a 3D osg scene. It moves the
// visual represnetation according to their mobility model. It also displays
// recent movement as a polyline, current velocity vector and current orientation.
//
// @see ~MobilityCanvasVisualizer, ~MobilityVisualizer, ~MobilityVisualizerBase, ~IMobilityVisualizer
//
simple MobilityOsgVisualizer extends MobilityVisualizerBase like IMobilityVisualizer
{
    parameters:
        @class(MobilityOsgVisualizer);
}
