// Including necessary ns3 core modules
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/random-variable-stream.h"
#include "ns3/ipv4-address.h"
#include "ns3/inet-socket-address.h"
#include <iostream>
#include <sstream>

using namespace ns3;

// Log component to debug 
NS_LOG_COMPONENT_DEFINE("EnhancedSmartHomeNetwork");

// HTTP Flood Application
class HttpFloodApp : public Application 
{
public:
    HttpFloodApp();
    virtual ~HttpFloodApp();

    // initializing app parameters
    void Setup(Ptr<Socket> socket, Address address, uint32_t minPacketSize, uint32_t maxPacketSize, uint32_t nPackets, DataRate dataRate);

private:
    // handling when the application starts and stops 
    virtual void StartApplication(void);
    virtual void StopApplication(void);

    // sending and scheduling packets
    void SendPacket(void);
    void ScheduleTx(uint32_t packetSize);
    
    // Generating HTTP GET request with random user agent 
    std::string GenerateHttpRequest(uint32_t packetSize);

    Ptr<Socket> m_socket;
    Address m_peer;
    uint32_t m_minPacketSize;
    uint32_t m_maxPacketSize;
    uint32_t m_nPackets;
    DataRate m_dataRate;
    EventId m_sendEvent;
    bool m_running;
    uint32_t m_packetsSent;
    std::vector<std::string> userAgents;  
    Ptr<UniformRandomVariable> m_rand;   
};

//initialising member variables
HttpFloodApp::HttpFloodApp() 
    : m_socket(0), 
      m_peer(),
      m_minPacketSize(0),
      m_maxPacketSize(0),
      m_nPackets(0), 
      m_dataRate(0), 
      m_sendEvent(),
      m_running(false),
      m_packetsSent(0),
      m_rand(CreateObject<UniformRandomVariable>())  // Create a random variable object
{
    // Populating common user agent strings for HTTP requests
    userAgents.push_back("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/91.0.4472.124 Safari/537.36");
    userAgents.push_back("Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/91.0.4472.101 Safari/537.36");
    userAgents.push_back("Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/91.0.4472.124 Safari/537.36");
    userAgents.push_back("Mozilla/5.0 (iPhone; CPU iPhone OS 14_6 like Mac OS X) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/14.0 Mobile/15E148 Safari/604.1");
}

HttpFloodApp::~HttpFloodApp()
{
    m_socket = 0;
}

// configureing the socket, address, packet sizes
void HttpFloodApp::Setup(Ptr<Socket> socket, Address address, uint32_t minPacketSize, uint32_t maxPacketSize, uint32_t nPackets, DataRate dataRate)
{
    m_socket = socket;
    m_peer = address;
    m_minPacketSize = minPacketSize;
    m_maxPacketSize = maxPacketSize;
    m_nPackets = nPackets;
    m_dataRate = dataRate;

    // variable attributes to select packet size
    m_rand->SetAttribute("Min", DoubleValue(minPacketSize));
    m_rand->SetAttribute("Max", DoubleValue(maxPacketSize));
}

// method to call when the application is started 
void HttpFloodApp::StartApplication(void)
{
    m_running = true;
    m_packetsSent = 0;
    m_socket->Bind();            
    m_socket->Connect(m_peer);   
    SendPacket();                
}

// method called when the app stops
void HttpFloodApp::StopApplication(void)
{
    m_running = false;

    if (m_sendEvent.IsPending())
    {
        Simulator::Cancel(m_sendEvent);  
    }

    if (m_socket)
    {
        m_socket->Close();  
    }
}

// HTTP GET request with user agent
std::string HttpFloodApp::GenerateHttpRequest(uint32_t packetSize)
{
    std::ostringstream httpRequest;
    std::string userAgent = userAgents[m_rand->GetInteger(0, userAgents.size() - 1)];
    std::string cacheBuster = std::to_string(m_rand->GetInteger(10000, 99999));
    
    // http request string
    httpRequest << "GET /?cachebuster=" << cacheBuster << " HTTP/1.1\r\n";
    httpRequest << "Host: " << InetSocketAddress::ConvertFrom(m_peer).GetIpv4() << "\r\n";
    httpRequest << "User-Agent: " << userAgent << "\r\n";
    httpRequest << "Connection: keep-alive\r\n";
    httpRequest << "\r\n";

    std::string requestStr = httpRequest.str();

    // adding padding when the size of the packet is too small
    if (requestStr.size() < packetSize)
    {
        requestStr.append(packetSize - requestStr.size(), ' ');
    }

    return requestStr;
}

