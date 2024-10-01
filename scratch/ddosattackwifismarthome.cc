// Importing the neccessary ns3 libraries
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/random-variable-stream.h"
#include "ns3/config.h"
#include "ns3/tcp-socket-factory.h"


using namespace ns3;

NS_LOG_COMPONENT_DEFINE("SmartHomeNetwork");

int main(int argc, char *argv[]) {
    
    // Enabling logging for individual components
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
    LogComponentEnable("OnOffApplication", LOG_LEVEL_INFO);
    LogComponentEnable("PacketSink", LOG_LEVEL_INFO);
    LogComponentEnable("FlowMonitor", LOG_LEVEL_INFO);                                    

    // Setting up the nodes for the smart devices
    Ptr<Node> smartCamera = CreateObject<Node>();
    Ptr<Node> router = CreateObject<Node>();
    Ptr<Node> smartTV = CreateObject<Node>();
    Ptr<Node> smartSpeaker = CreateObject<Node>();
    Ptr<Node> smartThermostat = CreateObject<Node>();

    // Setting up the internet node
    Ptr<Node> internetNode = CreateObject<Node>(); 

    // Setting up the apache server node
    Ptr<Node> apacheServer = CreateObject<Node>();

    // Setting up MTTQ nodes
    Ptr<Node> mqttBroker = CreateObject<Node>();   
    
    
    // Setting up node container to hold all home nodes
    NodeContainer homeNodes;
    homeNodes.Add(smartCamera);
    homeNodes.Add(router);
    homeNodes.Add(smartTV);
    homeNodes.Add(smartSpeaker);
    homeNodes.Add(smartThermostat);

    // Setting up a point-to-point link between router and internet
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("10ms"));

    NetDeviceContainer p2pDevices;
    p2pDevices = pointToPoint.Install(router, internetNode);

    // // Applying an error model to the point-to-point devices
    // Ptr<RateErrorModel> p2pErrorModel = CreateObject<RateErrorModel>();
    // p2pErrorModel->SetAttribute("ErrorRate", DoubleValue(0.0001));

    // // Applying the error model to devices in the point-to-point link
    // p2pDevices.Get(0)->SetAttribute("ReceiveErrorModel", PointerValue(p2pErrorModel));
    // p2pDevices.Get(1)->SetAttribute("ReceiveErrorModel", PointerValue(p2pErrorModel));


    // Setting up WiFi network for the home network
     YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper();
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211g);
    wifi.SetRemoteStationManager("ns3::AarfWifiManager");

    WifiMacHelper mac;
    Ssid ssid = Ssid("ns-3-ssid");

    // Setting up an internet gateway
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevice = wifi.Install(phy, mac, router); 

    // Seting up stations
    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices;
    staDevices.Add(wifi.Install(phy, mac, smartCamera));
    staDevices.Add(wifi.Install(phy, mac, smartTV));
    staDevices.Add(wifi.Install(phy, mac, smartSpeaker));
    staDevices.Add(wifi.Install(phy, mac, smartThermostat));
    NetDeviceContainer apacheDevice = wifi.Install(phy, mac, apacheServer);
    NetDeviceContainer mqttBrokerDevice = wifi.Install(phy, mac, mqttBroker);   


    //setting up mobility model for all nodes
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


    // // Setting up error model to simulate realistic packet losses
    // Ptr<NistErrorRateModel> wifiErrorModel = CreateObject<NistErrorRateModel>();

    // // Applying the error model to the Access Point 
    // Ptr<WifiNetDevice> wifiApDevice = apDevice.Get(0)->GetObject<WifiNetDevice>();
    // Ptr<YansWifiPhy> wifiApPhy = DynamicCast<YansWifiPhy>(wifiApDevice->GetPhy());
    // wifiApPhy->SetErrorRateModel(wifiErrorModel);

    // // Applying the error model to all smart devices
    // for (uint32_t i = 0; i < staDevices.GetN(); ++i) {
    //     Ptr<WifiNetDevice> wifiStaDevice = staDevices.Get(i)->GetObject<WifiNetDevice>();
    //     Ptr<YansWifiPhy> wifiStaPhy = DynamicCast<YansWifiPhy>(wifiStaDevice->GetPhy());
    //     wifiStaPhy->SetErrorRateModel(wifiErrorModel);
    // }


    // Adding Internet stack to all nodes 
    InternetStackHelper stack;
    stack.Install(homeNodes);
    stack.Install(internetNode);
    stack.Install(apacheServer);
    stack.Install(mqttBroker);

    // Assigning IP addresses to all nodes
    Ipv4AddressHelper address;

    // IP addresses for the point-to-point link
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer p2pInterfaces = address.Assign(p2pDevices);

    // IP addresses for the home network
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterface = address.Assign(apDevice);
    Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);

    // Assigning IP address to the Apache server node
    Ipv4InterfaceContainer apacheInterface = address.Assign(apacheDevice);

        // Assigning IP address to the MQTT broker node
    Ipv4InterfaceContainer mqttBrokerInterface = address.Assign(mqttBrokerDevice);


    // Start and Stop times for each application 
    Time simulationDuration = Seconds(600.0); 
    Simulator::Stop(simulationDuration);

    // Simulating MQTT-like Publish-Subscribe System
    uint16_t mqttPort = 1883; // MQTT typically operates on port 1883
    PacketSinkHelper mqttBrokerHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), mqttPort));
    ApplicationContainer mqttBrokerApp = mqttBrokerHelper.Install(mqttBroker);
    mqttBrokerApp.Start(Seconds(1.0));
    mqttBrokerApp.Stop(simulationDuration); // Duration of simulation

    // Smart Thermostat publishes temperature data to MQTT broker
    BulkSendHelper tempPublishHelper("ns3::TcpSocketFactory", InetSocketAddress(mqttBrokerInterface.GetAddress(0), mqttPort));
    tempPublishHelper.SetAttribute("MaxBytes", UintegerValue(0));  // Send indefinitely
    ApplicationContainer tempPublishApp = tempPublishHelper.Install(smartThermostat);
    tempPublishApp.Start(Seconds(2.0));
    tempPublishApp.Stop((simulationDuration));

    // Printing assigned IP addresses for each device
    NS_LOG_INFO("Assigned IP addresses:");
    NS_LOG_INFO("Smart Camera IP Address: " << staInterfaces.GetAddress(0));
    NS_LOG_INFO("Smart TV IP Address: " << staInterfaces.GetAddress(1));
    NS_LOG_INFO("Smart Speaker IP Address: " << staInterfaces.GetAddress(2));
    NS_LOG_INFO("Smart Thermostat IP Address: " << staInterfaces.GetAddress(3));
    NS_LOG_INFO("Apache Server IP Address: " << apacheInterface.GetAddress(0));
    NS_LOG_INFO("Router (AP) IP Address: " << apInterface.GetAddress(0));
    NS_LOG_INFO("Internet Node IP Address (p2p link): " << p2pInterfaces.GetAddress(1));
    NS_LOG_INFO("Router IP Address (p2p link): " << p2pInterfaces.GetAddress(0));

    std::cout << "Assigned IP addresses:\n";
    std::cout << "Smart Camera IP Address: " << staInterfaces.GetAddress(0) << std::endl;
    std::cout << "Smart TV IP Address: " << staInterfaces.GetAddress(1) << std::endl;
    std::cout << "Smart Speaker IP Address: " << staInterfaces.GetAddress(2) << std::endl;
    std::cout << "Smart Thermostat IP Address: " << staInterfaces.GetAddress(3) << std::endl;
    std::cout << "Apache Server IP Address: " << apacheInterface.GetAddress(0) << std::endl;
    std::cout << "Router (AP) IP Address: " << apInterface.GetAddress(0) << std::endl;
    std::cout << "Internet Node IP Address (p2p link): " << p2pInterfaces.GetAddress(1) << std::endl;
    std::cout << "Router IP Address (p2p link): " << p2pInterfaces.GetAddress(0) << std::endl;

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

    // Static routing for Apache server node
    Ptr<Ipv4StaticRouting> apacheStaticRouting = staticRouting.GetStaticRouting(apacheServer->GetObject<Ipv4>());
    apacheStaticRouting->AddNetworkRouteTo(Ipv4Address("10.1.1.0"), Ipv4Mask("255.255.255.0"), apInterface.GetAddress(0), 1);


    // Apache server application configuration
    uint16_t apachePort = 80; // Default HTTP port
    PacketSinkHelper apacheServerHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), apachePort));
    ApplicationContainer apacheServerApp = apacheServerHelper.Install(apacheServer);
    apacheServerApp.Start(Seconds(1.0));
    apacheServerApp.Stop(simulationDuration);

    // Smart TV as a client sending requests to Apache server
    BulkSendHelper clientHelper("ns3::TcpSocketFactory", InetSocketAddress(apacheInterface.GetAddress(0), apachePort));
    clientHelper.SetAttribute("MaxBytes", UintegerValue(0)); // Send data until the application stops

    ApplicationContainer clientApps = clientHelper.Install(smartTV);
    clientApps.Start(Seconds(2.0)); 
    clientApps.Stop(simulationDuration); 

