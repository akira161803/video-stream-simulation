#include "log.h"
#include "video-frame.h"
#include "ns3/simulator.h"
#include "ns3/qos-utils.h"
#include "ns3/ampdu-subframe-header.h"

namespace ns3 {


// PHY 層受信トレースコールバック
void PhyRxTrace(Ptr<OutputStreamWrapper> stream, std::string context, Ptr<const Packet> packet,
                uint16_t channelFreqMhz, WifiTxVector txVector,
                MpduInfo aMpdu, SignalNoiseDbm signalNoise, uint16_t staId)
{
    // A-MPDU 情報
    bool isAmpdu = (aMpdu.type != 0);
    uint32_t ampduRefNum = aMpdu.mpduRefNumber;

    // パケットのコピーを作成
    Ptr<Packet> pktCopy = packet->Copy();

    // A-MPDU サブフレームヘッダを処理
    if (txVector.IsAggregation()) {
        AmpduSubframeHeader subHdr;
        pktCopy->RemoveHeader(subHdr);
        uint32_t length = subHdr.GetLength();
        pktCopy = pktCopy->CreateFragment(0, length);
    }

    // WiFi MAC ヘッダを取得
    WifiMacHeader hdr;
    pktCopy->PeekHeader(hdr);

    // QoS 情報を取得
    if (hdr.IsQosData()) {
        uint8_t tid = hdr.GetQosTid();
        AcIndex ac = QosUtilsMapTidToAc(tid);

        // VideoFrameTag を取得（フレーム情報用）
        VideoFrameTag vTag;
        uint32_t frameId = 0;
        uint32_t frameType = 0;
        uint32_t packetIndex = 0;
        if (pktCopy->PeekPacketTag(vTag)) {
            frameId = vTag.GetFrameId();
            frameType = vTag.GetFrameType();
            packetIndex = vTag.GetPacketIndex();
        }

        const char* frameTypeStr[] = {"I", "P", "B"};
        *stream->GetStream ()
            << Simulator::Now().GetSeconds() << ","
            << frameId << ","
            << frameTypeStr[frameType] << ","
            << packetIndex << ","
            << (int)tid << ","
            << ac << ","
            << (isAmpdu ? "YES" : "NO") << ","
            << ampduRefNum << std::endl;
    }
}

}
