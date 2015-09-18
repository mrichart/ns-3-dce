/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
* Copyright (c) 2009 University of Washington
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

NS_LOG_COMPONENT_DEFINE ("DceRonBoxBox");

using namespace ns3;
using namespace std;

int main (int argc, char *argv[])
{

  CommandLine cmd;

  cmd.Parse (argc, argv);

  NodeContainer fixedNodes;
  fixedNodes.Create(10);

  NodeContainer mobileNodes1;
  mobileNodes1.Create (1);

  NodeContainer allNodes;
  allNodes.Add(fixedNodes);
  allNodes.Add(mobileNodes1);

  /*Create a channel helper in a default working state. By default, we create a channel model with a 
  propagation speed equal to a constant, the speed of light, and a propagation loss based on a log 
  distance model with a reference loss of 46.6777 dB at reference distance of 1m.*/


/*  
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  //phy.Set ("TxPowerEnd", DoubleValue (1.64) );
  //phy.Set ("TxPowerStart", DoubleValue (1.64) );
  phy.SetChannel (channel.Create ());  //All devices will use the same channel
*/

///*
	Config::SetDefault( "ns3::RangePropagationLossModel::MaxRange", DoubleValue( 210.0 ) );
	YansWifiChannelHelper channelHelper;	
	channelHelper.SetPropagationDelay( "ns3::ConstantSpeedPropagationDelayModel" );
	channelHelper.AddPropagationLoss(  "ns3::RangePropagationLossModel" );

	YansWifiPhyHelper phyHelper = YansWifiPhyHelper::Default();
	phyHelper.SetChannel( channelHelper.Create() );		
//*/


  /*The default state is defined as being an Adhoc MAC layer with an ARF rate control algorithm and 
  both objects using their default attribute values. By default, configure MAC and PHY for 802.11a.*/

  WifiHelper wifi = WifiHelper::Default ();

  NqosWifiMacHelper mac = NqosWifiMacHelper::Default (); //we will use non-qos MACs
  
  mac.SetType ("ns3::AdhocWifiMac");

  NetDeviceContainer allDevices = wifi.Install (phyHelper, mac, allNodes);

  //set seed for random variables for models
  /*struct timeval tv;
  uint32_t curtime;
  gettimeofday(&tv, NULL); 
  curtime=(uint32_t)tv.tv_sec;

  SeedManager::SetSeed(curtime);*/


  //Set mobility model

  MobilityHelper mobility_fixed;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  positionAlloc->Add (Vector (-100.0, 5.0, 0.0));
  positionAlloc->Add (Vector (0.0, 10.0, 0.0));
  positionAlloc->Add (Vector (10.0, 10.0, 0.0));
  positionAlloc->Add (Vector (0.0, 0.0, 0.0));
  positionAlloc->Add (Vector (10.0, 0.0, 0.0));

  positionAlloc->Add (Vector (270.0, 5.0, 0.0));
  positionAlloc->Add (Vector (160.0, 10.0, 0.0));
  positionAlloc->Add (Vector (170.0, 10.0, 0.0));
  positionAlloc->Add (Vector (160.0, 0.0, 0.0));
  positionAlloc->Add (Vector (170.0, 0.0, 0.0));

  mobility_fixed.SetPositionAllocator (positionAlloc);
  mobility_fixed.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility_fixed.Install (fixedNodes);

  
  MobilityHelper mobility_rd1;
  mobility_rd1.SetPositionAllocator ("ns3::RandomRectanglePositionAllocator",
    "X", StringValue ("ns3::UniformRandomVariable[Min=10.0|Max=160.0]"),
    "Y", StringValue ("ns3::UniformRandomVariable[Min=-45.0|Max=55.0]"));
  mobility_rd1.SetMobilityModel ("ns3::RandomDirection2dMobilityModel", 
    "Bounds", RectangleValue (Rectangle (10.0, 160.0, -45.0, 55.0)),
    "Speed", StringValue ("ns3::UniformRandomVariable[Min=1.0|Max=2.0]"));
  mobility_rd1.Install (mobileNodes1);

  Ipv4StaticRoutingHelper staticRouting;

  Ipv4ListRoutingHelper list;
  list.Add (staticRouting, 0);

  InternetStackHelper internet;
  internet.SetRoutingHelper (list);
  internet.Install (allNodes);
  
  Ipv4AddressHelper ipv4;
  NS_LOG_INFO ("Assign IP Addresses.");
  ipv4.SetBase ("10.1.0.0", "255.255.0.0");
  ipv4.Assign (allDevices);

  DceManagerHelper dceManager;
  dceManager.SetTaskManagerAttribute ("FiberManagerType",
    StringValue ("UcontextFiberManager"));
  dceManager.Install (allNodes);
  
  DceApplicationHelper dce;
  ApplicationContainer apps;

  dce.SetStackSize (1 << 20);

  dce.SetBinary ("../../lua-static/lua");
  dce.ResetArguments ();
  dce.AddArgument ("./rong-node.lua");
  apps = dce.Install (fixedNodes);
  apps.Start (Seconds (1.0));

  dce.SetBinary ("../../lua-static/lua");
  dce.ResetArguments ();
  dce.AddArgument ("./rong-node.lua");
  apps = dce.Install (mobileNodes1);
  //apps = dce.Install (mobileNodes2);
  apps.Start (Seconds (1.0));

  Simulator::Stop (Seconds (1000.0));
  Simulator::Run ();
  
  Simulator::Destroy ();

  return 0;
}