// Smart Camera Video Streaming 
uint16_t videoPort = 5001;
Address videoServerAddress(InetSocketAddress(p2pInterfaces.GetAddress(1), videoPort));

// Setting up the video server on the internet node
PacketSinkHelper videoSinkHelper("ns3::TcpSocketFactory", videoServerAddress);
ApplicationContainer videoServerApps = videoSinkHelper.Install(internetNode);
videoServerApps.Start(Seconds(1.0));
videoServerApps.Stop(simulationDuration);

// Set up the video client on the smart camera node
OnOffHelper videoClientHelper("ns3::TcpSocketFactory", videoServerAddress);

//  data rate to simulate 1080p streaming
videoClientHelper.SetAttribute("DataRate", DataRateValue(DataRate("5Mbps")));
videoClientHelper.SetAttribute("PacketSize", UintegerValue(1200)); 

// Simulating any occasional streaming interruptions
videoClientHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=300]")); // Stream for 5 minutes
videoClientHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=60]")); // Pause for 1 minute

ApplicationContainer videoClientApps = videoClientHelper.Install(smartCamera);
videoClientApps.Start(Seconds(2.0));
videoClientApps.Stop(simulationDuration);



// Smart Camera sending status updates 
uint16_t camStatusPort = 6001;
PacketSinkHelper camStatusSink("ns3::TcpSocketFactory", InetSocketAddress(p2pInterfaces.GetAddress(1), camStatusPort));
ApplicationContainer camStatusSinkApp = camStatusSink.Install(internetNode);
camStatusSinkApp.Start(Seconds(1.0));
camStatusSinkApp.Stop(simulationDuration);

