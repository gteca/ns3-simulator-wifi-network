/*
 * Copyright (c) 2016 SEBASTIEN DERONNE
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
 * Author: Sebastien Deronne <sebastien.deronne@gmail.com>
 */

#include "ns3/boolean.h"
#include "ns3/command-line.h"
#include "ns3/config.h"
#include "ns3/double.h"
#include "ns3/enum.h"
#include "ns3/he-phy.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/log.h"
#include "ns3/mobility-helper.h"
#include "ns3/multi-model-spectrum-channel.h"
#include "ns3/on-off-helper.h"
#include "ns3/packet-sink-helper.h"
#include "ns3/packet-sink.h"
#include "ns3/rng-seed-manager.h"
#include "ns3/spectrum-wifi-helper.h"
#include "ns3/ssid.h"
#include "ns3/string.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/uinteger.h"
#include "ns3/wifi-acknowledgment.h"
#include "ns3/yans-wifi-channel.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ip-l4-protocol.h"
#include "ns3/sta-wifi-mac.h"
#include "ns3/wifi-net-device.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"

#include <functional>

#include <iostream>
#include <fstream>
// This is a simple example in order to show how to configure an IEEE 802.11ax Wi-Fi network.
//
// It outputs the UDP or TCP goodput for every HE MCS value, which depends on the MCS value (0 to
// 11), the channel width (20, 40, 80 or 160 MHz) and the guard interval (800ns, 1600ns or 3200ns).
// The PHY bitrate is constant over all the simulation run. The user can also specify the distance
// between the access point and the station: the larger the distance the smaller the goodput.
//
// The simulation assumes a configurable number of stations in an infrastructure network:
//
//  STA     AP
//    *     *
//    |     |
//   n1     n2
//
// Packets in this simulation belong to BestEffort Access Class (AC_BE).
// By selecting an acknowledgment sequence for DL MU PPDUs, it is possible to aggregate a
// Round Robin scheduler to the AP, so that DL MU PPDUs are sent by the AP via DL OFDMA.

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("he-wifi-network");

Ptr<StaWifiMac> GetFirstStaWifiMac(NodeContainer& sta);
void PopulateARPcache ();