// single packet with generated http request
void HttpFloodApp::SendPacket(void)
{
    uint32_t packetSize = m_rand->GetInteger(m_minPacketSize, m_maxPacketSize);  
    std::string httpRequest = GenerateHttpRequest(packetSize);  
    Ptr<Packet> packet = Create<Packet>((uint8_t*) httpRequest.c_str(), packetSize);  
    m_socket->Send(packet);  

    if (++m_packetsSent < m_nPackets)
    {
        ScheduleTx(packetSize);  
    }
}

// Scheduling transmission of the next packet 
void HttpFloodApp::ScheduleTx(uint32_t packetSize)
{
    if (m_running)
    {
        // Calculating the next transmission 
        Time tNext(Seconds(packetSize * 8 / static_cast<double>(m_dataRate.GetBitRate())));
        m_sendEvent = Simulator::Schedule(tNext, &HttpFloodApp::SendPacket, this);
    }
}

// application for the slowloris attack
class SlowlorisApp : public Application
{
public:
    SlowlorisApp();
    virtual ~SlowlorisApp();
    
    void Setup(Ptr<Socket> socket, Address address, uint32_t nConnections, Time delayBetweenPackets);

private:
    // starting and stopping application
    virtual void StartApplication(void);
    virtual void StopApplication(void);
    
    void OpenConnection(void);
    void SendSlowPacket(void);
    void ScheduleNextPacket(void);

    Ptr<Socket> m_socket;
    Address m_peer;
    uint32_t m_nConnections;
    Time m_delayBetweenPackets;
    bool m_running;
    EventId m_sendEvent;
    uint32_t m_connectionsOpened;
};

SlowlorisApp::SlowlorisApp()
    : m_socket(0), m_peer(), m_nConnections(0), m_running(false), m_connectionsOpened(0)
{
}

// 
SlowlorisApp::~SlowlorisApp()
{
    m_socket = 0;
}

// configuring the socket and address, n_connections
void SlowlorisApp::Setup(Ptr<Socket> socket, Address address, uint32_t nConnections, Time delayBetweenPackets)
{
    m_socket = socket;
    m_peer = address;
    m_nConnections = nConnections;
    m_delayBetweenPackets = delayBetweenPackets;
}

// calling method when the application starts
void SlowlorisApp::StartApplication(void)
{
    m_running = true;
    m_connectionsOpened = 0;
    m_socket->Bind();            // Bind the socket
    m_socket->Connect(m_peer);   // Connect to the server
    OpenConnection();            // Open the first connection
}

// calling method when the application stops
void SlowlorisApp::StopApplication(void)
{
    m_running = false;
    if (m_sendEvent.IsPending())
    {
        Simulator::Cancel(m_sendEvent);  
    }
    if (m_socket)
    {
        m_socket->Close();  
    }
}

// Opening and scheldelling new connections to the server
void SlowlorisApp::OpenConnection(void)
{
    if (m_connectionsOpened < m_nConnections)
    {
        m_socket->Connect(m_peer);
        m_connectionsOpened++;
        ScheduleNextPacket();  
    }
}

// sendin part of  http request for the attack
void SlowlorisApp::SendSlowPacket(void)
{
    std::ostringstream partialRequest;
    partialRequest << "GET / HTTP/1.1\r\n";
    partialRequest << "Host: " << InetSocketAddress::ConvertFrom(m_peer).GetIpv4() << "\r\n";
    partialRequest << "\r\n";

    Ptr<Packet> packet = Create<Packet>((uint8_t*)partialRequest.str().c_str(), partialRequest.str().size());
    m_socket->Send(packet);  // Send the partial request packet
    ScheduleNextPacket();    // Schedule the next part of the request
}

