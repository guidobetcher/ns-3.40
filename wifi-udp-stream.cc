/*
 * Copyright (c) 2015, IMDEA Networks Institute
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Hany Assasa <hany.assasa@gmail.com>
.*
 * This is a simple example to test TCP over 802.11n (with MPDU aggregation enabled).
 *
 * Network topology:
 *
 *   Ap    STA
 *   *      *
 *   |      |
 *   n1     n2
 *
 * In this example, an HT station sends TCP packets to the access point.
 * We report the total throughput received during a window of 100ms.
 * The user can specify the application data rate and choose the variant
 * of TCP i.e. congestion control algorithm to use.
 */

#include "ns3/command-line.h"
#include "ns3/config.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/ipv4-list-routing-helper.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/log.h"
#include "ns3/mobility-helper.h"
#include "ns3/mobility-model.h"
#include "ns3/olsr-helper.h"
#include "ns3/on-off-helper.h"
#include "ns3/packet-sink-helper.h"
#include "ns3/packet-sink.h"
#include "ns3/ssid.h"
#include "ns3/string.h"
#include "ns3/tcp-westwood-plus.h"
#include "ns3/yans-wifi-channel.h"
#include "ns3/yans-wifi-helper.h"

NS_LOG_COMPONENT_DEFINE("wifi-tcp");

using namespace ns3;

Ptr<PacketSink> sink;     //!< Pointer to the packet sink application
uint64_t lastTotalRx = 0; //!< The value of the last total received bytes

/**
 * Calculate the throughput
 */
void
CalculateThroughput(double sampleInterval)
{
    Time now = Simulator::Now(); /* Return the simulator's virtual time. */
    double cur = (sink->GetTotalRx() - lastTotalRx) * 8.0 /
                 (sampleInterval * 1e3); /* Convert Application RX Packets to MBits. */
    std::cout << now.GetSeconds() << "s: \t" << cur << " Mbit/s" << std::endl;
    lastTotalRx = sink->GetTotalRx();
    Simulator::Schedule(MilliSeconds(sampleInterval), &CalculateThroughput, sampleInterval);
}

