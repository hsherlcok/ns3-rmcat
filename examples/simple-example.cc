/******************************************************************************
 * Copyright 2016-2017 cisco Systems, Inc.                                    *
 *                                                                            *
 * Licensed under the Apache License, Version 2.0 (the "License");            *
 * you may not use this file except in compliance with the License.           *
 * You may obtain a copy of the License at                                    *
 *                                                                            *
 *     http://www.apache.org/licenses/LICENSE-2.0                             *
 *                                                                            *
 * Unless required by applicable law or agreed to in writing, software        *
 * distributed under the License is distributed on an "AS IS" BASIS,          *
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.   *
 * See the License for the specific language governing permissions and        *
 * limitations under the License.                                             *
 ******************************************************************************/

/**
 * @file
 * Simple example demonstrating the usage of the rmcat ns3 module, using:
 *  - NADA as controller for rmcat flows
 *  - Statistics-based traffic source as codec
 *  - [Optionally] TCP flows
 *  - [Optionally] UDP flows
 *
 * @version 0.1.1
 * @author Jiantao Fu
 * @author Sergio Mena
 * @author Xiaoqing Zhu
 */

#include "ns3/nada-controller.h"
#include "ns3/rmcat-sender.h"
#include "ns3/rmcat-receiver.h"
#include "ns3/rmcat-constants.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/data-rate.h"
#include "ns3/bulk-send-helper.h"
#include "ns3/packet-sink-helper.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/traffic-control-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/core-module.h"
#include "ns3/ipv4-global-routing-helper.h"

#include "ns3/gcc-node.h"
#include <string>

// Maybe Ignore it 
const uint32_t GCC_DEFAULT_RMIN  =  150000;  // in bps: 150Kbps
const uint32_t GCC_DEFAULT_RMAX  = 1500000;  // in bps: 1.5Mbps

// TODO SHOULD MODIFY THIS BUT DON'T KNOW EXACT INITIAL VALUE.
const uint32_t GCC_DEFAULT_RINIT =  150000;  // in bps: 150Kbps (r_init)

const uint32_t TOPO_DEFAULT_BW     = 1000000;    // in bps: 1Mbps
const uint32_t TOPO_DEFAULT_PDELAY =      50;    // in ms:   50ms
const uint32_t TOPO_DEFAULT_QDELAY =     300;    // in ms:  300ms

using namespace ns3;

static NodeContainer BuildExampleTopo (uint64_t bps,
                                       uint32_t msDelay,
                                       uint32_t msQdelay)
{
    NodeContainer nodes;
    nodes.Create(4);

    NodeContainer n0n1;
    n0n1.Add (nodes.Get(0));
    n0n1.Add (nodes.Get(1));
 
    NodeContainer n1n2;
    n1n2.Add (nodes.Get (1));
    n1n2.Add (nodes.Get (2));
    
    NodeContainer n2n3;
    n2n3.Add (nodes.Get (2));
    n2n3.Add (nodes.Get (3));
    
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute ("DataRate", DataRateValue  (DataRate (bps)));
    p2p.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (msDelay)));
    auto bufSize = std::max<uint32_t> (DEFAULT_PACKET_SIZE, bps * msQdelay / 8000);
    p2p.SetQueue ("ns3::DropTailQueue",
                           "Mode", StringValue ("QUEUE_MODE_BYTES"),
                           "MaxBytes", UintegerValue (bufSize));
  
    NetDeviceContainer dev0 = p2p.Install (n0n1);
    NetDeviceContainer dev1 = p2p.Install (n1n2);
    NetDeviceContainer dev2 = p2p.Install (n2n3);
    
    InternetStackHelper internet;
    internet.InstallAll ();
    Ipv4AddressHelper ipv4;
    ipv4.SetBase ("10.1.1.0", "255.255.255.0");
    ipv4.Assign (dev0);
    ipv4.SetBase ("10.1.2.0", "255.255.255.0");
    ipv4.Assign (dev1);
    ipv4.SetBase ("10.1.3.0", "255.255.255.0");
    ipv4.Assign (dev2);
    Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
    
    // disable tc for now, some bug in ns3 causes extra delay
    TrafficControlHelper tch;
    tch.Uninstall (dev0);
    tch.Uninstall (dev1);
    tch.Uninstall (dev2);
    
    return nodes;
}