OnOffHelper camStatusClient("ns3::TcpSocketFactory", Address(InetSocketAddress(p2pInterfaces.GetAddress(1), camStatusPort)));
camStatusClient.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]")); // Active for 1 second
camStatusClient.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=599]")); // Idle for 9 minutes 59 seconds
camStatusClient.SetAttribute("PacketSize", UintegerValue(64)); 
camStatusClient.SetAttribute("DataRate", StringValue("500bps"));
ApplicationContainer camStatusClientApp = camStatusClient.Install(smartCamera);
camStatusClientApp.Start(Seconds(2.0));
camStatusClientApp.Stop(simulationDuration);

// Motion Detection for smart camera 
uint16_t motionAlertPort = 16001;
UdpServerHelper motionAlertServer(motionAlertPort);
ApplicationContainer motionAlertServerApp = motionAlertServer.Install(internetNode);
motionAlertServerApp.Start(Seconds(1.0));
motionAlertServerApp.Stop(simulationDuration);

// Simulating random motion events
Ptr<ExponentialRandomVariable> motionEventInterval = CreateObject<ExponentialRandomVariable>();
motionEventInterval->SetAttribute("Mean", DoubleValue(600.0)); 

double nextEventTime = 2.0; // Start after 2 seconds
while (nextEventTime < simulationDuration.GetSeconds()) {
    UdpClientHelper motionAlertClient(p2pInterfaces.GetAddress(1), motionAlertPort);
    motionAlertClient.SetAttribute("MaxPackets", UintegerValue(1)); // Single alert packet
    motionAlertClient.SetAttribute("Interval", TimeValue(Seconds(0.0)));
    motionAlertClient.SetAttribute("PacketSize", UintegerValue(128));

    ApplicationContainer motionAlertClientApp = motionAlertClient.Install(smartCamera);
    motionAlertClientApp.Start(Seconds(nextEventTime));
    motionAlertClientApp.Stop(Seconds(nextEventTime + 1.0));
    nextEventTime += motionEventInterval->GetValue();
}


