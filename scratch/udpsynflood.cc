// This section of the code is including necessary NS-3 core and network modules
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

// This section of the code is adding the log component for debugging 
NS_LOG_COMPONENT_DEFINE("UDPSYNFlood");

// This section of the code defining the syn flood class
class TcpSynFloodApp : public Application 
{
public:
    TcpSynFloodApp();
    virtual ~TcpSynFloodApp();

    //This section of the code is setting up flood parameters
    void Setup(Ptr<Socket> socket, Address address, uint32_t nPackets, DataRate dataRate, 
               Ipv4Address srcIp, uint16_t srcPort, bool spoof, uint8_t tcpFlags, uint32_t packetSize);

private:
    // This section of the code is setting up method to start and stop the application 
    virtual void StartApplication(void);
    virtual void StopApplication(void);

    // This section of the code is setting up method to schedule and send packets 
    void SendPacket(void);
    void ScheduleTx(void);

    // This section of the code is declaring the variables for the application
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
    Ptr<UniformRandomVariable> m_randSeqNum; 
};

//This section of the code is initiasing the variables
TcpSynFloodApp::TcpSynFloodApp() 
    : m_socket(0), m_peer(), m_nPackets(0), m_dataRate(0), m_sendEvent(), m_running(false), 
      m_packetsSent(0), m_srcIp(Ipv4Address("0.0.0.0")), m_srcPort(0), m_spoof(false), 
      m_tcpFlags(TcpHeader::SYN), m_packetSize(512)
{
    m_randSeqNum = CreateObject<UniformRandomVariable>(); 
}

TcpSynFloodApp::~TcpSynFloodApp()
{
    m_socket = 0;
}

//This section of the code is configuring the application with the required parameters 
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
    m_packetSize = packetSize;  
}

// This section of the code is the calling method when the application starts
void TcpSynFloodApp::StartApplication(void)
{
    m_running = true;
    m_packetsSent = 0;
    m_socket->Bind();
    m_socket->Connect(m_peer);   
    SendPacket();                
}

// This section of the code is the calling the method when the application is stopped 
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

// This section of the code is  sending packets
void TcpSynFloodApp::SendPacket(void)
{
    Ptr<Packet> packet = Create<Packet>(m_packetSize);  

    TcpHeader tcpHeader;
    tcpHeader.SetFlags(m_tcpFlags);                             
    tcpHeader.SetSourcePort(m_srcPort);
    tcpHeader.SetDestinationPort(InetSocketAddress::ConvertFrom(m_peer).GetPort());
    
    // This section of the code is setting a random sequence number for the syn flood attack
    tcpHeader.SetSequenceNumber(SequenceNumber32(m_randSeqNum->GetValue(1, 4294967295)));

    //This section of the code is adding ip spoofing 
    Ipv4Header ipHeader;
    if (m_spoof)  
    {
        ipHeader.SetSource(m_srcIp);
    }
    //This section of the code is setting up the destination ip 
    ipHeader.SetDestination(InetSocketAddress::ConvertFrom(m_peer).GetIpv4()); 

    //This section of the code is adding headers to the packet
    packet->AddHeader(tcpHeader);
    packet->AddHeader(ipHeader);

    // This section of the code is logging information about the packets that have been sent
    NS_LOG_INFO("Sending TCP SYN packet from " << m_srcIp << " to " << InetSocketAddress::ConvertFrom(m_peer).GetIpv4() 
                 << " with random sequence number: " << tcpHeader.GetSequenceNumber());

    // This section of the code is sending packets 
    m_socket->Send(packet);  

    // This section of the code is scheduling the next packets to be sent 
    if (++m_packetsSent < m_nPackets)
    {
        ScheduleTx();
    }
}

// This section of the code is scheduling the next transmission using on packet size and data rate 
void TcpSynFloodApp::ScheduleTx(void)
{
    if (m_running)
    {
        Time tNext(Seconds(m_packetSize * 8 / static_cast<double>(m_dataRate.GetBitRate()))); 
        m_sendEvent = Simulator::Schedule(tNext, &TcpSynFloodApp::SendPacket, this);
    }
}

