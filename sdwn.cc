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
#include "ns3/log.h"


#include <iomanip>
#include <cstdlib>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("SDWN_PoFi_NS3"); 


// *********************************************************************************
// ********************************* PoFiController ********************************
// *********************************************************************************
class PoFiController {
    public:
        enum Priority { HIGH, MEDIUM, LOW };
    
        struct FlowMod {
            Priority priority;
            uint32_t txopLimit;
        };
    
        FlowMod PacketIn(uint8_t tos, Ipv4Address staIp) {
            NS_LOG_INFO("[PoFiController] Received PacketIn from PoFiAp (Station: " 
                << staIp << ", ToS: 0x" << std::hex << uint32_t(tos) << ")");
    
            FlowMod mod;
            if (tos > 190) {
                mod.priority = HIGH;
                mod.txopLimit = 320;
            } else if ( tos >=  128 && tos <= 190) {
                mod.priority = MEDIUM;
                mod.txopLimit = 640;
            } else {
                mod.priority = LOW;
                mod.txopLimit = 960;
            }
    
            NS_LOG_INFO("[PoFiController] Send FlowMod to PoFiAp (Station: " << staIp << ", ToS: 0x" << std::hex << uint32_t(tos) 
                         << ", Priority:  " << (mod.priority == HIGH ? "HIGH )" :
                                            mod.priority == MEDIUM ? "MEDIUM )" : "LOW )")) ;
            return mod;
        }
    };
// *********************************************************************************
// ************************************* PoFiAp ************************************
// *********************************************************************************
class PoFiAp : public Application {
    public:
        PoFiAp() = default;
        ~PoFiAp() override = default;
        
        void Setup(uint16_t port) {
            m_port = port;
        }
        
