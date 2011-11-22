#include "ns3/network-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/dce-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/constant-position-mobility-model.h"
#include "ccnx/misc-tools.h"

using namespace ns3;

// ===========================================================================
//
//         node 0                 node 1
//   +----------------+    +----------------+
//   |                |    |                |
//   +----------------+    +----------------+
//   |    10.1.1.1    |    |    10.1.1.2    |
//   +----------------+    +----------------+
//   | point-to-point |    | point-to-point |
//   +----------------+    +----------------+
//           |                     |
//           +---------------------+
//                5 Mbps, 1 ms
//
// Just a ping !
//
//recompile iputils: edit Makefile: replace "CFLAGS=" with "CFLAGS+=" and run:
//                   "make CFLAGS=-fPIC LDFLAGS=-pie"
// ===========================================================================
int main (int argc, char *argv[])
{
  std::string animFile = "NetAnim.tr";
  bool useKernel = 0;
  CommandLine cmd;
  cmd.AddValue ("kernel", "Use kernel linux IP stack.", useKernel);
  cmd.Parse (argc, argv);

  NodeContainer nodes;
  nodes.Create (2);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("1ms"));

  NetDeviceContainer devices;
  devices = pointToPoint.Install (nodes);

  if (!useKernel)
    {
      InternetStackHelper stack;
      stack.Install (nodes);

      Ipv4AddressHelper address;
      address.SetBase ("10.1.1.0", "255.255.255.252");
      Ipv4InterfaceContainer interfaces = address.Assign (devices);

      // setup ip routes
      Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
    }

  DceManagerHelper dceManager;

  if (useKernel)
    {
      dceManager.SetNetworkStack("ns3::LinuxSocketFdFactory", "Library", StringValue ("libnet-next-2.6.so"));

      AddAddress (nodes.Get (0), Seconds (0.1), "sim0", "10.1.1.1/8");
      RunIp (nodes.Get (0), Seconds (0.2), "link set sim0 up arp off");

      AddAddress (nodes.Get (1), Seconds (0.3), "sim0","10.1.1.2/8");
      RunIp (nodes.Get (1), Seconds (0.4), "link set sim0 up arp off");

    }
  dceManager.Install (nodes);

  DceApplicationHelper dce;
  ApplicationContainer apps;

  dce.SetStackSize (1<<20);

  // Launch ping on node 0
  dce.SetBinary ("ping");
  dce.ResetArguments();
  dce.ResetEnvironment();
  dce.AddArgument ("-c 10");
  dce.AddArgument ("-s 1000");
  dce.AddArgument ("10.1.1.2");

  apps = dce.Install (nodes.Get (0));
  apps.Start (Seconds (1.0));

  // Launch ping on node 1
  dce.SetBinary ("ping");
  dce.ResetArguments();
  dce.ResetEnvironment();
  dce.AddArgument ("-c 10");
  dce.AddArgument ("-s 1000");
  dce.AddArgument ("10.1.1.1");

  apps = dce.Install (nodes.Get (1));
  apps.Start (Seconds (1.5));

  setPos (nodes.Get (0), 1, 10, 0);
  setPos (nodes.Get (1), 50,10, 0);

  // Create the animation object and configure for specified output

  AnimationInterface anim;

  anim.SetOutputFile (animFile);

  anim.StartAnimation ();

  Simulator::Stop (Seconds(40.0));
  Simulator::Run ();
  Simulator::Destroy ();

  anim.StopAnimation ();

  return 0;
}