// This section of the code is setting up and defining the UDP Flood class for the attack 
class UdpFloodApp : public Application 
{
public:
    UdpFloodApp();
    virtual ~UdpFloodApp();

    // This section of the code is configuring flood parameters
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
    Ptr<UniformRandomVariable> m_randPacketSize;  
};

// This section of the code is setting up  variables with random packet size 
UdpFloodApp::UdpFloodApp() 
    : m_socket(0), m_peer(), m_packetSize(512), m_nPackets(0), m_dataRate(0), m_sendEvent(), 
      m_running(false), m_packetsSent(0), m_srcIp(Ipv4Address("0.0.0.0")), m_srcPort(0), m_spoof(false)
{
    m_randPacketSize = CreateObject<UniformRandomVariable>();  
    m_randPacketSize->SetAttribute("Min", DoubleValue(64));
    m_randPacketSize->SetAttribute("Max", DoubleValue(1500));   
}

// This section of the code is cleaning the socket used 
UdpFloodApp::~UdpFloodApp()
{
    m_socket = 0;
}

// This section of the code is configuring the application parameters
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

// This section of the code is  the calling method when the application is started 
void UdpFloodApp::StartApplication(void)
{
    m_running = true;
    m_packetsSent = 0;
    m_socket->Bind();            
    m_socket->Connect(m_peer);   
    SendPacket();
}

// This section of the code is the calling  method when the application is stopped 
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

// This section of the code is sending packet
void UdpFloodApp::SendPacket(void)
{
    m_packetSize = m_randPacketSize->GetValue();
    Ptr<Packet> packet = Create<Packet>(m_packetSize);

    Ipv4Header ipHeader;
    // This section of the code is setting up ip spoofing 
    if (m_spoof)  
    {
        ipHeader.SetSource(m_srcIp);
    }
    ipHeader.SetDestination(InetSocketAddress::ConvertFrom(m_peer).GetIpv4());

    //This section of the code is including an IP header 
    packet->AddHeader(ipHeader);

    // This section of the code is logging information about the packets that have been sent 
    NS_LOG_INFO("Sending UDP packet from " << m_srcIp << " to " << InetSocketAddress::ConvertFrom(m_peer).GetIpv4() 
                 << " with packet size: " << m_packetSize);

    // sending the packets
    m_socket->Send(packet); 

    //This section of the code is sending more packets when it is requirements 
    if (++m_packetsSent < m_nPackets)
    {
        ScheduleTx();
    }
}

// This section of the code is scheduling the next packet transmission based on packet size and the data rate
void UdpFloodApp::ScheduleTx(void)
{
    if (m_running)
    {
        double packetDuration = static_cast<double>(m_packetSize * 8) / m_dataRate.GetBitRate(); 
        Time tNext = Seconds(packetDuration);  
        m_sendEvent = Simulator::Schedule(tNext, &UdpFloodApp::SendPacket, this);
    }
}

// This section of the code is defining and setting up the ICMP Flood class  with Variable Data Rates
class IcmpFloodApp : public Application
{
public:
    IcmpFloodApp();
    virtual ~IcmpFloodApp();