static void InstallTCP (Ptr<Node> sender,
                        Ptr<Node> receiver,
                        uint16_t port,
                        float startTime,
                        float stopTime)
{
    // configure TCP source/sender/client
    auto serverAddr = receiver->GetObject<Ipv4> ()->GetAddress (1,0).GetLocal ();
    BulkSendHelper source{"ns3::TcpSocketFactory",
                           InetSocketAddress{serverAddr, port}};
    // Set the amount of data to send in bytes. Zero is unlimited.
    source.SetAttribute ("MaxBytes", UintegerValue (0));
    source.SetAttribute ("SendSize", UintegerValue (DEFAULT_PACKET_SIZE));

    auto clientApps = source.Install (sender);
    clientApps.Start (Seconds (startTime));
    clientApps.Stop (Seconds (stopTime));

    // configure TCP sink/receiver/server
    PacketSinkHelper sink{"ns3::TcpSocketFactory",
                           InetSocketAddress{Ipv4Address::GetAny (), port}};

    auto serverApps = sink.Install (receiver);
    serverApps.Start (Seconds (startTime));
    serverApps.Stop (Seconds (stopTime));

}

static Time GetIntervalFromBitrate (uint64_t bitrate, uint32_t packetSize)
{
    if (bitrate == 0u) {
        return Time::Max ();
    }
    const auto secs = static_cast<double> (packetSize + IPV4_UDP_OVERHEAD) /
                            (static_cast<double> (bitrate) / 8. );
    return Seconds (secs);
}

static void InstallUDP (Ptr<Node> sender,
                        Ptr<Node> receiver,
                        uint16_t serverPort,
                        uint64_t bitrate,
                        uint32_t packetSize,
                        uint32_t startTime,
                        uint32_t stopTime)
{
    // configure UDP source/sender/client
    auto serverAddr = receiver->GetObject<Ipv4> ()->GetAddress (1,0).GetLocal ();
    const auto interPacketInterval = GetIntervalFromBitrate (bitrate, packetSize);
    uint32_t maxPacketCount = 0XFFFFFFFF;
    UdpClientHelper client{serverAddr, serverPort};
    client.SetAttribute ("MaxPackets", UintegerValue (maxPacketCount));
    client.SetAttribute ("Interval", TimeValue (interPacketInterval));
    client.SetAttribute ("PacketSize", UintegerValue (packetSize));

    auto clientApps = client.Install (sender);
    clientApps.Start (Seconds (startTime));
    clientApps.Stop (Seconds (stopTime));

    // configure TCP sink/receiver/server
    UdpServerHelper server{serverPort};
    auto serverApps = server.Install (receiver);
    serverApps.Start (Seconds (startTime));
    serverApps.Stop (Seconds (stopTime));
}

static void InstallGccApps (Ptr<Node> node_1,
                         Ptr<Node> node_2,
                         uint16_t port_1,
                         uint16_t port_2,
                         float startTime,
                         float stopTime)
{
    Ptr<GccNode> app_1 = CreateObject<GccNode> ();
    Ptr<GccNode> app_2 = CreateObject<GccNode> ();
   
    node_1->AddApplication (app_1);
    node_2->AddApplication (app_2);
 
    app_1->SetUp (port_1,1000000); 
    app_2->SetUp (port_2,0); 
    
    Ptr<Ipv4> ipv4_1 = node_1->GetObject<Ipv4> ();
    Ptr<Ipv4> ipv4_2 = node_2->GetObject<Ipv4> ();

    Ipv4Address ipAdd_1 = ipv4_1->GetAddress (1, 0).GetLocal ();
    Ipv4Address ipAdd_2 = ipv4_2->GetAddress (1, 0).GetLocal ();

    app_1->SetDest(ipAdd_2, port_2);
    app_2->SetDest(ipAdd_1, port_1);

    //app_1->AddMulStream(4, 0);
    //app_2->AddMulStream(4, 0);
    
    const auto fps = 30.;		// Set Video Fps.
    auto innerCodec_1 = new syncodecs::StatisticsCodec{fps};
    auto innerCodec_2 = new syncodecs::StatisticsCodec{fps};
    auto codec_1 = new syncodecs::ShapedPacketizer{innerCodec_1, DEFAULT_PACKET_SIZE};
    auto codec_2 = new syncodecs::ShapedPacketizer{innerCodec_2, DEFAULT_PACKET_SIZE};
    app_1->SetCodec (std::shared_ptr<syncodecs::Codec>{codec_1});
    app_2->SetCodec (std::shared_ptr<syncodecs::Codec>{codec_2});

    app_1->SetStartTime (Seconds (startTime));
    app_1->SetStopTime (Seconds (stopTime));
    
    app_2->SetStartTime (Seconds (startTime));
    app_2->SetStopTime (Seconds (stopTime));
}