        void StartApplication() override {
            NS_LOG_INFO("[PoFiAp] Starting application at port " << m_port);
            TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
            m_socket = Socket::CreateSocket(GetNode(), tid);
            InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), m_port);
            m_socket->Bind(local);
            m_socket->SetRecvCallback(MakeCallback(&PoFiAp::HandleRead, this));
        
            Ptr<Ipv4> ipv4 = GetNode()->GetObject<Ipv4>();
            ipv4->TraceConnectWithoutContext("Rx", MakeCallback(&PoFiAp::Ipv4PacketReceived, this));
            m_apIpv4 = ipv4;
        }
        
        
        void StopApplication() override {
            PoFiApStats();
            NS_LOG_INFO("[PoFiAp] Stopping application");
            if (m_socket) {
                m_socket->Close();
            }
        }
        
        void PrintRoutingTable() {
            std::cout << "AP Node Routing Table:" << std::endl;
            Ptr<OutputStreamWrapper> stream = Create<OutputStreamWrapper>(&std::cout);
            m_apIpv4->GetRoutingProtocol()->PrintRoutingTable(stream, Time::S);
        }
    
        struct QueueItem {
            uint8_t tos;
            Ptr<Packet> packet;
            Ipv4Address sender;
            Time arrivalTime;  
            
            bool operator<(const QueueItem& other) const {
                return tos < other.tos;
            }
        };
        
        struct Metrics {
            uint32_t packetsReceived = 0;
            uint32_t packetsSent = 0;
            uint32_t packetsLost = 0;
            uint64_t bytesReceived = 0;
            uint64_t bytesSent = 0;
            double latencyTotal = 0.0;
            double jitterTotal = 0.0;
        };
        
        std::map<PoFiController::Priority, Metrics> metricsMap;

		struct EdcaConfig {
    		uint32_t aifsn;
    		uint32_t cwMin;
    		uint32_t cwMax;
    		uint32_t ampduSize;
		};

		std::map<std::string, EdcaConfig> edcaParams = {
    		{"VO", {2, 15, 1023, 8192}},
    		{"VI", {2, 15, 1023, 16384}},
    		{"BE", {3, 15, 1023, 32768}},
    		{"BK", {7, 15, 1023, 65535}}
		};

    private:
        Ptr<Socket> m_socket;
        uint16_t m_port;
        Ptr<Ipv4> m_apIpv4;
        
        std::priority_queue<QueueItem> highPriorityQueue;
        std::priority_queue<QueueItem> mediumPriorityQueue;
        std::queue<QueueItem> lowPriorityQueue;
        
        std::map<Ipv4Address, uint8_t> tosMap;
        std::map<uint8_t, PoFiController::FlowMod> tosRegistry;
        bool isProcessing = false;
        
        void Ipv4PacketReceived(Ptr<const Packet> packet, Ptr<Ipv4> ipv4, uint32_t interface) {
            Ipv4Header ipHeader;
            Ptr<Packet> copy = packet->Copy();
            copy->RemoveHeader(ipHeader);
        
            Ipv4Address src = ipHeader.GetSource();
            uint8_t tos = ipHeader.GetTos();
            tosMap[src] = tos;
        
            PoFiController::Priority priority = tosRegistry.count(tos) ? 
                tosRegistry[tos].priority : PoFiController::LOW;
        
            metricsMap[priority].packetsReceived++;
            metricsMap[priority].bytesReceived += packet->GetSize();
        
            NS_LOG_INFO("[PoFiAp] Received Packet from Station: " 
                << src << " with ToS: 0x" << std::hex << uint32_t(tos));
        }
        
        void HandleRead(Ptr<Socket> socket) {
            Address from;
            while (Ptr<Packet> packet = socket->RecvFrom(from)) {
                InetSocketAddress addr = InetSocketAddress::ConvertFrom(from);
                Ipv4Address sender = addr.GetIpv4();
                uint8_t tos = tosMap[sender];
                PoFiController::FlowMod entry; 
        
                if (tosRegistry.find(tos) == tosRegistry.end()) {
                    PoFiController controller;
                    NS_LOG_INFO("[PoFiAp] Send PacketIn to PoFiController with ToS: 0x" << std::hex << uint32_t(tos));
                    tosRegistry[tos] = controller.PacketIn(tos, sender);
                    entry = tosRegistry[tos]; // Asignación dentro del if
                    NS_LOG_INFO("[PoFiAp] Received FlowMod from PoFiController with " 
                        << (entry.priority == 0 ? "HIGH" : 
                           (entry.priority == 1 ? "MEDIUM" : "LOW")) 
                        << " Priority and TxopLimit " 
                        << static_cast<uint32_t>(entry.txopLimit));
                } 
                else {
                    entry = tosRegistry[tos]; // Asignación dentro del else
                }
                 // Crear QueueItem con el tiempo actual
                QueueItem item{tos, packet, sender, Simulator::Now()};
                EnqueuePacket(entry.priority, item); 
            }
        }
        
        void EnqueuePacket(PoFiController::Priority priority, const QueueItem& item) {  // Cambiar parámetros
            std::string queueType;
            
            if (priority == PoFiController::HIGH) {
                highPriorityQueue.push(item);
                queueType = "HIGH";
            } else if (priority == PoFiController::MEDIUM) {
                mediumPriorityQueue.push(item);
                queueType = "MEDIUM";
            } else {
                lowPriorityQueue.push(item);
                queueType = "LOW";
            }
            
            NS_LOG_INFO("[PoFiAp] Packet in " << queueType << " QUEUE from " << item.sender 
                       << " arrived at " << item.arrivalTime.GetSeconds() << "s");
            
            if (!isProcessing) {
                isProcessing = true;
                Simulator::Schedule(MilliSeconds(1), &PoFiAp::ProcessQueue, this);
            }
        }
        
        void ProcessQueue() {
            NS_LOG_INFO("[PoFiAp] HIGH QUEUE " << highPriorityQueue.size()
                         << " MEDIUM QUEUE " << mediumPriorityQueue.size()
                         << " LOW QUEUE " << lowPriorityQueue.size());
            
            if (highPriorityQueue.empty() && mediumPriorityQueue.empty() && lowPriorityQueue.empty()) {
                isProcessing = false;
                return;
            }
            
            QueueItem item;
            if (!highPriorityQueue.empty()) {
                item = highPriorityQueue.top();
                highPriorityQueue.pop();
            } else if (!mediumPriorityQueue.empty()) {
                item = mediumPriorityQueue.top();
                mediumPriorityQueue.pop();
            } else {
                item = lowPriorityQueue.front();
                lowPriorityQueue.pop();
            }
            
            // Pasar el arrivalTime a ForwardPacket
            ForwardPacket(item.packet, item.tos, item.sender, item.arrivalTime);
            Simulator::Schedule(MilliSeconds(1), &PoFiAp::ProcessQueue, this);
        }

        void ForwardPacket(Ptr<Packet> packet, uint8_t tos, Ipv4Address originalSender, Time arrivalTime) {
            PoFiController::FlowMod entry = tosRegistry[tos];
        
            // 1. Configurar TXOP - AÑADIMOS ESTA LÍNEA CLAVE
            ConfigureTxop(entry.priority, entry.txopLimit);
        
            // 2. Configurar socket para el envío
            m_socket->SetIpTos(tos);
            
            // 3. Calcular métricas
            Time now = Simulator::Now();
            Time latency = now - arrivalTime;
            double latencyMs = latency.GetSeconds() * 1000.0;
            
            Metrics& metrics = metricsMap[entry.priority];
            metrics.packetsSent++;
            metrics.bytesSent += packet->GetSize();
            
            if (metrics.packetsSent > 1) {
                double lastLatency = metrics.latencyTotal / (metrics.packetsSent - 1);
                double jitter = std::abs(latencyMs - lastLatency);
                metrics.jitterTotal += jitter;
            }
            metrics.latencyTotal += latencyMs;
            
            NS_LOG_INFO("[PoFiAp] Sending packet to: " << originalSender 
                       << " | ToS: 0x" << std::hex << static_cast<uint32_t>(tos)
                       << " | Size: " << std::dec << packet->GetSize() << " bytes"
                       << " | Latency: " << latencyMs << "ms"
                       << " | TXOP: " << entry.txopLimit << " μs");
            
            m_socket->SendTo(packet, 0, InetSocketAddress(originalSender, m_port));
        }
        
        void ConfigureTxop(PoFiController::Priority priority, uint32_t txopMicroSeconds) {
    		Time txopLimit = MicroSeconds(txopMicroSeconds);

    		Ptr<NetDevice> device = GetNode()->GetDevice(0);
    		Ptr<WifiNetDevice> wifiDevice = DynamicCast<WifiNetDevice>(device);

    		if (!wifiDevice) {
        		NS_LOG_ERROR("[PoFiAp] Device 0 is not a WifiNetDevice!");
        		return;
    		}

    		Ptr<WifiMac> wifiMac = wifiDevice->GetMac();
    		PointerValue ptr;
    		Ptr<QosTxop> edca;

    		std::string ac;
    		switch (priority) {
        		case PoFiController::HIGH:   ac = "VO"; break;
        		case PoFiController::MEDIUM: ac = "VI"; break;
        		case PoFiController::LOW:    ac = "BE"; break;
        		default:                     ac = "BK"; break;
    		}

    		// Obtener configuración desde el mapa
    		EdcaConfig config = edcaParams[ac];

    		// Seleccionar el atributo correcto
    		wifiMac->GetAttribute(ac + "_Txop", ptr);
    		edca = ptr.Get<QosTxop>();

    		if (!edca) {
        		NS_LOG_ERROR("[PoFiAp] EDCA pointer is null for AC: " << ac);
        		return;
    		}

    		// Aplicar configuración
    		edca->SetTxopLimit(txopLimit);
    		edca->SetAifsn(config.aifsn);
    		edca->SetMinCw(config.cwMin);
    		edca->SetMaxCw(config.cwMax);

    		// (Opcional) Log para confirmar configuración
   		 	NS_LOG_INFO("[PoFiAp] Configured " << ac 
                 	<< " with AIFSN=" << config.aifsn 
                 	<< ", CWmin=" << config.cwMin 
                 	<< ", CWmax=" << config.cwMax 
                 	<< ", TXOP=" << txopLimit.GetMicroSeconds() << " μs");
		}


        void PoFiApStats() {
            uint32_t numStas = NodeList::GetNNodes() - 1; // Restamos 1 para excluir el AP
            
            // Nombre del archivo CSV con número de STAs
            const std::string category = "BE";
            const std::string packetsize = "256";
        	const std::string filepath = "scratch/Finals/estadisticas/" + category + "/" + packetsize;
			system(("mkdir -p " + filepath).c_str());
			const std::string csvFilename = filepath + "/SDWN_NS3_PoFiAp_"+ category + "_Priority_" + std::to_string(numStas) + "_DEVICES_" +  packetsize +"_PacketSize_"+ "_30_Min_Original.csv";
            

            // Abrir archivo CSV
            std::ofstream csvFile(csvFilename);
            if (!csvFile.is_open()) {
                NS_LOG_ERROR("No se pudo abrir el archivo CSV: " << csvFilename);
                return;
            }

            // Escribir encabezados del CSV
            csvFile << "Priority,PacketsReceived,PacketsSent,BytesReceived,BytesSent,"
                    << "ThroughputKbps,AvgLatencyMs,AvgJitterMs\n";

            std::map<PoFiController::Priority, std::string> labels = {
                {PoFiController::HIGH, "HIGH"},
                {PoFiController::MEDIUM, "MEDIUM"},
                {PoFiController::LOW, "LOW"}
            };

            for (const auto& [priority, stats] : metricsMap) {
                if (stats.packetsSent == 0) continue;

                // Calcular métricas
                double throughput = (stats.bytesSent * 8.0) / Simulator::Now().GetSeconds() / 1024.0;
                double avgLatency = stats.latencyTotal / stats.packetsSent;
                double avgJitter = (stats.packetsSent > 1) ? (stats.jitterTotal / (stats.packetsSent - 1)) : 0.0;

                // Generar logs
                NS_LOG_INFO("=== " << labels[priority] << " Priority Metrics ===");
                NS_LOG_INFO("Packets Received: " << stats.packetsReceived);
                NS_LOG_INFO("Packets Sent:     " << stats.packetsSent);
                NS_LOG_INFO("Bytes Received:   " << stats.bytesReceived);
                NS_LOG_INFO("Bytes Sent:       " << stats.bytesSent);
                NS_LOG_INFO("Throughput (Kbps): " << std::fixed << std::setprecision(2) << throughput);
                NS_LOG_INFO("Avg Latency (ms):  " << std::fixed << std::setprecision(2) << avgLatency);
                
                if (stats.packetsSent > 1) {
                    NS_LOG_INFO("Avg Jitter (ms):   " << std::fixed << std::setprecision(2) << avgJitter);
                }

                // Escribir en CSV
                csvFile << labels[priority] << ","
                        << stats.packetsReceived << ","
                        << stats.packetsSent << ","
                        << stats.bytesReceived << ","
                        << stats.bytesSent << ","
                        << std::fixed << std::setprecision(2) << throughput << ","
                        << avgLatency << ",";
                
                if (stats.packetsSent > 1) {
                    csvFile << avgJitter;
                } else {
                    csvFile << "0";
                }
                
                csvFile << "\n";
            }

            csvFile.close();
            NS_LOG_INFO("Estadísticas exportadas a CSV: " << csvFilename);
        }
    };
        
