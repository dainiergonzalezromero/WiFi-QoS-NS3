#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/aodv-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/ipv4-routing-helper.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/animation-interface.h"

#include <vector>
#include <map>

using namespace ns3;

// Variables globales
uint32_t nStaWifi = 10;
double radio = 22.0;

// Prototipos
MobilityHelper createscenario(double radio);
MobilityHelper createstarcenter();
WifiMacHelper ConfigureQoS(WifiMacHelper wifiMac, Ssid ssid, std::string ac, UintegerValue maxAmpduSize);
void Sta_Information (uint32_t index, uint32_t tosValue, Ipv4InterfaceContainer StaInterfaces , NetDeviceContainer wifiStaDevices);

// Clase para simular un controlador SDWN
class SdnController {
public:
    void AdjustQoS(Ptr<WifiNetDevice> device, std::string ac) {
        Ptr<WifiMac> mac = device->GetMac();
        PointerValue ptr;
        Ptr<QosTxop> edca;

        if (ac == "VO") {
            mac->GetAttribute("VO_Txop", ptr);
            edca = ptr.Get<QosTxop>();
            edca->SetTxopLimit(MicroSeconds(2048)); // Alta prioridad
        } else if (ac == "VI") {
            mac->GetAttribute("VI_Txop", ptr);
            edca = ptr.Get<QosTxop>();
            edca->SetTxopLimit(MicroSeconds(1024)); // Prioridad media
        } else if (ac == "BE") {
            mac->GetAttribute("BE_Txop", ptr);
            edca = ptr.Get<QosTxop>();
            edca->SetTxopLimit(MicroSeconds(512)); // Mejor esfuerzo
        } else if (ac == "BK") {
            mac->GetAttribute("BK_Txop", ptr);
            edca = ptr.Get<QosTxop>();
            edca->SetTxopLimit(MicroSeconds(448)); // Bajo prioridad
        }
    }

    void UpdateQoS(NetDeviceContainer devices, std::vector<std::string> acList) {
        for (uint32_t i = 0; i < devices.GetN(); ++i) {
            Ptr<WifiNetDevice> device = DynamicCast<WifiNetDevice>(devices.Get(i));
            std::string ac = acList[i % acList.size()];
            AdjustQoS(device, ac);
        }
    }
};

NS_LOG_COMPONENT_DEFINE("SDWN_Hospital");

