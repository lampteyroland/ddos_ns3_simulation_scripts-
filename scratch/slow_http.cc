#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/random-variable-stream.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("GoldenEyeAttackSimulation");

class SlowHttpClientApp : public Application
{
public:
  SlowHttpClientApp ();
  virtual ~SlowHttpClientApp ();

  void Setup (Ptr<Socket> socket, Address address, uint32_t packetSize, DataRate dataRate, std::string httpVerb, bool incompleteHeaders, Ipv4Address spoofedIp, uint16_t sourcePort);

private:
  virtual void StartApplication (void);
  virtual void StopApplication (void);

  void SendPacket (void);

  Ptr<Socket>     m_socket;
  Address         m_peer;
  uint32_t        m_packetSize;
  DataRate        m_dataRate;
  EventId         m_sendEvent;
  bool            m_running;
  uint32_t        m_packetsSent;
  std::string     m_httpVerb;
  bool            m_incompleteHeaders;
  Ipv4Address     m_spoofedIp;
  uint16_t        m_sourcePort;
};

SlowHttpClientApp::SlowHttpClientApp ()
  : m_socket (0),
    m_peer (),
    m_packetSize (0),
    m_dataRate (0),
    m_sendEvent (),
    m_running (false),
    m_packetsSent (0),
    m_incompleteHeaders(false),
    m_sourcePort(0)
{
}

SlowHttpClientApp::~SlowHttpClientApp()
{
  m_socket = 0;
}

void
SlowHttpClientApp::Setup (Ptr<Socket> socket, Address address, uint32_t packetSize, DataRate dataRate, std::string httpVerb, bool incompleteHeaders, Ipv4Address spoofedIp, uint16_t sourcePort)
{
  m_socket = socket;
  m_peer = address;
  m_packetSize = packetSize;
  m_dataRate = dataRate;
  m_httpVerb = httpVerb;
  m_incompleteHeaders = incompleteHeaders;
  m_spoofedIp = spoofedIp;
  m_sourcePort = sourcePort;
}

void
SlowHttpClientApp::StartApplication (void)
{
  m_running = true;

  if (m_spoofedIp != Ipv4Address::GetAny())  // Use != to compare Ipv4Address
    {
      // IP Spoofing: Set the source IP and source port manually
      m_socket->Bind(InetSocketAddress(m_spoofedIp, m_sourcePort));  // Bind to unique IP and source port
    }
  else
    {
      m_socket->Bind(InetSocketAddress(Ipv4Address::GetAny(), m_sourcePort));  // Bind to unique source port
    }

  m_socket->Connect(m_peer);
  SendPacket();
}

void
SlowHttpClientApp::StopApplication (void)
{
  m_running = false;

  if (m_sendEvent.IsPending())  // Use IsPending to check for pending events
    {
      Simulator::Cancel(m_sendEvent);
    }

  if (m_socket)
    {
      m_socket->Close();
    }
}

void
SlowHttpClientApp::SendPacket (void)
{
  // Randomize the inter-packet delay and packet size
  Ptr<UniformRandomVariable> randDelay = CreateObject<UniformRandomVariable>();
  Ptr<UniformRandomVariable> randPacketSize = CreateObject<UniformRandomVariable>();

  uint32_t randomSize = randPacketSize->GetValue(64, m_packetSize); // Randomized size of the request
  Time tNext = Seconds(randDelay->GetValue(1.0, 5.0));              // Randomized inter-packet delay between 1 and 5 seconds

  std::ostringstream requestStream;

  if (m_httpVerb == "GET")
    {
      requestStream << "GET / HTTP/1.1\r\n"
                    << "Host: apache-server\r\n"
                    << "User-Agent: GoldenEye-slow-client\r\n";
    }
  else if (m_httpVerb == "POST")
    {
      requestStream << "POST / HTTP/1.1\r\n"
                    << "Host: apache-server\r\n"
                    << "User-Agent: GoldenEye-slow-client\r\n"
                    << "Content-Length: 10000\r\n"
                    << "\r\n"
                    << std::string(randomSize, 'X'); // Simulate POST body
    }

  std::string fullRequest = requestStream.str();

  if (m_incompleteHeaders)
    {
      // Simulate incomplete HTTP headers by truncating part of the request
      randomSize = randPacketSize->GetValue(32, randomSize / 2); // Truncate the size further for incomplete headers
    }

  std::string headerPart = fullRequest.substr(0, randomSize); // Send part of the request

  Ptr<Packet> packet = Create<Packet>((uint8_t*)headerPart.c_str(), randomSize);
  m_socket->Send(packet);

  if (m_running)
    {
      // Schedule the next part of the request
      m_sendEvent = Simulator::Schedule(tNext, &SlowHttpClientApp::SendPacket, this);
    }
}

