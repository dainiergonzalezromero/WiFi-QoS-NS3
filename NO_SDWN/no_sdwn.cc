#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/animation-interface.h"

#include <iomanip>
#include <cstdlib>
#include <filesystem>

namespace fs = std::filesystem;
using namespace ns3;

NS_LOG_COMPONENT_DEFINE("NO_SDWN_NS3");

// *********************************************************************************
// ******************************* Global Variables ********************************
// *********************************************************************************
uint32_t nStaWifi = 10;             // Number of WiFi STAs
uint32_t PacketSize = 512;          // Packet size in bytes
double radio = 50.0;                // Coverage radius in meters
uint16_t port = 8080;               // Port for UDP communication
double TimeSimulationMin = 10.0;    // Simulation time in minutes
uint32_t interval = 1;              // Interval between packets in seconds
uint32_t nCorrida = 1;              // Run number
bool enablePcap = false;            // Enable PCAP capture
bool enableAnimation = false;       // Enable animation output
bool enableXml = false;             // Enable XML flow monitor output
double delayBetweenStartsMs = 25.0; // Delay between STA starts in ms
std::string category = "NO-SDWN";   // Category for file organization
uint32_t RngSeed = 0;               // Random seed for simulation
std::string mobilityType = "mixer";    // "yes", "no", o "mixer"
bool model_realist = true;          // Usar modelo de propagación realista (log-distance + Nakagami)


uint32_t nStaH = 3;                 // Number of High priority STAs (VO)
uint32_t nStaM = 3;                 // Number of Medium priority STAs (VI)
uint32_t nStaL = 2;                 // Number of Low priority STAs (BE)
uint32_t nStaNRT = 2;               // Number of Non Real Time STAs (BK)


// Variables globales para saber qué valores se están usando en la corrida
uint32_t CwMinH = 15;                       // CWmin High priority (VO)
uint32_t CwMaxH = 1023;                     // CWmax High priority (VO)
uint32_t CwMinM = 15;                       // CWmin Medium priority (VI)
uint32_t CwMaxM = 1023;                     // CWmax Medium priority (VI)
uint32_t CwMinL = 15;                       // CWmin Low priority (BE)
uint32_t CwMaxL = 1023;                     // CWmax Low priority (BE)
uint32_t CwMinNRT = 15;                     // CWmin NRT priority (BK)
uint32_t CwMaxNRT = 1023;                   // CWmax NRT priority (BK)

std::string AC;                                                     // Access Category

// *********************************************************************************
// *********************************** Functions ***********************************
// *********************************************************************************
void AnalyzeFlowMonitorResults(Ptr<FlowMonitor>, Ptr<Ipv4FlowClassifier>, 
                            uint32_t , std::string , std::string , 
                            uint8_t, uint32_t , uint32_t, uint32_t , 
                            uint32_t , uint32_t , uint32_t , uint32_t , uint32_t, uint32_t, uint32_t, uint32_t,uint32_t);
void SetupMobility(NodeContainer& wifiStaNodes, double radio, uint32_t RngSeed, 
                   const std::string& mobilityType);
void Sta_Information(uint32_t index, uint32_t tosValue,std::string ac,  Ipv4InterfaceContainer StaInterfaces, NetDeviceContainer wifiStaDevices);
// *********************************************************************************
// ************************************* Main **************************************
// *********************************************************************************