    // This section of the code is configuring all the flood parameters
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

//This section of the code is initialising the variables 
IcmpFloodApp::IcmpFloodApp()
    : m_socket(0), m_peer(), m_nPackets(0), m_dataRate(0), m_sendEvent(), 
      m_running(false), m_packetsSent(0), m_srcIp(Ipv4Address("0.0.0.0")), m_spoof(false)
{
}

// This section of the code is cleaning the sockets 
IcmpFloodApp::~IcmpFloodApp()
{
    m_socket = 0;
}

// This section of the code is configuring the parameters for the application
void IcmpFloodApp::Setup(Ptr<Socket> socket, Address address, uint32_t nPackets, DataRate dataRate, Ipv4Address srcIp, bool spoof)
{
    m_socket = socket;
    m_peer = address;
    m_nPackets = nPackets;
    m_dataRate = dataRate;
    m_srcIp = srcIp;
    m_spoof = spoof;
}

// This section of the code is setting up the method to call when the application is started 
void IcmpFloodApp::StartApplication(void)
{
    m_running = true;
    m_packetsSent = 0;
    m_socket->Bind();            
    m_socket->Connect(m_peer);   
    SendPacket();                
}

// This section of the code is calling the method when the application is stopped 
void IcmpFloodApp::StopApplication(void)
{
    m_running = false;

    if (m_sendEvent.IsPending())

    {
        //This section of the code is  stopping all scheduled transmissions 
        Simulator::Cancel(m_sendEvent);  
    }

    if (m_socket)
    {
        m_socket->Close();  
    }
}

// This section of the code is  sending packets 
void IcmpFloodApp::SendPacket(void)
{
    Ptr<Packet> packet = Create<Packet>(64);  

    Icmpv4Echo echo;
    echo.SetSequenceNumber(m_packetsSent);  
    echo.SetIdentifier(0x1234);

    //  This section of the code is adding ip header for spoofing
    if (m_spoof)
    {
        Ipv4Header ipHeader;
        ipHeader.SetSource(m_srcIp);
        packet->AddHeader(ipHeader);
    }

    //This section of the code is adding the icmp header 
    packet->AddHeader(echo);  

    // This section of the code is logging information about packets that have been sent
    NS_LOG_INFO("Sending ICMP Echo Request from " << m_srcIp << " to " << InetSocketAddress::ConvertFrom(m_peer).GetIpv4());

    //This section of the code is sending the packets 
    m_socket->Send(packet);  

    // This section of the code is scheduling the next packet in a case where more packets are to be sent
    if (++m_packetsSent < m_nPackets)
    {
        ScheduleTx();
    }
}

// This section of the code is scheduling the next packet transmission based on the data rate
void IcmpFloodApp::ScheduleTx(void)
{
    if (m_running)
    {
        Time tNext(Seconds(64 * 8 / static_cast<double>(m_dataRate.GetBitRate())));  
        m_sendEvent = Simulator::Schedule(tNext, &IcmpFloodApp::SendPacket, this);
    }
}

// This section of the code is the main function with the network topology where all the classes defined above are used 
int main(int argc, char *argv[]) {
    //  This section of the code is logging components for debugging 
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
    LogComponentEnable("OnOffApplication", LOG_LEVEL_INFO);
    LogComponentEnable("PacketSink", LOG_LEVEL_INFO);
    LogComponentEnable("FlowMonitor", LOG_LEVEL_INFO);
    LogComponentEnable("EnhancedFloodAttack", LOG_LEVEL_INFO);

    // This section of the code is creating nodes for smart home devices, router, internet, and the apache server
    Ptr<Node> smartCamera = CreateObject<Node>();
    Ptr<Node> router = CreateObject<Node>();
    Ptr<Node> smartTV = CreateObject<Node>();
    Ptr<Node> smartSpeaker = CreateObject<Node>();
    Ptr<Node> smartThermostat = CreateObject<Node>();
    Ptr<Node> internetNode = CreateObject<Node>();  
    Ptr<Node> apacheServer = CreateObject<Node>();  

   //This section of the code is putting all the nodes into a container 
    NodeContainer homeNodes;
    homeNodes.Add(smartCamera);
    homeNodes.Add(router);
    homeNodes.Add(smartTV);
    homeNodes.Add(smartSpeaker);
    homeNodes.Add(smartThermostat);

    // This section of the code is setting up the mobility for the nodes 
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator", "MinX", DoubleValue(0.0), "MinY", DoubleValue(0.0), 
                                  "DeltaX", DoubleValue(5.0), "DeltaY", DoubleValue(10.0), "GridWidth", UintegerValue(3), 
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(homeNodes);
    mobility.Install(internetNode);
    mobility.Install(apacheServer);

    //This section of the code is setting up a point-to-point link between the router and the internet 
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("10ms"));

    NetDeviceContainer p2pDevices = pointToPoint.Install(router, internetNode);