int
main(int argc, char* argv[])
{
    std::string phyMode("DsssRate1Mbps");
    uint32_t payloadSize = 1472;           /* Transport layer payload size in bytes. */
    std::string dataRate = "100Mbps";      /* Application layer datarate. */
    std::string tcpVariant = "TcpNewReno"; /* TCP variant type. */
    std::string phyRate = "HtMcs7";        /* Physical layer bitrate. */
    double simulationTime = 10;            /* Simulation time in seconds. */
    double startMeasureTime = 5;            /* Simulation time in seconds. */   
    double sampleInterval = 100;            /* Simulation time in milliseconds. */
    bool pcapTracing = false;              /* PCAP Tracing is enabled or not. */
    uint32_t numNodes = 3;
    double distance = 100;      // m

    /* Command line argument parser setup. */
    CommandLine cmd(__FILE__);
    cmd.AddValue("phyMode", "Wifi Phy mode", phyMode);
    cmd.AddValue("payloadSize", "Payload size in bytes", payloadSize);
    cmd.AddValue("dataRate", "Application data rate", dataRate);
    cmd.AddValue("tcpVariant",
                 "Transport protocol to use: TcpNewReno, "
                 "TcpHybla, TcpHighSpeed, TcpHtcp, TcpVegas, TcpScalable, TcpVeno, "
                 "TcpBic, TcpYeah, TcpIllinois, TcpWestwood, TcpWestwoodPlus, TcpLedbat ",
                 tcpVariant);
    cmd.AddValue("phyRate", "Physical layer bitrate", phyRate);
    cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
    cmd.AddValue("startMeasureTime", "Start measure time in seconds", startMeasureTime);
    cmd.AddValue("sampleInterval", "Sample interval time in milliseconds", sampleInterval);
    cmd.AddValue("pcap", "Enable/disable PCAP Tracing", pcapTracing);
    cmd.AddValue("numNodes", "number of nodes", numNodes);
    cmd.AddValue("distance", "distance (m)", distance);
    cmd.Parse(argc, argv););

    // /* Configure TCP Options */
    WifiMacHelper wifiMac;
    WifiHelper wifiHelper;
    // wifiHelper.SetStandard(WIFI_STANDARD_80211n);
    wifiHelper.SetStandard(WIFI_STANDARD_80211a);

    /* Set up Legacy Channel */
    YansWifiChannelHelper wifiChannel;
    wifiChannel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    wifiChannel.AddPropagationLoss("ns3::FriisPropagationLossModel", "Frequency", DoubleValue(5e9));

    /* Setup Physical Layer */
    YansWifiPhyHelper wifiPhy;
    wifiPhy.Set("RxGain", DoubleValue(-10));
    wifiPhy.SetChannel(wifiChannel.Create());
    wifiPhy.SetErrorRateModel("ns3::YansErrorRateModel");
    
    wifiHelper.SetRemoteStationManager("ns3::MinstrelWifiManager");

    NodeContainer networkNodes;
    networkNodes.Create(numNodes);
    
    Ptr<Node> sinkNode = networkNodes.Get(0);
    Ptr<Node> sourceNode = networkNodes.Get(numNodes - 1);

    // /* Configure AP */
    Ssid ssid = Ssid("network");

    /* Configure STA */
    wifiMac.SetType("ns3::AdhocWifiMac", "Ssid", SsidValue(ssid));
    NetDeviceContainer devices;
    devices = wifiHelper.Install(wifiPhy, wifiMac, networkNodes);

    /* Mobility model */
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    // positionAlloc->Add(Vector(0.0, 0.0, 0.0));
    // positionAlloc->Add(Vector(distance, 0.0, 0.0));
    // positionAlloc->Add(Vector(2 * distance, 0.0, 0.0));
    for (int i = 0; i < numNodes; i++) {
        positionAlloc->Add(Vector(i * distance, 0.0, 0.0));
    }
    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(networkNodes);

    // Enable OLSR
    // OlsrHelper olsr;
    // Ipv4StaticRoutingHelper staticRouting;
 
    // Ipv4ListRoutingHelper list;
    // list.Add(staticRouting, 0);
    // list.Add(olsr, 10);

    /* Internet stack */
    // InternetStackHelper stack;
    // stack.SetRoutingHelper(list); // has effect on the next Install ()
    // stack.Install(networkNodes);

    Ipv4AddressHelper address;
    NS_LOG_INFO("Assign IP Addresses.");
    address.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces;
    interfaces = address.Assign(devices);
    
    for (int i = 1; i < numNodes; i++) {
        Ipv4StaticRoutingHelper ipv4RoutingHelper;
        // Create static routes from A to C
        Ptr<Ipv4StaticRouting> staticRouting = ipv4RoutingHelper.GetStaticRouting(interfaces.GetAddress(i));
        staticRouting->AddHostRouteTo(Ipv4Address("10.0.0.1"), Ipv4Address("10.0.0." + i), i);
    }
    
    /* Install UDP Receiver on the access point */
    PacketSinkHelper sinkHelper("ns3::UdpSocketFactory",
                                InetSocketAddress(Ipv4Address::GetAny(), 9));
    ApplicationContainer sinkApp = sinkHelper.Install(sinkNode);
    sink = StaticCast<PacketSink>(sinkApp.Get(0));

    /* Install TCP/UDP Transmitter on the station */
    TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
    OnOffHelper server("ns3::UdpSocketFactory", (InetSocketAddress(interfaces.GetAddress(0), 9)));
    server.SetAttribute("PacketSize", UintegerValue(payloadSize));
    server.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    server.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    server.SetAttribute("DataRate", DataRateValue(DataRate(dataRate)));
    ApplicationContainer serverApp = server.Install(sourceNode);
    
    /* Start Applications */
    sinkApp.Start(Seconds(0.0));
    serverApp.Start(Seconds(startMeasureTime - sampleInterval/1000));
    Simulator::Schedule(Seconds(startMeasureTime), &CalculateThroughput, sampleInterval);

    /* Enable Traces */
    if (pcapTracing)
    {
        wifiPhy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);
        wifiPhy.EnablePcap("Devices", devices);
    }

    /* Start Simulation */
    Simulator::Stop(Seconds(simulationTime + startMeasureTime));
    Simulator::Run();

    double averageThroughput = ((sink->GetTotalRx() * 8) / (1e6 * (simulationTime)));

    Simulator::Destroy();

    std::cout << "\nAverage throughput: " << averageThroughput << " Mbit/s" << std::endl;
    return 0;
}
