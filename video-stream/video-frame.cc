#include "video-frame.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("VideoFrame");

// ========================================
// VideoFrameTag Implementation
// ========================================
TypeId VideoFrameTag::GetTypeId() {
    static TypeId tid = TypeId("ns3::VideoFrameTag")
        .SetParent<Tag>()
        .SetGroupName("VideoFrame")
        .AddConstructor<VideoFrameTag>();
    return tid;
}

TypeId VideoFrameTag::GetInstanceTypeId() const {
    return GetTypeId();
}

VideoFrameTag::VideoFrameTag()
    : m_frameId(0), m_frameType(0), m_packetIndex(0), m_totalPackets(0),
      m_forwardRefFrameId(-1), m_backwardRefFrameId(-1), m_transmissionStartTime(0.0) {
}

VideoFrameTag::VideoFrameTag(uint32_t frameId, uint32_t frameType, uint32_t packetIndex, uint32_t totalPackets,
                             int32_t forwardRefFrameId, int32_t backwardRefFrameId, double transmissionStartTime)
    : m_frameId(frameId), m_frameType(frameType), m_packetIndex(packetIndex), m_totalPackets(totalPackets),
      m_forwardRefFrameId(forwardRefFrameId), m_backwardRefFrameId(backwardRefFrameId), m_transmissionStartTime(transmissionStartTime) {
}

uint32_t VideoFrameTag::GetSerializedSize() const {
    return 4 + 4 + 4 + 4 + 4 + 4 + 8;  // 4 uint32_t + 2 int32_t + 1 double
}

void VideoFrameTag::Serialize(TagBuffer i) const {
    i.WriteU32(m_frameId);
    i.WriteU32(m_frameType);
    i.WriteU32(m_packetIndex);
    i.WriteU32(m_totalPackets);
    i.WriteU32(static_cast<uint32_t>(m_forwardRefFrameId));
    i.WriteU32(static_cast<uint32_t>(m_backwardRefFrameId));
    i.WriteDouble(m_transmissionStartTime);
}

void VideoFrameTag::Deserialize(TagBuffer i) {
    m_frameId = i.ReadU32();
    m_frameType = i.ReadU32();
    m_packetIndex = i.ReadU32();
    m_totalPackets = i.ReadU32();
    m_forwardRefFrameId = static_cast<int32_t>(i.ReadU32());
    m_backwardRefFrameId = static_cast<int32_t>(i.ReadU32());
    m_transmissionStartTime = i.ReadDouble();
}

void VideoFrameTag::Print(std::ostream& os) const {
    os << "FrameId=" << m_frameId << " Type=" << m_frameType
       << " Packet=" << m_packetIndex << "/" << m_totalPackets
       << " FwdRef=" << m_forwardRefFrameId << " BwdRef=" << m_backwardRefFrameId
       << " TxTime=" << m_transmissionStartTime;
}

uint32_t VideoFrameTag::GetFrameId() const { return m_frameId; }
uint32_t VideoFrameTag::GetFrameType() const { return m_frameType; }
uint32_t VideoFrameTag::GetPacketIndex() const { return m_packetIndex; }
uint32_t VideoFrameTag::GetTotalPackets() const { return m_totalPackets; }
int32_t VideoFrameTag::GetForwardRefFrameId() const { return m_forwardRefFrameId; }
int32_t VideoFrameTag::GetBackwardRefFrameId() const { return m_backwardRefFrameId; }
double VideoFrameTag::GetTransmissionStartTime() const { return m_transmissionStartTime; }

// ========================================
// VideoFrameSenderApplication Implementation
// ========================================
TypeId VideoFrameSenderApplication::GetTypeId() {
    static TypeId tid = TypeId("ns3::VideoFrameSenderApplication")
        .SetParent<Application>()
        .SetGroupName("VideoFrame")
        .AddConstructor<VideoFrameSenderApplication>();
    return tid;
}

VideoFrameSenderApplication::VideoFrameSenderApplication()
    : m_peerPort(0), m_packetSize(512), m_gopSize(12), m_frameNum(0), m_edcaEnabled(true) {
    m_frameInterval = Seconds(0.033);  // 30fps
}