// *********************************************************************************
// ********************************* PoFiApHelper **********************************
// *********************************************************************************
class PoFiApHelper {
    public:
        PoFiApHelper(uint16_t port) : m_port(port) {}
    
        void SetPort(uint16_t port) {
            m_port = port;
        }
    
        ApplicationContainer Install(NodeContainer nodes) const {
            ApplicationContainer apps;
            for (uint32_t i = 0; i < nodes.GetN(); ++i) {
                Ptr<PoFiAp> app = CreateObject<PoFiAp>();
                app->Setup(m_port);
                nodes.Get(i)->AddApplication(app);
                apps.Add(app);
            }
            return apps;
        }
    
    private:
        uint16_t m_port;
};

// *********************************************************************************
// ******************************* Global Variables ********************************
// *********************************************************************************
uint8_t     factor      = 10; // 1- 10
double      radio       = 50.0;
uint16_t    port        = 8080;
uint16_t    PacketSize  = 256; // 256,512,1024
bool        enablePcap  = true;
uint16_t    TimeSimulation = 30; //10,20,30

uint8_t		FrameRetryLimit	= 7;
bool 		RentryPackets 	= false;
std::string	FragmentationThreshold = "2200";

std::vector<uint32_t> IntervalValues= { 10,  10,  10,  10,   5,   5,   1,   1};
std::vector<std::string> AcValues   = {"BE", "BK", "BK", "BE", "VI", "VI", "VO", "VO"};
std::vector<uint32_t> TosValues     = {0x00, 0x20, 0x40, 0x60, 0x80, 0xa0, 0xc0, 0xe0};

