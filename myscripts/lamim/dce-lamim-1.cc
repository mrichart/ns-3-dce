/*
* This simulation tests the modification of the rate using prc-monitor and prc-pep.
* A client is connected to 1 AP and tranmist data with a CBR of 54Mb/s
*/

#include "ns3/dce-module.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/stats-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/gnuplot.h"

#include <sstream>
#include <fstream>


using namespace ns3;
using namespace std;

NS_LOG_COMPONENT_DEFINE ("LamimExperiment");

class ThroughputCounter
{
public:
  ThroughputCounter();

  void RxCallback (std::string path, Ptr<const Packet> packet, const Address &from);
  void CheckThroughput ();
  Gnuplot2dDataset GetDatafile();

  uint32_t bytesTotal;
  Gnuplot2dDataset m_output;
};

ThroughputCounter::ThroughputCounter() : m_output ("Throughput Mbit/s")
{
  bytesTotal = 0;
  m_output.SetStyle (Gnuplot2dDataset::LINES);
}

void
ThroughputCounter::RxCallback (std::string path, Ptr<const Packet> packet, const Address &from)
{
  bytesTotal += packet->GetSize();
}

void
ThroughputCounter::CheckThroughput()
{
  double mbs = ((bytesTotal * 8.0) /100000);
  bytesTotal = 0;
  m_output.Add ((Simulator::Now ()).GetSeconds (), mbs);
  Simulator::Schedule (Seconds (0.1), &ThroughputCounter::CheckThroughput, this);
}

Gnuplot2dDataset
ThroughputCounter::GetDatafile()
{ return m_output; }


