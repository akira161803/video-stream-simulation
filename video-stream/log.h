#ifndef LOG_H
#define LOG_H

#include "ns3/packet.h"
#include "ns3/wifi-phy.h"
#include "ns3/wifi-mac-header.h"
#include "ns3/output-stream-wrapper.h"
#include <fstream>
#include <string>

namespace ns3 {

// PHY 層受信トレースコールバック
void PhyRxTrace(Ptr<OutputStreamWrapper> stream,
                std::string context,
                ns3::Ptr<const ns3::Packet> packet,
                uint16_t channelFreqMhz,
                ns3::WifiTxVector txVector,
                ns3::MpduInfo aMpdu,
                ns3::SignalNoiseDbm signalNoise,
                uint16_t staId);

}

#endif // LOG_H