VideoFrameSenderApplication::~VideoFrameSenderApplication() {
}

void VideoFrameSenderApplication::SetRemoteAddress(Ipv4Address address) {
    m_peerAddress = address;
}

void VideoFrameSenderApplication::SetRemotePort(uint16_t port) {
    m_peerPort = port;
}

void VideoFrameSenderApplication::SetPacketSize(uint32_t size) {
    m_packetSize = size;
}

void VideoFrameSenderApplication::SetGopSize(uint32_t gopSize) {
    m_gopSize = gopSize;
}

void VideoFrameSenderApplication::SetFrameInterval(Time interval) {
    m_frameInterval = interval;
}

void VideoFrameSenderApplication::SetEdcaEnabled(bool enabled) {
    m_edcaEnabled = enabled;
}

uint32_t VideoFrameSenderApplication::GetFrameType(uint32_t frameNum) {
    uint32_t pos = frameNum % m_gopSize;
    if (pos == 0) return 0;  // I frame
    if (pos % 3 == 0) return 1;  // P frame
    return 2;  // B frame
}

uint32_t VideoFrameSenderApplication::GetFramePackets(uint32_t frameType) {
    // I > P > B の関係を反映したパケット数
    switch (frameType) {
        case 0:  // I frame
            return 50;
        case 1:  // P frame
            return 20;
        case 2:  // B frame
            return 10;
        default:
            return 10;
    }
}

// 前方参照フレームIDを取得
int32_t VideoFrameSenderApplication::GetForwardRefFrameId(uint32_t frameNum, uint32_t frameType) {
    // I フレーム: 参照なし
    if (frameType == 0) {
        return -1;
    }

    uint32_t gopPos = frameNum % m_gopSize;

    // GOP構造: I B B P B B P B B P B B
    // 位置:    0 1 2 3 4 5 6 7 8 9 10 11

    // 直前のI/Pフレーム位置を計算
    uint32_t prevKeyPos;
    if (gopPos < 3) {
        // 位置1,2（最初のP(3)より前）はI(0)を参照
        prevKeyPos = 0;
    } else {
        // 位置3以降は直前のPを参照
        // gopPos=3 -> prevKeyPos=0 (I)
        // gopPos=4,5 -> prevKeyPos=3 (P)
        // gopPos=6 -> prevKeyPos=3 (P)
        // gopPos=7,8 -> prevKeyPos=6 (P)
        prevKeyPos = ((gopPos - 1) / 3) * 3;
        if (prevKeyPos == 0) {
            prevKeyPos = 0;  // Iフレーム
        }
    }

    return (int32_t)(frameNum - gopPos + prevKeyPos);
}

// 後方参照フレームIDを取得（Bフレームのみ）
int32_t VideoFrameSenderApplication::GetBackwardRefFrameId(uint32_t frameNum, uint32_t frameType) {
    // I/Pフレーム: 後方参照なし
    if (frameType != 2) {
        return -1;
    }

    uint32_t gopPos = frameNum % m_gopSize;

    // GOP構造: I B B P B B P B B P B B
    // 次のI/P位置を計算
    uint32_t nextKeyPos = ((gopPos / 3) + 1) * 3;
    if (nextKeyPos >= m_gopSize) {
        // GOPの最後のBフレームは次のGOPのIフレームを参照
        nextKeyPos = m_gopSize;
    }

    return (int32_t)(frameNum - gopPos + nextKeyPos);
}

void VideoFrameSenderApplication::StartApplication() {
    if (m_socket == nullptr) {
        TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
        m_socket = Socket::CreateSocket(GetNode(), tid);
        m_socket->Bind();
        m_socket->Connect(InetSocketAddress(m_peerAddress, m_peerPort));
    }

    m_frameNum = 0;
    m_sendEvent = Simulator::Schedule(Seconds(0.0), &VideoFrameSenderApplication::GenerateFrame, this);
}