int main (int argc, char *argv[])
{
  uint32_t nAp = 1;
  
  LogComponentEnable("PrcMonitor", LOG_LEVEL_INFO);
  LogComponentEnable("PrcPep", LOG_LEVEL_INFO);


  // Define the APs
  NodeContainer wifiApNodes;
  wifiApNodes.Create (nAp);

  //Define the STAs
  NodeContainer wifiStaNodes;
  wifiStaNodes.Create(1);

  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", StringValue ("100Mbps"));
  csma.SetChannelAttribute ("Delay", TimeValue (NanoSeconds (6560)));

  //Connect all APs
  NetDeviceContainer csmaApDevices;
  csmaApDevices = csma.Install (wifiApNodes);

  WifiHelper wifi = WifiHelper::Default ();

  NqosWifiMacHelper wifiMac = NqosWifiMacHelper::Default ();

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();

  //Create a channel helper in a default working state. By default, we create a channel model with a propagation delay equal to a constant, the speed of light,
  // and a propagation loss based on a log distance model with a reference loss of 46.6777 dB at reference distance of 1m
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
  wifiPhy.SetChannel (wifiChannel.Create ());

  wifi.SetStandard(WIFI_PHY_STANDARD_80211g);

  NetDeviceContainer wifiApDevices;
  NetDeviceContainer wifiStaDevices;
  NetDeviceContainer wifiDevices;

  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager", "DataMode",StringValue ("ErpOfdmRate24Mbps"));
  wifiPhy.Set("TxPowerStart", DoubleValue(17));
  wifiPhy.Set("TxPowerEnd", DoubleValue(17));

  // setup sta0 and ap0.
  Ssid ssid = Ssid ("AP0");
  wifiMac.SetType ("ns3::StaWifiMac",
		  	  	  "Ssid", SsidValue (ssid),
		  	  	  "ActiveProbing", BooleanValue (false));
  wifiStaDevices.Add(wifi.Install (wifiPhy, wifiMac, wifiStaNodes.Get(0)));

  wifi.SetRemoteStationManager ("ns3::RuleBasedWifiManager",
		  	  	  	  	  	  	 "DefaultTxPowerLevel", UintegerValue(17));
  wifiPhy.Set("TxPowerStart", DoubleValue(0));
  wifiPhy.Set("TxPowerEnd", DoubleValue(17));
  wifiPhy.Set("TxPowerLevels", UintegerValue(18));

  wifiMac.SetType ("ns3::ApWifiMac",
		  	  	  "Ssid", SsidValue (ssid));
  wifiApDevices.Add(wifi.Install (wifiPhy, wifiMac, wifiApNodes.Get(0)));

  wifiDevices.Add(wifiStaDevices);
  wifiDevices.Add(wifiApDevices);

  // mobility.
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  positionAlloc->Add (Vector (0.0, 0.0, 0.0));
  positionAlloc->Add (Vector (20.0, 0.0, 0.0));
  mobility.SetPositionAllocator (positionAlloc);
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (wifiApNodes.Get(0));
  mobility.Install (wifiStaNodes.Get(0));

  InternetStackHelper stack;
  stack.Install (wifiApNodes);
  stack.Install (wifiStaNodes);

  Ipv4AddressHelper address;

  address.SetBase ("10.1.0.0", "255.255.255.0");
  address.Assign (csmaApDevices);

  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i = address.Assign (wifiDevices);

  Ipv4Address sinkAddress = i.GetAddress(0);
  uint16_t port = 9;

  PacketSinkHelper sink ("ns3::UdpSocketFactory", InetSocketAddress (sinkAddress, port));
  ApplicationContainer apps_sink = sink.Install (wifiStaNodes.Get (0));

  OnOffHelper onoff ("ns3::UdpSocketFactory", InetSocketAddress (sinkAddress, port));
  onoff.SetConstantRate (DataRate ("54Mb/s"), 1420);
  onoff.SetAttribute ("StartTime", TimeValue (Seconds (10.0)));
  onoff.SetAttribute ("StopTime", TimeValue (Seconds (100.0)));
  ApplicationContainer apps_source = onoff.Install (wifiApNodes.Get (0));

  apps_sink.Start (Seconds (2.0));
  //apps_sink.Stop (Seconds (44.0));

  //
  // Create one PrcMonitor application
  //
  Ptr<PrcMonitor> monitor = CreateObject<PrcMonitor> ();
  wifiApNodes.Get (0)->AddApplication (monitor);
  monitor->SetStartTime (Seconds (10.0));
  monitor->SetStopTime (Seconds (100.0));

  Ptr<PrcPep> pep = CreateObject<PrcPep> ();
  wifiApNodes.Get (0)->AddApplication (pep);
  pep->SetStartTime (Seconds (10.0));
  pep->SetStopTime (Seconds (100.0));

  DceManagerHelper dceManager;

  dceManager.SetTaskManagerAttribute ("FiberManagerType",
                                      StringValue ("UcontextFiberManager"));
  dceManager.Install (wifiApNodes);

  DceApplicationHelper dce;
  ApplicationContainer apps;

  dce.SetStackSize (1 << 20);

  dce.SetBinary ("./lua");
  dce.ResetArguments ();
  dce.AddArgument ("rnr/rnr.lua");
  dce.AddArgument ("rnr/config_lamim_NS3.txt");
  apps = dce.Install (wifiApNodes);
  apps.Start (Seconds (4.0));

  dce.SetBinary ("./lua");
  dce.ResetArguments ();
  dce.AddArgument ("lupa/lupa.lua");
  dce.AddArgument ("lupa/config_lamim_NS3.txt");
  apps = dce.Install (wifiApNodes);
  apps.Start (Seconds (5.0));

  dce.SetBinary ("./lua");
  dce.ResetArguments ();
  dce.AddArgument ("lupa/tests/test_lupa_ns3Apps_setfsm.lua");
  apps = dce.Install (wifiApNodes.Get(0));
  apps.Start (Seconds (10.0));

//  dce.SetBinary ("./lua");
//  dce.ResetArguments ();
//  dce.AddArgument ("env-interface.lua");
//  apps = dce.Install (wifiApNodes);
//  apps.Start (Seconds (30.0));


  //------------------------------------------------------------
    //-- Setup stats and data collection
    //--------------------------------------------

  ThroughputCounter* throughputCounter = new ThroughputCounter();

  Config::Connect ("/NodeList/1/ApplicationList/*/$ns3::PacketSink/Rx",
    				MakeCallback (&ThroughputCounter::RxCallback, throughputCounter));

  throughputCounter->CheckThroughput();


  // Calculate Throughput using Flowmonitor
  //
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> fMonitor = flowmon.InstallAll();
//
  Simulator::Stop (Seconds (100.0));
  Simulator::Run ();


    //monitor->CheckForLostPackets ();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = fMonitor->GetFlowStats ();
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
  	Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
      if ((t.sourceAddress=="10.1.1.2" && t.destinationAddress == "10.1.1.1"))
      {
    	NS_LOG_UNCOND("Flow " << i->first  << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n");
        NS_LOG_UNCOND("  Tx Bytes:   " << i->second.txBytes << "\n");
        NS_LOG_UNCOND("  Rx Bytes:   " << i->second.rxBytes << "\n");
        NS_LOG_UNCOND("  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds())/1024/1024  << " Mbps\n");
      }
    }

  std::ofstream outfile ("throughput.plt");
  Gnuplot gnuplot = Gnuplot("th.eps", "Throughput");
  gnuplot.SetTerminal("post eps enhanced");
  gnuplot.AddDataset (throughputCounter->GetDatafile());
  gnuplot.GenerateOutput (outfile);

  Simulator::Destroy ();

  return 0;
}
