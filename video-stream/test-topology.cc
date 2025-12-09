#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "video-frame.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // ======================
    // コマンドライン引数
    // ======================
    bool enableAmpdu = true;
    bool enableEdca = true;
    uint32_t packetSize = 1400;
    uint32_t gopSize = 12;
    double distance = 20.0;
    double simulationTime = 2.0;
    std::string outputDir = "/Users/akira/workspace/ns-3.46.1/scratch/video-sim-log";

    CommandLine cmd;
    cmd.AddValue("ampdu", "Enable A-MPDU aggregation", enableAmpdu);
    cmd.AddValue("edca", "Enable EDCA priority differentiation", enableEdca);
    cmd.AddValue("packetSize", "Packet size in bytes", packetSize);
    cmd.AddValue("gopSize", "GOP size", gopSize);
    cmd.AddValue("distance", "Distance between AP and STA (m)", distance);
    cmd.AddValue("simTime", "Simulation time (s)", simulationTime);
    cmd.AddValue("outputDir", "Output directory for CSV files", outputDir);
    cmd.Parse(argc, argv);

    LogComponentEnable("VideoFrame", LOG_LEVEL_INFO);
    Time::SetResolution(Time::NS);

    // 設定を表示
    std::cout << "\n=== Simulation Configuration ===" << std::endl;
    std::cout << "A-MPDU: " << (enableAmpdu ? "ON" : "OFF") << std::endl;
    std::cout << "EDCA: " << (enableEdca ? "ON" : "OFF") << std::endl;
    std::cout << "Packet Size: " << packetSize << " bytes" << std::endl;
    std::cout << "GOP Size: " << gopSize << std::endl;
    std::cout << "Distance: " << distance << " m" << std::endl;
    std::cout << "Simulation Time: " << simulationTime << " s" << std::endl;
    std::cout << "================================\n" << std::endl;

    // ======================
    // ノード作成
    // ======================
    NodeContainer server;
    server.Create(1);

    NodeContainer ap;
    ap.Create(1);

    NodeContainer sta;
    sta.Create(1);

    // ======================
    // 有線リンク（サーバー - AP）
    // ======================
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("1Gbps"));
    p2p.SetChannelAttribute("Delay", StringValue("1ms"));
    NetDeviceContainer p2pDevices = p2p.Install(server.Get(0), ap.Get(0));

    // ======================
    // WiFi（AP - STA）ダウンリンク方向
    // ======================
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211ac);
    wifi.SetRemoteStationManager("ns3::MinstrelHtWifiManager");

    // WiFiチャネル
    YansWifiChannelHelper channel;
    channel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    channel.AddPropagationLoss("ns3::LogDistancePropagationLossModel",
                              "Exponent", DoubleValue(3.0),
                              "ReferenceDistance", DoubleValue(1.0));

    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());
    phy.SetErrorRateModel("ns3::YansErrorRateModel");

    WifiMacHelper mac;
    Ssid ssid = Ssid("video-network");

    // A-MPDUサイズの設定
    uint32_t ampduSize = enableAmpdu ? 65535 : 0;

    // AP設定
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid),
                "BE_MaxAmpduSize", UintegerValue(ampduSize),
                "BK_MaxAmpduSize", UintegerValue(ampduSize),
                "VI_MaxAmpduSize", UintegerValue(ampduSize),
                "VO_MaxAmpduSize", UintegerValue(ampduSize));
    NetDeviceContainer apDevice = wifi.Install(phy, mac, ap.Get(0));

    // STA設定
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false),
                "BE_MaxAmpduSize", UintegerValue(ampduSize),
                "BK_MaxAmpduSize", UintegerValue(ampduSize),
                "VI_MaxAmpduSize", UintegerValue(ampduSize),
                "VO_MaxAmpduSize", UintegerValue(ampduSize));
    NetDeviceContainer staDevice = wifi.Install(phy, mac, sta.Get(0));

    // ======================
    // モビリティ（位置設定）
    // ======================
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");

    // APの位置: 原点
    Ptr<ListPositionAllocator> apPos = CreateObject<ListPositionAllocator>();
    apPos->Add(Vector(0.0, 0.0, 0.0));
    mobility.SetPositionAllocator(apPos);
    mobility.Install(ap);

    // STAの位置: APからdistanceメートル離れた位置
    Ptr<ListPositionAllocator> staPos = CreateObject<ListPositionAllocator>();
    staPos->Add(Vector(distance, 0.0, 0.0));
    mobility.SetPositionAllocator(staPos);
    mobility.Install(sta);

    // ======================
    // IPアドレス設定
    // ======================
    InternetStackHelper stack;
    stack.Install(server);
    stack.Install(ap);
    stack.Install(sta);

    Ipv4AddressHelper address;

    // 有線リンク
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer p2pIf = address.Assign(p2pDevices);

    // 無線リンク
    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer apIf = address.Assign(apDevice);
    Ipv4InterfaceContainer staIf = address.Assign(staDevice);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // ======================
    // アプリケーション設定
    // ダウンリンク: サーバー → STA
    // ======================

    // 受信アプリ（WiFi STA側）
    Ptr<VideoFrameReceiverApplication> receiver = CreateObject<VideoFrameReceiverApplication>();
    receiver->SetPort(9);
    sta.Get(0)->AddApplication(receiver);
    receiver->SetStartTime(Seconds(0.5));
    receiver->SetStopTime(Seconds(simulationTime));

    // 送信アプリ（サーバー側）
    Ptr<VideoFrameSenderApplication> sender = CreateObject<VideoFrameSenderApplication>();
    sender->SetRemoteAddress(staIf.GetAddress(0));  // STAのIPアドレス
    sender->SetRemotePort(9);
    sender->SetPacketSize(packetSize);
    sender->SetGopSize(gopSize);
    sender->SetFrameInterval(Seconds(0.033));  // 30fps
    sender->SetEdcaEnabled(enableEdca);  // EDCA有効/無効
    server.Get(0)->AddApplication(sender);
    sender->SetStartTime(Seconds(1.0));
    sender->SetStopTime(Seconds(simulationTime));

    // ======================
    // シミュレーション実行
    // ======================
    Simulator::Stop(Seconds(simulationTime + 1.0));
    Simulator::Run();

    // ======================
    // 結果出力
    // ======================
    std::string ampduStr = enableAmpdu ? "on" : "off";
    std::string edcaStr = enableEdca ? "on" : "off";
    std::ostringstream csvPath;
    csvPath << outputDir << "/stats_ampdu_" << ampduStr
            << "_edca_" << edcaStr
            << "_d" << static_cast<int>(distance) << "m.csv";

    receiver->SaveStatisticsToFile(csvPath.str());
    receiver->PrintStatistics();

    // サマリー出力
    std::cout << "\n=== Simulation Summary ===" << std::endl;
    std::cout << "A-MPDU: " << (enableAmpdu ? "ON" : "OFF") << std::endl;
    std::cout << "EDCA: " << (enableEdca ? "ON" : "OFF") << std::endl;
    std::cout << "Distance: " << distance << " m" << std::endl;
    std::cout << "Output: " << csvPath.str() << std::endl;
    std::cout << "==========================\n" << std::endl;

    Simulator::Destroy();
    return 0;
}
