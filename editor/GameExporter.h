#ifndef GAMEEXPORTER_H
#define GAMEEXPORTER_H

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLibraryInfo>
#include <QSaveFile>
#include <QString>
#include <QStringList>
#include <QUuid>

// ============================================================
// GameExporter — 导出游戏（对标 Godot 导出）
//
// 先生成并验证同级临时目录，再替换目标目录。复制或配置写入失败时，
// 保留已有导出，不把半成品报告为成功。
// ============================================================
class GameExporter
{
public:
    struct Result
    {
        bool ok = false;
        QString message;
    };

    /// 执行导出。scenePath=assets/scenes 内的主场景，outDir=导出目录。
    static Result exportGame(const QString &scenePath, const QString &outDir)
    {
        const QString gameExe = QCoreApplication::applicationDirPath() + "/my_game.exe";
        if (!QFileInfo(gameExe).isFile())
            return failure("找不到 my_game.exe（先构建 my_game 目标）");

        const QString assetRoot = QFileInfo(QString::fromUtf8(ASSET_DIR)).canonicalFilePath();
        const QString scenesRoot = QDir(assetRoot).filePath("scenes");
        const QString scenesCanonical = QFileInfo(scenesRoot).canonicalFilePath();
        const QString sceneCanonical = QFileInfo(scenePath).canonicalFilePath();
        if (assetRoot.isEmpty() || scenesCanonical.isEmpty() || sceneCanonical.isEmpty() ||
            !QFileInfo(sceneCanonical).isFile())
            return failure("主场景或 assets/scenes 目录不存在");

        QString relativeScene = QDir(scenesCanonical).relativeFilePath(sceneCanonical);
        relativeScene = QDir::cleanPath(relativeScene).replace('\\', '/');
        if (relativeScene == "." || relativeScene == ".." ||
            relativeScene.startsWith("../") || QDir::isAbsolutePath(relativeScene))
            return failure("主场景必须位于 assets/scenes 目录内");

        const QString finalPath = QDir::cleanPath(QFileInfo(outDir).absoluteFilePath());
        const QFileInfo finalInfo(finalPath);
        const QString finalName = finalInfo.fileName();
        const QString parentPath = finalInfo.absolutePath();
        if (outDir.trimmed().isEmpty() || finalName.isEmpty())
            return failure("导出目录无效");
        if (finalInfo.exists() && !finalInfo.isDir())
            return failure("导出目标已存在且不是目录");

        const QString relativeOutput = QDir(assetRoot).relativeFilePath(finalPath);
        if (relativeOutput == "." || relativeOutput == ".." ||
            (!relativeOutput.startsWith("../") && !QDir::isAbsolutePath(relativeOutput)))
            return failure("导出目录不能放在 assets 目录内");

        QDir parentDir;
        if (!parentDir.mkpath(parentPath))
            return failure("无法创建导出目录的父目录：" + parentPath);
        parentDir.setPath(parentPath);

        const QString suffix = QUuid::createUuid().toString(QUuid::WithoutBraces);
        const QString stageName = finalName + ".staging-" + suffix;
        const QString backupName = finalName + ".backup-" + suffix;
        const QString stagePath = parentDir.filePath(stageName);
        if (!parentDir.mkdir(stageName))
            return failure("无法创建临时导出目录：" + stagePath);

        auto abort = [&](const QString &message)
        {
            QDir(stagePath).removeRecursively();
            return failure(message);
        };

        QStringList errors;
        if (!copyFile(gameExe, QDir(stagePath).filePath("my_game.exe"), errors))
            return abort(errors.join('\n'));

        const char *assetDirs[] = {"scenes", "scripts", "anims", "sprites",
                                   "prefabs", "models", "textures", "Shaders"};
        for (const char *dirName : assetDirs)
        {
            const QString name = QString::fromLatin1(dirName);
            const QString source = QDir(assetRoot).filePath(name);
            if (!QDir(source).exists())
            {
                if (name == "scenes")
                    errors << "缺少资源目录：" + source;
                continue;
            }
            if (!copyDirRecursive(source, QDir(stagePath).filePath("assets/" + name), errors))
                return abort(errors.join('\n'));
        }
        if (!errors.isEmpty())
            return abort(errors.join('\n'));

        const QString stagedScene = QDir(stagePath).filePath("assets/scenes/" + relativeScene);
        if (!QFileInfo(stagedScene).isFile())
            return abort("主场景没有复制到导出包：" + relativeScene);

        QJsonObject config;
        config.insert("main_scene", "scenes/" + relativeScene);
        const QByteArray configBytes = QJsonDocument(config).toJson(QJsonDocument::Indented);
        QSaveFile configFile(QDir(stagePath).filePath("assets/game.json"));
        if (!configFile.open(QIODevice::WriteOnly) ||
            configFile.write(configBytes) != configBytes.size() || !configFile.commit())
            return abort("无法写入 assets/game.json");

        int copiedQtFiles = 0;
        if (!deployQtRuntime(stagePath, copiedQtFiles, errors))
            return abort(errors.join('\n'));

        bool movedOldOutput = false;
        if (parentDir.exists(finalName))
        {
            if (!parentDir.rename(finalName, backupName))
                return abort("无法暂存旧导出目录：" + finalPath);
            movedOldOutput = true;
        }

        if (!parentDir.rename(stageName, finalName))
        {
            if (movedOldOutput)
                parentDir.rename(backupName, finalName);
            return abort("无法发布新导出目录：" + finalPath);
        }

        QString message = QString("导出成功，部署 Qt 文件 %1 个。\n主场景：assets/%2")
                              .arg(copiedQtFiles)
                              .arg("scenes/" + relativeScene);
        if (movedOldOutput && !QDir(parentDir.filePath(backupName)).removeRecursively())
            message += "\n旧导出备份未能清理：" + parentDir.filePath(backupName);
        return {true, message};
    }

private:
    static Result failure(const QString &message)
    {
        return {false, message};
    }

