#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/ipv4-routing-helper.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/animation-interface.h"
#include "ns3/wifi-mac-header.h"
#include "ns3/packet.h"

#include <iomanip>
#include <cstdlib>

using namespace ns3;


// *********************************************************************************
// ******************************* Global Variables ********************************
// *********************************************************************************
uint8_t                 factor             = 10;
double                  radio              = 50.0;
uint16_t                port               = 8080;
std::vector<uint32_t>   PacketsSize        = {256,512,1024};
bool                    enablePcap         = true;
uint16_t                TimeSimulation     = 10;
uint8_t		        interval	   = 1;
uint32_t                MaxPackets         = TimeSimulation * 60 / interval;	
const std::string       category           = "NO-SDWN"; 
Time                    delayBetweenStarts = MilliSeconds(25); 
uint8_t                 nCorrida           = 10;


// *********************************************************************************
// *********************************** Functions ***********************************
// *********************************************************************************
void AnalyzeFlowMonitorResults(Ptr<FlowMonitor>, Ptr<Ipv4FlowClassifier>, uint32_t , std::string , std::string , uint8_t );

// *********************************************************************************
// ************************************* Main **************************************
// *********************************************************************************


int main(int argc, char* argv[]) {

    CommandLine cmd;
    cmd.Parse(argc, argv);
    //GlobalValue::Bind("SimulatorImplementationType", StringValue("ns3::RealtimeSimulatorImpl"));

    Time::SetResolution(Time::NS);
	LogComponentEnableAll(LOG_PREFIX_TIME); // Agrega timestamp

    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
    SeedManager::SetSeed(time(NULL)); // Usa el tiempo actual como semilla base (aleatorio)
    
    for (auto PacketSize : PacketsSize) {
        for (uint32_t i = 1; i <= factor; ++i) {
            for (uint32_t Corrida = 1; Corrida <= nCorrida; ++Corrida) {
            
            SeedManager::SetRun(Corrida); // Para tener una secuencia diferente en cada corrida
            
            // Crear nodos
            uint16_t    nStaWifi    = i * 10;
            std::cout << "Realizando analisis para corrida " << Corrida <<" con "<< nStaWifi << " dispositivos de tamaño " << PacketSize <<  std::endl;

            NodeContainer wifiApNode;
            wifiApNode.Create(1);

            NodeContainer wifiStaNodes;
            wifiStaNodes.Create(nStaWifi);

            // Configuración de movilidad
            MobilityHelper mobility;
            mobility.SetPositionAllocator("ns3::UniformDiscPositionAllocator",
                                        "rho", DoubleValue(radio),
                                        "X", DoubleValue(0.0),
                                        "Y", DoubleValue(0.0));
            mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
            mobility.Install(wifiStaNodes);

            MobilityHelper centerPosition;
            centerPosition.SetPositionAllocator("ns3::GridPositionAllocator",
                                                "MinX", DoubleValue(0.0),
                                                "MinY", DoubleValue(0.0),
                                                "DeltaX", DoubleValue(1.0),
                                                "DeltaY", DoubleValue(1.0),
                                                "GridWidth", UintegerValue(1),
                                                "LayoutType", StringValue("RowFirst"));
            centerPosition.SetMobilityModel("ns3::ConstantPositionMobilityModel");
            centerPosition.Install(wifiApNode.Get(0));

            // Configurar WiFi
            WifiHelper wifi;
            wifi.SetStandard(WIFI_STANDARD_80211n);

            YansWifiPhyHelper wifiPhy;
            
            YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
            wifiPhy.SetChannel(wifiChannel.Create());

            //wifiPhy.Set("ChannelSettings", StringValue("{11, 20, BAND_2_4GHZ, 0}"));
            wifiPhy.Set("ChannelSettings", StringValue("{36, 20, BAND_5GHZ, 0}"));
            
            WifiMacHelper wifiMac;
            Ssid ssid = Ssid("SDWN_PoFi_NS3");

            // Configurar AP
            NetDeviceContainer wifiApDevice;
            wifiMac.SetType("ns3::ApWifiMac", 
                            "Ssid", SsidValue(ssid));

            wifiApDevice = wifi.Install(wifiPhy, wifiMac, wifiApNode.Get(0));

            // Configurar estaciones con QoS
            NetDeviceContainer wifiStaDevices;
            WifiMacHelper staWifiMac;
        
            staWifiMac.SetType("ns3::StaWifiMac",
                    "Ssid", SsidValue(ssid),
                    "ActiveProbing", BooleanValue(false));
        
            wifiStaDevices = wifi.Install(wifiPhy, staWifiMac, wifiStaNodes);

            InternetStackHelper internet;
            internet.Install(wifiApNode);
            internet.Install(wifiStaNodes);

            Ipv4AddressHelper address;
            address.SetBase("192.168.1.0", "255.255.255.0");
            Ipv4InterfaceContainer staInterfaces = address.Assign(wifiStaDevices);
            Ipv4InterfaceContainer apInterface = address.Assign(wifiApDevice);
        
            // Configurar el servidor UDP en el AP
            UdpEchoServerHelper echoServer(port);
            ApplicationContainer serverApp = echoServer.Install(wifiApNode.Get(0));
            serverApp.Start(Seconds(0.0));  // Inicia el servidor al comienzo de la simulación
            serverApp.Stop(Minutes(TimeSimulation + 1));  // Detiene el servidor al final

            Time startTime  = Seconds(1.0); 
            // Configurar clientes UDP en las estaciones
            ApplicationContainer clientApps;
            for (uint32_t i = 0; i < wifiStaNodes.GetN(); ++i) {
                UdpEchoClientHelper echoClient(apInterface.GetAddress(0), port);
                echoClient.SetAttribute("MaxPackets", UintegerValue(MaxPackets));
                echoClient.SetAttribute("Interval", TimeValue(Seconds(interval)));
                echoClient.SetAttribute("PacketSize", UintegerValue(PacketSize));
                
        
                ApplicationContainer app = echoClient.Install(wifiStaNodes.Get(i));
                app.Start(startTime);
                app.Stop(Minutes(TimeSimulation + 1));
                
                clientApps.Add(app);
                startTime += delayBetweenStarts;
            }

            // Añadido para habilitar el monitoreo de flujo
            Ptr<FlowMonitor> flowMonitor;
            FlowMonitorHelper flowHelper;
            flowMonitor = flowHelper.InstallAll();
            Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowHelper.GetClassifier());

            // Animación y rastreo
            const std::string   packetsize  = std::to_string(PacketSize);
            const std::string filepath_xml = "scratch/Finals/xml/" + category + "/1S/" + packetsize;
            system(("mkdir -p " + filepath_xml).c_str());
            
            const std::string filename = "/NO_SDWN_NS3_PoFi_"+ category + "_Priority_" + std::to_string(nStaWifi) + "_DEVICES_" +  packetsize +"_PacketSize_"+ std::to_string(TimeSimulation) +"_Min_Original_Corrida_"+std::to_string(Corrida);
            AnimationInterface anim(filepath_xml + filename + ".xml");
            
            anim.EnablePacketMetadata(true);
            anim.UpdateNodeDescription(0, "Access Point");
            anim.SetConstantPosition(wifiApNode.Get(0), 0.0, 0.0);

            // Rastreo
            AsciiTraceHelper rastreo_ascii;
            // wifiPhy.EnableAsciiAll(rastreo_ascii.CreateFileStream("scratch/Finals/tracers/BK/SDWN_NS3_"+ std::to_string(PacketSize)+"_PacketSize_"+ std::to_string(nStaWifi) + "_DEVICES.tr"));
            if (enablePcap) {
                const std::string filepath_pcap = "scratch/Finals/pcap/" + category + "/1S/" + packetsize;
                system(("mkdir -p " + filepath_pcap).c_str());
                wifiPhy.EnablePcap(filepath_pcap + filename, wifiApDevice.Get(0));
            }
            
            Simulator::Stop(Minutes(TimeSimulation + 1.5));
            Simulator::Run();

            AnalyzeFlowMonitorResults(flowMonitor, classifier, nStaWifi, category, packetsize, Corrida );
            const std::string filepath_flowmon = "scratch/Finals/flowmon/"+ category + "/1S/" + packetsize;
            system(("mkdir -p " + filepath_flowmon).c_str());
            flowMonitor->SerializeToXmlFile(filepath_flowmon + filename + ".xml", true, true);
            Simulator::Destroy();
        
        }}}
	return 0;
}


