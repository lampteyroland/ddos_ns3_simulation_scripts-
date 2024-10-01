#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/random-variable-stream.h"
#include <iostream>
#include <sstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("EnhancedFloodAttack");

// TCP SYN Flood Application with Port and Sequence Number Randomization
class TcpSynFloodApp : public Application 
{
public:
    TcpSynFloodApp();
    virtual ~TcpSynFloodApp();
    void Setup(Ptr<Socket> socket, Address address, uint32_t nPackets, DataRate dataRate, 
               Ipv4Address srcIp, uint16_t srcPort, bool spoof, uint8_t tcpFlags, uint32_t packetSize);

private:
    virtual void StartApplication(void);
    virtual void StopApplication(void);
    void SendPacket(void);
    void ScheduleTx(void);

    Ptr<Socket> m_socket;
    Address m_peer;
    uint32_t m_nPackets;
    DataRate m_dataRate;
    EventId m_sendEvent;
    bool m_running;
    uint32_t m_packetsSent;
    Ipv4Address m_srcIp;
    uint16_t m_srcPort;
    bool m_spoof;
    uint8_t m_tcpFlags;
    uint32_t m_packetSize;
    Ptr<UniformRandomVariable> m_randSeqNum;  // Random sequence number generator
};

TcpSynFloodApp::TcpSynFloodApp() 
    : m_socket(0), m_peer(), m_nPackets(0), m_dataRate(0), m_sendEvent(), m_running(false), 
      m_packetsSent(0), m_srcIp(Ipv4Address("0.0.0.0")), m_srcPort(0), m_spoof(false), m_tcpFlags(TcpHeader::SYN), m_packetSize(512)
{
    m_randSeqNum = CreateObject<UniformRandomVariable>();  // Initialize random sequence number generator
}

TcpSynFloodApp::~TcpSynFloodApp()
{
    m_socket = 0;
}

void TcpSynFloodApp::Setup(Ptr<Socket> socket, Address address, uint32_t nPackets, DataRate dataRate, 
                           Ipv4Address srcIp, uint16_t srcPort, bool spoof, uint8_t tcpFlags, uint32_t packetSize)
{
    m_socket = socket;
    m_peer = address;
    m_nPackets = nPackets;
    m_dataRate = dataRate;
    m_srcIp = srcIp;
    m_srcPort = srcPort;
    m_spoof = spoof;
    m_tcpFlags = tcpFlags;
    m_packetSize = packetSize;  // Packet size is now dynamic
}

void TcpSynFloodApp::StartApplication(void)
{
    m_running = true;
    m_packetsSent = 0;
    m_socket->Bind();
    m_socket->Connect(m_peer);
    SendPacket();
}

void TcpSynFloodApp::StopApplication(void)
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

void TcpSynFloodApp::SendPacket(void)
{
    Ptr<Packet> packet = Create<Packet>(m_packetSize);  // Varying packet size

    TcpHeader tcpHeader;
    tcpHeader.SetFlags(m_tcpFlags);
    tcpHeader.SetSourcePort(m_srcPort);
    tcpHeader.SetDestinationPort(InetSocketAddress::ConvertFrom(m_peer).GetPort());
    
    // Random sequence number for SYN flood
    tcpHeader.SetSequenceNumber(SequenceNumber32(m_randSeqNum->GetValue(1, 4294967295)));

    Ipv4Header ipHeader;
    if (m_spoof)
    {
        ipHeader.SetSource(m_srcIp);
    }
    ipHeader.SetDestination(InetSocketAddress::ConvertFrom(m_peer).GetIpv4());

    packet->AddHeader(tcpHeader);
    packet->AddHeader(ipHeader);

    NS_LOG_INFO("Sending TCP SYN packet from " << m_srcIp << " to " << InetSocketAddress::ConvertFrom(m_peer).GetIpv4() 
                 << " with random sequence number: " << tcpHeader.GetSequenceNumber());

    m_socket->Send(packet);

    if (++m_packetsSent < m_nPackets)
    {
        ScheduleTx();
    }
}