int
main(int argc, char* argv[])
{
    bool udp{false};
    bool downlink{true};
    bool useRts{false};
    bool useExtendedBlockAck{false};
    double simulationTime{10}; // seconds
    double distance{1.0};      // meters
    double frequency{5};       // whether 2.4, 5 or 6 GHz
    std::size_t nStations{1};
    std::string dlAckSeqType{"NO-OFDMA"};
    bool enableUlOfdma{false};
    bool enableBsrp{false};
    uint32_t payloadSize {1472}; // must fit in the max TX duration when transmitting at MCS 0 over an RU of 26 tones
    std::string phyModel{"Yans"};
    double minExpectedThroughput{0};
    double maxExpectedThroughput{0};
    Time accessReqInterval{0};
    int channelWidth  {40};
    int mcs {11};
    double load {300 * 1000 * 1000};
    double   offeredLoad = load / nStations; // [Mbps]
    double   timeInterval = (payloadSize * 8)/(offeredLoad);
    bool   displayFlowStats {true};
    bool showStats {true};
    int RgnRun {0};


    CommandLine cmd(__FILE__);
    cmd.AddValue("frequency",
                 "Whether working in the 2.4, 5 or 6 GHz band (other values gets rejected)",
                 frequency);
    cmd.AddValue("distance",
                 "Distance in meters between the station and the access point",
                 distance);
    cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
    cmd.AddValue("udp", "UDP if set to 1, TCP otherwise", udp);
    cmd.AddValue("downlink",
                 "Generate downlink flows if set to 1, uplink flows otherwise",
                 downlink);
    cmd.AddValue("useRts", "Enable/disable RTS/CTS", useRts);
    cmd.AddValue("useExtendedBlockAck", "Enable/disable use of extended BACK", useExtendedBlockAck);
    cmd.AddValue("nStations", "Number of non-AP HE stations", nStations);
    cmd.AddValue("dlAckType",
                 "Ack sequence type for DL OFDMA (NO-OFDMA, ACK-SU-FORMAT, MU-BAR, AGGR-MU-BAR)",
                 dlAckSeqType);
    cmd.AddValue("enableUlOfdma",
                 "Enable UL OFDMA (useful if DL OFDMA is enabled and TCP is used)",
                 enableUlOfdma);
    cmd.AddValue("enableBsrp",
                 "Enable BSRP (useful if DL and UL OFDMA are enabled and TCP is used)",
                 enableBsrp);
    cmd.AddValue(
        "muSchedAccessReqInterval",
        "Duration of the interval between two requests for channel access made by the MU scheduler",
        accessReqInterval);
    cmd.AddValue("mcs", "if set, limit testing to a specific MCS (0-11)", mcs);
    cmd.AddValue("payloadSize", "The application payload size in bytes", payloadSize);
    cmd.AddValue("phyModel",
                 "PHY model to use when OFDMA is disabled (Yans or Spectrum). If OFDMA is enabled "
                 "then Spectrum is automatically selected",
                 phyModel);
    cmd.AddValue("minExpectedThroughput",
                 "if set, simulation fails if the lowest throughput is below this value",
                 minExpectedThroughput);
    cmd.AddValue("maxExpectedThroughput",
                 "if set, simulation fails if the highest throughput is above this value",
                 maxExpectedThroughput);
    cmd.AddValue ("RgnRun", "Create randomness in the simulation", RgnRun);
    cmd.Parse(argc, argv);

    RngSeedManager::SetRun(RgnRun);

    if (useRts)
    {
        Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue("0"));
        Config::SetDefault("ns3::WifiDefaultProtectionManager::EnableMuRts", BooleanValue(true));
    }

    if (dlAckSeqType == "ACK-SU-FORMAT")
    {
        Config::SetDefault("ns3::WifiDefaultAckManager::DlMuAckSequenceType",
                           EnumValue(WifiAcknowledgment::DL_MU_BAR_BA_SEQUENCE));
    }
    else if (dlAckSeqType == "MU-BAR")
    {
        Config::SetDefault("ns3::WifiDefaultAckManager::DlMuAckSequenceType",
                           EnumValue(WifiAcknowledgment::DL_MU_TF_MU_BAR));
    }
    else if (dlAckSeqType == "AGGR-MU-BAR")
    {
        Config::SetDefault("ns3::WifiDefaultAckManager::DlMuAckSequenceType",
                           EnumValue(WifiAcknowledgment::DL_MU_AGGREGATE_TF));
    }
    else if (dlAckSeqType != "NO-OFDMA")
    {
        NS_ABORT_MSG("Invalid DL ack sequence type (must be NO-OFDMA, ACK-SU-FORMAT, MU-BAR or "
                     "AGGR-MU-BAR)");
    }

    if (phyModel != "Yans" && phyModel != "Spectrum")
    {
        NS_ABORT_MSG("Invalid PHY model (must be Yans or Spectrum)");
    }
    if (dlAckSeqType != "NO-OFDMA")
    {
        // SpectrumWifiPhy is required for OFDMA
        phyModel = "Spectrum";
    }

    if (!udp)
    {
        Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(payloadSize));
    }

    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(nStations);
    NodeContainer wifiApNode;
    wifiApNode.Create(1);

    NetDeviceContainer apDevice;
    NetDeviceContainer staDevices;
    WifiMacHelper mac;
    WifiHelper wifi;
    std::string channelStr("{0, " + std::to_string(channelWidth) + ", ");
    StringValue ctrlRate;
    auto nonHtRefRateMbps = HePhy::GetNonHtReferenceRate(mcs) / 1e6;

    std::ostringstream ossDataMode;
    ossDataMode << "HeMcs" << mcs;

    if (frequency == 6)
    {
        wifi.SetStandard(WIFI_STANDARD_80211ax);
        ctrlRate = StringValue(ossDataMode.str());
        channelStr += "BAND_6GHZ, 0}";
        Config::SetDefault("ns3::LogDistancePropagationLossModel::ReferenceLoss",
                            DoubleValue(48));
    }
    else if (frequency == 5)
    {
        wifi.SetStandard(WIFI_STANDARD_80211ax);
        std::ostringstream ossControlMode;
        ossControlMode << "OfdmRate" << nonHtRefRateMbps << "Mbps";
        ctrlRate = StringValue(ossControlMode.str());
        channelStr += "BAND_5GHZ, 0}";
    }
    else if (frequency == 2.4)
    {
        wifi.SetStandard(WIFI_STANDARD_80211ax);
        std::ostringstream ossControlMode;
        ossControlMode << "ErpOfdmRate" << nonHtRefRateMbps << "Mbps";
        ctrlRate = StringValue(ossControlMode.str());
        channelStr += "BAND_2_4GHZ, 0}";
        Config::SetDefault("ns3::LogDistancePropagationLossModel::ReferenceLoss",
                            DoubleValue(40));
    }
    else
    {
        std::cout << "Wrong frequency value!" << std::endl;
        return 0;
    }

    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                    "DataMode",
                                    StringValue(ossDataMode.str()),
                                    "ControlMode",
                                    ctrlRate);
    // Set guard interval and MPDU buffer size
    wifi.ConfigHeOptions("GuardInterval",
                            TimeValue(NanoSeconds(800)),
                            "MpduBufferSize",
                            UintegerValue(useExtendedBlockAck ? 256 : 64));

    Ssid ssid = Ssid("ns3-80211ax");

    if (phyModel == "Spectrum")
    {
        /*
            * SingleModelSpectrumChannel cannot be used with 802.11ax because two
            * spectrum models are required: one with 78.125 kHz bands for HE PPDUs
            * and one with 312.5 kHz bands for, e.g., non-HT PPDUs (for more details,
            * see issue #408 (CLOSED))
            */
        Ptr<MultiModelSpectrumChannel> spectrumChannel =
            CreateObject<MultiModelSpectrumChannel>();

        Ptr<LogDistancePropagationLossModel> lossModel =
            CreateObject<LogDistancePropagationLossModel>();
        spectrumChannel->AddPropagationLossModel(lossModel);

        SpectrumWifiPhyHelper phy;
        phy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);
        phy.SetChannel(spectrumChannel);

        mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid));
        phy.Set("ChannelSettings", StringValue(channelStr));
        staDevices = wifi.Install(phy, mac, wifiStaNodes);

        if (dlAckSeqType != "NO-OFDMA")
        {
            mac.SetMultiUserScheduler("ns3::RrMultiUserScheduler",
                                        "EnableUlOfdma",
                                        BooleanValue(enableUlOfdma),
                                        "EnableBsrp",
                                        BooleanValue(enableBsrp),
                                        "AccessReqInterval",
                                        TimeValue(accessReqInterval));
        }
        mac.SetType("ns3::ApWifiMac",
                    "EnableBeaconJitter",
                    BooleanValue(false),
                    "Ssid",
                    SsidValue(ssid));
        apDevice = wifi.Install(phy, mac, wifiApNode);
    }
    else
    {
        YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
        YansWifiPhyHelper phy;
        phy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);
        phy.SetChannel(channel.Create());

        mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid));
        phy.Set("ChannelSettings", StringValue(channelStr));
        staDevices = wifi.Install(phy, mac, wifiStaNodes);

        mac.SetType("ns3::ApWifiMac",
                    "EnableBeaconJitter",
                    BooleanValue(false),
                    "Ssid",
                    SsidValue(ssid));
        apDevice = wifi.Install(phy, mac, wifiApNode);
    }

    RngSeedManager::SetSeed(1);
    RngSeedManager::SetRun(1);
    int64_t streamNumber = 150;
    streamNumber += wifi.AssignStreams(apDevice, streamNumber);
    streamNumber += wifi.AssignStreams(staDevices, streamNumber);

    Ptr<StaWifiMac> covertStaWifiMac = GetFirstStaWifiMac(wifiStaNodes);
    covertStaWifiMac->SetCovertStatus (true);
    
    // mobility.
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();

    positionAlloc->Add(Vector(0.0, 0.0, 0.0));
    positionAlloc->Add(Vector(distance, 0.0, 0.0));
    mobility.SetPositionAllocator(positionAlloc);

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");

    mobility.Install(wifiApNode);
    mobility.Install(wifiStaNodes);

    /* Internet stack*/
    InternetStackHelper stack;
    stack.Install(wifiApNode);
    stack.Install(wifiStaNodes);

    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer staNodeInterfaces;
    Ipv4InterfaceContainer apNodeInterface;

    staNodeInterfaces = address.Assign(staDevices);
    apNodeInterface = address.Assign(apDevice);

    /* Setting applications */
    ApplicationContainer serverApp;
    auto serverNodes = downlink ? std::ref(wifiStaNodes) : std::ref(wifiApNode);
    Ipv4InterfaceContainer serverInterfaces;
    NodeContainer clientNodes;
    for (std::size_t i = 0; i < nStations; i++)
    {
        serverInterfaces.Add(downlink ? staNodeInterfaces.Get(i)
                                        : apNodeInterface.Get(0));
        clientNodes.Add(downlink ? wifiApNode.Get(0) : wifiStaNodes.Get(i));
    }

    if (udp)
    {
        // UDP flow
        uint16_t port = 9;
        UdpServerHelper server(port);
        serverApp = server.Install(serverNodes.get());
        serverApp.Start(Seconds(0.0));
        serverApp.Stop(Seconds(simulationTime + 1));

        for (std::size_t i = 0; i < nStations; i++)
        {
            UdpClientHelper client(serverInterfaces.GetAddress(i), port);
            client.SetAttribute("MaxPackets", UintegerValue (4294967295U));
            client.SetAttribute("Interval", TimeValue (Seconds(timeInterval))); // packets/s
            client.SetAttribute("PacketSize", UintegerValue (payloadSize));
            ApplicationContainer clientApp = client.Install (clientNodes.Get(i));
            clientApp.Start(Seconds(1.0));
            clientApp.Stop(Seconds(simulationTime + 1));
        }
    }
    else
    {
        // TCP flow
        uint16_t port = 50000;
        Address localAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
        PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", localAddress);
        serverApp = packetSinkHelper.Install(serverNodes.get());
        serverApp.Start(Seconds(0.0));
        serverApp.Stop(Seconds(simulationTime + 1));

        for (std::size_t i = 0; i < nStations; i++)
        {
            OnOffHelper onoff("ns3::TcpSocketFactory", Ipv4Address::GetAny());
            onoff.SetAttribute("OnTime",
                                StringValue("ns3::ConstantRandomVariable[Constant=1]"));
            onoff.SetAttribute("OffTime",
                                StringValue("ns3::ConstantRandomVariable[Constant=0]"));
            onoff.SetAttribute("PacketSize", UintegerValue(payloadSize));
            onoff.SetAttribute("DataRate", DataRateValue(DataRate("300Mbps"))); // bit/s
            AddressValue remoteAddress(
                InetSocketAddress(serverInterfaces.GetAddress(i), port));
            onoff.SetAttribute("Remote", remoteAddress);
            ApplicationContainer clientApp = onoff.Install(clientNodes.Get(i));
            clientApp.Start(Seconds(1.0));
            clientApp.Stop(Seconds(simulationTime + 1));
        }
    }

    Simulator::Schedule(Seconds(0), &Ipv4GlobalRoutingHelper::PopulateRoutingTables);
    PopulateARPcache();

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

    Simulator::Stop(Seconds(simulationTime + 1));
    Simulator::Run();

    double throughput = 0.0;
    double totalThroughput = 0.0;
    double delay = 0.0;
    //std::ofstream outputFile("output.csv", std::ios::app);

    uint64_t rxBytes = 0;
    if (udp)
    {
        for (uint32_t i = 0; i < serverApp.GetN(); i++)
        {
            rxBytes += payloadSize * DynamicCast<UdpServer>(serverApp.Get(i))->GetReceived();
        }
    }
    else
    {
        for (uint32_t i = 0; i < serverApp.GetN(); i++)
        {
            rxBytes += DynamicCast<PacketSink>(serverApp.Get(i))->GetTotalRx();
        }
    }
    double throughput_ = (rxBytes * 8) / (simulationTime * 1000000.0); // Mbit/s


    monitor->CheckForLostPackets ();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();

    if(displayFlowStats)
    {
        for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
        {
            if (i->first > 0)
            {
                Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
                throughput = (rxBytes * 8) / (simulationTime * 1000000.0); // Mbit/s
                delay = i->second.delaySum.GetSeconds () / i->second.rxPackets;
                
                totalThroughput = totalThroughput + throughput;

                if(showStats)
                {
                    std::cout << "+++++++++++++++++++++++++++++++++++++++++++++++++++" << "\n";
                    std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
                    std::cout << "Throughput:  " <<  throughput  << " [Mbps]\n";
                    std::cout << "Tx Packets: " <<  i->second.txPackets << " [Packets] \n";
                    std::cout << "Rx Packets: " <<  i->second.rxPackets << " [Packets] \n";
                    std::cout << "Delay: " << delay  << " [ms]\n";
                    std::cout << "Lost # 1: " <<  i->second.txPackets - i->second.rxPackets << " [Packets] \n";
                    std::cout << "+++++++++++++++++++++++++++++++++++++++++++++++++++" << "\n\n";
                }
            }
        }
    }

    std::cout << "mcs" << "\t\t\t" << "channelWidth [MHz]" << " \t\t\t" << "throughput [Mbit/s]" << std::endl;
    std::cout << mcs << "\t\t\t" << channelWidth << "\t\t\t" << throughput_ << " Mbit/s" << std::endl;

    Simulator::Destroy();
    return 0;
}

