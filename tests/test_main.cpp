// ============================================================
// 单元测试入口 — 自定义 main
//
// 为什么不用 gtest_main：SaveGame 依赖 QCoreApplication::
// applicationDirPath()（存档目录定位），必须先建 QCoreApplication
// 实例，否则返回空路径导致存档测试全挂。
// ============================================================
#include <gtest/gtest.h>
#include <QCoreApplication>
#include "Core/EngineLog.h"

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv); // SaveGame 定位 exe 目录用
    ::testing::InitGoogleTest(&argc, argv);
    int rc = RUN_ALL_TESTS();
    shutdownEngineLog(); // spdlog 显式收尾（防静态析构顺序崩溃）
    return rc;
}