int main(int argc, char* argv[]) {
    
    CommandLine cmd(__FILE__);
    
    // Parámetros principales
    cmd.AddValue("nStaWifi", "Total number of WiFi STAs", nStaWifi);
    cmd.AddValue("PacketSize", "Packet size in bytes", PacketSize);
    cmd.AddValue("TimeSimulationMin", "Simulation time in minutes", TimeSimulationMin);
    cmd.AddValue("nCorrida", "Run number", nCorrida);
    cmd.AddValue("RngSeed", "Random seed for simulation", RngSeed);
    cmd.AddValue("category", "Category for file organization", category);
    cmd.AddValue("mobilityType", "Mobility type: yes, no, or mixer", mobilityType);
    
    // nStas por prioridad (si cualquiera >0 -> usamos distribución ordenada y NO iteramos devices)
    cmd.AddValue("nStaH", "Number of High priority STAs (VO)", nStaH);
    cmd.AddValue("nStaM", "Number of Medium priority STAs (VI)", nStaM);
    cmd.AddValue("nStaL", "Number of Low priority STAs (BE)", nStaL);
    cmd.AddValue("nStaNRT", "Number of Non Real Time STAs (BK)", nStaNRT);

    // Parámetros de red
    cmd.AddValue("radio", "Coverage radius in meters", radio);
    cmd.AddValue("port", "Port for UDP communication", port);
    cmd.AddValue("delayBetweenStartsMs", "Delay between STA starts in ms", delayBetweenStartsMs);
    
    // Parámetros de salida/debugging
    cmd.AddValue("enablePcap", "Enable PCAP capture", enablePcap);
    cmd.AddValue("enableAnimation", "Enable animation output", enableAnimation);
    cmd.AddValue("enableXml", "Enable XML flow monitor output", enableXml);
    
    cmd.Parse(argc, argv);
    
    // ========== VALIDAR PARÁMETRO DE MOVILIDAD ==========
    if (mobilityType != "yes" && mobilityType != "no" && mobilityType != "mixer") {
        std::cerr << "Error: mobilityType debe ser 'yes', 'no' o 'mixer'\n";
        return 1;
    }
    
    // ========== CONFIGURACIÓN DE SEMILLA ==========
    Time::SetResolution(Time::NS);
    
    if (RngSeed == 0) {
        RngSeed = time(NULL) + nCorrida * 1000;
    }
    
    SeedManager::SetSeed(RngSeed);
    SeedManager::SetRun(nCorrida);

    uint32_t nStaWifi = nStaH + nStaM + nStaL + nStaNRT;
    
    // ========== MOSTRAR CONFIGURACIÓN ==========
    std::cout << "\n=== "<< category <<" Simulation ===\n";
    std::cout << "STAs: " << nStaWifi << ", PacketSize: " << PacketSize << " bytes\n";
    std::cout << "STAs (H/M/L/NRT): " << nStaH << "/" << nStaM << "/" << nStaL << "/" << nStaNRT << "\n";
    std::cout << "PacketSize: " << PacketSize << " bytes, Time: " << TimeSimulationMin << " min, Corridas: " << (int)nCorrida << "\n";
    std::cout << "Mobility Type: " << mobilityType << "\n";
    std::cout << "===========================================\n";
    
    // ========== CÁLCULOS ==========
    uint32_t MaxPackets = static_cast<uint32_t>((TimeSimulationMin * 60) / interval);
    Time delayBetweenStarts = MilliSeconds(delayBetweenStartsMs);
    double simulationTimeSeconds = TimeSimulationMin * 60.0;
    
    // ========== CREACIÓN DE NODOS ==========
    NodeContainer wifiApNode;
    wifiApNode.Create(1);
    
    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(nStaWifi);
    
    // ========== CONFIGURAR MOVILIDAD ==========
    SetupMobility(wifiStaNodes, radio, RngSeed, mobilityType);
    
    // ========== AP FIJO EN EL CENTRO ==========
    MobilityHelper apMobility;
    apMobility.SetPositionAllocator("ns3::GridPositionAllocator",
                    "MinX", DoubleValue(0.0),
                    "MinY", DoubleValue(0.0),
                    "DeltaX", DoubleValue(0.0),
                    "DeltaY", DoubleValue(0.0),
                    "GridWidth", UintegerValue(1),
                    "LayoutType", StringValue("RowFirst"));
    apMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    apMobility.Install(wifiApNode.Get(0));
    
    // ========== MODELO DE PROPAGACIÓN REALISTA ==========
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n);
    wifi.SetRemoteStationManager("ns3::MinstrelHtWifiManager");
    
    YansWifiPhyHelper wifiPhy;
    // YansWifiChannelHelper wifiChannel  = YansWifiChannelHelper::Default();
    YansWifiChannelHelper wifiChannel;
    
    if (model_realist)
        {
            // Configuración del canal con modelo realista
            wifiChannel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
            
            // 2. Modelo de pérdida por distancia (Se agrega primero)
            wifiChannel.AddPropagationLoss("ns3::LogDistancePropagationLossModel",
                                        "Exponent", DoubleValue(3.0),
                                        "ReferenceLoss", DoubleValue(46.6777));

            // 3. Modelo de desvanecimiento Nakagami (Se encadena automáticamente al anterior)
            wifiChannel.AddPropagationLoss("ns3::NakagamiPropagationLossModel");
        }

    wifiPhy.SetChannel(wifiChannel.Create());

    // Configuración realista para 802.11n en 5GHz
    wifiPhy.Set("ChannelSettings", StringValue("{36, 20, BAND_5GHZ, 0}"));
    
    if (model_realist)
        {
            wifiPhy.Set("TxPowerStart", DoubleValue(16.0));
            wifiPhy.Set("TxPowerEnd", DoubleValue(16.0));

            // Agregar try-catch para compatibilidad
            try {
                wifiPhy.Set("RxNoiseFigure", DoubleValue(7.0));
                wifiPhy.Set("RxGain", DoubleValue(0.0));
                wifiPhy.Set("TxGain", DoubleValue(0.0));
                wifiPhy.Set("CcaEdThreshold", DoubleValue(-62.0));
            } catch (std::exception& e) {
                std::cout << "Nota: Algunos atributos no disponibles en esta versión de NS3\n";
            }
        }
        
    WifiMacHelper wifiMac;
    Ssid ssid = Ssid("NO_SDWN_NS3");
    
    // ========== CONFIGURACIÓN AP ==========
    NetDeviceContainer wifiApDevice;
    wifiMac.SetType("ns3::ApWifiMac", 
                    "Ssid", SsidValue(ssid),
                    "QosSupported", BooleanValue(false),
                    "BeaconInterval", TimeValue(MicroSeconds(102400)));  // Intervalo beacon estándar
    wifiApDevice = wifi.Install(wifiPhy, wifiMac, wifiApNode.Get(0));
    
    // ========== CONFIGURACIÓN STAs ==========
    NetDeviceContainer wifiStaDevices;
    WifiMacHelper staWifiMac;
    staWifiMac.SetType("ns3::StaWifiMac",
                       "Ssid", SsidValue(ssid),
                       "ActiveProbing", BooleanValue(false),
                       "QosSupported", BooleanValue(false));
    wifiStaDevices = wifi.Install(wifiPhy, staWifiMac, wifiStaNodes);
    
    // ========== STACK DE INTERNET ==========
    InternetStackHelper internet;
    internet.Install(wifiApNode);
    internet.Install(wifiStaNodes);
    
    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer staInterfaces = address.Assign(wifiStaDevices);
    Ipv4InterfaceContainer apInterface = address.Assign(wifiApDevice);
    
    // ========== APLICACIONES ==========
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApp = echoServer.Install(wifiApNode.Get(0));
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(Seconds(simulationTimeSeconds + 60));
    
    ApplicationContainer clientApps;
    Time startTime = Seconds(1.0);
    
    for (uint32_t i = 0; i < wifiStaNodes.GetN(); ++i) {

        UdpEchoClientHelper echoClient(apInterface.GetAddress(0), port);
        echoClient.SetAttribute("MaxPackets", UintegerValue(MaxPackets));
        echoClient.SetAttribute("Interval", TimeValue(Seconds(interval)));
        echoClient.SetAttribute("PacketSize", UintegerValue(PacketSize));
        
        ApplicationContainer app = echoClient.Install(wifiStaNodes.Get(i));
        app.Start(startTime);
        app.Stop(Seconds(simulationTimeSeconds + 60));
        clientApps.Add(app);
        
        // Delay fijo entre inicio de STAs
        startTime += delayBetweenStarts;

        //Sta_Information(i, staInterfaces, wifiStaDevices);
    }
    
    // ========== MONITOR DE FLUJO ==========
    Ptr<FlowMonitor> flowMonitor;
    FlowMonitorHelper flowHelper;
    flowMonitor = flowHelper.InstallAll();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowHelper.GetClassifier());
    
    // ========== ANIMACIÓN (OPCIONAL) ==========
    if (enableAnimation) {
        const std::string packetsize = std::to_string(PacketSize);
        const std::string filepath_anim = "scratch/Estadisticas/" + category + "/1S/" + packetsize + "/" + std::to_string(nStaWifi) + "/animation/";
        fs::create_directories(filepath_anim);
        
        const std::string filename_anim = "/NO_SDWN_" + std::to_string(nStaWifi) + "STA_" + 
                                         packetsize + "B_Run" + std::to_string(nCorrida) +
                                         "_Mobility" + mobilityType +
                                         "_Seed" + std::to_string(RngSeed) + ".xml";
        
        AnimationInterface anim(filepath_anim + filename_anim);
        anim.EnablePacketMetadata(true);
        anim.UpdateNodeDescription(0, "Access Point");
        anim.SetConstantPosition(wifiApNode.Get(0), 0.0, 0.0);
        anim.UpdateNodeColor(0, 0, 255, 0);  // AP en verde
        
        std::cout << "Animation enabled: " << filepath_anim + filename_anim << "\n";
    }
    
    // ========== PCAP (OPCIONAL) ==========
    if (enablePcap) {
        const std::string packetsize = std::to_string(PacketSize);
        const std::string filepath_pcap = "scratch/Estadisticas/" + category + "/1S/" + packetsize + "/" + std::to_string(nStaWifi) + "/pcap/";
        fs::create_directories(filepath_pcap);
        
        const std::string filename_pcap = "/NO_SDWN_" + std::to_string(nStaWifi) + "STA_" + 
                                         packetsize + "B_Run" + std::to_string(nCorrida) +
                                         "_Mobility" + mobilityType +
                                         "_Seed" + std::to_string(RngSeed);
        
        wifiPhy.EnablePcap(filepath_pcap + filename_pcap + "_AP", wifiApDevice.Get(0));
        
        std::cout << "PCAP capture enabled\n";
    }
    
    // ========== EJECUCIÓN ==========
    std::cout << "\n=== Iniciando simulación ===\n";
    Simulator::Stop(Seconds(simulationTimeSeconds + 90));
    Simulator::Run();
    
    // ========== XML OUTPUT (OPCIONAL) ==========
    if (enableXml) {
        const std::string packetsize = std::to_string(PacketSize);
        const std::string filepath_xml = "scratch/Estadisticas/" + category + "/1S/" + packetsize + "/" + std::to_string(nStaWifi) + "/xml/";
        fs::create_directories(filepath_xml);
        
        const std::string filename_xml = "/NO_SDWN_" + std::to_string(nStaWifi) + "STA_" + 
                                        packetsize + "B_Run" + std::to_string(nCorrida) +
                                        "_Mobility" + mobilityType +
                                        "_Seed" + std::to_string(RngSeed) + ".xml";
        
        flowMonitor->SerializeToXmlFile(filepath_xml + filename_xml, true, true);
        std::cout << "XML output saved: " << filepath_xml + filename_xml << "\n";
    }
    
    // ========== ANÁLISIS DE RESULTADOS ==========
    AnalyzeFlowMonitorResults(flowMonitor, classifier, nStaWifi, category, std::to_string(PacketSize), nCorrida, CwMinH, CwMaxH, CwMinM, CwMaxM, CwMinL, CwMaxL, CwMinNRT, CwMaxNRT, nStaH, nStaM, nStaL, nStaNRT);

    Simulator::Destroy();
    
    return 0;
}