// streaming with smart tv 
uint16_t contentPort = 8001;
Address contentServerAddress(InetSocketAddress(p2pInterfaces.GetAddress(1), contentPort));

//content server on the internet node
PacketSinkHelper contentSinkHelper("ns3::TcpSocketFactory", contentServerAddress);
ApplicationContainer contentServerApps = contentSinkHelper.Install(internetNode);
contentServerApps.Start(Seconds(1.0));
contentServerApps.Stop(simulationDuration);

// content client on the smart TV node
OnOffHelper contentClientHelper("ns3::TcpSocketFactory", contentServerAddress);
contentClientHelper.SetAttribute("DataRate", DataRateValue(DataRate("5Mbps")));
contentClientHelper.SetAttribute("PacketSize", UintegerValue(1500));
contentClientHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=3600]"));
contentClientHelper.SetAttribute("OffTime", StringValue("ns3::ExponentialRandomVariable[Mean=7200]")); 

ApplicationContainer contentClientApps = contentClientHelper.Install(smartTV);
contentClientApps.Start(Seconds(2.0));
contentClientApps.Stop(simulationDuration);


// Smart TV: Control Commands 
uint16_t tvCommandPort = 9001;
PacketSinkHelper tvCommandSink("ns3::TcpSocketFactory", InetSocketAddress(staInterfaces.GetAddress(1), tvCommandPort));
ApplicationContainer tvCommandSinkApp = tvCommandSink.Install(smartTV);
tvCommandSinkApp.Start(Seconds(1.0));
tvCommandSinkApp.Stop(simulationDuration);
OnOffHelper tvCommandClient("ns3::TcpSocketFactory", Address(InetSocketAddress(staInterfaces.GetAddress(1), tvCommandPort)));

//  user interactions with the TV
tvCommandClient.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]")); 
tvCommandClient.SetAttribute("OffTime", StringValue("ns3::ExponentialRandomVariable[Mean=600]")); 
tvCommandClient.SetAttribute("PacketSize", UintegerValue(64));
tvCommandClient.SetAttribute("DataRate", StringValue("1kbps"));
ApplicationContainer tvCommandClientApp = tvCommandClient.Install(internetNode);
tvCommandClientApp.Start(Seconds(2.0));
tvCommandClientApp.Stop(simulationDuration);


// sofware update on the smart tv
uint16_t updatePort = 17001;
PacketSinkHelper updateSink("ns3::TcpSocketFactory", InetSocketAddress(staInterfaces.GetAddress(1), updatePort));
ApplicationContainer updateSinkApp = updateSink.Install(smartTV);
updateSinkApp.Start(Seconds(1.0));
updateSinkApp.Stop(simulationDuration);

