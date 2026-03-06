// ════════════════════════════════════════════════════════════════
//  main.cpp
//
//  双模式入口：
//    ① GUI 模式（无 --replay）：启动 HTTP Server 监听 8888
//    ② Replay 模式（有 --replay）：
//         http_ticket_server.exe --replay <json>
//         http_ticket_server.exe --replay <json> --golden <golden.hex>
//         http_ticket_server.exe --replay <json> --golden <golden.hex> --out <out.hex>
//       控制台输出 diff 报告，返回 0(PASS)/1(FAIL)
// ════════════════════════════════════════════════════════════════
#include <QApplication>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QCoreApplication>
#include "TicketHttpServer.h"
#include "LogWindow.h"
#include "ReplayRunner.h"

int main(int argc, char *argv[])
{
    // ── 先用 QCoreApplication 解析 CLI，不创建 GUI ──────────────
    // 但 QApplication 是需要的（因为 GUI 模式也在同一 exe）
    // 技巧：先检查 argv 里有没有 --replay，有就走 CLI 分支
    // ────────────────────────────────────────────────────────────
    bool hasReplay = false;
    for (int i = 1; i < argc; ++i) {
        if (QString::fromLocal8Bit(argv[i]) == "--replay") {
            hasReplay = true;
            break;
        }
    }

    if (hasReplay) {
        // ── Replay / CLI 模式 ──────────────────────────────────
        QCoreApplication app(argc, argv);
        app.setApplicationName("http_ticket_server_replay");

        QCommandLineParser parser;
        parser.setApplicationDescription(
            QString::fromUtf8("Stage 5.4 回放测试 + HEX diff 工具\n"
                              "用法:\n"
                              "  --replay <json>              仅生成帧\n"
                              "  --replay <json> --golden <hex>   生成 + 对比\n"
                              "  --replay <json> --golden <hex> --out <out.hex>  同上并指定输出路径"));
        parser.addHelpOption();

        QCommandLineOption replayOpt(
            "replay",
            QString::fromUtf8("票据 JSON 文件路径（UTF-8，原产品 HTTP body 或测试用例）"),
            "json");
        QCommandLineOption goldenOpt(
            "golden",
            QString::fromUtf8("原产品串口抓包 HEX 文件路径（空格/换行分隔，可含 SEN:/RCV: 前缀）"),
            "hex");
        QCommandLineOption outOpt(
            "out",
            QString::fromUtf8("生成帧输出路径（可选，默认与 JSON 同目录，后缀 _generated.hex）"),
            "out_hex");

        parser.addOption(replayOpt);
        parser.addOption(goldenOpt);
        parser.addOption(outOpt);
        parser.process(app);

        QString jsonPath   = parser.value(replayOpt);
        QString goldenPath = parser.value(goldenOpt);
        QString outPath    = parser.value(outOpt);

        if (jsonPath.isEmpty()) {
            fprintf(stderr, "错误: --replay 需要指定 JSON 文件路径\n");
            parser.showHelp(1);
        }

        return ReplayRunner::run(jsonPath, goldenPath, outPath);

    } else {
        // ── GUI 模式（原服务器模式）──────────────────────────────
        QApplication app(argc, argv);

        LogWindow logWindow;
        TicketHttpServer server;

        QObject::connect(&server, &TicketHttpServer::requestReceived,
                         &logWindow, &LogWindow::appendLog);
        QObject::connect(&server, &TicketHttpServer::jsonBodySaved,
                         &logWindow, &LogWindow::onHttpJsonSaved);

        const quint16 port = 8888;
        if (!server.start(QHostAddress("127.0.0.1"), port)) {
            return 1;
        }

        logWindow.setStatusText(
            QString::fromUtf8("● 正在监听 http://127.0.0.1:%1  |  POST /ywticket/WebApi/transTicket\n"
                              "  Replay 用法: http_ticket_server.exe --replay <json> [--golden <hex>]")
                .arg(port));
        logWindow.initReplayPanel();   // 自动填充 Replay/Diff 默认路径
        logWindow.show();

        return app.exec();
    }
}
