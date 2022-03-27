#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/flow-monitor-module.h"

#include <iostream>
#include <fstream>
#include <vector>
#include <string>

/* Default Network Topology IBSS (Independent Basic Service Set)
   Wifi 10.1.1.0
                      AP
  *    *    *   ...   *
  |    |    |   ...   |
 n0   n1   n2   ...  n4 (AP will be always the last node in right side,but node 0 in inner topology created by wifiApNode)
  C                  S
  C = CLIENT         S = Server
  X = Number of nodes
*/
using namespace ns3;                            
using namespace std;

NS_LOG_COMPONENT_DEFINE ("WifiNetwork"); 

int main (int argc, char *argv[])
{
    bool       verbose             = true;
    bool       tracing             = false;
    double     simulationTime      = 10; 
    uint32_t   payloadSize         = 1024;
    uint64_t   numberOfPackets     = 10000;
    double     offeredLoad         = 0;
    double     intervalSize        = 0;
    int        nStations           = 2;
    bool       showDetails         = true;

    CommandLine cmd;
    cmd.AddValue ("nStations", "Number of wifi STATION devices", nStations);
    cmd.AddValue ("verbose", "Tell echo applications to log messages about their activities if true", verbose);
    cmd.AddValue ("tracing", "Enable pcap tracing", tracing);
    cmd.AddValue ("simulationTime", "Time of simulation", simulationTime);
    cmd.AddValue ("payLoadSize", "The size of Packet to be send by client", payloadSize);
    cmd.Parse (argc,argv);

    if (nStations == 0 || nStations > 254)
    {
      cout << "Something is wrong, check nStations parameter " <<endl;
      return 1;
    }

    offeredLoad  = 10*1000*1000/nStations;
    intervalSize = (payloadSize*8)/(offeredLoad);

    if (verbose)
    {
     LogComponentEnableAll(LOG_PREFIX_NODE);
     LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
     LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);
    }

    NodeContainer wifiStaNodes;
    wifiStaNodes.Create (nStations);
    NodeContainer wifiApNode ;
    wifiApNode.Create(1);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
    YansWifiPhyHelper phy;
    phy.SetChannel (channel.Create ());

    WifiHelper wifi;
    wifi.SetStandard (WIFI_STANDARD_80211ac);
    wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager","DataMode", StringValue ("VhtMcs9"),
                                   "ControlMode", StringValue ("VhtMcs0")
                                  );
    Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/ChannelWidth", UintegerValue (80));
      
    WifiMacHelper mac;
    Ssid ssid = Ssid ("covert-wifi");
    mac.SetType ("ns3::StaWifiMac",
                 "Ssid", SsidValue (ssid),
                 "ActiveProbing", BooleanValue (false)
                 );

    NetDeviceContainer staDevices;
    staDevices = wifi.Install (phy, mac, wifiStaNodes);

    mac.SetType ("ns3::ApWifiMac",
                "Ssid", SsidValue (ssid),
                "BeaconGeneration", BooleanValue (true),
                "BeaconInterval", TimeValue (Seconds (0.1024))
                );

    NetDeviceContainer apDevices;
    apDevices = wifi.Install (phy, mac, wifiApNode);
    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/HtConfiguration/ShortGuardIntervalSupported", BooleanValue (true));
  
    
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
    positionAlloc->Add (Vector (0.0, 0.0, 0.0));
    positionAlloc->Add (Vector (1, 0.0, 0.0));
    mobility.SetPositionAllocator (positionAlloc);
    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    mobility.Install (wifiStaNodes);
    mobility.Install (wifiApNode);

    InternetStackHelper stack;
    stack.Install (wifiApNode);
    stack.Install (wifiStaNodes);

    Ipv4AddressHelper address;
    address.SetBase ("10.1.1.0", "255.255.255.0");
    address.Assign (staDevices);
    Ipv4InterfaceContainer apInterfaces;
    apInterfaces =  address.Assign (apDevices);

    
     uint16_t port = 9;
     UdpServerHelper Server (port);
     ApplicationContainer serverApp = Server.Install (wifiApNode.Get (0));
     serverApp.Start (Seconds (1.0));
     serverApp.Stop (Seconds (simulationTime + 1.0));
    
     UdpClientHelper Client (apInterfaces.GetAddress (0), 9);
     Client.SetAttribute ("MaxPackets", UintegerValue (numberOfPackets));
     Client.SetAttribute ("Interval", TimeValue (Seconds (intervalSize)));
     Client.SetAttribute ("PacketSize", UintegerValue (payloadSize ));

      for(int iterator = 0 ; iterator < nStations; ++iterator)
      {
       ApplicationContainer clientApp [nStations];
       clientApp[iterator]= Client.Install (wifiStaNodes.Get (nStations - (iterator+1)));
       clientApp[iterator].Start (Seconds (2.0));
       clientApp[iterator].Stop  (Seconds(simulationTime + 1.0));
      }

      Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

      if (tracing == true)
      {
        phy.EnablePcap ("covertWifi", staDevices);
      }

     FlowMonitorHelper flowmon;
     Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

     Simulator::Stop (Seconds (simulationTime + 1));

     GlobalValue::Bind ("ChecksumEnabled", BooleanValue (true));
     Simulator::Run ();

     monitor->CheckForLostPackets ();
     Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
     FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();

     double total_throughput = 0;
     for (map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {

          if (i->first > 0)
          {
            Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
            cout <<"\n";
            cout << " Flow " << i->first <<" From (Station)"<< " with IP : " << t.sourceAddress << " To (AP) with address : " << t.destinationAddress <<"\n";
            if(showDetails)
            {
            cout << "  Tx Packets: " << i->second.txPackets << "\n";
            cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
            cout << "  TxOffered:  " << i->second.txBytes * 8.0 / simulationTime / 1000 / 1000  << " Mbps\n";
            cout << "  Rx Packets: " << i->second.rxPackets << "\n";
            cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
           }
           total_throughput = total_throughput + i->second.rxBytes * 8.0 / simulationTime / 1000 / 1000;
       }
    }
     cout << "Offered = "<< offeredLoad <<endl;
     cout << "Throughput = " << total_throughput <<endl;

   Simulator::Destroy ();
   return 0;
}