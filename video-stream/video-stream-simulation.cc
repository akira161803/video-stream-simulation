#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/ipv4-flow-classifier.h"
#include "video-frame.h"
#include "log.h"


using namespace ns3;

int main(int argc, char *argv[]) {
    bool enableAmpdu = false;
    bool enableEdca = false;
    uint32_t packetSize = 1400;
    uint32_t gopSize = 60;
    double distance = 20.0;
    double simulationTime = 10.0;
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

    //LogComponentEnable("VideoFrame", LOG_LEVEL_INFO);
    //LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
    //LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
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

    // ノード作成
    NodeContainer server;
    server.Create(1);

    NodeContainer ap;
    ap.Create(1);

    NodeContainer sta;
    sta.Create(1);

    // 有線リンク（サーバー - AP）
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("1Gbps"));
    p2p.SetChannelAttribute("Delay", StringValue("1ms"));
    NetDeviceContainer p2pDevices = p2p.Install(server.Get(0), ap.Get(0));

    // WiFi（AP - STA）ダウンリンク方向
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211be);
    wifi.SetRemoteStationManager ("ns3::IdealWifiManager");
//    wifi.SetRemoteStationManager(
//            "ns3::ConstantRateWifiManager",
//            "DataMode", StringValue("EhtMcs8"),
//            "ControlMode", StringValue("EhtMcs0")  // 制御フレームは通常低MCS
//            );

    // WiFiチャネル（2.4GHz, 20MHz帯域幅）
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();

    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());
    phy.Set("ChannelSettings", StringValue("{0, 20, BAND_2_4GHZ, 0}"));  // 2.4GHz, 20MHz
    phy.SetErrorRateModel("ns3::YansErrorRateModel");

    WifiMacHelper mac;
    Ssid ssid = Ssid("video-network");

    // A-MPDUサイズの設定
    uint32_t ampduSize = enableAmpdu ? 65535 : 0;
    uint32_t VO_MaxAmpduSize = enableAmpdu ? 15000000 : 0;
    uint32_t BE_MaxAmpduSize = enableAmpdu ? 15000000 : 0;

    // AP設定
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid),
                "BE_MaxAmpduSize", UintegerValue(BE_MaxAmpduSize),
                "BK_MaxAmpduSize", UintegerValue(ampduSize),
                "VI_MaxAmpduSize", UintegerValue(ampduSize),
                "VO_MaxAmpduSize", UintegerValue(VO_MaxAmpduSize));
    NetDeviceContainer apDevice = wifi.Install(phy, mac, ap.Get(0));

    // STA設定
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false),
                "BE_MaxAmpduSize", UintegerValue(BE_MaxAmpduSize),
                "BK_MaxAmpduSize", UintegerValue(ampduSize),
                "VI_MaxAmpduSize", UintegerValue(ampduSize),
                "VO_MaxAmpduSize", UintegerValue(VO_MaxAmpduSize));
    NetDeviceContainer staDevice = wifi.Install(phy, mac, sta.Get(0));

    // モビリティ（位置設定）
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

    // IPアドレス設定
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

    // デバッグ: IPアドレスの確認
    std::cout << "\n=== IP Address Assignment ===" << std::endl;
    for (uint32_t i = 0; i < server.GetN(); i++) {
        Ptr<Ipv4> ipv4 = server.Get(i)->GetObject<Ipv4>();
        std::cout << "Server " << i << " addresses:" << std::endl;
        for (uint32_t j = 0; j < ipv4->GetNInterfaces(); j++) {
            std::cout << "  Interface " << j << ": " << ipv4->GetAddress(j, 0).GetLocal() << std::endl;
        }
    }
    for (uint32_t i = 0; i < ap.GetN(); i++) {
        Ptr<Ipv4> ipv4 = ap.Get(i)->GetObject<Ipv4>();
        std::cout << "AP " << i << " addresses:" << std::endl;
        for (uint32_t j = 0; j < ipv4->GetNInterfaces(); j++) {
            std::cout << "  Interface " << j << ": " << ipv4->GetAddress(j, 0).GetLocal() << std::endl;
        }
    }
    for (uint32_t i = 0; i < sta.GetN(); i++) {
        Ptr<Ipv4> ipv4 = sta.Get(i)->GetObject<Ipv4>();
        std::cout << "STA " << i << " addresses:" << std::endl;
        for (uint32_t j = 0; j < ipv4->GetNInterfaces(); j++) {
            std::cout << "  Interface " << j << ": " << ipv4->GetAddress(j, 0).GetLocal() << std::endl;
        }
    }

    // ルーティングテーブル表示
    std::cout << "\n=== Routing Tables ===" << std::endl;
    for (uint32_t i = 0; i < server.GetN(); i++) {
        Ptr<Ipv4> ipv4 = server.Get(i)->GetObject<Ipv4>();
        std::cout << "Server " << i << " has routing protocol: " << (ipv4->GetRoutingProtocol() ? "YES" : "NO") << std::endl;
    }
    std::cout << "=====================\n" << std::endl;

    // アプリケーション設定
    // 受信アプリ（WiFi STA側）
    Ptr<VideoFrameReceiverApplication> receiver = CreateObject<VideoFrameReceiverApplication>();
    receiver->SetPort(9);

    // パケットログファイル設定
    std::string ampduStrLog = enableAmpdu ? "on" : "off";
    std::string edcaStrLog = enableEdca ? "on" : "off";
    std::ostringstream packetLogPath;
    packetLogPath << outputDir << "/packet_log_ampdu_" << ampduStrLog
                  << "_edca_" << edcaStrLog
                  << "_d" << static_cast<int>(distance) << "m.csv";
    receiver->SetPacketLogFile(packetLogPath.str());

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
    sender->SetStartTime(Seconds(3.0));
    sender->SetStopTime(Seconds(simulationTime));

    // QoS ログファイルを開く
    std::string ampduStrQos = enableAmpdu ? "on" : "off";
    std::string edcaStrQos = enableEdca ? "on" : "off";
    std::ostringstream qosLogPath;
    qosLogPath << outputDir << "/qos_log_ampdu_" << ampduStrQos
              << "_edca_" << edcaStrQos
              << "_d" << static_cast<int>(distance) << "m.csv";


    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream (outputDir + "/PhyRx.csv");
    *stream->GetStream () << "PhyRxTime,FrameID,FrameType,PacketIndex,TID,AccessCategory,IsAMPDU,AMPDURefNum" << std::endl; //header


    // PHY 層の受信トレースを接続（STA 側のみ）
    Config::Connect("/NodeList/" + std::to_string(sta.Get(0)->GetId()) +
                    "/DeviceList/*/$ns3::WifiNetDevice/Phy/$ns3::WifiPhy/MonitorSnifferRx",
                    MakeBoundCallback(&PhyRxTrace, stream));

    // Flow Monitor設定
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    // シミュレーション実行
    Simulator::Stop(Seconds(simulationTime + 1.0));
    Simulator::Run();

    // Flow Monitor統計出力
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

    std::cout << "\n=== Flow Monitor Statistics ===" << std::endl;
    uint32_t totalPackets = 0;
    uint32_t totalRxPackets = 0;
    for (auto it = stats.begin(); it != stats.end(); it++) {
        totalPackets += it->second.txPackets;
        totalRxPackets += it->second.rxPackets;
        std::cout << "Flow " << it->first << ":" << std::endl
                  << "  Tx Packets: " << it->second.txPackets << std::endl
                  << "  Rx Packets: " << it->second.rxPackets << std::endl;
    }
    std::cout << "Total Tx: " << totalPackets << ", Total Rx: " << totalRxPackets << std::endl;
    std::cout << "============================\n" << std::endl;

    // 結果出力
    std::string ampduStr = enableAmpdu ? "on" : "off";
    std::string edcaStr = enableEdca ? "on" : "off";
    std::ostringstream csvPath;
    csvPath << outputDir << "/stats_ampdu_" << ampduStr
            << "_edca_" << edcaStr
            << "_d" << static_cast<int>(distance) << "m.csv";

    receiver->SaveStatisticsToFile(csvPath.str());

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
