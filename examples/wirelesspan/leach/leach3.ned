
package inet.examples.wirelesspan.leach;
import inet.networklayer.configurator.ipv4.Ipv4NetworkConfigurator;
import inet.node.contract.INetworkNode;
import inet.node.inet.SensorNode;
import inet.physicallayer.wireless.common.contract.packetlevel.IRadioMedium;
import inet.visualizer.contract.IIntegratedVisualizer;
import inet.environment.common.PhysicalEnvironment;

network LeachSimpleNet
{
    parameters:
        int numNodes;
        @display("bgb=1209.63,909.50995;bgg=100,1,grey95");
        @figure[title](type=label; pos=0,-1; anchor=sw; color=darkblue; text="");

        @statistic[residualEnergyCapacity](source="energyStorage"; record=count);

        @signal[subIntervalTot](type="int");
        @statistic[rounds](title="num of rounds"; source="subIntervalTot"; record=vector?,counts?);

//        @figure[rcvdPkText](type=indicatorText; pos=380,20; anchor=w; font=,18; textFormat="packets received: %g"; initialValue=0);
//        @statistic[packetReceived](source=host2.app[0].packetReceived; record=figure(count); targetFigure=rcvdPkText);

    submodules:
        visualizer: <default(firstAvailableOrEmpty("IntegratedCanvasVisualizer"))> like IIntegratedVisualizer if typename != "" {
            @display("p=550,50");
        }
        configurator: Ipv4NetworkConfigurator {
            @display("p=512.39996,181.17");
        }
        radioMedium: <default("UnitDiskRadioMedium")> like IRadioMedium {
            @display("p=512.39996,289.13998");
        }
        physicalEnvironment: PhysicalEnvironment {
            @display("p=512.39996,441.02997");
        }
        host[numNodes]: <default("WirelessHost")> like INetworkNode {
            @display("i=misc/sensor2");
        }
        baseStation: <default("WirelessHost")> like INetworkNode {
            @display("p=276.33,355.02;i=misc/sensorgateway");
        }

}