void TcpSynFloodApp::ScheduleTx(void)
{
    if (m_running)
    {
        Time tNext(Seconds(m_packetSize * 8 / static_cast<double>(m_dataRate.GetBitRate())));  // Based on packet size
        m_sendEvent = Simulator::Schedule(tNext, &TcpSynFloodApp::SendPacket, this);
    }
}

// UDP Flood Application with Random Packet Sizes and Port Randomization
class UdpFloodApp : public Application 
{
public:
    UdpFloodApp();
    virtual ~UdpFloodApp();
    void Setup(Ptr<Socket> socket, Address address, uint32_t packetSize, uint32_t nPackets, 
               DataRate dataRate, Ipv4Address srcIp, uint16_t srcPort, bool spoof);

private:
    virtual void StartApplication(void);
    virtual void StopApplication(void);
    void SendPacket(void);
    void ScheduleTx(void);

    Ptr<Socket> m_socket;
    Address m_peer;
    uint32_t m_packetSize;
    uint32_t m_nPackets;
    DataRate m_dataRate;
    EventId m_sendEvent;
    bool m_running;
    uint32_t m_packetsSent;
    Ipv4Address m_srcIp;
    uint16_t m_srcPort;
    bool m_spoof;
    Ptr<UniformRandomVariable> m_randPacketSize;  // Random packet size generator
};

UdpFloodApp::UdpFloodApp() 
    : m_socket(0), m_peer(), m_packetSize(512), m_nPackets(0), m_dataRate(0), m_sendEvent(), 
      m_running(false), m_packetsSent(0), m_srcIp(Ipv4Address("0.0.0.0")), m_srcPort(0), m_spoof(false)
{
    m_randPacketSize = CreateObject<UniformRandomVariable>();  // Initialize random packet size generator
    m_randPacketSize->SetAttribute("Min", DoubleValue(64));     // Minimum packet size
    m_randPacketSize->SetAttribute("Max", DoubleValue(1500));   // Maximum packet size
}

UdpFloodApp::~UdpFloodApp()
{
    m_socket = 0;
}

void UdpFloodApp::Setup(Ptr<Socket> socket, Address address, uint32_t packetSize, uint32_t nPackets, 
                        DataRate dataRate, Ipv4Address srcIp, uint16_t srcPort, bool spoof)
{
    m_socket = socket;
    m_peer = address;
    m_packetSize = packetSize;
    m_nPackets = nPackets;
    m_dataRate = dataRate;
    m_srcIp = srcIp;
    m_srcPort = srcPort;
    m_spoof = spoof;
}

void UdpFloodApp::StartApplication(void)
{
    m_running = true;
    m_packetsSent = 0;
    m_socket->Bind();
    m_socket->Connect(m_peer);
    SendPacket();
}

void UdpFloodApp::StopApplication(void)
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

void UdpFloodApp::SendPacket(void)
{
    m_packetSize = m_randPacketSize->GetValue();  // Varying packet size dynamically
    Ptr<Packet> packet = Create<Packet>(m_packetSize);

    Ipv4Header ipHeader;
    if (m_spoof)
    {
        ipHeader.SetSource(m_srcIp);
    }
    ipHeader.SetDestination(InetSocketAddress::ConvertFrom(m_peer).GetIpv4());

    packet->AddHeader(ipHeader);

    NS_LOG_INFO("Sending UDP packet from " << m_srcIp << " to " << InetSocketAddress::ConvertFrom(m_peer).GetIpv4() 
                 << " with packet size: " << m_packetSize);

    m_socket->Send(packet);

    if (++m_packetsSent < m_nPackets)
    {
        ScheduleTx();
    }
}

void UdpFloodApp::ScheduleTx(void)
{
    if (m_running)
    {
        double packetDuration = static_cast<double>(m_packetSize * 8) / m_dataRate.GetBitRate();  // Time for sending a packet (in seconds)
        Time tNext = Seconds(packetDuration);
        m_sendEvent = Simulator::Schedule(tNext, &UdpFloodApp::SendPacket, this);
    }
}

// ICMP Flood Application with Variable Data Rates
class IcmpFloodApp : public Application
{
public:
    IcmpFloodApp();
    virtual ~IcmpFloodApp();
    void Setup(Ptr<Socket> socket, Address address, uint32_t nPackets, DataRate dataRate, Ipv4Address srcIp, bool spoof);

private:
    virtual void StartApplication(void);
    virtual void StopApplication(void);
    void SendPacket(void);
    void ScheduleTx(void);