    // This section of the code is is setting up  wifi for the  smart home network
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper();
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211g);
    wifi.SetRemoteStationManager("ns3::AarfWifiManager");

    WifiMacHelper mac;
    Ssid ssid = Ssid("ns-3-ssid");

    // This section of the code is setting wifi on the router 
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevice = wifi.Install(phy, mac, router);

    // This section of the code is setting up WiFi on the smart devices 
    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices;
    staDevices.Add(wifi.Install(phy, mac, smartCamera));
    staDevices.Add(wifi.Install(phy, mac, smartTV));
    staDevices.Add(wifi.Install(phy, mac, smartSpeaker));
    staDevices.Add(wifi.Install(phy, mac, smartThermostat));
    NetDeviceContainer apacheDevice = wifi.Install(phy, mac, apacheServer);

    // This section of the code is setting up internet  on the nodes
    InternetStackHelper stack;
    stack.Install(homeNodes);
    stack.Install(internetNode);
    stack.Install(apacheServer);

    // This section of the code is assigning ip address for the point-to-point and WiFi 
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer p2pInterfaces = address.Assign(p2pDevices);
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);
    Ipv4InterfaceContainer apacheInterface = address.Assign(apacheDevice);

    // This section of the code is creating random variables for port numbers and data rates
    Ptr<UniformRandomVariable> randPort = CreateObject<UniformRandomVariable>();
    randPort->SetAttribute("Min", DoubleValue(1024));
    randPort->SetAttribute("Max", DoubleValue(65535));

    Ptr<UniformRandomVariable> randDataRate = CreateObject<UniformRandomVariable>();
    randDataRate->SetAttribute("Min", DoubleValue(1));  
    randDataRate->SetAttribute("Max", DoubleValue(100));  

    // This section of the code is configuring syn flood from smartCamera with variable data rate
    uint16_t srcPortCamera = randPort->GetValue();
    uint16_t destPortCamera = randPort->GetValue();
    DataRate dataRateCamera = DataRate(randDataRate->GetValue() * 1000000);  
    Ptr<Socket> synSocketCamera = Socket::CreateSocket(smartCamera, TcpSocketFactory::GetTypeId());
    Address apacheAddressCamera(InetSocketAddress(apacheInterface.GetAddress(0), destPortCamera));
    Ptr<TcpSynFloodApp> synFloodAppCamera = CreateObject<TcpSynFloodApp>();
    synFloodAppCamera->Setup(synSocketCamera, apacheAddressCamera, 1000000, dataRateCamera, 
                             Ipv4Address("192.168.1.100"), srcPortCamera, true, TcpHeader::SYN, 512);
    smartCamera->AddApplication(synFloodAppCamera);
    synFloodAppCamera->SetStartTime(Seconds(2.0));
    synFloodAppCamera->SetStopTime(Seconds(300.0));

    //This section of the code is configuring udp flood using the smart camera
    uint16_t srcUdpPortCamera = randPort->GetValue();
    uint16_t destUdpPortCamera = randPort->GetValue();
    DataRate udpDataRateCamera = DataRate(randDataRate->GetValue() * 1000000);
    Ptr<Socket> udpSocketCamera = Socket::CreateSocket(smartCamera, UdpSocketFactory::GetTypeId());
    Ptr<UdpFloodApp> udpFloodAppCamera = CreateObject<UdpFloodApp>();
    udpFloodAppCamera->Setup(udpSocketCamera, InetSocketAddress(apacheInterface.GetAddress(0), destUdpPortCamera), 
                             512, 1000000, udpDataRateCamera, Ipv4Address("192.168.1.101"), srcUdpPortCamera, true);
    smartCamera->AddApplication(udpFloodAppCamera);
    udpFloodAppCamera->SetStartTime(Seconds(95.0));
    udpFloodAppCamera->SetStopTime(Seconds(245.0));

    // This section of the code is configuring icmp flood using the smart camera 
    Ptr<Socket> icmpSocketCamera = Socket::CreateSocket(smartCamera, Ipv4RawSocketFactory::GetTypeId());
    icmpSocketCamera->SetAttribute("Protocol", UintegerValue(1));  
    DataRate icmpDataRateCamera = DataRate(randDataRate->GetValue() * 1000000);
    Address targetAddressCamera(InetSocketAddress(apacheInterface.GetAddress(0), 1));
    Ptr<IcmpFloodApp> icmpFloodAppCamera = CreateObject<IcmpFloodApp>();
    icmpFloodAppCamera->Setup(icmpSocketCamera, targetAddressCamera, 10000, icmpDataRateCamera, Ipv4Address("192.168.1.100"), true);
    smartCamera->AddApplication(icmpFloodAppCamera);
    icmpFloodAppCamera->SetStartTime(Seconds(50.0));
    icmpFloodAppCamera->SetStopTime(Seconds(200.0));

    // This section of the code is configuring syn flood with the smart tv 
    uint16_t srcPortTV = randPort->GetValue();
    uint16_t destPortTV = randPort->GetValue();
    DataRate dataRateTV = DataRate(randDataRate->GetValue() * 1000000);  
    Ptr<Socket> synSocketTV = Socket::CreateSocket(smartTV, TcpSocketFactory::GetTypeId());
    Address apacheAddressTV(InetSocketAddress(apacheInterface.GetAddress(0), destPortTV));
    Ptr<TcpSynFloodApp> synFloodAppTV = CreateObject<TcpSynFloodApp>();
    synFloodAppTV->Setup(synSocketTV, apacheAddressTV, 1000000, dataRateTV, 
                         Ipv4Address("192.168.1.102"), srcPortTV, true, TcpHeader::SYN, 512);
    smartTV->AddApplication(synFloodAppTV);
    synFloodAppTV->SetStartTime(Seconds(3.0));
    synFloodAppTV->SetStopTime(Seconds(303.0));

    // This section of the code is configuring udp flood with the smart tv
    uint16_t srcUdpPortTV = randPort->GetValue();
    uint16_t destUdpPortTV = randPort->GetValue();
    DataRate udpDataRateTV = DataRate(randDataRate->GetValue() * 1000000);  
    Ptr<Socket> udpSocketTV = Socket::CreateSocket(smartTV, UdpSocketFactory::GetTypeId());
    Ptr<UdpFloodApp> udpFloodAppTV = CreateObject<UdpFloodApp>();
    udpFloodAppTV->Setup(udpSocketTV, InetSocketAddress(apacheInterface.GetAddress(0), destUdpPortTV), 
                         512, 1000000, udpDataRateTV, Ipv4Address("192.168.1.103"), srcUdpPortTV, true);
    smartTV->AddApplication(udpFloodAppTV);
    udpFloodAppTV->SetStartTime(Seconds(96.0));
    udpFloodAppTV->SetStopTime(Seconds(236.0));

    //This section of the code is configuring  icmp flood with the smart tv
    Ptr<Socket> icmpSocketTV = Socket::CreateSocket(smartTV, Ipv4RawSocketFactory::GetTypeId());
    icmpSocketTV->SetAttribute("Protocol", UintegerValue(1));  
    DataRate icmpDataRateTV = DataRate(randDataRate->GetValue() * 1000000);  
    Address targetAddressTV(InetSocketAddress(apacheInterface.GetAddress(0), 1));
    Ptr<IcmpFloodApp> icmpFloodAppTV = CreateObject<IcmpFloodApp>();
    icmpFloodAppTV->Setup(icmpSocketTV, targetAddressTV, 10000, icmpDataRateTV, Ipv4Address("192.168.1.102"), true);
    smartTV->AddApplication(icmpFloodAppTV);
    icmpFloodAppTV->SetStartTime(Seconds(55.0));
    icmpFloodAppTV->SetStopTime(Seconds(235.0));

    // This section of the code is configuring syn flood with smart speaker 
    uint16_t srcPortSpeaker = randPort->GetValue();
    uint16_t destPortSpeaker = randPort->GetValue();
    DataRate dataRateSpeaker = DataRate(randDataRate->GetValue() * 1000000);  
    Ptr<Socket> synSocketSpeaker = Socket::CreateSocket(smartSpeaker, TcpSocketFactory::GetTypeId());
    Address apacheAddressSpeaker(InetSocketAddress(apacheInterface.GetAddress(0), destPortSpeaker));
    Ptr<TcpSynFloodApp> synFloodAppSpeaker = CreateObject<TcpSynFloodApp>();
    synFloodAppSpeaker->Setup(synSocketSpeaker, apacheAddressSpeaker, 1000000, dataRateSpeaker, 
                              Ipv4Address("192.168.1.104"), srcPortSpeaker, true, TcpHeader::SYN, 512);
    smartSpeaker->AddApplication(synFloodAppSpeaker);
    synFloodAppSpeaker->SetStartTime(Seconds(4.0));
    synFloodAppSpeaker->SetStopTime(Seconds(304.0));

    // This section of the code is configurig udp flood with smart speaker
    uint16_t srcUdpPortSpeaker = randPort->GetValue();
    uint16_t destUdpPortSpeaker = randPort->GetValue();
    DataRate udpDataRateSpeaker = DataRate(randDataRate->GetValue() * 1000000);
    Ptr<Socket> udpSocketSpeaker = Socket::CreateSocket(smartSpeaker, UdpSocketFactory::GetTypeId());
    Ptr<UdpFloodApp> udpFloodAppSpeaker = CreateObject<UdpFloodApp>();
    udpFloodAppSpeaker->Setup(udpSocketSpeaker, InetSocketAddress(apacheInterface.GetAddress(0), destUdpPortSpeaker), 
                              512, 1000000, udpDataRateSpeaker, Ipv4Address("192.168.1.105"), srcUdpPortSpeaker, true);
    smartSpeaker->AddApplication(udpFloodAppSpeaker);
    udpFloodAppSpeaker->SetStartTime(Seconds(97.0));
    udpFloodAppSpeaker->SetStopTime(Seconds(337.0));

    // This section of the code is configuring icmp flood with smart speaker 
    Ptr<Socket> icmpSocketSpeaker = Socket::CreateSocket(smartSpeaker, Ipv4RawSocketFactory::GetTypeId());
    icmpSocketSpeaker->SetAttribute("Protocol", UintegerValue(1));  
    DataRate icmpDataRateSpeaker = DataRate(randDataRate->GetValue() * 1000000);  
    Address targetAddressSpeaker(InetSocketAddress(apacheInterface.GetAddress(0), 1));
    Ptr<IcmpFloodApp> icmpFloodAppSpeaker = CreateObject<IcmpFloodApp>();
    icmpFloodAppSpeaker->Setup(icmpSocketSpeaker, targetAddressSpeaker, 10000, icmpDataRateSpeaker, Ipv4Address("192.168.1.104"), true);
    smartSpeaker->AddApplication(icmpFloodAppSpeaker);
    icmpFloodAppSpeaker->SetStartTime(Seconds(60.0));
    icmpFloodAppSpeaker->SetStopTime(Seconds(200.0));

    // This section of the code is setting up static routing for the network
    Ipv4StaticRoutingHelper staticRouting;
    Ptr<Ipv4StaticRouting> routerStaticRouting = staticRouting.GetStaticRouting(router->GetObject<Ipv4>());
    routerStaticRouting->AddNetworkRouteTo(Ipv4Address("0.0.0.0"), Ipv4Mask("0.0.0.0"), p2pInterfaces.GetAddress(1), 1);

    Ptr<Ipv4StaticRouting> apacheStaticRouting = staticRouting.GetStaticRouting(apacheServer->GetObject<Ipv4>());
    apacheStaticRouting->AddNetworkRouteTo(Ipv4Address("10.1.1.0"), Ipv4Mask("255.255.255.0"), p2pInterfaces.GetAddress(0), 1);

    // This section of the code is stopping the simulation at the set time 
    Simulator::Stop(Seconds(200.0));

    //This section of the code is setting up packet sink on Apache server to receive attack traffic
    PacketSinkHelper apacheServerHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), 80));
    ApplicationContainer apacheServerApp = apacheServerHelper.Install(apacheServer);
    apacheServerApp.Start(Seconds(1.0));
    apacheServerApp.Stop(Seconds(200.0));

    // This section of the code is setting flow monitoring
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();
    
    //This section of the code is setting pcap tracing to capture traffic into pcap file 
    phy.EnablePcap("/media/sf_Ubuntu/tcp-udp-floodd", apDevice.Get(0));

    // This section of the code is running the simulation
    Simulator::Run();

    // This section of the code is is setting Serialize flow monitor and saving it an to XML file
    monitor->SerializeToXmlFile("tcp-udp-flood-flow.xml", true, true);

    // This section of the code is terminating the simulation 
    Simulator::Destroy();
    return 0;
}