// *********************************************************************************
// ***************************** FUNCIÓN AUXILIAR DE MOVILIDAD *********************
// *********************************************************************************

void SetupMobility(NodeContainer& wifiStaNodes, double radio, uint32_t RngSeed, 
                   const std::string& mobilityType) {
    
    uint32_t fixedSeed = 42; 
    double Radio = 0.6 * radio;
    // Margen de seguridad para evitar que NS_ASSERT falle por precisión decimal
    double epsilon = 0.1; 
    
    Ptr<UniformRandomVariable> randVar = CreateObject<UniformRandomVariable>();
    randVar->SetStream(fixedSeed);
    
    MobilityHelper mobility;

    if (mobilityType == "no") {
        std::cout << "Configurando las STAs como fijas (Disco)\n";
        mobility.SetPositionAllocator("ns3::UniformDiscPositionAllocator",
                  "rho", DoubleValue(Radio),
                  "X", DoubleValue(0.0),
                  "Y", DoubleValue(0.0));
        mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
        mobility.Install(wifiStaNodes);
        
    } else if (mobilityType == "yes") {
        std::cout << "Configurando las STAs como moviles (Caminata Humana)\n";
        
        mobility.SetPositionAllocator("ns3::UniformDiscPositionAllocator",
                  "rho", DoubleValue(Radio),
                  "X", DoubleValue(0.0),
                  "Y", DoubleValue(0.0));
        
        // CORRECCIÓN: Expandimos los Bounds ligeramente (+ epsilon)
        mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                    "Bounds", RectangleValue(Rectangle(-Radio - epsilon, Radio + epsilon, -Radio - epsilon, Radio + epsilon)),
                    "Distance", DoubleValue(1.5),
                    "Speed", StringValue("ns3::UniformRandomVariable[Min=1.0|Max=1.3]"),
                    "Time", TimeValue(Seconds(2.0)),
                    "Mode", StringValue("Time"));
        
        mobility.Install(wifiStaNodes);
        
    } else if (mobilityType == "mixer") {
        uint32_t totalNodes = wifiStaNodes.GetN();
        uint32_t maxMobileNodes = totalNodes * 0.2; 
        uint32_t mobileCount = 0;
        
        std::cout << "Configurando mezcla: Máximo " << maxMobileNodes << " móviles (20%)\n";

        for (uint32_t i = 0; i < totalNodes; ++i) {
            if (mobileCount < maxMobileNodes) {
                mobileCount++;
                MobilityHelper mobileMobility;
                mobileMobility.SetPositionAllocator("ns3::UniformDiscPositionAllocator",
                          "rho", DoubleValue(Radio), 
                          "X", DoubleValue(0.0),
                          "Y", DoubleValue(0.0));
                
                // CORRECCIÓN: Expandimos los Bounds aquí también
                mobileMobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                                      "Bounds", RectangleValue(Rectangle(-Radio - epsilon, Radio + epsilon, -Radio - epsilon, Radio + epsilon)),
                                      "Distance", DoubleValue(1.0),
                                      "Speed", StringValue("ns3::UniformRandomVariable[Min=1.0|Max=1.3]"),
                                      "Time", TimeValue(Seconds(2.0)));
                
                mobileMobility.Install(wifiStaNodes.Get(i));
            } else {
                MobilityHelper fixedMobility;
                fixedMobility.SetPositionAllocator("ns3::UniformDiscPositionAllocator",
                          "rho", DoubleValue(Radio),
                          "X", DoubleValue(0.0),
                          "Y", DoubleValue(0.0));
                
                fixedMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
                fixedMobility.Install(wifiStaNodes.Get(i));
            }
        }
    } else {
        NS_FATAL_ERROR("Error: mobilityType '" << mobilityType << "' no es válido.");
    }
}