void AnalyzeFlowMonitorResults(Ptr<FlowMonitor> flowMonitor, Ptr<Ipv4FlowClassifier> classifier, uint32_t nStaWifi, std::string category, std::string packetsize, uint8_t Corrida) {
    auto stats = flowMonitor->GetFlowStats();

    // Variables para métricas agregadas
    double totalThroughput = 0.0;
    double totalDelay = 0.0;
    uint32_t totalLostPackets = 0;
    uint32_t totalPacketsSent = 0;
    uint32_t totalPacketsReceived = 0;
    
    // Variables para análisis avanzado
    double maxThroughput = 0.0;
    FlowId maxFlowId = 0;
    double latencySum = 0.0;
    double latencySquaredSum = 0.0;
    
    // Vectores para distribución
    std::vector<double> throughputs;
    std::vector<double> delays;
    std::map<uint32_t, double> packetLossByFlow;

    // Crear archivo CSV

    // Nombre del archivo CSV con número de STAs
    const std::string filepath_statistics = "scratch/Finals/estadisticas/" + category + "/1S/" + packetsize;
	system(("mkdir -p " + filepath_statistics).c_str());

    const std::string filename = "/NO_SDWN_NS3_PoFi_"+ category + "_Priority_" + std::to_string(nStaWifi) + "_DEVICES_" +  packetsize +"_PacketSize_"+ std::to_string(TimeSimulation) +"_Min_Original_Corrida_" + std::to_string(Corrida);
	const std::string csvFilename = filepath_statistics + filename + ".csv";
            

    std::ofstream csvFile(csvFilename);
    if (!csvFile.is_open()) {
        std::cerr << "Error: no se pudo abrir el archivo CSV: " << csvFilename << std::endl;
        return;
    }

    // Encabezados CSV
    csvFile << "FlowID,SourceAddress,DestAddress,Throughput(Kbps),Delay(ms),LostPackets,SentPackets,ReceivedPackets\n";

    // Procesar cada flujo
    for (const auto &flow : stats) {
        auto flowId = flow.first;
        auto flowStats = flow.second;
        auto flowClass = classifier->FindFlow(flowId);

        // Calcular throughput
        double throughput = 0.0;
        if (flowStats.timeLastRxPacket > flowStats.timeFirstTxPacket) {
            throughput = (flowStats.rxBytes * 8.0) / 
                         (flowStats.timeLastRxPacket.GetSeconds() - flowStats.timeFirstTxPacket.GetSeconds()) / 
                         1024; // Kbps
        }

        // Calcular delay
        double delay = (flowStats.rxPackets > 0) ? (flowStats.delaySum.GetSeconds() / flowStats.rxPackets) : 0.0;

        // Actualizar métricas agregadas
        totalThroughput += throughput;
        totalDelay += delay * flowStats.rxPackets;
        totalLostPackets += flowStats.lostPackets;
        totalPacketsSent += flowStats.txPackets;
        totalPacketsReceived += flowStats.rxPackets;

        // Actualizar métricas avanzadas
        if (throughput > maxThroughput) {
            maxThroughput = throughput;
            maxFlowId = flowId;
        }
        
        latencySum += delay;
        latencySquaredSum += delay * delay;

        // Almacenar para distribución
        throughputs.push_back(throughput);
        delays.push_back(delay);
        packetLossByFlow[flowId] = (flowStats.txPackets > 0) ? 
                                 (double)flowStats.lostPackets / flowStats.txPackets * 100 : 0.0;

        // Escribir en CSV
        csvFile << flowId << ","
                << flowClass.sourceAddress << ","
                << flowClass.destinationAddress << ","
                << std::fixed << std::setprecision(2) << throughput << ","
                << delay * 1000 << ","  // en ms
                << flowStats.lostPackets << ","
                << flowStats.txPackets << ","
                << flowStats.rxPackets << "\n";

        // Mostrar por consola
        std::cout << "Flujo ID: " << flowId
                  << ", Protocolo: " << flowClass.protocol
                  << ", Desde: " << flowClass.sourceAddress
                  << ", Hacia: " << flowClass.destinationAddress
                  << "\n\tTasa de transferencia: " << throughput << " Kbps"
                  << "\n\tLatencia promedio: " << delay * 1000 << " ms"
                  << "\n\tPérdida de paquetes: " << flowStats.lostPackets << " (" 
                  << packetLossByFlow[flowId] << "%)"
                  << "\n\tPaquetes enviados: " << flowStats.txPackets
                  << "\n\tPaquetes recibidos: " << flowStats.rxPackets << "\n";
    }

    // Cálculos finales
    double averageDelay = totalPacketsReceived > 0 ? totalDelay / totalPacketsReceived : 0.0;
    double packetLossRate = totalPacketsSent > 0 ? (double)totalLostPackets / totalPacketsSent * 100 : 0.0;
    double meanLatency = stats.size() > 0 ? latencySum / stats.size() : 0.0;
    double variance = stats.size() > 0 ? (latencySquaredSum / stats.size()) - (meanLatency * meanLatency) : 0.0;
    double stddevLatency = std::sqrt(variance);

    // Escribir resumen en CSV
    csvFile << "\nResumen General\n";
    csvFile << "Metrica,Valor\n";
    csvFile << "Tasa de transferencia total (Kbps)," << totalThroughput << "\n";
    csvFile << "Latencia promedio (ms)," << averageDelay * 1000 << "\n";
    csvFile << "Desviación estándar latencia (ms)," << stddevLatency * 1000 << "\n";
    csvFile << "Porcentaje de pérdida de paquetes (%)," << packetLossRate << "\n";
    csvFile << "Tasa de transferencia máxima (Kbps)," << maxThroughput << "\n";
    csvFile << "Flujo con máximo throughput," << maxFlowId << "\n";
    csvFile << "Total paquetes enviados," << totalPacketsSent << "\n";
    csvFile << "Total paquetes recibidos," << totalPacketsReceived << "\n";
    csvFile << "Total paquetes perdidos," << totalLostPackets << "\n";

    // Mostrar resumen por consola
    std::cout << "\nResumen General:\n";
    std::cout << "Tasa de transferencia total: " << totalThroughput << " Kbps\n";
    std::cout << "Latencia promedio: " << averageDelay * 1000 << " ms\n";
    std::cout << "Desviación estándar de la latencia: " << stddevLatency * 1000 << " ms\n";
    std::cout << "Porcentaje de pérdida de paquetes: " << packetLossRate << " %\n";
    std::cout << "Tasa de transferencia máxima: " << maxThroughput << " Kbps (Flujo ID: " << maxFlowId << ")\n";
    std::cout << "Total paquetes enviados: " << totalPacketsSent << "\n";
    std::cout << "Total paquetes recibidos: " << totalPacketsReceived << "\n";
    std::cout << "Total paquetes perdidos: " << totalLostPackets << "\n";

    csvFile.close();
}
