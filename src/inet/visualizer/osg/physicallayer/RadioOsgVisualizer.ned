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

import inet.visualizer.base.RadioVisualizerBase;
import inet.visualizer.contract.IRadioVisualizer;

//
// This module visualizes radios on a 3D osg scene.
//
// @see ~RadioCanvasVisualizer, ~RadioVisualizer, ~RadioVisualizerBase, ~IRadioVisualizer
//
simple RadioOsgVisualizer extends RadioVisualizerBase like IRadioVisualizer
{
    parameters:
        @class(RadioOsgVisualizer);
}