void VideoFrameSenderApplication::StopApplication() {
    if (m_sendEvent.IsPending()) {
        Simulator::Cancel(m_sendEvent);
    }
    if (m_socket) {
        m_socket->Close();
    }
}

void VideoFrameSenderApplication::GenerateFrame() {
    uint32_t frameType = GetFrameType(m_frameNum);
    uint32_t framePackets = GetFramePackets(frameType);
    int32_t fwdRefFrameId = GetForwardRefFrameId(m_frameNum, frameType);
    int32_t bwdRefFrameId = GetBackwardRefFrameId(m_frameNum, frameType);
    const char* frameTypeStr[] = {"I", "P", "B"};
    double txStartTime = Simulator::Now().GetSeconds();  // フレーム送信開始時刻

    NS_LOG_INFO("Generating " << frameTypeStr[frameType] << " frame " << m_frameNum
               << " (" << framePackets << " packets, fwdRef=" << fwdRefFrameId
               << ", bwdRef=" << bwdRefFrameId << ")");

    // フレームの全パケットをバースト的に送信
    for (uint32_t i = 0; i < framePackets; i++) {
        // パケットを作成（実際のペイロードはm_packetSizeバイト）
        Ptr<Packet> packet = Create<Packet>(m_packetSize);

        // VideoFrameTagを付加（メタデータ）
        VideoFrameTag tag(m_frameNum, frameType, i, framePackets, fwdRefFrameId, bwdRefFrameId, txStartTime);
        packet->AddPacketTag(tag);

        // EDCA優先度制御: SocketPriorityTagを使用してWiFi MAC層に優先度を指定
        if (m_edcaEnabled) {
            // I フレーム: Priority=5 (AC_VI, Video, 高優先度)
            // P/B フレーム: Priority=0 (AC_BE, Best Effort)
            SocketPriorityTag priorityTag;
            if (frameType == 0) {
                priorityTag.SetPriority(5);  // AC_VI
            } else {
                priorityTag.SetPriority(0);  // AC_BE
            }
            packet->AddPacketTag(priorityTag);
        }

        int ret = m_socket->Send(packet);
        if (ret < 0) {
            NS_LOG_ERROR("Failed to send packet: " << ret);
        }
    }

    m_frameNum++;
    m_sendEvent = Simulator::Schedule(m_frameInterval, &VideoFrameSenderApplication::GenerateFrame, this);
}

// ========================================
// VideoFrameReceiverApplication Implementation
// ========================================
TypeId VideoFrameReceiverApplication::GetTypeId() {
    static TypeId tid = TypeId("ns3::VideoFrameReceiverApplication")
        .SetParent<Application>()
        .SetGroupName("VideoFrame")
        .AddConstructor<VideoFrameReceiverApplication>();
    return tid;
}

VideoFrameReceiverApplication::VideoFrameReceiverApplication()
    : m_port(0) {
}

VideoFrameReceiverApplication::~VideoFrameReceiverApplication() {
}

void VideoFrameReceiverApplication::SetPort(uint16_t port) {
    m_port = port;
}

void VideoFrameReceiverApplication::StartApplication() {
    if (m_socket == nullptr) {
        TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
        m_socket = Socket::CreateSocket(GetNode(), tid);

        InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), m_port);
        if (m_socket->Bind(local) == -1) {
            NS_FATAL_ERROR("Failed to bind socket");
        }
        m_socket->SetRecvCallback(MakeCallback(&VideoFrameReceiverApplication::HandleRead, this));
    }
}

void VideoFrameReceiverApplication::StopApplication() {
    if (m_socket) {
        m_socket->Close();
        m_socket->SetRecvCallback(MakeNullCallback<void, Ptr<Socket>>());
    }
}