struct EdcaConfig {
    uint32_t aifsn;
    uint32_t cwMin;
    uint32_t cwMax;
    uint32_t ampduSize;
};

std::map<std::string, EdcaConfig> edcaParams = {
    {"VO", {2, 15, 1023, 8192}},
    {"VI", {2, 15, 1023, 16384}},
    {"BE", {3, 15, 1023, 32768}},
    {"BK", {7, 15, 1023, 65535}}
};

// *********************************************************************************
// *********************************** Functions ***********************************
// *********************************************************************************
void AnalyzeFlowMonitorResults(Ptr<FlowMonitor>, Ptr<Ipv4FlowClassifier>, uint32_t);
void Sta_Information(uint32_t index, uint32_t tosValue,std::string ac,  Ipv4InterfaceContainer StaInterfaces, NetDeviceContainer wifiStaDevices);

// *********************************************************************************
// ************************************* Main **************************************
// *********************************************************************************


int main(int argc, char* argv[]) {

    	CommandLine cmd;
    	cmd.Parse(argc, argv);
    	GlobalValue::Bind("SimulatorImplementationType", StringValue("ns3::RealtimeSimulatorImpl"));

    	Time::SetResolution(Time::NS);
    	LogComponentEnableAll(LOG_PREFIX_TIME); // Agrega timestamp

    	LogComponentEnable("SDWN_PoFi_NS3", LOG_LEVEL_INFO);
    	//LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    	//LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

        // Crear nodos
        uint16_t    nStaWifi    = i * 10;
        std::cout << "Realizando analisis para " << nStaWifi << " Dispositivos." << std::endl;

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
        
        if (RentryPackets){     
            Config::SetDefault ("ns3::WifiMac::FrameRetryLimit", UintegerValue(FrameRetryLimit));
            Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue ("0")); // Forzar RTS/CTS
            Config::SetDefault ("ns3::WifiRemoteStationManager::FragmentationThreshold", StringValue (FragmentationThreshold)); // Desactiva fragmentación
        }

        WifiMacHelper wifiMac;
        Ssid ssid = Ssid("SDWN_PoFi_NS3");

        // Configurar AP
        NetDeviceContainer wifiApDevice;
        wifiMac.SetType("ns3::ApWifiMac", 
                        "Ssid", SsidValue(ssid), 
                        "QosSupported", BooleanValue(true));

        wifiApDevice = wifi.Install(wifiPhy, wifiMac, wifiApNode.Get(0));

        // Configurar estaciones con QoS
        NetDeviceContainer wifiStaDevices;
        WifiMacHelper staWifiMac;

        for (uint32_t i = 0; i < nStaWifi; ++i) {
            std::string AC = AcValues[i % AcValues.size()];
            auto cfg = edcaParams[AC];

            staWifiMac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "QosSupported", BooleanValue(true),
                AC + "_MaxAmpduSize", UintegerValue(cfg.ampduSize),
                "ActiveProbing", BooleanValue(false));

            NetDeviceContainer staDev = wifi.Install(wifiPhy, staWifiMac, wifiStaNodes.Get(i));
            Ptr<NetDevice> device = staDev.Get(0);
            wifiStaDevices.Add(device);
        
            Ptr<WifiNetDevice> wifiDevice = DynamicCast<WifiNetDevice>(device);
            Ptr<WifiMac> wifiMac = wifiDevice->GetMac();

            PointerValue ptr;
            wifiMac->GetAttribute(AC + "_Txop", ptr);
            Ptr<QosTxop> edca = ptr.Get<QosTxop>();

            if (edca) {
                edca->SetAifsn(cfg.aifsn);
                edca->SetMinCw(cfg.cwMin);
                edca->SetMaxCw(cfg.cwMax);
            }
        }

        InternetStackHelper internet;
        internet.Install(wifiApNode);
        internet.Install(wifiStaNodes);

        Ipv4AddressHelper address;
        address.SetBase("192.168.1.0", "255.255.255.0");
        Ipv4InterfaceContainer staInterfaces = address.Assign(wifiStaDevices);
        Ipv4InterfaceContainer apInterface = address.Assign(wifiApDevice);

        // Crear el controlador SDN para el AP
        PoFiApHelper pofiHelper(port);
        ApplicationContainer pofiApps = pofiHelper.Install(wifiApNode);

        pofiApps.Start(Seconds(0.0));
        pofiApps.Stop(Minutes(TimeSimulation));

        ApplicationContainer clientApps;
        Time startTime = Seconds(1.0); // Tiempo inicial de inicio
        Time delayBetweenStarts = MilliSeconds(25); // Retraso entre cada inicio de cliente
        

        for (uint32_t i = 0; i < wifiStaNodes.GetN(); ++i) {
            uint32_t tosValue = TosValues[i % TosValues.size()];
            std::string AC = AcValues[i % AcValues.size()];
            uint32_t interval = IntervalValues[i % IntervalValues.size()];
            uint32_t   MaxPackets = TimeSimulation * 60 / interval;

            
            UdpEchoClientHelper echoClient(apInterface.GetAddress(0), port);
            echoClient.SetAttribute("MaxPackets", UintegerValue(MaxPackets));
            echoClient.SetAttribute("Interval", TimeValue(Seconds(interval)));
            echoClient.SetAttribute("PacketSize", UintegerValue(PacketSize));
            echoClient.SetAttribute("Tos", UintegerValue(tosValue));
            
            ApplicationContainer app = echoClient.Install(wifiStaNodes.Get(i));
            
            app.Start(startTime);
            app.Stop(Minutes(TimeSimulation+1));
            
            clientApps.Add(app);
            startTime += delayBetweenStarts;
            
            //Sta_Information(i, tosValue, AC, staInterfaces, wifiStaDevices);
        }
        
        
        // Añadido para habilitar el monitoreo de flujo
        Ptr<FlowMonitor> flowMonitor;
        FlowMonitorHelper flowHelper;
        flowMonitor = flowHelper.InstallAll();
        Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowHelper.GetClassifier());

        // Animación y rastreo
        const std::string category = "BE"; // Cambiar según la prioridad deseada
        const std::string packetsize = std::to_string(PacketSize);

        const std::string filepath_xml = "scratch/Finals/xml/" + category + "/" + packetsize;
        system(("mkdir -p " + filepath_xml).c_str());
        
        const std::string filename = "/SDWN_NS3_PoFi_"+ category + "_Priority_" + std::to_string(nStaWifi) + "_DEVICES_" +  packetsize +"_PacketSize_"+ std::to_string(TimeSimulation) +"_Min_ORIGINAL";
        AnimationInterface anim(filepath_xml + filename + ".xml");
        

        anim.EnablePacketMetadata(true);
        anim.UpdateNodeDescription(0, "Access Point");

        anim.SetConstantPosition(wifiApNode.Get(0), 0.0, 0.0);

        // Rastreo
        AsciiTraceHelper rastreo_ascii;
        // wifiPhy.EnableAsciiAll(rastreo_ascii.CreateFileStream("scratch/Finals/tracers/BK/SDWN_NS3_"+ std::to_string(PacketSize)+"_PacketSize_"+ std::to_string(nStaWifi) + "_DEVICES.tr"));
        if (enablePcap) {
            const std::string filepath_pcap = "scratch/Finals/pcap/" + category + "/" + packetsize;
            system(("mkdir -p " + filepath_pcap).c_str());
            wifiPhy.EnablePcap(filepath_pcap + filename, wifiApDevice.Get(0));
        }
        
        Simulator::Stop(Minutes(TimeSimulation + 1.5));
        Simulator::Run();

        AnalyzeFlowMonitorResults(flowMonitor, classifier, nStaWifi);
        const std::string filepath_flowmon = "scratch/Finals/flowmon/"+ category + "/" + packetsize;
        system(("mkdir -p " + filepath_flowmon).c_str());
        flowMonitor->SerializeToXmlFile(filepath_flowmon + filename + ".xml", true, true);
        Simulator::Destroy();
        
	return 0;
}

