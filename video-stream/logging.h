#ifndef LOGGING_H
#define LOGGING_H

#include <fstream>
#include <map>
#include "ns3/simulator.h"
#include "ns3/packet.h"
#include "ns3/ampdu-tag.h"
#include "ns3/ampdu-subframe-header.h"
#include "ns3/wifi-mac-header.h"
#include "ns3/qos-utils.h"
#include "video-frame.h"

using namespace ns3;

// QoS情報を保存する構造体
struct QosInfo {
    uint8_t tid;
    AcIndex ac;
    bool isAmpdu;
    uint32_t ampduRefNum;
};

// パケットごとのQoS情報を保存するMap (キー: frameId * 1000000 + packetIndex)
inline std::map<uint64_t, QosInfo>& getQosInfoMap() {
    static std::map<uint64_t, QosInfo> g_qosInfoMap;
    return g_qosInfoMap;
}
#define g_qosInfoMap getQosInfoMap()

// Access Category の名前
inline const char* GetAcName(AcIndex ac) {
    switch (ac) {
        case AC_BE: return "AC_BE";
        case AC_BK: return "AC_BK";
        case AC_VI: return "AC_VI";
        case AC_VO: return "AC_VO";
        default: return "AC_UNKNOWN";
    }
}

// PHY 層受信トレースコールバック
inline void PhyRxTrace(std::string context, Ptr<const Packet> packet,
                uint16_t channelFreqMhz, WifiTxVector txVector,
                MpduInfo aMpdu, SignalNoiseDbm signalNoise, uint16_t staId)
{
    // A-MPDU 情報
    bool isAmpdu = (aMpdu.type != 0);
    uint32_t ampduRefNum = aMpdu.mpduRefNumber;

    // パケットのコピーを作成
    Ptr<Packet> pktCopy = packet->Copy();

    // VideoFrameTag を先に取得（CreateFragmentでタグが失われるため）
    VideoFrameTag vTag;
    bool hasVideoTag = pktCopy->PeekPacketTag(vTag);

    // デバッグ出力なし

    // A-MPDU サブフレームヘッダを処理
    if (txVector.IsAggregation()) {
        AmpduSubframeHeader subHdr;
        pktCopy->RemoveHeader(subHdr);
        uint32_t length = subHdr.GetLength();
        pktCopy = pktCopy->CreateFragment(0, length);
    }

    // VideoFrameTag があれば QoS 情報を取得・保存
    if (hasVideoTag) {
        uint32_t frameId = vTag.GetFrameId();
        uint32_t packetIndex = vTag.GetPacketIndex();

        // WiFi MAC ヘッダから TID を取得（可能な場合）
        WifiMacHeader hdr;
        pktCopy->PeekHeader(hdr);

        uint8_t tid = 0;
        AcIndex ac = AC_BE;  // デフォルト
        if (hdr.IsQosData()) {
            tid = hdr.GetQosTid();
            ac = QosUtilsMapTidToAc(tid);
        }

        // QoS情報をMapに保存（A-MPDU情報用）
        uint64_t key = static_cast<uint64_t>(frameId) * 1000000 + packetIndex;
        QosInfo info;
        info.tid = tid;
        info.ac = ac;
        info.isAmpdu = isAmpdu;
        info.ampduRefNum = ampduRefNum;
        g_qosInfoMap[key] = info;
    }
}

#endif // LOGGING_H