void VideoFrameReceiverApplication::HandleRead(Ptr<Socket> socket) {
    Ptr<Packet> packet;
    Address from;
    double rxTime = Simulator::Now().GetSeconds();

    while ((packet = socket->RecvFrom(from))) {
        // VideoFrameTagからメタデータを取得
        VideoFrameTag tag;
        if (packet->PeekPacketTag(tag)) {
            uint32_t frameId = tag.GetFrameId();
            uint32_t frameType = tag.GetFrameType();
            uint32_t totalPackets = tag.GetTotalPackets();
            int32_t fwdRefFrameId = tag.GetForwardRefFrameId();
            int32_t bwdRefFrameId = tag.GetBackwardRefFrameId();
            double txStartTime = tag.GetTransmissionStartTime();

            // フレーム情報の初期化
            if (m_frameStats.find(frameId) == m_frameStats.end()) {
                FrameStatistics stat;
                stat.frameId = frameId;
                stat.frameType = frameType;
                stat.receivedPackets = 0;
                stat.totalPackets = totalPackets;
                stat.forwardRefFrameId = fwdRefFrameId;
                stat.backwardRefFrameId = bwdRefFrameId;
                stat.forwardRefLost = false;
                stat.backwardRefLost = false;
                stat.packetReceptionRatio = 0.0;
                stat.effectiveReceptionRatio = 0.0;
                stat.transmissionStartTime = txStartTime;
                stat.firstPacketArrivalTime = rxTime;
                stat.lastPacketArrivalTime = rxTime;
                stat.latency = 0.0;
                stat.withinDeadline = false;
                m_frameStats[frameId] = stat;
            }

            // 最後のパケット到着時刻を更新
            m_frameStats[frameId].lastPacketArrivalTime = rxTime;
            m_frameStats[frameId].receivedPackets++;

            const char* frameTypeStr[] = {"I", "P", "B"};
            NS_LOG_INFO("Received packet from " << frameTypeStr[frameType] << " frame " << frameId
                       << " (" << m_frameStats[frameId].receivedPackets << "/" << totalPackets
                       << ", fwdRef=" << fwdRefFrameId << ", bwdRef=" << bwdRefFrameId << ")");
        }
    }
}

void VideoFrameReceiverApplication::CalculateStatistics() {
    const double DEADLINE_MS = 33.3;  // 30fps = 33.3ms

    // 第1パス: パケット受信率と遅延を計算
    for (auto& stat : m_frameStats) {
        stat.second.packetReceptionRatio = (stat.second.receivedPackets * 100.0) / stat.second.totalPackets;

        // 遅延計算: 送信開始から最後のパケット受信までの時間 (ミリ秒)
        stat.second.latency = (stat.second.lastPacketArrivalTime - stat.second.transmissionStartTime) * 1000.0;

        // 許容遅延判定
        stat.second.withinDeadline = (stat.second.latency <= DEADLINE_MS);
    }

    // 第2パス: 参照フレームの状態を確認してロス連鎖を判定
    for (auto& stat : m_frameStats) {
        stat.second.forwardRefLost = false;
        stat.second.backwardRefLost = false;

        // 前方参照フレームのチェック
        if (stat.second.forwardRefFrameId != -1) {
            auto refIt = m_frameStats.find(stat.second.forwardRefFrameId);
            if (refIt != m_frameStats.end()) {
                // 参照フレームが完全受信されていない場合
                if (refIt->second.receivedPackets < refIt->second.totalPackets) {
                    stat.second.forwardRefLost = true;
                }
            }
        }

        // 後方参照フレームのチェック（Bフレームのみ）
        if (stat.second.backwardRefFrameId != -1) {
            auto refIt = m_frameStats.find(stat.second.backwardRefFrameId);
            if (refIt != m_frameStats.end()) {
                // 参照フレームが完全受信されていない場合
                if (refIt->second.receivedPackets < refIt->second.totalPackets) {
                    stat.second.backwardRefLost = true;
                }
            }
        }

        // 有効受信率を計算（どちらかの参照がロスしていたら0）
        if (stat.second.forwardRefLost || stat.second.backwardRefLost) {
            stat.second.effectiveReceptionRatio = 0.0;
        } else {
            stat.second.effectiveReceptionRatio = stat.second.packetReceptionRatio;
        }
    }
}