// *********************************************************************************
// ***************************** FUNCIÓN DE ANÁLISIS *******************************
// *********************************************************************************

void AnalyzeFlowMonitorResults(Ptr<FlowMonitor> flowMonitor, Ptr<Ipv4FlowClassifier> classifier, uint32_t nStaWifi, std::string category, std::string packetsize, uint8_t nCorrida, uint32_t CwMinH, uint32_t CwMaxH, uint32_t CwMinM, uint32_t CwMaxM, uint32_t CwMinL, uint32_t CwMaxL, uint32_t CwMinNRT, uint32_t CwMaxNRT, uint32_t nStaH, uint32_t nStaM, uint32_t nStaL, uint32_t nStaNRT) {
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
    const std::string filepath_statistics = "scratch/Estadisticas/" + category + "/1S/" + packetsize+"/"+ std::to_string(nStaWifi)+"/" ;
    fs::create_directories(filepath_statistics);

    const std::string filename =  category + "_" +std::to_string(nStaWifi) + "STA_" + packetsize + "B_" +"CWMin(" + std::to_string(CwMinH) + "-" + std::to_string(CwMinM)+ "-" + std::to_string(CwMinL) + "-" + std::to_string(CwMinNRT) + ")" +"_CWMax(" + std::to_string(CwMaxH) + "-" + std::to_string(CwMaxM)+ "-" + std::to_string(CwMaxL) + "-" + std::to_string(CwMaxNRT) + ")" + "_Mobility_" + mobilityType + "_Run" + std::to_string(nCorrida);

    const std::string csvFilename = filepath_statistics + filename + ".csv";        

    std::ofstream csvFile(csvFilename);
    if (!csvFile.is_open()) {
        std::cerr << "Error: no se pudo abrir el archivo CSV: " << csvFilename << std::endl;
        return;
    }

    // Encabezados CSV
    csvFile << "FlowID,Packet Size,nStaWifi,nStaH,nStaM,nStaL,nStaNRT,CWminH,CWmaxH,CWminM,CWmaxM,CWminL,CWmaxL,CWminNRT,CWmaxNRT,SourceAddress,DestAddress,Throughput(Kbps),Delay(ms),LostPackets,SentPackets,ReceivedPackets\n";

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
                         1000; // Kbps
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
                << packetsize << ","
                << nStaWifi << ","
                << nStaH << ","
                << nStaM << ","
                << nStaL << ","
                << nStaNRT << ","
                << CwMinH << ","
                << CwMaxH << ","
                << CwMinM << ","
                << CwMaxM << ","
                << CwMinL << ","
                << CwMaxL << ","
                << CwMinNRT << ","
                << CwMaxNRT << ","
                << flowClass.sourceAddress << ","
                << flowClass.destinationAddress << ","
                << std::fixed << std::setprecision(2) << throughput << ","
                << delay * 1000 << ","
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