    static bool copyFile(const QString &source, const QString &destination,
                         QStringList &errors)
    {
        if (!QFileInfo(source).isFile())
        {
            errors << "缺少必需文件：" + source;
            return false;
        }
        if (!QDir().mkpath(QFileInfo(destination).absolutePath()) ||
            !QFile::copy(source, destination))
        {
            errors << "复制文件失败：" + source + " -> " + destination;
            return false;
        }
        return true;
    }

    /// 递归复制目录，任何可见文件复制失败都使整个导出失败。
    static bool copyDirRecursive(const QString &source, const QString &destination,
                                 QStringList &errors)
    {
        QDir sourceDir(source);
        if (!sourceDir.exists() || !sourceDir.isReadable())
        {
            errors << "无法读取资源目录：" + source;
            return false;
        }
        if (!QDir().mkpath(destination))
        {
            errors << "无法创建资源目录：" + destination;
            return false;
        }

        const auto files = sourceDir.entryInfoList(
            QDir::Files | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot,
            QDir::Name);
        for (const QFileInfo &file : files)
        {
            if (file.fileName().startsWith("__"))
                continue;
            const QString target = QDir(destination).filePath(file.fileName());
            if (!QFile::copy(file.filePath(), target))
            {
                errors << "复制资源失败：" + file.filePath() + " -> " + target;
                return false;
            }
        }

        const auto dirs = sourceDir.entryInfoList(
            QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System,
            QDir::Name);
        for (const QFileInfo &dir : dirs)
        {
            if (!copyDirRecursive(dir.filePath(), QDir(destination).filePath(dir.fileName()), errors))
                return false;
        }
        return true;
    }

    /// 部署 Qt 运行时。核心模块和 Windows 平台插件必需，其余 DLL 可选。
    static bool deployQtRuntime(const QString &outDir, int &copied, QStringList &errors)
    {
        const QString qtBin = QLibraryInfo::location(QLibraryInfo::BinariesPath);
        const char *requiredDlls[] = {
            "Qt5Core.dll", "Qt5Gui.dll", "Qt5Widgets.dll", "Qt5OpenGL.dll"};
        const char *optionalDlls[] = {
            "Qt5Svg.dll", "libEGL.dll", "libGLESv2.dll", "opengl32sw.dll",
            "D3Dcompiler_47.dll"};

        copied = 0;
        for (const char *dll : requiredDlls)
        {
            if (!copyFile(QDir(qtBin).filePath(QString::fromLatin1(dll)),
                          QDir(outDir).filePath(QString::fromLatin1(dll)), errors))
                return false;
            ++copied;
        }
        for (const char *dll : optionalDlls)
        {
            const QString source = QDir(qtBin).filePath(QString::fromLatin1(dll));
            if (QFileInfo(source).exists())
            {
                if (!copyFile(source, QDir(outDir).filePath(QString::fromLatin1(dll)), errors))
                    return false;
                ++copied;
            }
        }

        const QString platformSource =
            QDir(QLibraryInfo::location(QLibraryInfo::PluginsPath))
                .filePath("platforms/qwindows.dll");
        if (!copyFile(platformSource, QDir(outDir).filePath("platforms/qwindows.dll"), errors))
            return false;
        ++copied;
        return true;
    }
};

#endif