void PopulateARPcache () 
{
	Ptr<ArpCache> arp = CreateObject<ArpCache> ();
	arp->SetAliveTimeout (Seconds (3600 * 24 * 365) );

	for (NodeList::Iterator i = NodeList::Begin (); i != NodeList::End (); ++i)
	{
		Ptr<Ipv4L3Protocol> ip = (*i)->GetObject<Ipv4L3Protocol> ();
		NS_ASSERT (ip != nullptr);
		ObjectVectorValue interfaces;
		ip->GetAttribute ("InterfaceList", interfaces);

		for (ObjectVectorValue::Iterator j = interfaces.Begin (); j != interfaces.End (); j++)
		{
			Ptr<Ipv4Interface> ipIface = (*j).second->GetObject<Ipv4Interface> ();
			NS_ASSERT (ipIface != nullptr);
			Ptr<NetDevice> device = ipIface->GetDevice ();
			NS_ASSERT (device != nullptr);
			Mac48Address addr = Mac48Address::ConvertFrom (device->GetAddress () );

			for (uint32_t k = 0; k < ipIface->GetNAddresses (); k++)
			{
				Ipv4Address ipAddr = ipIface->GetAddress (k).GetLocal();
				if (ipAddr == Ipv4Address::GetLoopback ())
					continue;

				ArpCache::Entry *entry = arp->Add (ipAddr);
				Ipv4Header ipv4Hdr;
				ipv4Hdr.SetDestination (ipAddr);
				Ptr<Packet> p = Create<Packet> (100);
				entry->MarkWaitReply (ArpCache::Ipv4PayloadHeaderPair (p, ipv4Hdr));
				entry->MarkAlive (addr);
			}
		}
	}

	for (NodeList::Iterator i = NodeList::Begin (); i != NodeList::End (); ++i)
	{
		Ptr<Ipv4L3Protocol> ip = (*i)->GetObject<Ipv4L3Protocol> ();
		NS_ASSERT (ip != nullptr);
		ObjectVectorValue interfaces;
		ip->GetAttribute ("InterfaceList", interfaces);

		for (ObjectVectorValue::Iterator j = interfaces.Begin (); j != interfaces.End (); j ++)
		{
			Ptr<Ipv4Interface> ipIface = (*j).second->GetObject<Ipv4Interface> ();
			ipIface->SetAttribute ("ArpCache", PointerValue (arp) );
		}
	}
}

Ptr<StaWifiMac> GetFirstStaWifiMac(NodeContainer& ap)
{
  // We assume that covert sta is always the node 0.
  Ptr<NetDevice> netDevice = ap.Get(0)->GetDevice (0);
  Ptr<WifiNetDevice> wifiNetDevice = DynamicCast<WifiNetDevice> (netDevice);
  Ptr<WifiMac> wifiMac = wifiNetDevice->GetMac(); 
  Ptr<StaWifiMac> staMac = DynamicCast<StaWifiMac> (wifiMac);
  return staMac;
}