BulkSendHelper updateClient("ns3::TcpSocketFactory", InetSocketAddress(staInterfaces.GetAddress(1), updatePort));
updateClient.SetAttribute("MaxBytes", UintegerValue(100 * 1024 * 1024));  
ApplicationContainer updateClientApp = updateClient.Install(internetNode);
updateClientApp.Start(Seconds(2.0));
updateClientApp.Stop(simulationDuration);




// music streaming on the smart speaker
uint16_t musicPort = 10001;

// Music server on the internet 
UdpServerHelper musicServer(musicPort);
ApplicationContainer musicServerApp = musicServer.Install(internetNode);
musicServerApp.Start(Seconds(1.0));
musicServerApp.Stop(simulationDuration);

Ptr<ExponentialRandomVariable> musicSessionInterval = CreateObject<ExponentialRandomVariable>();
musicSessionInterval->SetAttribute("Mean", DoubleValue(3600.0)); 

double sessionStartTime = 2.0;
while (sessionStartTime < simulationDuration.GetSeconds()) {
    double sessionDuration = musicSessionInterval->GetValue();
    sessionDuration = std::min(sessionDuration, simulationDuration.GetSeconds() - sessionStartTime);

    UdpClientHelper musicClient(p2pInterfaces.GetAddress(1), musicPort);
    musicClient.SetAttribute("MaxPackets", UintegerValue(0)); 
    musicClient.SetAttribute("Interval", TimeValue(MilliSeconds(20))); 
    musicClient.SetAttribute("PacketSize", UintegerValue(512)); 

    ApplicationContainer musicClientApp = musicClient.Install(smartSpeaker);
    musicClientApp.Start(Seconds(sessionStartTime));
    musicClientApp.Stop(Seconds(sessionStartTime + sessionDuration));
    sessionStartTime += sessionDuration + 3600.0; 
}


// voice commands to the smart speaker
uint16_t voiceCommandPort = 11001;
PacketSinkHelper voiceCommandSink("ns3::TcpSocketFactory", InetSocketAddress(staInterfaces.GetAddress(2), voiceCommandPort));
ApplicationContainer voiceCommandSinkApp = voiceCommandSink.Install(smartSpeaker);
voiceCommandSinkApp.Start(Seconds(1.0));
voiceCommandSinkApp.Stop(simulationDuration);
OnOffHelper voiceCommandClient("ns3::TcpSocketFactory", Address(InetSocketAddress(staInterfaces.GetAddress(2), voiceCommandPort)));
voiceCommandClient.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=5]"));  
voiceCommandClient.SetAttribute("OffTime", StringValue("ns3::ExponentialRandomVariable[Mean=600]"));
voiceCommandClient.SetAttribute("PacketSize", UintegerValue(512)); 
voiceCommandClient.SetAttribute("DataRate", StringValue("64kbps")); 
ApplicationContainer voiceCommandClientApp = voiceCommandClient.Install(internetNode);
voiceCommandClientApp.Start(Seconds(2.0));
voiceCommandClientApp.Stop(simulationDuration);

// status update on the snmart speaker 
uint16_t speakerStatusPort = 12001;
PacketSinkHelper speakerStatusSink("ns3::TcpSocketFactory", InetSocketAddress(p2pInterfaces.GetAddress(1), speakerStatusPort));
ApplicationContainer speakerStatusSinkApp = speakerStatusSink.Install(internetNode);
speakerStatusSinkApp.Start(Seconds(1.0));
speakerStatusSinkApp.Stop(simulationDuration);
OnOffHelper speakerStatusClient("ns3::TcpSocketFactory", Address(InetSocketAddress(p2pInterfaces.GetAddress(1), speakerStatusPort)));
speakerStatusClient.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]")); 
speakerStatusClient.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=299]"));