// next packet to be sent 
void SlowlorisApp::ScheduleNextPacket(void)
{
    if (m_running)
    {
        m_sendEvent = Simulator::Schedule(m_delayBetweenPackets, &SlowlorisApp::SendSlowPacket, this);  // Schedule the next slow packet
    }
}

// Main simulation function
int main(int argc, char *argv[]) {
    // simulation duration
    Time simulationDuration = Seconds(600.0);  

    // network nodes for smart devices, router and server.
    Ptr<Node> smartCamera = CreateObject<Node>();
    Ptr<Node> router = CreateObject<Node>();
    Ptr<Node> smartTV = CreateObject<Node>();
    Ptr<Node> smartSpeaker = CreateObject<Node>();
    Ptr<Node> smartThermostat = CreateObject<Node>();
    Ptr<Node> internetNode = CreateObject<Node>(); 
    Ptr<Node> apacheServer = CreateObject<Node>(); 
    Ptr<Node> mqttBroker = CreateObject<Node>();

    // Container for home devices
    NodeContainer homeNodes;
    homeNodes.Add(smartCamera);
    homeNodes.Add(router);
    homeNodes.Add(smartTV);
    homeNodes.Add(smartSpeaker);
    homeNodes.Add(smartThermostat);

    // point-to-point link between router and internet
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("10ms"));

    // point-to-point link between the router and the internet
    NetDeviceContainer p2pDevices;
    p2pDevices = pointToPoint.Install(router, internetNode);

    // error model 
    Ptr<RateErrorModel> p2pErrorModel = CreateObject<RateErrorModel>();
    p2pErrorModel->SetAttribute("ErrorRate", DoubleValue(0.0001));

    // error model to point-to-point link
    p2pDevices.Get(0)->SetAttribute("ReceiveErrorModel", PointerValue(p2pErrorModel));
    p2pDevices.Get(1)->SetAttribute("ReceiveErrorModel", PointerValue(p2pErrorModel));

    // WiFi channel and devices for the smart home network
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper();
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211g);  
    wifi.SetRemoteStationManager("ns3::AarfWifiManager");  

    WifiMacHelper mac;
    Ssid ssid = Ssid("ns-3-ssid");

    // router setup
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevice = wifi.Install(phy, mac, router);

    //  setting up other devices on the WiFi 
    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices;
    staDevices.Add(wifi.Install(phy, mac, smartCamera));
    staDevices.Add(wifi.Install(phy, mac, smartTV));
    staDevices.Add(wifi.Install(phy, mac, smartSpeaker));
    staDevices.Add(wifi.Install(phy, mac, smartThermostat));
    NetDeviceContainer apacheDevice = wifi.Install(phy, mac, apacheServer);
    NetDeviceContainer mqttBrokerDevice = wifi.Install(phy, mac, mqttBroker);

    //  mobility for dvices  
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

    // internet protocol  on the nodes
    InternetStackHelper stack;
    stack.Install(homeNodes);
    stack.Install(internetNode);
    stack.Install(apacheServer);

    // ip addresses for point-to-point link and WiFi devices
    Ipv4AddressHelper address;

    address.SetBase("10.1.1.0", "255.255.255.0");  
    Ipv4InterfaceContainer p2pInterfaces = address.Assign(p2pDevices);

    address.SetBase("192.168.1.0", "255.255.255.0");  
    Ipv4InterfaceContainer apInterface = address.Assign(apDevice);
    Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);

    // ip for the  Apache server and MQTT broker
    Ipv4InterfaceContainer apacheInterface = address.Assign(apacheDevice);

    // variables for start time and delay
    Ptr<UniformRandomVariable> randomVar = CreateObject<UniformRandomVariable>();
    randomVar->SetAttribute("Min", DoubleValue(90.0)); 
    randomVar->SetAttribute("Max", DoubleValue(110.0));

    Ptr<UniformRandomVariable> startTimeVar = CreateObject<UniformRandomVariable>();
    startTimeVar->SetAttribute("Min", DoubleValue(0.0)); 
    startTimeVar->SetAttribute("Max", DoubleValue(600.0));

    ApplicationContainer floodApps;

    // function to create http flood and slowloris attack 
    auto createAttackApp = [&](Ptr<Node> node, DataRate rate, bool useSlowloris) {
        Ptr<Socket> attackSocket = Socket::CreateSocket(node, TcpSocketFactory::GetTypeId());
        Address apacheAddress(InetSocketAddress(apacheInterface.GetAddress(0), 80));

        if (useSlowloris) {
            Ptr<SlowlorisApp> slowlorisApp = CreateObject<SlowlorisApp>();
            slowlorisApp->Setup(attackSocket, apacheAddress, 50, Seconds(10));
            node->AddApplication(slowlorisApp);
            slowlorisApp->SetStartTime(Seconds(startTimeVar->GetValue()));
            slowlorisApp->SetStopTime(simulationDuration);
        } else {
            Ptr<HttpFloodApp> floodApp = CreateObject<HttpFloodApp>();
            floodApp->Setup(attackSocket, apacheAddress, 10, 80, 1000000, rate);
            node->AddApplication(floodApp);
            floodApp->SetStartTime(Seconds(startTimeVar->GetValue()));
            floodApp->SetStopTime(simulationDuration);
        }
    };

    // smart devices for attack traffic with different rates and attacks
    createAttackApp(smartCamera, DataRate("5Mbps"), false);  
    createAttackApp(smartTV, DataRate("3Mbps"), true);  
    createAttackApp(smartSpeaker, DataRate("1Mbps"), false);  
    createAttackApp(smartThermostat, DataRate("100Kbps"), false);  

    // Static routing  for the router and nodes
    Ipv4StaticRoutingHelper staticRouting;
    Ptr<Ipv4StaticRouting> routerStaticRouting = staticRouting.GetStaticRouting(router->GetObject<Ipv4>());
    routerStaticRouting->AddNetworkRouteTo(Ipv4Address("0.0.0.0"), Ipv4Mask("0.0.0.0"), p2pInterfaces.GetAddress(1), 1);

    // static routes for the home nodes
    for (uint32_t i = 0; i < homeNodes.GetN(); ++i) {
        if (homeNodes.Get(i) != router) { 
            Ptr<Ipv4StaticRouting> staStaticRouting = staticRouting.GetStaticRouting(homeNodes.Get(i)->GetObject<Ipv4>());
            staStaticRouting->AddNetworkRouteTo(Ipv4Address("10.1.1.0"), Ipv4Mask("255.255.255.0"), apInterface.GetAddress(0), 1);
        }
    }

    // Static routes for internet and apache server
    Ptr<Ipv4StaticRouting> internetStaticRouting = staticRouting.GetStaticRouting(internetNode->GetObject<Ipv4>());
    internetStaticRouting->AddNetworkRouteTo(Ipv4Address("192.168.1.0"), Ipv4Mask("255.255.255.0"), p2pInterfaces.GetAddress(0), 1);

    Ptr<Ipv4StaticRouting> apacheStaticRouting = staticRouting.GetStaticRouting(apacheServer->GetObject<Ipv4>());
    apacheStaticRouting->AddNetworkRouteTo(Ipv4Address("10.1.1.0"), Ipv4Mask("255.255.255.0"), apInterface.GetAddress(0), 1);

    // stopping the simulation
    Simulator::Stop(simulationDuration);

    // Install a packet sink on the apache server to receive attack traffic
    PacketSinkHelper apacheServerHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), 80));
    ApplicationContainer apacheServerApp = apacheServerHelper.Install(apacheServer);
    apacheServerApp.Start(Seconds(1.0));   
    apacheServerApp.Stop(simulationDuration);

    //  flow monitoring
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    //  pcap tracing
    phy.EnablePcap("/media/sf_Ubuntu/New_pcap/http_flood", apDevice.Get(0));

    // run simulator
    Simulator::Run();

    //  flow statistics 
    monitor->SerializeToXmlFile("smart-home-network-flow.xml", true, true);

    // end the simulation 
    Simulator::Destroy();
    return 0;
}