// *********************************************************************************
// ***************************** Implemented Functions *****************************
// *********************************************************************************

void Sta_Information(uint32_t index, uint32_t tosValue, std::string ac, Ipv4InterfaceContainer staInterfaces, NetDeviceContainer wifiStaDevices) {
    // Obtener la dirección IP del nodo
    Ipv4Address ip = staInterfaces.GetAddress(index);

    // Obtener la dirección MAC del nodo
    Ptr<NetDevice> staDevice = wifiStaDevices.Get(index);
    Ptr<WifiNetDevice> wifiStaDevice = DynamicCast<WifiNetDevice>(staDevice);
    ns3::Address macAddr = wifiStaDevice->GetAddress();  
    Mac48Address macAddress = Mac48Address::ConvertFrom(macAddr);  

    Ptr<WifiMac> macLayer = wifiStaDevice->GetMac();
    PointerValue ptr;
    Ptr<QosTxop> edca;
    
    // Configuración de TXOP según la categoría de acceso (AC)
    macLayer->GetAttribute(ac + "_Txop", ptr);
    edca = ptr.Get<QosTxop>();

    // Extraer el valor AC (Access Category) asociado.
    uint32_t acValue = edca->GetAccessCategory();
    std::string  AC; 

    if (acValue == 0)
        AC = "AC_BE";
    else if (acValue == 1)
        AC = "AC_BK";
    else if (acValue == 2)
        AC = "AC_VI";
    else if (acValue == 3)
        AC = "AC_VO";

    // Imprimir los valores
    std::cout << "Device " << index + 1 << " : "
              << "IP: " << ip << ", "
              << "Tos: 0x" << std::hex << tosValue << std::dec << ", "
              << "AC: " << AC << ", "
              << "Max CW: " << edca->GetMaxCw(0) << ", "
              << "Min CW: " << edca->GetMinCw(0) << ", "
              << "Txop Limit: " << edca->GetTxopLimit(0) << ", "
              << "MAC: " << macAddress << std::endl;
}

void AnalyzeFlowMonitorResults(Ptr<FlowMonitor> flowMonitor, Ptr<Ipv4FlowClassifier> classifier, uint32_t nStaWifi) {
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
    const std::string category = "BE";
    const std::string packetsize = "256";
    const std::string filepath_statistics = "scratch/Finals/estadisticas/" + category + "/" + packetsize;
	system(("mkdir -p " + filepath_statistics).c_str());

    const std::string filename = "/SDWN_NS3_PoFiStas_"+ category + "_Priority_" + std::to_string(nStaWifi) + "_DEVICES_" +  packetsize +"_PacketSize_30_Min_Original";
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