speakerStatusClient.SetAttribute("PacketSize", UintegerValue(64)); 
speakerStatusClient.SetAttribute("DataRate", StringValue("500bps")); 

ApplicationContainer speakerStatusClientApp = speakerStatusClient.Install(smartSpeaker);
speakerStatusClientApp.Start(Seconds(2.0));
speakerStatusClientApp.Stop(simulationDuration);

// Smart Speaker streaming 
uint16_t adStreamPort = 18001;
UdpServerHelper adStreamServer(adStreamPort);
ApplicationContainer adStreamServerApp = adStreamServer.Install(smartSpeaker); 
adStreamServerApp.Start(Seconds(1.0));
adStreamServerApp.Stop(simulationDuration);
Ptr<UniformRandomVariable> adInterval = CreateObject<UniformRandomVariable>();
adInterval->SetAttribute("Min", DoubleValue(300.0)); 
adInterval->SetAttribute("Max", DoubleValue(600.0)); 

double currentTime = 120.0; 
while (currentTime < simulationDuration.GetSeconds()) {
    uint32_t adStreamPacketSize = 1024;
    uint32_t maxAdStreamPacketCount = 400; 
    Time adStreamInterPacketInterval = Seconds(0.05); 

    UdpClientHelper adStreamClient(p2pInterfaces.GetAddress(1), adStreamPort);
    adStreamClient.SetAttribute("MaxPackets", UintegerValue(maxAdStreamPacketCount));
    adStreamClient.SetAttribute("Interval", TimeValue(adStreamInterPacketInterval));
    adStreamClient.SetAttribute("PacketSize", UintegerValue(adStreamPacketSize));

    ApplicationContainer adStreamClientApp = adStreamClient.Install(internetNode);
    adStreamClientApp.Start(Seconds(currentTime));
    adStreamClientApp.Stop(Seconds(currentTime + adStreamInterPacketInterval.GetSeconds() * maxAdStreamPacketCount));
    currentTime += adInterval->GetValue();
}

// Sending temperature updates 
uint16_t tempPort = 13001;
PacketSinkHelper tempSink("ns3::TcpSocketFactory", InetSocketAddress(p2pInterfaces.GetAddress(1), tempPort));
ApplicationContainer tempServerApp = tempSink.Install(internetNode);
tempServerApp.Start(Seconds(1.0));
tempServerApp.Stop(simulationDuration);

OnOffHelper tempClient("ns3::TcpSocketFactory", Address(InetSocketAddress(p2pInterfaces.GetAddress(1), tempPort)));
tempClient.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
tempClient.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=59]"));  // Idle for 59 seconds

tempClient.SetAttribute("PacketSize", UintegerValue(128));  // Small temperature update packet
tempClient.SetAttribute("DataRate", StringValue("1kbps"));  // Low data rate

ApplicationContainer tempClientApp = tempClient.Install(smartThermostat);
tempClientApp.Start(Seconds(2.0));
tempClientApp.Stop(simulationDuration);


uint16_t thermostatCommandPort = 14001;
PacketSinkHelper thermostatCommandSink("ns3::TcpSocketFactory", InetSocketAddress(staInterfaces.GetAddress(3), thermostatCommandPort));
ApplicationContainer thermostatCommandSinkApp = thermostatCommandSink.Install(smartThermostat);
thermostatCommandSinkApp.Start(Seconds(1.0));
thermostatCommandSinkApp.Stop(simulationDuration);

OnOffHelper thermostatCommandClient("ns3::TcpSocketFactory", Address(InetSocketAddress(staInterfaces.GetAddress(3), thermostatCommandPort)));


thermostatCommandClient.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]")); // Command duration (1 second)
thermostatCommandClient.SetAttribute("OffTime", StringValue("ns3::ExponentialRandomVariable[Mean=3600]")); // Average 1 hour between commands

