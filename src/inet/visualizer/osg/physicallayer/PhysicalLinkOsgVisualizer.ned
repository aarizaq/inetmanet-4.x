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

package inet.visualizer.osg.physicallayer;

import inet.visualizer.base.PhysicalLinkVisualizerBase;
import inet.visualizer.contract.IPhysicalLinkVisualizer;

//
// This module visualizes physical layer links on a 3D osg scene. It displays an
// arrow for each active link in the network. See the documentation of the base
// module for what constitutes an active link. The arrow points from the source
// towards the destination. If a link is used in both directions then there are
// separate arrows for them. Each arrow fades out over time as the link becomes
// inactive unless it is reinforced by another packet. The arrow automatically
// follows the movement of mobile nodes.
//
// @see ~PhysicalLinkCanvasVisualizer, ~PhysicalLinkVisualizer, ~PhysicalLinkVisualizerBase, ~IPhysicalLinkVisualizer
//
simple PhysicalLinkOsgVisualizer extends PhysicalLinkVisualizerBase like IPhysicalLinkVisualizer
{
    parameters:
        @class(PhysicalLinkOsgVisualizer);
}