    Ptr<Socket> m_socket;
    Address m_peer;
    uint32_t m_nPackets;
    DataRate m_dataRate;
    EventId m_sendEvent;
    bool m_running;
    uint32_t m_packetsSent;
    Ipv4Address m_srcIp;
    bool m_spoof;
};

IcmpFloodApp::IcmpFloodApp()
    : m_socket(0), m_peer(), m_nPackets(0), m_dataRate(0), m_sendEvent(), 
      m_running(false), m_packetsSent(0), m_srcIp(Ipv4Address("0.0.0.0")), m_spoof(false)
{
}

IcmpFloodApp::~IcmpFloodApp()
{
    m_socket = 0;
}

void IcmpFloodApp::Setup(Ptr<Socket> socket, Address address, uint32_t nPackets, DataRate dataRate, Ipv4Address srcIp, bool spoof)
{
    m_socket = socket;
    m_peer = address;
    m_nPackets = nPackets;
    m_dataRate = dataRate;
    m_srcIp = srcIp;
    m_spoof = spoof;
}

void IcmpFloodApp::StartApplication(void)
{
    m_running = true;
    m_packetsSent = 0;
    m_socket->Bind();
    m_socket->Connect(m_peer);
    SendPacket();
}

void IcmpFloodApp::StopApplication(void)
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

void IcmpFloodApp::SendPacket(void)
{
    Ptr<Packet> packet = Create<Packet>(64);  // Typical size of an ICMP echo request packet
    Icmpv4Echo echo;
    echo.SetSequenceNumber(m_packetsSent);
    echo.SetIdentifier(0x1234);  // Example identifier

    if (m_spoof)
    {
        Ipv4Header ipHeader;
        ipHeader.SetSource(m_srcIp);
        packet->AddHeader(ipHeader);
    }

    packet->AddHeader(echo);

    NS_LOG_INFO("Sending ICMP Echo Request from " << m_srcIp << " to " << InetSocketAddress::ConvertFrom(m_peer).GetIpv4());

    m_socket->Send(packet);

    if (++m_packetsSent < m_nPackets)
    {
        ScheduleTx();
    }
}

void IcmpFloodApp::ScheduleTx(void)
{
    if (m_running)
    {
        Time tNext(Seconds(64 * 8 / static_cast<double>(m_dataRate.GetBitRate())));  // Vary packet sending rate based on data rate
        m_sendEvent = Simulator::Schedule(tNext, &IcmpFloodApp::SendPacket, this);
    }
}

