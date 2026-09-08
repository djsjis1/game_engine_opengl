#ifndef SCRIPTAPISTUB_H
#define SCRIPTAPISTUB_H

#include "Scripting/ScriptApiRegistry.h"
#include <QFile>
#include <QTextStream>
#include <QTextCodec>
#include <QString>

// ============================================================
// ScriptApiStub —— 生成 VSCode 用的 Lua API 存根（definition file）
//
// 【原理】VSCode 的 lua-language-server（Sumneko）会扫描工作区里的
//   .lua 文件收集全局函数定义。我们生成一个 ---@meta 标记的存根文件，
//   用 EmmyLua 注解（---@param/---@return）声明所有引擎注入的 API。
//   → VSCode 打开脚本目录即获得：补全 + 参数提示 + 悬停文档 + 拼错报红
//
// 【和内置编辑器补全同源】都读 scriptApiList()，改一处两处同步。
// 【何时生成】程序启动时写一次到脚本目录（覆盖，保持与代码同步）。
// ============================================================
namespace ScriptApiStub
{
    /// 生成存根内容（EmmyLua 注解格式）
    inline QString generate()
    {
        QString s;
        QTextStream out(&s);
        out.setCodec(QTextCodec::codecForName("UTF-8")); // 写入 QString 缓冲也固定 UTF-8
        // ★ api.doc 等是 const char*（源码 UTF-8 字节），必须 fromUtf8 入流：
        //   直接 out << const char* 会用 locale（GBK）解码 → 乱码（实际踩过）
        out << "---@meta\n";
        out << "-- ============================================================\n";
        out << QString::fromUtf8("-- 引擎注入的 Lua API 存根（自动生成，勿手改）\n");
        out << QString::fromUtf8("-- 由 ScriptApiStub 从 ScriptApiRegistry 生成，供 VSCode\n");
        out << QString::fromUtf8("-- lua-language-server 提供补全/参数提示/文档/报错。\n");
        out << "-- ============================================================\n\n";

        for (const auto &api : scriptApiList())
        {
            // 文档注释（doc 含中文，必须 fromUtf8）
            out << "--- " << QString::fromUtf8(api.doc) << "\n";
            // @param 注释（每个形参一行；跳过 ... 变参和空参）
            QString params = QString::fromUtf8(api.params);
            if (!params.isEmpty() && params != "...")
            {
                for (const QString &p : params.split(",", Qt::SkipEmptyParts))
                {
                    QString name = p.trimmed();
                    out << "---@param " << name << " any\n";
                }
            }
            // @return 注释
            if (api.ret[0])
                out << "---@return " << QString::fromUtf8(api.ret) << "\n";
            // 函数存根（空实现）
            out << "function " << QString::fromUtf8(api.name)
                << "(" << params << ") end\n\n";
        }
        return s;
    }

    /// 写入指定目录（返回是否成功）。dir 应为脚本目录绝对路径。
    /// ★ 固定 UTF-8 写入：否则系统默认编码（Windows=GBK）写中文 →
    /// VSCode/编辑器按 UTF-8 读就乱码（与项目统一 UTF-8）
    inline bool writeTo(const QString &dir)
    {
        QFile f(dir + "/engine_api.lua");
        if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
            return false;
        QTextStream out(&f);
        out.setCodec(QTextCodec::codecForName("UTF-8"));
        out << generate();
        return true;
    }
}

#endif
