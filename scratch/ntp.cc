#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/random-variable-stream.h"
#include "ns3/tcp-socket-factory.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("SmartHomeNetwork");

class NtpServerApp : public Application {
public:
    void Setup(uint16_t port, Ipv4Address victimAddress, uint16_t victimPort, uint32_t amplificationFactor) {
        m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
        InetSocketAddress localAddress = InetSocketAddress(Ipv4Address::GetAny(), port);
        m_socket->Bind(localAddress);
        m_socket->SetRecvCallback(MakeCallback(&NtpServerApp::HandleRead, this));
        m_victimAddress = victimAddress;
        m_victimPort = victimPort;
        m_amplificationFactor = amplificationFactor;
    }

private:
    void HandleRead(Ptr<Socket> socket) {
        Ptr<Packet> packet;
        Address from;
        while ((packet = socket->RecvFrom(from))) {
            uint32_t requestSize = packet->GetSize();
            uint32_t responseSize = requestSize * m_amplificationFactor;
            Ptr<Packet> responsePacket = Create<Packet>(responseSize);
            socket->SendTo(responsePacket, 0, InetSocketAddress(m_victimAddress, m_victimPort));
        }
    }

    Ptr<Socket> m_socket;
    Ipv4Address m_victimAddress;
    uint16_t m_victimPort;
    uint32_t m_amplificationFactor;
};