// Implementación principal
int main(int argc, char* argv[]) {
    CommandLine cmd;
    cmd.AddValue("nStaWifi", "Número de estaciones WiFi", nStaWifi);
    cmd.AddValue("radio", "Radio del escenario en metros", radio);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    bool enablePcap = true;

    // Crear nodos
    NodeContainer wifiApNode;
    wifiApNode.Create(1);

    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(nStaWifi);

    // Configuración de movilidad
    MobilityHelper scenario = createscenario(radio);
    scenario.Install(wifiStaNodes);

    MobilityHelper centerPosition = createstarcenter();
    centerPosition.Install(wifiApNode.Get(0));

    // Configurar WiFi
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n);

    YansWifiPhyHelper wifiPhy;
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    WifiMacHelper wifiMac;
    Ssid ssid = Ssid("SDWN_Hospital");

    // Configurar AP
    NetDeviceContainer wifiApDevice;
    wifiMac.SetType("ns3::ApWifiMac", 
                    "Ssid", SsidValue(ssid), 
                    "QosSupported", BooleanValue(true));

    wifiApDevice = wifi.Install(wifiPhy, wifiMac, wifiApNode.Get(0));

    // Configurar estaciones con QoS
    NetDeviceContainer wifiStaDevices;

    std::vector<std::string> acList = {"BK", "BE", "VI", "VO"};
    std::map<std::string, UintegerValue> ampduSizes = {
        {"BK", UintegerValue(65535)},
        {"BE", UintegerValue(32768)},
        {"VI", UintegerValue(16384)},
        {"VO", UintegerValue(8192)}
    };

    for (uint32_t i = 0; i < nStaWifi; ++i) {
        std::string ac = acList[i % acList.size()];
        WifiMacHelper mac = ConfigureQoS(wifiMac, ssid, ac, ampduSizes[ac]);
        wifiStaDevices.Add(wifi.Install(wifiPhy, mac, wifiStaNodes.Get(i)));
    }

    // Instalar AODV
    AodvHelper aodv;
    InternetStackHelper internet;
    internet.SetRoutingHelper(aodv);
    internet.Install(wifiApNode);
    internet.Install(wifiStaNodes);

    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterface = address.Assign(wifiApDevice);
    Ipv4InterfaceContainer staInterfaces = address.Assign(wifiStaDevices);

    // Instanciar controlador SDWN
    SdnController controller;
    controller.UpdateQoS(wifiStaDevices, acList);

    // Configurar aplicaciones
    uint16_t port = 8080;
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApp = echoServer.Install(wifiApNode.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient(apInterface.GetAddress(0), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(MilliSeconds(20)));
    echoClient.SetAttribute("PacketSize", UintegerValue(512));

    std::vector<uint32_t> values = {0x20, 0x60, 0xa0, 0xe0};

    ApplicationContainer clientApps;
    for (uint32_t i = 0; i < wifiStaNodes.GetN(); ++i) {
        uint32_t tosValue = values[i % 4];
        Sta_Information ( i, tosValue, staInterfaces , wifiStaDevices);
        echoClient.SetAttribute("Tos", UintegerValue(tosValue));
        clientApps.Add(echoClient.Install(wifiStaNodes.Get(i)));
    }
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Añadido para habilitar el monitoreo de flujo
    Ptr<FlowMonitor> flowMonitor;
    FlowMonitorHelper flowHelper;
    flowMonitor = flowHelper.InstallAll();

    // Animación y rastreo
    AnimationInterface anim("scratch/xml/sdwn_hospital.xml");
    anim.EnablePacketMetadata(true);
    anim.EnableIpv4L3ProtocolCounters(Seconds(0), Seconds(10));

    // Distribuir posiciones de las estaciones
    for (uint32_t i = 0; i < nStaWifi; ++i) {
        double angle = i * 2 * M_PI / nStaWifi;
        anim.SetConstantPosition(wifiStaNodes.Get(i), 30.0 + 20.0 * cos(angle), 30.0 + 20.0 * sin(angle));
    }
    anim.SetConstantPosition(wifiApNode.Get(0), 30.0, 30.0);

    // Rastreo
    AsciiTraceHelper rastreo_ascii;
    wifiPhy.EnableAsciiAll(rastreo_ascii.CreateFileStream("scratch/tracers/sdwn_hospital.tr"));
    if (enablePcap) {
        wifiPhy.EnablePcap("scratch/pcap/AP_DEVICE_SDWN_QoS", wifiApDevice.Get(0));
    }
    
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    flowMonitor->SerializeToXmlFile("scratch/flowmon/sdwn_hospital.xml", true, true);
    Simulator::Destroy();
    return 0;
}

// Funciones auxiliares
MobilityHelper createscenario(double radio) {
    MobilityHelper mobility;
    mobility.SetPositionAllocator(
        "ns3::UniformDiscPositionAllocator",
        "rho", DoubleValue(radio),
        "X", DoubleValue(0.0),
        "Y", DoubleValue(0.0));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    return mobility;
}

MobilityHelper createstarcenter() {
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(1.0),
                                  "DeltaY", DoubleValue(1.0),
                                  "GridWidth", UintegerValue(1),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    return mobility;
}

WifiMacHelper ConfigureQoS(WifiMacHelper wifiMac, Ssid ssid, std::string ac, UintegerValue maxAmpduSize) {
    wifiMac.SetType("ns3::StaWifiMac",
                    "Ssid", SsidValue(ssid),
                    "QosSupported", BooleanValue(true),
                    ac + "_MaxAmpduSize", maxAmpduSize,
                    "ActiveProbing", BooleanValue(false));
    return wifiMac;
}

void Sta_Information (uint32_t index, uint32_t tosValue, Ipv4InterfaceContainer staInterfaces , NetDeviceContainer wifiStaDevices){
    // Obtener la dirección IP del nodo
    Ipv4Address ip = staInterfaces.GetAddress(index);

    // Obtener la dirección MAC del nodo
    Ptr<NetDevice> staDevice = wifiStaDevices.Get(index);
    Ptr<WifiNetDevice> wifiStaDevice = DynamicCast<WifiNetDevice>(staDevice);
    ns3::Address macAddr = wifiStaDevice->GetAddress();  // Obtén la dirección MAC como Address
    Mac48Address macAddress = Mac48Address::ConvertFrom(macAddr);  // Convierte Address a Mac48Address

    // Imprimir los valores
    std::cout << "Nodo " << index + 1 << " -> "
              << "IP: " << ip << ", "
              << "Tos: 0x" << std::hex << tosValue << std::dec << ", "
              << "MAC: " << macAddress << std::endl;
}