int main(int argc, char *argv[]) {
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
    LogComponentEnable("OnOffApplication", LOG_LEVEL_INFO);
    LogComponentEnable("PacketSink", LOG_LEVEL_INFO);
    LogComponentEnable("FlowMonitor", LOG_LEVEL_INFO);
    LogComponentEnable("EnhancedFloodAttack", LOG_LEVEL_INFO);

    Ptr<Node> smartCamera = CreateObject<Node>();
    Ptr<Node> router = CreateObject<Node>();
    Ptr<Node> smartTV = CreateObject<Node>();
    Ptr<Node> smartSpeaker = CreateObject<Node>();
    Ptr<Node> smartThermostat = CreateObject<Node>();
    Ptr<Node> internetNode = CreateObject<Node>();  // node for internet
    Ptr<Node> apacheServer = CreateObject<Node>();  // Apache server node

    NodeContainer homeNodes;
    homeNodes.Add(smartCamera);
    homeNodes.Add(router);
    homeNodes.Add(smartTV);
    homeNodes.Add(smartSpeaker);
    homeNodes.Add(smartThermostat);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator", "MinX", DoubleValue(0.0), "MinY", DoubleValue(0.0), 
                                  "DeltaX", DoubleValue(5.0), "DeltaY", DoubleValue(10.0), "GridWidth", UintegerValue(3), 
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(homeNodes);
    mobility.Install(internetNode);
    mobility.Install(apacheServer);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("10ms"));

    NetDeviceContainer p2pDevices = pointToPoint.Install(router, internetNode);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper();
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211g);
    wifi.SetRemoteStationManager("ns3::AarfWifiManager");

    WifiMacHelper mac;
    Ssid ssid = Ssid("ns-3-ssid");
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevice = wifi.Install(phy, mac, router);

    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices;
    staDevices.Add(wifi.Install(phy, mac, smartCamera));
    staDevices.Add(wifi.Install(phy, mac, smartTV));
    staDevices.Add(wifi.Install(phy, mac, smartSpeaker));
    staDevices.Add(wifi.Install(phy, mac, smartThermostat));
    NetDeviceContainer apacheDevice = wifi.Install(phy, mac, apacheServer);

    InternetStackHelper stack;
    stack.Install(homeNodes);
    stack.Install(internetNode);
    stack.Install(apacheServer);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer p2pInterfaces = address.Assign(p2pDevices);
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);
    Ipv4InterfaceContainer apacheInterface = address.Assign(apacheDevice);

    Ptr<UniformRandomVariable> randPort = CreateObject<UniformRandomVariable>();
    randPort->SetAttribute("Min", DoubleValue(1024));
    randPort->SetAttribute("Max", DoubleValue(65535));

    Ptr<UniformRandomVariable> randDataRate = CreateObject<UniformRandomVariable>();
    randDataRate->SetAttribute("Min", DoubleValue(1));  // Minimum data rate in Mbps
    randDataRate->SetAttribute("Max", DoubleValue(100));  // Maximum data rate in Mbps

    // Configure TCP SYN Flood from smartCamera with variable data rate
    uint16_t srcPortCamera = randPort->GetValue();
    uint16_t destPortCamera = randPort->GetValue();
    DataRate dataRateCamera = DataRate(randDataRate->GetValue() * 1000000);  // Random data rate
    Ptr<Socket> synSocketCamera = Socket::CreateSocket(smartCamera, TcpSocketFactory::GetTypeId());
    Address apacheAddressCamera(InetSocketAddress(apacheInterface.GetAddress(0), destPortCamera));
    Ptr<TcpSynFloodApp> synFloodAppCamera = CreateObject<TcpSynFloodApp>();
    synFloodAppCamera->Setup(synSocketCamera, apacheAddressCamera, 1000000, dataRateCamera, 
                             Ipv4Address("192.168.1.100"), srcPortCamera, true, TcpHeader::SYN, 512);
    smartCamera->AddApplication(synFloodAppCamera);
    synFloodAppCamera->SetStartTime(Seconds(2.0));
    synFloodAppCamera->SetStopTime(Seconds(300.0));

    // Configure UDP Flood from smartCamera with variable data rate
    uint16_t srcUdpPortCamera = randPort->GetValue();
    uint16_t destUdpPortCamera = randPort->GetValue();
    DataRate udpDataRateCamera = DataRate(randDataRate->GetValue() * 1000000);  // Random data rate
    Ptr<Socket> udpSocketCamera = Socket::CreateSocket(smartCamera, UdpSocketFactory::GetTypeId());
    Ptr<UdpFloodApp> udpFloodAppCamera = CreateObject<UdpFloodApp>();
    udpFloodAppCamera->Setup(udpSocketCamera, InetSocketAddress(apacheInterface.GetAddress(0), destUdpPortCamera), 
                             512, 1000000, udpDataRateCamera, Ipv4Address("192.168.1.101"), srcUdpPortCamera, true);
    smartCamera->AddApplication(udpFloodAppCamera);
    udpFloodAppCamera->SetStartTime(Seconds(95.0));
    udpFloodAppCamera->SetStopTime(Seconds(245.0));

    // Configure ICMP Flood from smartCamera with variable data rate
    Ptr<Socket> icmpSocketCamera = Socket::CreateSocket(smartCamera, Ipv4RawSocketFactory::GetTypeId());
    icmpSocketCamera->SetAttribute("Protocol", UintegerValue(1));  // Protocol number for ICMP
    DataRate icmpDataRateCamera = DataRate(randDataRate->GetValue() * 1000000);  // Random data rate
    Address targetAddressCamera(InetSocketAddress(apacheInterface.GetAddress(0), 1));
    Ptr<IcmpFloodApp> icmpFloodAppCamera = CreateObject<IcmpFloodApp>();
    icmpFloodAppCamera->Setup(icmpSocketCamera, targetAddressCamera, 10000, icmpDataRateCamera, Ipv4Address("192.168.1.100"), true);
    smartCamera->AddApplication(icmpFloodAppCamera);
    icmpFloodAppCamera->SetStartTime(Seconds(50.0));
    icmpFloodAppCamera->SetStopTime(Seconds(200.0));

    // Configure TCP SYN Flood from smartTV with variable data rate
    uint16_t srcPortTV = randPort->GetValue();
    uint16_t destPortTV = randPort->GetValue();
    DataRate dataRateTV = DataRate(randDataRate->GetValue() * 1000000);  // Random data rate
    Ptr<Socket> synSocketTV = Socket::CreateSocket(smartTV, TcpSocketFactory::GetTypeId());
    Address apacheAddressTV(InetSocketAddress(apacheInterface.GetAddress(0), destPortTV));
    Ptr<TcpSynFloodApp> synFloodAppTV = CreateObject<TcpSynFloodApp>();
    synFloodAppTV->Setup(synSocketTV, apacheAddressTV, 1000000, dataRateTV, 
                         Ipv4Address("192.168.1.102"), srcPortTV, true, TcpHeader::SYN, 512);
    smartTV->AddApplication(synFloodAppTV);
    synFloodAppTV->SetStartTime(Seconds(3.0));
    synFloodAppTV->SetStopTime(Seconds(303.0));

    // Configure UDP Flood from smartTV with variable data rate
    uint16_t srcUdpPortTV = randPort->GetValue();
    uint16_t destUdpPortTV = randPort->GetValue();
    DataRate udpDataRateTV = DataRate(randDataRate->GetValue() * 1000000);  // Random data rate
    Ptr<Socket> udpSocketTV = Socket::CreateSocket(smartTV, UdpSocketFactory::GetTypeId());
    Ptr<UdpFloodApp> udpFloodAppTV = CreateObject<UdpFloodApp>();
    udpFloodAppTV->Setup(udpSocketTV, InetSocketAddress(apacheInterface.GetAddress(0), destUdpPortTV), 
                         512, 1000000, udpDataRateTV, Ipv4Address("192.168.1.103"), srcUdpPortTV, true);
    smartTV->AddApplication(udpFloodAppTV);
    udpFloodAppTV->SetStartTime(Seconds(96.0));
    udpFloodAppTV->SetStopTime(Seconds(236.0));

    // Configure ICMP Flood from smartTV with variable data rate
    Ptr<Socket> icmpSocketTV = Socket::CreateSocket(smartTV, Ipv4RawSocketFactory::GetTypeId());
    icmpSocketTV->SetAttribute("Protocol", UintegerValue(1));  // Protocol number for ICMP
    DataRate icmpDataRateTV = DataRate(randDataRate->GetValue() * 1000000);  // Random data rate
    Address targetAddressTV(InetSocketAddress(apacheInterface.GetAddress(0), 1));
    Ptr<IcmpFloodApp> icmpFloodAppTV = CreateObject<IcmpFloodApp>();
    icmpFloodAppTV->Setup(icmpSocketTV, targetAddressTV, 10000, icmpDataRateTV, Ipv4Address("192.168.1.102"), true);
    smartTV->AddApplication(icmpFloodAppTV);
    icmpFloodAppTV->SetStartTime(Seconds(55.0));
    icmpFloodAppTV->SetStopTime(Seconds(235.0));

    // Configure TCP SYN Flood from smartSpeaker with variable data rate
    uint16_t srcPortSpeaker = randPort->GetValue();
    uint16_t destPortSpeaker = randPort->GetValue();
    DataRate dataRateSpeaker = DataRate(randDataRate->GetValue() * 1000000);  // Random data rate
    Ptr<Socket> synSocketSpeaker = Socket::CreateSocket(smartSpeaker, TcpSocketFactory::GetTypeId());
    Address apacheAddressSpeaker(InetSocketAddress(apacheInterface.GetAddress(0), destPortSpeaker));
    Ptr<TcpSynFloodApp> synFloodAppSpeaker = CreateObject<TcpSynFloodApp>();
    synFloodAppSpeaker->Setup(synSocketSpeaker, apacheAddressSpeaker, 1000000, dataRateSpeaker, 
                              Ipv4Address("192.168.1.104"), srcPortSpeaker, true, TcpHeader::SYN, 512);
    smartSpeaker->AddApplication(synFloodAppSpeaker);
    synFloodAppSpeaker->SetStartTime(Seconds(4.0));
    synFloodAppSpeaker->SetStopTime(Seconds(304.0));

    // Configure UDP Flood from smartSpeaker with variable data rate
    uint16_t srcUdpPortSpeaker = randPort->GetValue();
    uint16_t destUdpPortSpeaker = randPort->GetValue();
    DataRate udpDataRateSpeaker = DataRate(randDataRate->GetValue() * 1000000);  // Random data rate
    Ptr<Socket> udpSocketSpeaker = Socket::CreateSocket(smartSpeaker, UdpSocketFactory::GetTypeId());
    Ptr<UdpFloodApp> udpFloodAppSpeaker = CreateObject<UdpFloodApp>();
    udpFloodAppSpeaker->Setup(udpSocketSpeaker, InetSocketAddress(apacheInterface.GetAddress(0), destUdpPortSpeaker), 
                              512, 1000000, udpDataRateSpeaker, Ipv4Address("192.168.1.105"), srcUdpPortSpeaker, true);
    smartSpeaker->AddApplication(udpFloodAppSpeaker);
    udpFloodAppSpeaker->SetStartTime(Seconds(97.0));
    udpFloodAppSpeaker->SetStopTime(Seconds(337.0));

    // Configure ICMP Flood from smartSpeaker with variable data rate
    Ptr<Socket> icmpSocketSpeaker = Socket::CreateSocket(smartSpeaker, Ipv4RawSocketFactory::GetTypeId());
    icmpSocketSpeaker->SetAttribute("Protocol", UintegerValue(1));  // Protocol number for ICMP
    DataRate icmpDataRateSpeaker = DataRate(randDataRate->GetValue() * 1000000);  // Random data rate
    Address targetAddressSpeaker(InetSocketAddress(apacheInterface.GetAddress(0), 1));
    Ptr<IcmpFloodApp> icmpFloodAppSpeaker = CreateObject<IcmpFloodApp>();
    icmpFloodAppSpeaker->Setup(icmpSocketSpeaker, targetAddressSpeaker, 10000, icmpDataRateSpeaker, Ipv4Address("192.168.1.104"), true);
    smartSpeaker->AddApplication(icmpFloodAppSpeaker);
    icmpFloodAppSpeaker->SetStartTime(Seconds(60.0));
    icmpFloodAppSpeaker->SetStopTime(Seconds(200.0));

    // Add static routing
    Ipv4StaticRoutingHelper staticRouting;
    Ptr<Ipv4StaticRouting> routerStaticRouting = staticRouting.GetStaticRouting(router->GetObject<Ipv4>());
    routerStaticRouting->AddNetworkRouteTo(Ipv4Address("0.0.0.0"), Ipv4Mask("0.0.0.0"), p2pInterfaces.GetAddress(1), 1);

    Ptr<Ipv4StaticRouting> apacheStaticRouting = staticRouting.GetStaticRouting(apacheServer->GetObject<Ipv4>());
    apacheStaticRouting->AddNetworkRouteTo(Ipv4Address("10.1.1.0"), Ipv4Mask("255.255.255.0"), p2pInterfaces.GetAddress(0), 1);

    Simulator::Stop(Seconds(200.0));

    PacketSinkHelper apacheServerHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), 80));
    ApplicationContainer apacheServerApp = apacheServerHelper.Install(apacheServer);
    apacheServerApp.Start(Seconds(1.0));
    apacheServerApp.Stop(Seconds(200.0));

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();
    phy.EnablePcap("/media/sf_Ubuntu/tcp-udp-floodd", apDevice.Get(0));

    Simulator::Run();

    monitor->SerializeToXmlFile("tcp-udp-flood-flow.xml", true, true);
    Simulator::Destroy();
    return 0;
}