int main (int argc, char *argv[])
{
    // Number of Flows 
    int nRmcat = 2;
    int nTcp = 0;
    int nUdp = 0;
    bool log = true;
    std::string mode = "gcc";
    
    CommandLine cmd;
    cmd.AddValue ("rmcat", "Number of rmcat (GCC) flows", nRmcat);
    cmd.AddValue ("tcp", "Number of TCP flows", nTcp);
    cmd.AddValue ("udp",  "Number of UDP flows", nUdp);
    cmd.AddValue ("log", "Turn on logs", log);
    cmd.AddValue ("mode", "nada/gcc/vcc", mode);   // Default is declared in rmcat-sender.cc
    cmd.Parse (argc, argv);

    if (log) {
        LogComponentEnable ("GccNode", LOG_LEVEL_ALL);
        LogComponentEnable ("GfpHeader", LOG_LEVEL_ALL);
        LogComponentEnable ("GccReceiverController", LOG_LEVEL_ALL);
        LogComponentEnable ("GccSenderController", LOG_LEVEL_ALL);
        LogComponentEnable ("TcpSocketBase", LOG_LEVEL_ALL);
    }

    // configure default TCP parameters
    Config::SetDefault ("ns3::TcpSocket::DelAckCount", UintegerValue (0));
    Config::SetDefault ("ns3::TcpL4Protocol::SocketType", StringValue ("ns3::TcpNewReno"));    // Tcp Type
    Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue (1000));

    const uint64_t linkBw   = TOPO_DEFAULT_BW;
    const uint32_t msDelay  = TOPO_DEFAULT_PDELAY;
    const uint32_t msQDelay = TOPO_DEFAULT_QDELAY;

    const float minBw =  GCC_DEFAULT_RMIN;
    const float maxBw =  GCC_DEFAULT_RMAX;
    const float initBw = GCC_DEFAULT_RINIT;

    const float endTime = 500.;

    NodeContainer nodes = BuildExampleTopo (linkBw, msDelay, msQDelay);

    int port = 8000;
    for (int i = 0; i < nRmcat; i++) {
        auto start = 10. * i;
        auto end = std::max (start + 1., endTime - start);

        uint16_t port_1 = port++;
        uint16_t port_2 = port++;
        if(mode == "gcc")
          InstallGccApps (nodes.Get (0), nodes.Get (3), port_1, port_2, start, end);
    }

    for (int i = 0; i < nTcp; i++) {
        auto start = 17+ 17.*i;
        auto end = std::max (start + 1., endTime - start);
        InstallTCP (nodes.Get (0), nodes.Get (3), port++, start, end);
    }

    // UDP parameters
    const uint64_t bandwidth = GCC_DEFAULT_RMAX / 4;
    const uint32_t pktSize = DEFAULT_PACKET_SIZE;

    for (int i = 0; i < nUdp; i++) {
        auto start = 23+23. * i;
        auto end = std::max (start + 1., endTime - start);
        InstallUDP (nodes.Get (0), nodes.Get (3), port++,
                    bandwidth, pktSize, start, end);
    }
   
    std::cout << "Running Simulation..." << std::endl;
    Simulator::Stop (Seconds (endTime));
    Simulator::Run ();
    Simulator::Destroy ();
    std::cout << "Done" << std::endl;

    return 0;
}