void VideoFrameReceiverApplication::PrintStatistics() {
    CalculateStatistics();

    // CSV ヘッダー行
    std::cout << "\nFrameID,Type,PacketRatio(%),FwdRef,BwdRef,RefStatus,EffectiveRatio(%),"
              << "Latency(ms),WithinDeadline,FirstArrival(sec),LastArrival(sec)" << std::endl;

    // データ行
    for (auto& stat : m_frameStats) {
        const char* typeStr[] = {"I", "P", "B"};

        // 参照状態を判定
        std::string refStatus;
        if (stat.second.forwardRefFrameId == -1 && stat.second.backwardRefFrameId == -1) {
            refStatus = "N/A";
        } else if (stat.second.forwardRefLost && stat.second.backwardRefLost) {
            refStatus = "BOTH_LOST";
        } else if (stat.second.forwardRefLost) {
            refStatus = "FWD_LOST";
        } else if (stat.second.backwardRefLost) {
            refStatus = "BWD_LOST";
        } else {
            refStatus = "OK";
        }

        std::string deadlineStatus = stat.second.withinDeadline ? "YES" : "NO";

        std::cout << stat.first << ","
                  << typeStr[stat.second.frameType] << ","
                  << std::fixed << std::setprecision(1) << stat.second.packetReceptionRatio << ","
                  << stat.second.forwardRefFrameId << ","
                  << stat.second.backwardRefFrameId << ","
                  << refStatus << ","
                  << std::fixed << std::setprecision(1) << stat.second.effectiveReceptionRatio << ","
                  << std::fixed << std::setprecision(2) << stat.second.latency << ","
                  << deadlineStatus << ","
                  << std::fixed << std::setprecision(4) << stat.second.firstPacketArrivalTime << ","
                  << std::fixed << std::setprecision(4) << stat.second.lastPacketArrivalTime << std::endl;
    }
}

void VideoFrameReceiverApplication::SaveStatisticsToFile(std::string filename) {
    CalculateStatistics();

    std::ofstream outfile(filename);
    if (!outfile.is_open()) {
        NS_LOG_ERROR("Failed to open file: " << filename);
        return;
    }

    // CSV ヘッダー行
    outfile << "FrameID,Type,PacketRatio(%),FwdRef,BwdRef,RefStatus,EffectiveRatio(%),"
            << "Latency(ms),WithinDeadline,FirstArrival(sec),LastArrival(sec)" << std::endl;

    // データ行
    for (auto& stat : m_frameStats) {
        const char* typeStr[] = {"I", "P", "B"};

        // 参照状態を判定
        std::string refStatus;
        if (stat.second.forwardRefFrameId == -1 && stat.second.backwardRefFrameId == -1) {
            refStatus = "N/A";
        } else if (stat.second.forwardRefLost && stat.second.backwardRefLost) {
            refStatus = "BOTH_LOST";
        } else if (stat.second.forwardRefLost) {
            refStatus = "FWD_LOST";
        } else if (stat.second.backwardRefLost) {
            refStatus = "BWD_LOST";
        } else {
            refStatus = "OK";
        }

        std::string deadlineStatus = stat.second.withinDeadline ? "YES" : "NO";

        outfile << stat.first << ","
                << typeStr[stat.second.frameType] << ","
                << std::fixed << std::setprecision(1) << stat.second.packetReceptionRatio << ","
                << stat.second.forwardRefFrameId << ","
                << stat.second.backwardRefFrameId << ","
                << refStatus << ","
                << std::fixed << std::setprecision(1) << stat.second.effectiveReceptionRatio << ","
                << std::fixed << std::setprecision(2) << stat.second.latency << ","
                << deadlineStatus << ","
                << std::fixed << std::setprecision(4) << stat.second.firstPacketArrivalTime << ","
                << std::fixed << std::setprecision(4) << stat.second.lastPacketArrivalTime << std::endl;
    }

    outfile.close();
    NS_LOG_INFO("Statistics saved to: " << filename);
    std::cout << "\nStatistics saved to: " << filename << std::endl;
}