int main(int argc, char *argv[])
{
  // Enable real-time simulation mode
  GlobalValue::Bind ("SimulatorImplementationType", StringValue ("ns3::RealtimeSimulatorImpl"));

  LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
  LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
  LogComponentEnable("OnOffApplication", LOG_LEVEL_INFO);
  LogComponentEnable("PacketSink", LOG_LEVEL_INFO);
  LogComponentEnable("FlowMonitor", LOG_LEVEL_INFO);

  NodeContainer homeNodes;
  homeNodes.Create(5);
  Ptr<Node> smartCamera = homeNodes.Get(0);
  Ptr<Node> smartTV = homeNodes.Get(1);
  Ptr<Node> smartSpeaker = homeNodes.Get(2);
  Ptr<Node> smartThermostat = homeNodes.Get(3);
  Ptr<Node> router = homeNodes.Get(4);
  Ptr<Node> apacheServer = CreateObject<Node>();
  Ptr<Node> internetNode = CreateObject<Node>();

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("2ms"));
  NetDeviceContainer p2pDevices = p2p.Install(router, internetNode);

  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper();
  wifiPhy.SetChannel(wifiChannel.Create());

  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211g);
  wifi.SetRemoteStationManager("ns3::AarfWifiManager");

  WifiMacHelper wifiMac;
  Ssid ssid = Ssid("ns3-iot");

  wifiMac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
  NetDeviceContainer apDevice = wifi.Install(wifiPhy, wifiMac, router);

  wifiMac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));
  NetDeviceContainer staDevices;
  staDevices.Add(wifi.Install(wifiPhy, wifiMac, smartCamera));
  staDevices.Add(wifi.Install(wifiPhy, wifiMac, smartTV));
  staDevices.Add(wifi.Install(wifiPhy, wifiMac, smartSpeaker));
  staDevices.Add(wifi.Install(wifiPhy, wifiMac, smartThermostat));
  NetDeviceContainer apacheDevice = wifi.Install(wifiPhy, wifiMac, apacheServer);

  InternetStackHelper stack;
  stack.Install(homeNodes);
  stack.Install(apacheServer);
  stack.Install(internetNode);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer p2pInterfaces = address.Assign(p2pDevices);

  address.SetBase("192.168.1.0", "255.255.255.0");
  Ipv4InterfaceContainer apInterfaces = address.Assign(apDevice);
  Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);
  Ipv4InterfaceContainer apacheInterfaces = address.Assign(apacheDevice);

  MobilityHelper mobility;
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(homeNodes);
  mobility.Install(apacheServer);
  mobility.Install(internetNode);

  uint16_t apachePort = 80;
  PacketSinkHelper apacheSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), apachePort));
  ApplicationContainer apacheApp = apacheSinkHelper.Install(apacheServer);
  apacheApp.Start(Seconds(0.0));
  apacheApp.Stop(Seconds(450.0));

  // Simulate multiple connections with incomplete headers, IP spoofing, and randomized behavior
  for (uint32_t i = 0; i < 5; i++)
    {
      // Distribute attacks across different devices
      Ptr<Node> attackNode;
      switch (i % 4) {
        case 0: attackNode = smartCamera; break;
        case 1: attackNode = smartTV; break;
        case 2: attackNode = smartSpeaker; break;
        case 3: attackNode = smartThermostat; break;
      }

      Ptr<Socket> slowHttpSocket = Socket::CreateSocket(attackNode, TcpSocketFactory::GetTypeId());
      Address serverAddress = InetSocketAddress(apacheInterfaces.GetAddress(0), apachePort);

      // Randomly choose between GET and POST for each connection
      Ptr<UniformRandomVariable> randVerb = CreateObject<UniformRandomVariable>();
      std::string httpVerb = randVerb->GetValue(0, 1) > 0.5 ? "GET" : "POST";

      // Randomly decide whether to send incomplete headers
      bool incompleteHeaders = randVerb->GetValue(0, 1) > 0.5;

      // Randomly generate a spoofed IP address
      Ipv4Address spoofedIp = Ipv4Address::GetAny(); // Set to GetAny if you don't want to spoof every connection
      if (randVerb->GetValue(0, 1) > 0.5)
        {
          // Generate a random IP in the range 192.168.1.100-192.168.1.150
          std::string ipString = "192.168.1." + std::to_string(randVerb->GetInteger(100, 150));  // Create std::string
          spoofedIp = Ipv4Address(ipString.c_str());  // Convert std::string to C-string with .c_str()
        }

      // Assign unique source port for each connection
      uint16_t sourcePort = 5000 + i;  // Unique source port

      Ptr<SlowHttpClientApp> slowHttpApp = CreateObject<SlowHttpClientApp>();
      slowHttpApp->Setup(slowHttpSocket, serverAddress, 1024, DataRate("10kbps"), httpVerb, incompleteHeaders, spoofedIp, sourcePort); // Randomized GET/POST
      attackNode->AddApplication(slowHttpApp);
      slowHttpApp->SetStartTime(Seconds(60.0 + i * 5));  // Start each connection with a slight delay
      slowHttpApp->SetStopTime(Seconds(150.0));
    }

  FlowMonitorHelper flowHelper;
  Ptr<FlowMonitor> flowMonitor = flowHelper.InstallAll();

  // Enable PCAP tracing for all devices
  wifiPhy.EnablePcap("goldeneye-slow-http-camera", staDevices.Get(0));
  wifiPhy.EnablePcap("goldeneye-slow-http-tv", staDevices.Get(1));
  wifiPhy.EnablePcap("goldeneye-slow-http-speaker", staDevices.Get(2));
  wifiPhy.EnablePcap("goldeneye-slow-http-thermostat", staDevices.Get(3));

  Simulator::Stop(Seconds(450.0));
  Simulator::Run();

  flowMonitor->SerializeToXmlFile("goldeneye-flow.xml", true, true);
  Simulator::Destroy();

  return 0;
}