int main(int argc, char *argv[]) {
    
    // Enable logging for specific components
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
    LogComponentEnable("OnOffApplication", LOG_LEVEL_INFO);
    LogComponentEnable("PacketSink", LOG_LEVEL_INFO);
    LogComponentEnable("FlowMonitor", LOG_LEVEL_INFO);                                    

    // Creating nodes for smart devices and other components
    Ptr<Node> smartCamera = CreateObject<Node>();
    Ptr<Node> router = CreateObject<Node>();
    Ptr<Node> smartTV = CreateObject<Node>();
    Ptr<Node> smartSpeaker = CreateObject<Node>();
    Ptr<Node> smartThermostat = CreateObject<Node>();
    Ptr<Node> internetNode = CreateObject<Node>(); // Node for internet
    Ptr<Node> apacheServer = CreateObject<Node>(); // Apache server node
    Ptr<Node> mqttBroker = CreateObject<Node>();   // MQTT broker node
    Ptr<Node> victimNode = CreateObject<Node>();   // Victim node
    
    // NodeContainer to hold all home nodes
    NodeContainer homeNodes;
    homeNodes.Add(smartCamera);
    homeNodes.Add(router);
    homeNodes.Add(smartTV);
    homeNodes.Add(smartSpeaker);
    homeNodes.Add(smartThermostat);

    // Creating a point-to-point link between router and internet
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("10ms"));

    NetDeviceContainer p2pDevices;
    p2pDevices = pointToPoint.Install(router, internetNode);

    // WiFi network for the home network
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper();
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211g);
    wifi.SetRemoteStationManager("ns3::AarfWifiManager");

    WifiMacHelper mac;
    Ssid ssid = Ssid("ns-3-ssid");

    // Set up the AP
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevice = wifi.Install(phy, mac, router); // Router as AP

    // Set up the STAs (IoT devices, Apache server, and victim node)
    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices;
    staDevices.Add(wifi.Install(phy, mac, smartCamera));
    staDevices.Add(wifi.Install(phy, mac, smartTV));
    staDevices.Add(wifi.Install(phy, mac, smartSpeaker));
    staDevices.Add(wifi.Install(phy, mac, smartThermostat));
    NetDeviceContainer apacheDevice = wifi.Install(phy, mac, apacheServer);
    NetDeviceContainer victimDevice = wifi.Install(phy, mac, victimNode);
    NetDeviceContainer mqttBrokerDevice = wifi.Install(phy, mac, mqttBroker);   

    // Mobility model for all nodes
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(10.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(homeNodes);
    mobility.Install(internetNode);
    mobility.Install(apacheServer);
    mobility.Install(mqttBroker);
    mobility.Install(victimNode);

    // Installing Internet stack on all nodes
    InternetStackHelper stack;
    stack.Install(homeNodes);
    stack.Install(internetNode);
    stack.Install(apacheServer);
    stack.Install(mqttBroker);
    stack.Install(victimNode);

    // Assigning IP addresses
    Ipv4AddressHelper address;

    // IP addresses for the point-to-point link
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer p2pInterfaces = address.Assign(p2pDevices);

    // IP addresses for the home network
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterface = address.Assign(apDevice);
    Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);

    // Assign IP address to the Apache server node
    Ipv4InterfaceContainer apacheInterface = address.Assign(apacheDevice);

    // Assign IP address to the MQTT broker node
    Ipv4InterfaceContainer mqttBrokerInterface = address.Assign(mqttBrokerDevice);

    // Assign IP address to the victim node
    Ipv4InterfaceContainer victimInterface = address.Assign(victimDevice);

    // Add static routing
    Ipv4StaticRoutingHelper staticRouting;
    Ptr<Ipv4StaticRouting> routerStaticRouting = staticRouting.GetStaticRouting(router->GetObject<Ipv4>());
    routerStaticRouting->AddNetworkRouteTo(Ipv4Address("0.0.0.0"), Ipv4Mask("0.0.0.0"), p2pInterfaces.GetAddress(1), 1);

    for (uint32_t i = 0; i < homeNodes.GetN(); ++i) {
        if (homeNodes.Get(i) != router) { // Skip the router node
            Ptr<Ipv4StaticRouting> staStaticRouting = staticRouting.GetStaticRouting(homeNodes.Get(i)->GetObject<Ipv4>());
            staStaticRouting->AddNetworkRouteTo(Ipv4Address("10.1.1.0"), Ipv4Mask("255.255.255.0"), apInterface.GetAddress(0), 1);
        }
    }

    Ptr<Ipv4StaticRouting> internetStaticRouting = staticRouting.GetStaticRouting(internetNode->GetObject<Ipv4>());
    internetStaticRouting->AddNetworkRouteTo(Ipv4Address("192.168.1.0"), Ipv4Mask("255.255.255.0"), p2pInterfaces.GetAddress(0), 1);

    Ptr<Ipv4StaticRouting> apacheStaticRouting = staticRouting.GetStaticRouting(apacheServer->GetObject<Ipv4>());
    apacheStaticRouting->AddNetworkRouteTo(Ipv4Address("10.1.1.0"), Ipv4Mask("255.255.255.0"), apInterface.GetAddress(0), 1);

    Ptr<Ipv4StaticRouting> victimStaticRouting = staticRouting.GetStaticRouting(victimNode->GetObject<Ipv4>());
    victimStaticRouting->AddNetworkRouteTo(Ipv4Address("10.1.1.0"), Ipv4Mask("255.255.255.0"), apInterface.GetAddress(0), 1);

    // Apache Server acting as an NTP server on port 123 (NTP)
    uint16_t ntpPort = 123;
    uint16_t victimPort = 9999; // Arbitrary port on victim to receive attack traffic
    Ptr<NtpServerApp> ntpServerApp = CreateObject<NtpServerApp>();
    ntpServerApp->Setup(ntpPort, victimInterface.GetAddress(0), victimPort, 50); // Amplification factor of 50
    apacheServer->AddApplication(ntpServerApp);
    ntpServerApp->SetStartTime(Seconds(1.0));
    ntpServerApp->SetStopTime(Seconds(600.0));

    // Simulate an NTP amplification attack from smartCamera to Apache Server
    UdpClientHelper ntpAttackClient(apacheInterface.GetAddress(0), ntpPort);
    ntpAttackClient.SetAttribute("MaxPackets", UintegerValue(100000)); // Send many packets for amplification
    ntpAttackClient.SetAttribute("Interval", TimeValue(MilliSeconds(0.001))); // Fast packet interval
    ntpAttackClient.SetAttribute("PacketSize", UintegerValue(64)); // Small request size

    ApplicationContainer ntpAttackClientApp = ntpAttackClient.Install(smartCamera); // smartCamera is the attacker in this case
    ntpAttackClientApp.Start(Seconds(5.0)); // Start the attack at 5 seconds into the simulation
    ntpAttackClientApp.Stop(Seconds(50.0)); // Attack ends at 50 seconds

    // Simulate normal traffic (e.g., smartTV sending requests to Apache server)
    uint16_t apachePort = 80; // Default HTTP port
    PacketSinkHelper apacheServerHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), apachePort));
    ApplicationContainer apacheServerApp = apacheServerHelper.Install(apacheServer);
    apacheServerApp.Start(Seconds(1.0));
    apacheServerApp.Stop(Seconds(600.0));

    // Smart TV as a client sending requests to Apache server
    BulkSendHelper clientHelper("ns3::TcpSocketFactory", InetSocketAddress(apacheInterface.GetAddress(0), apachePort));
    clientHelper.SetAttribute("MaxBytes", UintegerValue(0)); // Send data until the application stops
    ApplicationContainer clientApps = clientHelper.Install(smartTV);
    clientApps.Start(Seconds(2.0)); // Start sending requests at 2 seconds into the simulation
    clientApps.Stop(Seconds(600.0)); // Stop at the end of the simulation

    // Setup flow monitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    // Enable pcap tracing for the access point Pcap for the router
    phy.EnablePcap("/media/sf_Ubuntu/New_pcap/ntp_attack", apDevice.Get(0));

    // Run the simulation
    Simulator::Run();

    // Print flow monitor statistics
    flowmon.SerializeToXmlFile("ntp-attack-flow-monitor.xml", true, true);

    Simulator::Destroy();

    return 0;
}