thermostatCommandClient.SetAttribute("PacketSize", UintegerValue(64)); // Small command packet
thermostatCommandClient.SetAttribute("DataRate", StringValue("1kbps"));

ApplicationContainer thermostatCommandClientApp = thermostatCommandClient.Install(internetNode);
thermostatCommandClientApp.Start(Seconds(2.0));
thermostatCommandClientApp.Stop(simulationDuration);

// updates on smart thermoststat
uint16_t thermostatStatusPort = 15001;
PacketSinkHelper thermostatStatusSink("ns3::TcpSocketFactory", InetSocketAddress(p2pInterfaces.GetAddress(1), thermostatStatusPort));
ApplicationContainer thermostatStatusSinkApp = thermostatStatusSink.Install(internetNode);
thermostatStatusSinkApp.Start(Seconds(1.0));
thermostatStatusSinkApp.Stop(simulationDuration);

OnOffHelper thermostatStatusClient("ns3::TcpSocketFactory", Address(InetSocketAddress(p2pInterfaces.GetAddress(1), thermostatStatusPort)));
thermostatStatusClient.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
thermostatStatusClient.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=3599]")); /

thermostatStatusClient.SetAttribute("PacketSize", UintegerValue(64)); 
thermostatStatusClient.SetAttribute("DataRate", StringValue("500bps"));

ApplicationContainer thermostatStatusClientApp = thermostatStatusClient.Install(smartThermostat);
thermostatStatusClientApp.Start(Seconds(2.0));
thermostatStatusClientApp.Stop(simulationDuration);

// reporting energy usage on the smart thermostat 
uint16_t energyReportPort = 19001;
PacketSinkHelper energyReportServer("ns3::TcpSocketFactory", InetSocketAddress(p2pInterfaces.GetAddress(1), energyReportPort));
ApplicationContainer energyReportServerApp = energyReportServer.Install(internetNode);
energyReportServerApp.Start(Seconds(1.0));
energyReportServerApp.Stop(simulationDuration);
uint32_t energyReportSize = 500 * 1024; // 500 KB report
BulkSendHelper energyReportClient("ns3::TcpSocketFactory", InetSocketAddress(p2pInterfaces.GetAddress(1), energyReportPort));
energyReportClient.SetAttribute("MaxBytes", UintegerValue(energyReportSize));

ApplicationContainer energyReportClientApp = energyReportClient.Install(smartThermostat);
energyReportClientApp.Start(Seconds(86400.0)); 
energyReportClientApp.Stop(simulationDuration);


    // Broadcast Messages for Network Discovery
  uint16_t broadcastPort = 20001;
    UdpServerHelper broadcastServer(broadcastPort);
    ApplicationContainer broadcastServerApp = broadcastServer.Install(router);
    broadcastServerApp.Start(Seconds(1.0));
    broadcastServerApp.Stop(Seconds(600.0));

    UdpClientHelper broadcastClient(Ipv4Address("192.168.1.255"), broadcastPort);
    broadcastClient.SetAttribute("MaxPackets", UintegerValue(200));
    broadcastClient.SetAttribute("Interval", TimeValue(Seconds(0.5)));
    broadcastClient.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer broadcastClientApp = broadcastClient.Install(homeNodes);
    broadcastClientApp.Start(Seconds(2.0));
    broadcastClientApp.Stop(simulationDuration);


    // Setup flow monitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    //  pcap tracing for the access point Pcap for the router
   
    pointToPoint.EnablePcap("/media/sf_Ubuntu/New_pcap/try/internet_nodee", p2pDevices.Get(1));
    pointToPoint.EnablePcap("/media/sf_Ubuntu/New_pcap/try/router_p2pp", p2pDevices.Get(0));


    // Run the simulation
    Simulator::Run();

    // Print flow monitor statistics
    flowmon.SerializeToXmlFile("smart-home-network-flow.xml", true, true);

    Simulator::Destroy();

    return 0;
}
