#ifndef LOG_H
#define LOG_H

#include "ns3/packet.h"
#include "ns3/wifi-phy.h"
#include "ns3/wifi-mac-header.h"
#include <fstream>
#include <string>

// グローバル変数の宣言（extern）
extern std::ofstream g_qosLogFile;

// PHY 層受信トレースコールバック
void PhyRxTrace(std::string context,
                ns3::Ptr<const ns3::Packet> packet,
                uint16_t channelFreqMhz,
                ns3::WifiTxVector txVector,
                ns3::MpduInfo aMpdu,
                ns3::SignalNoiseDbm signalNoise,
                uint16_t staId);

#endif // LOG_H
