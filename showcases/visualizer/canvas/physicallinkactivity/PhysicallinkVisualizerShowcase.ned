//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

package inet.showcases.visualizer.canvas.physicallinkactivity;

import inet.networklayer.configurator.ipv4.Ipv4NetworkConfigurator;
import inet.node.inet.AdhocHost;
import inet.node.inet.WirelessHost;
import inet.node.wireless.AccessPoint;
import inet.physicallayer.wireless.ieee80211.packetlevel.Ieee80211RadioMedium;
import inet.visualizer.canvas.integrated.IntegratedCanvasVisualizer;
import inet.visualizer.canvas.physicallayer.PhysicalLinkCanvasVisualizer;


network PhysicalLinkVisualizerEnablingShowcase
{
    @display("bgb=600,300");
    submodules:
        linkVisualizer: PhysicalLinkCanvasVisualizer {
            parameters:
                @display("p=100,50");
        }
        configurator: Ipv4NetworkConfigurator {
            parameters:
                @display("p=100,150");
        }
        radioMedium: Ieee80211RadioMedium {
            parameters:
                @display("p=100,250");
        }
        source: AdhocHost {
            @display("p=250,150");
        }
        destination: AdhocHost {
            @display("p=500,150");
        }
}


network PhysicalLinkVisualizerFilteringShowcase
{
    @display("bgb=780,330");
    submodules:
        visualizer: IntegratedCanvasVisualizer {
            parameters:
                @display("p=100,50");
        }
        configurator: Ipv4NetworkConfigurator {
            parameters:
                @display("p=100,150");
        }
        radioMedium: Ieee80211RadioMedium {
            parameters:
                @display("p=100,250");
        }
        source: WirelessHost {
            @display("p=648,92");
        }
        destination: WirelessHost {
            @display("p=268,150");
        }
        host1: WirelessHost {
            @display("p=648,216");
        }
        accessPoint: AccessPoint {
            @display("p=471,149");
        }
}

network PhysicalLinkVisualizerMobileShowcase
{
    @display("bgb=660,630");
    submodules:
        visualizer: IntegratedCanvasVisualizer {
            @display("p=100,50");
        }
        configurator: Ipv4NetworkConfigurator {
            @display("p=100,150");
        }
        radioMedium: Ieee80211RadioMedium {
            @display("p=100,250");
        }
        host1: AdhocHost {
            @display("p=509.736,103.976");
        }
        host2: AdhocHost {
            @display("p=365.184,213.024");
        }
        host3: AdhocHost {
            @display("p=498.324,229.508");
        }
        host4: AdhocHost {
            @display("p=280.228,365.184");
        }
        host5: AdhocHost {
            @display("p=365.184,517.344");
        }
        host6: AdhocHost {
            @display("p=523.684,372.792");
        }
        host7: AdhocHost {
            @display("p=267.548,139.48");
        }
}

