#ifndef VIDEO_FRAME_H
#define VIDEO_FRAME_H

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/socket.h"
#include "ns3/socket-factory.h"
#include "ns3/udp-socket-factory.h"
#include "ns3/simulator.h"
#include "ns3/log.h"
#include <map>
#include <iostream>
#include <iomanip>
#include <fstream>

using namespace ns3;

// VideoFrameTag: フレーム情報を保持
class VideoFrameTag : public Tag {
public:
    static TypeId GetTypeId();
    virtual TypeId GetInstanceTypeId() const;

    VideoFrameTag();
    VideoFrameTag(uint32_t frameId, uint32_t frameType, uint32_t packetIndex, uint32_t totalPackets,
                  int32_t forwardRefFrameId, int32_t backwardRefFrameId, double transmissionStartTime);

    virtual uint32_t GetSerializedSize() const;
    virtual void Serialize(TagBuffer i) const;
    virtual void Deserialize(TagBuffer i);
    virtual void Print(std::ostream& os) const;

    uint32_t GetFrameId() const;
    uint32_t GetFrameType() const;
    uint32_t GetPacketIndex() const;
    uint32_t GetTotalPackets() const;
    int32_t GetForwardRefFrameId() const;   // 前方参照フレームID
    int32_t GetBackwardRefFrameId() const;  // 後方参照フレームID（Bフレームのみ）
    double GetTransmissionStartTime() const;

private:
    uint32_t m_frameId;
    uint32_t m_frameType;  // 0=I, 1=P, 2=B
    uint32_t m_packetIndex;
    uint32_t m_totalPackets; //一つのフレームを構成する総パケット数(定数)
    int32_t m_forwardRefFrameId;   // 前方参照フレームID (-1=参照なし)
    int32_t m_backwardRefFrameId;  // 後方参照フレームID (-1=参照なし, Bフレームのみ使用)
    double m_transmissionStartTime;  // フレーム送信開始時刻 (秒)
};

// FrameStatistics: 各フレーム統計情報
struct FrameStatistics {
    uint32_t frameId;
    uint32_t frameType;
    uint32_t receivedPackets;
    uint32_t totalPackets;
    int32_t forwardRefFrameId;   // 前方参照フレームID (-1=参照なし)
    int32_t backwardRefFrameId;  // 後方参照フレームID (-1=参照なし)
    bool forwardRefLost;         // 前方参照フレームがロスしているか
    bool backwardRefLost;        // 後方参照フレームがロスしているか
    double packetReceptionRatio;  // パケット受信率
    double effectiveReceptionRatio;  // ロス連鎖を考慮した有効受信率
    double transmissionStartTime;  // フレーム送信開始時刻 (秒)
    double firstPacketArrivalTime;  // 最初のパケット到着時刻 (秒)
    double lastPacketArrivalTime;   // 最後のパケット到着時刻 (秒)
    double latency;  // 遅延時間 (ミリ秒): 送信開始から最後のパケット受信まで
    bool withinDeadline;  // 許容遅延内かどうか (30fps = 33.3ms)
};

// VideoFrameSenderApplication: 送信アプリ
class VideoFrameSenderApplication : public Application {
public:
    static TypeId GetTypeId();
    VideoFrameSenderApplication();
    virtual ~VideoFrameSenderApplication();

    void SetRemoteAddress(Ipv4Address address);
    void SetRemotePort(uint16_t port);
    void SetPacketSize(uint32_t size);
    void SetGopSize(uint32_t gopSize);
    void SetFrameInterval(Time interval);
    void SetEdcaEnabled(bool enabled);

private:
    virtual void StartApplication();
    virtual void StopApplication();

    void SendWarmupPackets();
    void GenerateFrame();
    uint32_t GetFrameType(uint32_t frameNum);
    uint32_t GetFramePackets(uint32_t frameType);
    int32_t GetForwardRefFrameId(uint32_t frameNum, uint32_t frameType);
    int32_t GetBackwardRefFrameId(uint32_t frameNum, uint32_t frameType);

    Ptr<Socket> m_socket;
    Ipv4Address m_peerAddress;
    uint16_t m_peerPort;
    uint32_t m_packetSize;
    uint32_t m_gopSize;
    Time m_frameInterval;
    EventId m_sendEvent;
    uint32_t m_frameNum;
    bool m_edcaEnabled;
};

// VideoFrameReceiverApplication: 受信アプリ
class VideoFrameReceiverApplication : public Application {
public:
    static TypeId GetTypeId();
    VideoFrameReceiverApplication();
    virtual ~VideoFrameReceiverApplication();

    void SetPort(uint16_t port);
    void SetPacketLogFile(std::string filename);
    void PrintStatistics();
    void SaveStatisticsToFile(std::string filename);

private:
    virtual void StartApplication();
    virtual void StopApplication();

    void HandleRead(Ptr<Socket> socket);
    void CalculateStatistics();
    void LogPacket(uint32_t frameId, uint32_t frameType, uint32_t packetIndex,
                   uint32_t totalPackets, double txTime, double rxTime, int32_t fwdRef, int32_t bwdRef);

    Ptr<Socket> m_socket;
    uint16_t m_port;
    std::map<uint32_t, FrameStatistics> m_frameStats;
    std::string m_packetLogFile;
    std::ofstream m_packetLogStream;
};

#endif // VIDEO_FRAME_H
