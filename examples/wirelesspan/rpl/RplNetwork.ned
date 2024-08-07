package inet.examples.wirelesspan.rpl;

import inet.common.scenario.ScenarioManager;
import inet.physicallayer.wireless.common.medium.UnitDiskRadioMedium;
import inet.physicallayer.wireless.common.contract.packetlevel.IRadioMedium;
import inet.visualizer.contract.IIntegratedVisualizer;
import inet.wirelesspan.nodes.RplRouter;
import inet.wirelesspan.networklayer.configurator.RplNetConfigurator;
import inet.networklayer.configurator.ipv6.Ipv6FlatNetworkConfigurator;

network RplNetwork
{
    parameters:
        int numNodes;
        int numSinks = default(1);
        @display("bgb=450,650");
    submodules:
        visualizer: <default(firstAvailableOrEmpty("IntegratedCanvasVisualizer"))> like IIntegratedVisualizer if typename != "" {
            @display("p=550,50");
            networkRouteVisualizer.displayRoutes = true;
        }
        radioMedium: <> like IRadioMedium {
            @display("p=550,250;is=s");
        }
        configurator: Ipv6FlatNetworkConfigurator {
            @display("p=550,150;is=s");
        }
        
        sink[numSinks]: RplRouter {
            @display("i=device/pocketpc_s;p=149.112,75.864");
        }
        host[numNodes]: RplRouter {
            @display("i=device/pocketpc_s;p=83.712,179.196");
        }

    connections allowunconnected:
        host[0].ethg++ <--> {  delay = 0.5us; datarate = 100Mbps; } <--> sink[0].ethg++;

}
