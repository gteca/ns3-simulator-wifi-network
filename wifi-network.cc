/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
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
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/ssid.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("wifi-network");

void PopulateARPcache ();

int main (int argc, char *argv[])
{
  bool verbose = true;
  uint32_t nStations = 1;
  bool tracing = true;
  double      simulationTime      = 5; // [seconds]
  uint32_t    packetSize          = 1024;
  double timeInterval = 1.0;
  uint32_t nPackets = 3;

  CommandLine cmd (__FILE__);
  cmd.AddValue ("nStations", "Number of wifi STA devices", nStations);
  cmd.AddValue ("verbose", "Tell echo applications to log if true", verbose);
  cmd.AddValue ("tracing", "Enable pcap tracing", tracing);

  cmd.Parse (argc,argv);

  if (verbose)
    {
      LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
      LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);
    }

  NodeContainer wifiStaNodes;
  wifiStaNodes.Create (nStations);
  NodeContainer wifiApNode;
  wifiApNode.Create(1);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy;
  phy.SetChannel (channel.Create ());

  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211ac);
  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager","DataMode", StringValue ("VhtMcs9"),
                                   "ControlMode", StringValue ("VhtMcs0")
                                  );
  Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/ChannelWidth", UintegerValue (20));
  Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/HtConfiguration/ShortGuardIntervalSupported", BooleanValue (false));

  WifiMacHelper mac;
  Ssid ssid = Ssid ("Krakow-Wifi");
  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid),
               "ActiveProbing", BooleanValue (false));

  NetDeviceContainer staDevices;
  staDevices = wifi.Install (phy, mac, wifiStaNodes);

  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid));

  NetDeviceContainer apDevices;
  apDevices = wifi.Install (phy, mac, wifiApNode);

  MobilityHelper mobility;
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (wifiStaNodes);
  mobility.Install (wifiApNode);


  InternetStackHelper stack;
  stack.Install (wifiApNode);
  stack.Install (wifiStaNodes);

  Ipv4AddressHelper address;
  address.SetBase ("192.168.0.0", "255.255.255.0");
  address.Assign (staDevices);
  Ipv4InterfaceContainer apInterfaces;
  apInterfaces = address.Assign (apDevices);

  uint16_t serverPort = 9;
  UdpServerHelper udpServer (serverPort);
  ApplicationContainer serverApp = udpServer.Install (wifiApNode.Get(0));
  serverApp.Start (Seconds (1.0));
  serverApp.Stop (Seconds (simulationTime + 1));

  UdpClientHelper udpClient (apInterfaces.GetAddress(0), serverPort);
  udpClient.SetAttribute ("MaxPackets", UintegerValue (nPackets));
  udpClient.SetAttribute ("Interval", TimeValue (Seconds (timeInterval)));
  udpClient.SetAttribute ("PacketSize", UintegerValue (packetSize));
  ApplicationContainer clientApps = udpClient.Install (wifiStaNodes.Get(0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (simulationTime + 1));

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
  PopulateARPcache();

  Simulator::Stop (Seconds (simulationTime + 1));

  if (tracing)
    {
      phy.SetPcapDataLinkType (WifiPhyHelper::DLT_IEEE802_11_RADIO);
      phy.EnablePcap ("wifi-sta", staDevices.Get (0));
    }

  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}

void PopulateARPcache () 
{
	Ptr<ArpCache> arp = CreateObject<ArpCache> ();
	arp->SetAliveTimeout (Seconds (3600 * 24 * 365) );

	for (NodeList::Iterator i = NodeList::Begin (); i != NodeList::End (); ++i)
	{
		Ptr<Ipv4L3Protocol> ip = (*i)->GetObject<Ipv4L3Protocol> ();
		NS_ASSERT (ip !=0);
		ObjectVectorValue interfaces;
		ip->GetAttribute ("InterfaceList", interfaces);

		for (ObjectVectorValue::Iterator j = interfaces.Begin (); j != interfaces.End (); j++)
		{
			Ptr<Ipv4Interface> ipIface = (*j).second->GetObject<Ipv4Interface> ();
			NS_ASSERT (ipIface != 0);
			Ptr<NetDevice> device = ipIface->GetDevice ();
			NS_ASSERT (device != 0);
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
		NS_ASSERT (ip !=0);
		ObjectVectorValue interfaces;
		ip->GetAttribute ("InterfaceList", interfaces);

		for (ObjectVectorValue::Iterator j = interfaces.Begin (); j != interfaces.End (); j ++)
		{
			Ptr<Ipv4Interface> ipIface = (*j).second->GetObject<Ipv4Interface> ();
			ipIface->SetAttribute ("ArpCache", PointerValue (arp) );
		}
	}
}
