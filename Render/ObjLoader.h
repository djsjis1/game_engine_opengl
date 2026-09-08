#ifndef OBJLOADER_H
#define OBJLOADER_H

#include "Render/MeshFactory.h" // Vertex（与 cube 同布局：pos3+normal3+uv2）
#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

// ============================================================
// ObjLoader — Wavefront OBJ 模型解析（自写，学习目的）
//
// 支持的子集（覆盖 Blender/网上素材导出的绝大多数静态模型）：
//   v  x y z        顶点位置
//   vt u v          纹理坐标
//   vn x y z        法线
//   f  a/b/c ...    面（支持 v、v/vt、v//vn、v/vt/vn 四种写法；
//                     四边形/多边形自动扇形三角化）
// 不支持：材质(.mtl)/组/平滑组——颜色走引擎 MaterialComponent（PBR）
//
// 【输出】展开的非索引三角形顶点数组（Vertex 布局与 cube 完全一致
//   → 复用 default.vert/frag 全套光照/阴影/PBR，模型进来就有质感）
// 【无法线的 OBJ】自动算面法线（平面着色，方块风素材常见）
// 【纯逻辑无 GL】可单元测试；GPU 上传由 MeshFactory 负责
// ============================================================
namespace ObjLoader
{

    /// 从流解析（测试直接喂 stringstream；文件版在下面）
    inline bool parse(std::istream &in, std::vector<Vertex> &out)
    {
        std::vector<float> pos, uv, nrm; // 原始数据池（f 用索引引用它们）
        out.clear();

        // f 的一个角 "3/1/5" → 三个索引（OBJ 从 1 计数，负数=从尾部倒数）
        auto resolveIdx = [](int idx, size_t count) -> int
        {
            if (idx > 0)
                return idx - 1;
            if (idx < 0)
                return (int)count + idx; // -1 = 最后一个
            return -1;
        };

        std::string line;
        while (std::getline(in, line))
        {
            if (line.size() < 2)
                continue;
            std::istringstream ls(line);
            std::string tag;
            ls >> tag;

            if (tag == "v")
            {
                float x = 0, y = 0, z = 0;
                ls >> x >> y >> z;
                pos.insert(pos.end(), {x, y, z});
            }
            else if (tag == "vt")
            {
                float u = 0, v = 0;
                ls >> u >> v;
                uv.insert(uv.end(), {u, v});
            }
            else if (tag == "vn")
            {
                float x = 0, y = 0, z = 0;
                ls >> x >> y >> z;
                nrm.insert(nrm.end(), {x, y, z});
            }
            else if (tag == "f")
            {
                // 收集本面全部角（可能 3~n 个），再扇形三角化
                struct Corner
                {
                    int p = -1, t = -1, n = -1;
                };
                std::vector<Corner> corners;
                std::string vert;
                while (ls >> vert)
                {
                    Corner c;
                    // 解析 "p"、"p/t"、"p//n"、"p/t/n"
                    int vals[3] = {0, 0, 0};
                    int vi = 0;
                    size_t start = 0;
                    for (size_t i = 0; i <= vert.size(); ++i)
                    {
                        if (i == vert.size() || vert[i] == '/')
                        {
                            if (i > start && vi < 3)
                                vals[vi] = std::stoi(vert.substr(start, i - start));
                            start = i + 1;
                            ++vi;
                        }
                    }
                    c.p = resolveIdx(vals[0], pos.size() / 3);
                    c.t = resolveIdx(vals[1], uv.size() / 2);
                    c.n = resolveIdx(vals[2], nrm.size() / 3);
                    corners.push_back(c);
                }
                if (corners.size() < 3)
                    continue;

                // 扇形三角化：(0, i, i+1)
                for (size_t i = 1; i + 1 < corners.size(); ++i)
                {
                    const Corner tri[3] = {corners[0], corners[i], corners[i + 1]};
                    Vertex vtx[3] = {};
                    bool hasN = true;
                    for (int k = 0; k < 3; ++k)
                    {
                        const Corner &c = tri[k];
                        if (c.p < 0 || c.p * 3 + 2 >= (int)pos.size())
                            return false; // 位置索引非法：文件损坏
                        vtx[k].px = pos[c.p * 3 + 0];
                        vtx[k].py = pos[c.p * 3 + 1];
                        vtx[k].pz = pos[c.p * 3 + 2];
                        if (c.t >= 0 && c.t * 2 + 1 < (int)uv.size())
                        {
                            vtx[k].u = uv[c.t * 2 + 0];
                            vtx[k].v = uv[c.t * 2 + 1];
                        }
                        if (c.n >= 0 && c.n * 3 + 2 < (int)nrm.size())
                        {
                            vtx[k].nx = nrm[c.n * 3 + 0];
                            vtx[k].ny = nrm[c.n * 3 + 1];
                            vtx[k].nz = nrm[c.n * 3 + 2];
                        }
                        else
                            hasN = false;
                    }
                    if (!hasN)
                    {
                        // 无法线 → 面法线（叉积），平面着色
                        float ux = vtx[1].px - vtx[0].px, uy = vtx[1].py - vtx[0].py,
                              uz = vtx[1].pz - vtx[0].pz;
                        float vx = vtx[2].px - vtx[0].px, vy = vtx[2].py - vtx[0].py,
                              vz = vtx[2].pz - vtx[0].pz;
                        float nx = uy * vz - uz * vy;
                        float ny = uz * vx - ux * vz;
                        float nz = ux * vy - uy * vx;
                        float len = std::sqrt(nx * nx + ny * ny + nz * nz);
                        if (len > 1e-8f)
                        {
                            nx /= len;
                            ny /= len;
                            nz /= len;
                        }
                        for (auto &vv : vtx)
                        {
                            vv.nx = nx;
                            vv.ny = ny;
                            vv.nz = nz;
                        }
                    }
                    out.insert(out.end(), {vtx[0], vtx[1], vtx[2]});
                }
            }
            // 其余行（#注释/o/g/s/usemtl/mtllib）静默跳过
        }
        return !out.empty();
    }

    /// 从文件解析（失败打日志返回 false，路径为绝对路径）
    inline bool loadFile(const std::string &absPath, std::vector<Vertex> &out)
    {
        std::ifstream f(absPath);
        if (!f.is_open())
        {
            fprintf(stderr, "[ObjLoader] 打不开模型: %s\n", absPath.c_str());
            return false;
        }
        if (!parse(f, out))
        {
            fprintf(stderr, "[ObjLoader] 解析失败(无有效面): %s\n", absPath.c_str());
            return false;
        }
        return true;
    }

} // namespace ObjLoader

#endif
