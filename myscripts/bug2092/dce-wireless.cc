/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2015 Universidad de la República - Uruguay
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
 * Author: Matias Richart <mrichart@fing.edu.uy>
 */

#include <sstream>
#include <fstream>

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/dce-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"

using namespace ns3;
using namespace std;

NS_LOG_COMPONENT_DEFINE ("DceWireless");

static void
SetPosition (Ptr<Node> node, Vector position)
{
  Ptr<MobilityModel> mobility = node->GetObject<MobilityModel> ();
  mobility->SetPosition (position);
}

static Vector
GetPosition (Ptr<Node> node)
{
  Ptr<MobilityModel> mobility = node->GetObject<MobilityModel> ();
  return mobility->GetPosition ();
}

static void
AdvancePosition (Ptr<Node> node, int stepsSize)
{
  Vector pos = GetPosition (node);
  pos.x += stepsSize;
  SetPosition (node, pos);
  NS_LOG_INFO ("At time " << Simulator::Now ().GetSeconds () << " sec; setting new position to " << pos);
}

int main (int argc, char *argv[])
{
  uint32_t rtsThreshold = 2346;
  int ap1_x = 0;
  int ap1_y = 0;
  int sta1_x = 30;
  int sta1_y = 0;
  uint32_t simuTime = 300;

  CommandLine cmd;
  cmd.AddValue ("simuTime", "Total simulation time (sec)", simuTime);
  cmd.AddValue ("AP1_x", "Position of AP1 in x coordinate", ap1_x);
  cmd.AddValue ("AP1_y", "Position of AP1 in y coordinate", ap1_y);
  cmd.AddValue ("STA1_x", "Position of STA1 in x coordinate", sta1_x);
  cmd.AddValue ("STA1_y", "Position of STA1 in y coordinate", sta1_y);
  cmd.Parse (argc, argv);

  // Define the AP
  NodeContainer wifiApNodes;
  wifiApNodes.Create (1);

  //Define the STA
  NodeContainer wifiStaNodes;
  wifiStaNodes.Create (1);

  WifiHelper wifi = WifiHelper::Default ();
  wifi.SetStandard (WIFI_PHY_STANDARD_80211a);
  NqosWifiMacHelper wifiMac = NqosWifiMacHelper::Default ();
  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();

  wifiPhy.SetChannel (wifiChannel.Create ());

  NetDeviceContainer wifiApDevices;
  NetDeviceContainer wifiStaDevices;
  NetDeviceContainer wifiDevices;

  //Configure the STA node
  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager", "DataMode", StringValue ("OfdmRate6Mbps"), "ControlMode", StringValue ("ErpOfdmRate6Mbps"));

  Ssid ssid = Ssid ("AP0");
  wifiMac.SetType ("ns3::StaWifiMac",
                   "Ssid", SsidValue (ssid),
                   "ActiveProbing", BooleanValue (false),
                   "MaxMissedBeacons", UintegerValue (1000));
  wifiStaDevices.Add (wifi.Install (wifiPhy, wifiMac, wifiStaNodes.Get (0)));

  //Configure the AP node
  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager", "DataMode", StringValue ("OfdmRate54Mbps"),"ControlMode", StringValue ("OfdmRate6Mbps"), "RtsCtsThreshold", UintegerValue (rtsThreshold));
  
  ssid = Ssid ("AP0");
  wifiMac.SetType ("ns3::ApWifiMac",
                   "Ssid", SsidValue (ssid));
  wifiApDevices.Add (wifi.Install (wifiPhy, wifiMac, wifiApNodes.Get (0)));

  wifiDevices.Add (wifiStaDevices);
  wifiDevices.Add (wifiApDevices);

  // Configure the mobility.
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  positionAlloc->Add (Vector (ap1_x, ap1_y, 0.0));
  positionAlloc->Add (Vector (sta1_x, sta1_y, 0.0));
  mobility.SetPositionAllocator (positionAlloc);
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (wifiApNodes.Get (0));
  mobility.Install (wifiStaNodes.Get (0));


  //Configure the IP stack
  InternetStackHelper stack;
  stack.Install (wifiApNodes);
  stack.Install (wifiStaNodes);
  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i = address.Assign (wifiDevices);
  Ipv4Address sinkAddress = i.GetAddress (0);
  uint16_t port = 9; 

  DceManagerHelper dceManager;
  dceManager.Install (wifiApNodes);
  dceManager.Install (wifiStaNodes);

  DceApplicationHelper dce;
  ApplicationContainer apps;
  
  dce.SetBinary ("tcp-server-2");
  dce.ResetArguments ();
  dce.SetStackSize (1 << 16);
  apps = dce.Install (wifiStaNodes.Get (0));
  apps.Start (Seconds (2.0));

  dce.SetBinary ("tcp-client-2");
  dce.ResetArguments ();
  dce.ParseArguments ("10.1.1.1");
  apps = dce.Install (wifiApNodes.Get (0));
  apps.Start (Seconds (2.5));

  Simulator::Schedule (Seconds (2.511), &AdvancePosition, wifiApNodes.Get (0), -50);
  Simulator::Schedule (Seconds (200), &AdvancePosition, wifiApNodes.Get (0), 50);
  
  Simulator::Stop (Seconds (simuTime));
  Simulator::Run ();

  Simulator::Destroy ();

  return 0;
}
