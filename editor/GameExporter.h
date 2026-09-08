#ifndef GAMEEXPORTER_H
#define GAMEEXPORTER_H

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLibraryInfo>
#include <QString>
#include <functional>

// ============================================================
// GameExporter — 导出游戏（对标 Godot 导出）
//
// 产物：目标目录/my_game.exe + assets/{scenes,scripts,...} + game.json
//       + Qt DLL + platforms/qwindows.dll
// my_game.exe 启动即进 Play 直接跑主场景，无任何编辑器 UI。
//
// 【解耦】纯逻辑类，零 QWidget 依赖：文件对话框/结果弹窗是 UI 层
//   （MainWindow）的事——将来换 UI 框架时本类原样保留。
//   返回 Result 结构体，UI 层自行决定怎么展示。
// ============================================================
class GameExporter
{
public:
    struct Result
    {
        bool ok = false;
        QString message; // 成功：部署摘要；失败：原因（UI 层直接展示）
    };

    /// 执行导出。scenePath=主场景绝对路径，outDir=导出目录
    static Result exportGame(const QString &scenePath, const QString &outDir)
    {
        Result r;

        // ① 拷运行时 exe（和编辑器同目录编出的 my_game.exe）
        QString gameExe = QCoreApplication::applicationDirPath() + "/my_game.exe";
        if (!QFile::exists(gameExe))
        {
            r.message = "找不到 my_game.exe（先构建 my_game 目标）";
            return r;
        }
        QFile::remove(outDir + "/my_game.exe"); // 覆盖旧导出
        QFile::copy(gameExe, outDir + "/my_game.exe");

        // ② 递归拷资源目录 → assets/（游戏资源跟着 exe 走）
        const char *assetDirs[] = {"scenes", "scripts", "anims",
                                   "sprites", "prefabs", "models", "textures", "Shaders"};
        for (const char *d : assetDirs)
        {
            QString src = QString(ASSET_DIR) + d;
            if (QDir(src).exists())
                copyDirRecursive(src, outDir + "/assets/" + d);
        }

        // ③ 写游戏配置：主场景用相对路径（相对 assets/）
        QFile cfg(outDir + "/assets/game.json");
        if (cfg.open(QIODevice::WriteOnly | QIODevice::Text))
        {
            QString name = QFileInfo(scenePath).fileName();
            cfg.write(QString("{ \"main_scene\": \"scenes/%1\" }\n")
                          .arg(name)
                          .toUtf8());
        }

        // ④ 部署 Qt DLL —— 直接从"本编辑器链接的那个 Qt"拷贝（关键！）
        //   QLibraryInfo::BinariesPath = 编译期确定的 Qt bin，天然与
        //   my_game.exe 编译器一致（MSVC）。不靠 windeployqt 顺 PATH 抓——
        //   多 Qt（MinGW+MSVC）混装时它会抓错编译器版本、版本判重不覆盖、
        //   漏部署 Qt5OpenGL（实测："无法定位程序输入点"）。
        //   详见 docs/troubleshooting.md 坑 25。
        int copied = deployQtRuntime(outDir);

        r.ok = true;
        r.message = copied > 0
                        ? QString("Qt DLL 已部署（%1 个，取自 %2）")
                              .arg(copied)
                              .arg(QLibraryInfo::location(QLibraryInfo::BinariesPath))
                        : "⚠ 未找到 Qt DLL，请手动拷贝到导出目录。";
        return r;
    }

private:
    /// 递归拷目录（跳过 __ 前缀的内部文件，如 Play 备份）
    static void copyDirRecursive(const QString &src, const QString &dst)
    {
        QDir().mkpath(dst);
        QDir s(src);
        for (const auto &f : s.entryList(QDir::Files))
        {
            if (f.startsWith("__"))
                continue; // 内部文件不进游戏包
            QFile::remove(dst + "/" + f);
            QFile::copy(src + "/" + f, dst + "/" + f);
        }
        for (const auto &d : s.entryList(QDir::Dirs | QDir::NoDotAndDotDot))
            copyDirRecursive(src + "/" + d, dst + "/" + d);
    }

    /// 部署 Qt 运行时（核心 DLL + 平台插件），返回拷贝成功的 DLL 数
    static int deployQtRuntime(const QString &outDir)
    {
        QString qtBin = QLibraryInfo::location(QLibraryInfo::BinariesPath);
        // 核心 Qt 模块 + ANGLE/软渲染依赖（my_game 用到的）
        const char *qtDlls[] = {
            "Qt5Core.dll", "Qt5Gui.dll", "Qt5Widgets.dll", "Qt5OpenGL.dll",
            "Qt5Svg.dll", "libEGL.dll", "libGLESv2.dll", "opengl32sw.dll",
            "D3Dcompiler_47.dll"};
        int copied = 0;
        for (const char *dll : qtDlls)
        {
            QString srcDll = qtBin + "/" + dll;
            if (QFile::exists(srcDll))
            {
                QFile::remove(outDir + "/" + dll);
                if (QFile::copy(srcDll, outDir + "/" + dll))
                    ++copied;
            }
        }
        // 平台插件必须放 platforms/ 子目录（缺它 Qt 程序直接闪退）
        QString platSrc = QLibraryInfo::location(QLibraryInfo::PluginsPath) + "/platforms/qwindows.dll";
        if (QFile::exists(platSrc))
        {
            QDir().mkpath(outDir + "/platforms");
            QFile::remove(outDir + "/platforms/qwindows.dll");
            QFile::copy(platSrc, outDir + "/platforms/qwindows.dll");
        }
        return copied;
    }
};

#endif
