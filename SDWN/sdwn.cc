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
#include <filesystem>

namespace fs = std::filesystem;
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
            if (tos >= 0xe0) { // Voz (AC_VO)
				mod.priority = HIGH;
				mod.txopLimit = 1504; // Estándar ~1.5ms
			} else if (tos >= 0xa0) { // Video (AC_VI)
				mod.priority = MEDIUM;
				mod.txopLimit = 3008; // Estándar ~3ms
			} else {
				mod.priority = LOW;
				mod.txopLimit = 0;    // Best Effort
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

// Declaraciones externas antes de la clase PoFiAp
// ===================== Declaraciones externas =====================
extern uint32_t CwMinH;
extern uint32_t CwMaxH;
extern uint32_t CwMinM;
extern uint32_t CwMaxM;
extern uint32_t CwMinL;
extern uint32_t CwMaxL;
extern uint32_t CwMinNRT;
extern uint32_t CwMaxNRT;
// ==================================================================


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
            //PoFiApStats();
            NS_LOG_INFO("[PoFiAp] Stopping application");
            if (m_socket) {
                m_socket->Close();
            }
        }
        
        void PrintRoutingTable() {
            std::cout << "Tabla de Enrutamiento del Nodo AP:" << std::endl;
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

		std::map<std::string, EdcaConfig> edcaParams {
    		{"VO", {2, CwMinH, CwMaxH, 8192}},
    		{"VI", {2, CwMinM, CwMaxM, 16384}},
    		{"BE", {3, CwMinL, CwMaxL, 32768}},
    		{"BK", {7, CwMinNRT, CwMaxNRT, 65535}}
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
            
            /*NS_LOG_INFO("[PoFiAp] Packet in " << queueType << " QUEUE from " << item.sender 
                       << " arrived at " << item.arrivalTime.GetSeconds() << "s");*/
            
            if (!isProcessing) {
                isProcessing = true;
                Simulator::Schedule(MilliSeconds(1), &PoFiAp::ProcessQueue, this);
            }
        }
        
        void ProcessQueue() {
            /*NS_LOG_INFO("[PoFiAp] HIGH QUEUE " << highPriorityQueue.size()
                         << " MEDIUM QUEUE " << mediumPriorityQueue.size()
                         << " LOW QUEUE " << lowPriorityQueue.size());*/
            
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
        
            // 1. Configurar TXOP
            ConfigureEdca(entry.priority, entry.txopLimit);
        
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
        
        void ConfigureEdca(PoFiController::Priority priority, uint32_t txopMicroSeconds) {
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
            const std::string category = "BE+BK+VI+VO";
            const std::string packetsize = "1024";
        	const std::string filepath = "scratch/Finals/estadisticas/" + category + "/10S-5S-1S/Modified/" + packetsize;
			fs::create_directories(filepath);
            // system(("mkdir -p " + filepath).c_str());
			const std::string csvFilename = filepath + "/SDWN_NS3_PoFiAp_"+ category + "_Priority_" + std::to_string(numStas) + "_DEVICES_" +  packetsize +"_PacketSize_10_Min_Modified.csv";
            

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
uint32_t nStaWifi = 10;             // Number of WiFi STAs
uint32_t PacketSize = 512;          // Packet size in bytes
double radio = 50.0;                // Coverage radius in meters
uint16_t port = 8080;               // Port for UDP communication
double TimeSimulationMin = 10.0;    // Simulation time in minutes
uint32_t nCorrida = 1;              // Run number
bool enablePcap = false;            // Enable PCAP capture
bool enableAnimation = false;       // Enable animation output
bool enableXml = false;             // Enable XML flow monitor output
double delayBetweenStartsMs = 25.0; // Delay between STA starts in ms
std::string category = "SDWN_EDCA"; // Category for file organization
uint32_t RngSeed = 0;               // Random seed for simulation
std::string mobilityType = "mixer"; // "yes", "no", o "mixer"
bool model_realist = true;          // Modelado realista

uint32_t nStaH = 3;                 // Number of High priority STAs (VO)
uint32_t nStaM = 3;                 // Number of Medium priority STAs (VI)
uint32_t nStaL = 2;                 // Number of Low priority STAs (BE)
uint32_t nStaNRT = 2;               // Number of Non Real Time STAs (BK)

uint8_t		FrameRetryLimit	= 7;                // WiFi Frame Retry Limit
bool 		RentryPackets 	= false;            // Enable packet retry
std::string	FragmentationThreshold = "2200";    // WiFi fragmentation threshold

        

std::string AC;                                                     // Access Category
std::vector<uint32_t> IntervalValues= {1,  1,  1,  1 };             // Interval between packets in seconds per AC
std::vector<std::string> AcValues   = {"BK", "BE", "VI", "VO"};     // Access Categories
std::vector<uint32_t> TosValues     = {0x20, 0x60, 0xa0, 0xe0};     // ToS values per AC
std::map<std::string, uint32_t> ACIndex = {{"BK", 0}, {"BE", 1}, {"VI", 2}, {"VO", 3}};     // AC to index mapping

// Variables globales para saber qué valores se están usando en la corrida
uint32_t CwMinH = 3;                // CWmin High priority (VO)
uint32_t CwMaxH = 7;                // CWmax High priority (VO)
uint32_t CwMinM = 7;                // CWmin Medium priority (VI)
uint32_t CwMaxM = 15;               // CWmax Medium priority (VI)
uint32_t CwMinL = 15;               // CWmin Low priority (BE)
uint32_t CwMaxL = 1023;             // CWmax Low priority (BE)
uint32_t CwMinNRT = 15;             // CWmin NRT priority (BK)
uint32_t CwMaxNRT = 1023;           // CWmax NRT priority (BK)


struct EdcaConfig {                 // Estructura para configuración EDCA
    uint32_t aifsn;
    uint32_t cwMin;
    uint32_t cwMax;
    uint32_t ampduSize;
};


// *********************************************************************************
// *********************************** Functions ***********************************
// *********************************************************************************
void AnalyzeFlowMonitorResults(Ptr<FlowMonitor>, Ptr<Ipv4FlowClassifier>, 
                            uint32_t , std::string , std::string , 
                            uint8_t, uint32_t , uint32_t, uint32_t , 
                            uint32_t , uint32_t , uint32_t , uint32_t , uint32_t, uint32_t, uint32_t, uint32_t,uint32_t);
void Sta_Information(uint32_t index, uint32_t tosValue,std::string ac,  Ipv4InterfaceContainer StaInterfaces, NetDeviceContainer wifiStaDevices);
void SetupMobility(NodeContainer& wifiStaNodes, double radio, uint32_t RngSeed, 
                   const std::string& mobilityType);
// *********************************************************************************
// ************************************* Main **************************************
// *********************************************************************************


int main(int argc, char* argv[]) {
    
    // ---------- Parseo de línea de comandos ----------
    CommandLine cmd(__FILE__);

    // parámetros generales
    cmd.AddValue("nStaWifi", "Total number of WiFi STAs", nStaWifi);
    cmd.AddValue("PacketSize", "Packet size in bytes", PacketSize);
    cmd.AddValue("TimeSimulationMin", "Simulation time in minutes", TimeSimulationMin);
    cmd.AddValue("nCorrida", "Number of runs per scenario", nCorrida);
    cmd.AddValue("RngSeed", "Random seed for simulation", RngSeed);
    cmd.AddValue("category", "Category for file organization", category);
    cmd.AddValue("mobilityType", "Mobility type: yes, no, or mixer", mobilityType);

    // nStas por prioridad (si cualquiera >0 -> usamos distribución ordenada y NO iteramos devices)
    cmd.AddValue("nStaH", "Number of High priority STAs (VO)", nStaH);
    cmd.AddValue("nStaM", "Number of Medium priority STAs (VI)", nStaM);
    cmd.AddValue("nStaL", "Number of Low priority STAs (BE)", nStaL);
    cmd.AddValue("nStaNRT", "Number of Non Real Time STAs (BK)", nStaNRT);

    // CW por prioridad (pasarlos por parámetro)
    cmd.AddValue("CwMinH", "CWmin High priority (VO)", CwMinH);
    cmd.AddValue("CwMaxH", "CWmax High priority (VO)", CwMaxH);
    cmd.AddValue("CwMinM", "CWmin Medium priority (VI)", CwMinM);
    cmd.AddValue("CwMaxM", "CWmax Medium priority (VI)", CwMaxM);
    cmd.AddValue("CwMinL", "CWmin Low priority (BE)", CwMinL);
    cmd.AddValue("CwMaxL", "CWmax Low priority (BE)", CwMaxL);
    cmd.AddValue("CwMinNRT", "CWmin NRT priority (BK)", CwMinNRT);
    cmd.AddValue("CwMaxNRT", "CWmax NRT priority (BK)", CwMaxNRT);

    // parámetros de red
    cmd.AddValue("port", "Port for PoFiAp", port);
    cmd.AddValue("radio", "Coverage radius", radio);
    cmd.AddValue("delayBetweenStartsMs", "Delay between STA starts in ms", delayBetweenStartsMs);
    
    cmd.AddValue("FrameRetryLimit", "WiFi Frame Retry Limit", FrameRetryLimit);
    cmd.AddValue("RentryPackets", "Enable packet retry", RentryPackets);
    cmd.AddValue("FragmentationThreshold", "WiFi fragmentation threshold", FragmentationThreshold);

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

    // ========== MOSTRAR CONFIGURACIÓN ==========
    std::cout << "\n=== "<< category <<" Simulation ===\n";
    std::cout << "STAs: " << nStaWifi << ", PacketSize: " << PacketSize << " bytes\n";
    std::cout << "STAs (H/M/L/NRT): " << nStaH << "/" << nStaM << "/" << nStaL << "/" << nStaNRT << "\n";
    std::cout << "CWmin(H/M/L/NRT): " << CwMinH << "/" << CwMinM << "/" << CwMinL << "/" << CwMinNRT << "\n";
    std::cout << "CWmax(H/M/L/NRT): " << CwMaxH << "/" << CwMaxM << "/" << CwMaxL << "/" << CwMaxNRT << "\n";
    std::cout << "PacketSize: " << PacketSize << " bytes, Time: " << TimeSimulationMin << " min, Corridas: " << (int)nCorrida << "\n";
    std::cout << "Mobility Type: " << mobilityType << "\n";
    std::cout << "===========================================\n";

    // ========== HABILITAR LOGS ==========
    LogComponentEnableAll(LOG_PREFIX_TIME);
    LogComponentEnable("SDWN_PoFi_NS3", LOG_LEVEL_INFO);

    uint32_t nStaWifi = nStaH + nStaM + nStaL + nStaNRT;
    Time delayBetweenStarts = MilliSeconds(delayBetweenStartsMs);

    //========== CONFIGURACION DE EDCA POR AC ==========
    std::map<std::string, PoFiAp::EdcaConfig> edcaParams = {
        {"VO", {2, CwMinH, CwMaxH, 8192}},
        {"VI", {2, CwMinM, CwMaxM, 16384}},
        {"BE", {3, CwMinL, CwMaxL, 32768}},
        {"BK", {7, CwMinNRT, CwMaxNRT, 65535}}
    };
    
    std::cout << "Corrida " << nCorrida
              << " con " << nStaWifi<< " dispositivos "
              << "CWmin(H/M/L/NRT)=(" << CwMinH << "/" << CwMinM << "/" << CwMinL << "/" << CwMinNRT << ") "
              << "CWmax(H/M/L/NRT)=(" << CwMaxH << "/" << CwMaxM << "/" << CwMaxL << "/" << CwMaxNRT << ")"
              << ", PacketSize=" << PacketSize
              << std::endl;

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
                    "DeltaX", DoubleValue(1.0),
                    "DeltaY", DoubleValue(1.0),
                    "GridWidth", UintegerValue(1),
                    "LayoutType", StringValue("RowFirst"));
    apMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    apMobility.Install(wifiApNode.Get(0));

    // ========== CONFIGURAR DISPOSITIVOS WIFI ==========
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n);
    wifi.SetRemoteStationManager("ns3::MinstrelHtWifiManager");

    YansWifiPhyHelper wifiPhy;
    //YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    YansWifiChannelHelper wifiChannel;

    if (model_realist){
        
        // Configuración del canal con modelo realista
        wifiChannel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
        
        // Modelo de pérdida por distancia
        wifiChannel.AddPropagationLoss("ns3::LogDistancePropagationLossModel",
                                    "Exponent", DoubleValue(3.0),
                                    "ReferenceLoss", DoubleValue(46.6777));

        // Modelo de desvanecimiento Nakagami 
        wifiChannel.AddPropagationLoss("ns3::NakagamiPropagationLossModel");
    }
    

    wifiPhy.SetChannel(wifiChannel.Create());

    // Configuración realista para 802_11 en 5GHz
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
    

    if (RentryPackets) {
        Config::SetDefault("ns3::WifiMac::FrameRetryLimit", UintegerValue(FrameRetryLimit));
        Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue("0"));
        Config::SetDefault("ns3::WifiRemoteStationManager::FragmentationThreshold", StringValue(FragmentationThreshold));
    }

    WifiMacHelper wifiMac;
    Ssid ssid = Ssid("SDWN_PoFi_NS3");

    // ========== CONFIGURACIÓN AP ==========
    NetDeviceContainer wifiApDevice;
    wifiMac.SetType("ns3::ApWifiMac",
                    "Ssid", SsidValue(ssid),
                    "QosSupported", BooleanValue(true),
                    "BeaconInterval", TimeValue(MicroSeconds(102400)));
    wifiApDevice = wifi.Install(wifiPhy, wifiMac, wifiApNode.Get(0));

    // ========== CONFIGURACIÓN STAs ==========
    NetDeviceContainer wifiStaDevices;
    WifiMacHelper staWifiMac;
    for (uint32_t i = 0; i < nStaWifi; ++i) {
        if (i < nStaH) AC = "VO";
        else if (i < nStaH + nStaM) AC = "VI";
        else if (i < nStaH + nStaM + nStaL) AC = "BE";
        else AC = "BK";

        auto cfg = edcaParams[AC];

        staWifiMac.SetType("ns3::StaWifiMac",
                           "Ssid", SsidValue(ssid),
                           "ActiveProbing", BooleanValue(false),
                           "QosSupported", BooleanValue(true),
                           AC + "_MaxAmpduSize", UintegerValue(cfg.ampduSize));                     

        NetDeviceContainer staDev = wifi.Install(wifiPhy, staWifiMac, wifiStaNodes.Get(i));
        wifiStaDevices.Add(staDev.Get(0));

        //========== CONFIGURAR EDCA EN STAs ==========
        Ptr<WifiNetDevice> wifiDevice = DynamicCast<WifiNetDevice>(staDev.Get(0));
        Ptr<WifiMac> wifiMacPtr = wifiDevice->GetMac();
        PointerValue ptr;
        wifiMacPtr->GetAttribute(AC + "_Txop", ptr);
        Ptr<QosTxop> edca = ptr.Get<QosTxop>();
        if (edca) {
            edca->SetAifsn(cfg.aifsn);
            edca->SetMinCw(cfg.cwMin);
            edca->SetMaxCw(cfg.cwMax);
        }
    }

    // ========== STACK DE INTERNET ==========
    InternetStackHelper internet;
    internet.Install(wifiApNode);
    internet.Install(wifiStaNodes);

    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer staInterfaces = address.Assign(wifiStaDevices);
    Ipv4InterfaceContainer apInterface = address.Assign(wifiApDevice);

    // ========== APLICACIONES ==========
    PoFiApHelper pofiHelper(port);
    ApplicationContainer pofiApps = pofiHelper.Install(wifiApNode);
    pofiApps.Start(Seconds(0.0));
    pofiApps.Stop(Minutes(TimeSimulationMin +1.5));

    ApplicationContainer clientApps;
    Time startTime = Seconds(1.0);

    for (uint32_t i = 0; i < wifiStaNodes.GetN(); ++i) {
        
        if (i < nStaH) AC = "VO";
        else if (i < nStaH + nStaM) AC = "VI";
        else if (i < nStaH + nStaM + nStaL) AC = "BE";
        else AC = "BK";

        uint32_t tosValue = TosValues[ACIndex[AC]];
        uint32_t interval = IntervalValues[ACIndex[AC]];
        
        uint32_t MaxPackets = TimeSimulationMin * 60 / interval;

        UdpEchoClientHelper echoClient(apInterface.GetAddress(0), port);
        echoClient.SetAttribute("MaxPackets", UintegerValue(MaxPackets));
        echoClient.SetAttribute("Interval", TimeValue(Seconds(interval)));
        echoClient.SetAttribute("PacketSize", UintegerValue(PacketSize));
        echoClient.SetAttribute("Tos", UintegerValue(tosValue));

        ApplicationContainer app = echoClient.Install(wifiStaNodes.Get(i));
        app.Start(startTime);
        app.Stop(Minutes(TimeSimulationMin + 1));
        clientApps.Add(app);

        // Incrementar el tiempo de inicio para la próxima STA
        startTime += delayBetweenStarts;

        Sta_Information(i, tosValue, AC, staInterfaces, wifiStaDevices);
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
        
        const std::string filename_anim = "/SDWN_" + std::to_string(nStaWifi) + "STA_" + 
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
    if (enablePcap){

        const std::string pcapBasePath = "scratch/Estadisticas/" + category + "/1S/" + std::to_string(nStaWifi) + "/" + std::to_string(PacketSize) + "/pcap/";
        fs::create_directories(pcapBasePath);

        // Nombres específicos para diferentes interfaces
        const std::string pcapPrefix = category + "_" + std::to_string(nStaWifi) + "STA_" + 
                                    std::to_string(PacketSize) + "B_" +
                                    "CWMin(" + std::to_string(CwMinH) + "-" + std::to_string(CwMinM) + 
                                    "-" + std::to_string(CwMinL) + "-" + std::to_string(CwMinNRT) + ")" +
                                    "_CWMax(" + std::to_string(CwMaxH) + "-" + std::to_string(CwMaxM) + 
                                    "-" + std::to_string(CwMaxL) + "-" + std::to_string(CwMaxNRT) + ")" +
                                    "_Run" + std::to_string(nCorrida);
        wifiPhy.EnablePcap(pcapBasePath + pcapPrefix + "_sta", wifiStaNodes);
        wifiPhy.EnablePcap(pcapBasePath + pcapPrefix + "_wifi", wifiApNode);
    }
    
    // ========== EJECUCIÓN ==========
    std::cout << "\n=== Iniciando simulación ===\n";
    Simulator::Stop(Minutes(TimeSimulationMin + 1.5));
    Simulator::Run();

    // ========== XML OUTPUT (OPCIONAL) ==========
    if (enableXml) {
        const std::string packetsize = std::to_string(PacketSize);
        const std::string filepath_xml = "scratch/Estadisticas/" + category + "/1S/" + packetsize + "/" + std::to_string(nStaWifi) + "/xml/";
        fs::create_directories(filepath_xml);
        
        const std::string filename_xml = category + "_" + std::to_string(nStaWifi) + "STA_" + 
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
