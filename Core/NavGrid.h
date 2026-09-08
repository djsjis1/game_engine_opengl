#ifndef ENGINE_NAVGRID_H
#define ENGINE_NAVGRID_H

#include <cmath>
#include <cstring>
#include <queue>
#include <vector>

// ============================================================
// NavGrid — 网格寻路（对标 Godot NavigationServer 的方块世界版）
//
// 【为什么是网格不是 NavMesh 多边形】本引擎场景以方块/盒碰撞体为主，
//   1m 网格与世界天然对齐——多边形 NavMesh（Recast 那套烘焙）在这里
//   是杀鸡用牛刀。网格版 A* + 视线平滑在方块世界效果与 NavMesh 相当。
//
// 【三步用法】（脚本侧见 nav_* API）
//   ① 烘焙：把静态碰撞体的占地格标记为障碍（nav_bake，进关卡时一次）
//   ② 寻路：findPath = A*（8 方向）+ 视线平滑（拉直锯齿路径）
//   ③ 跟随：敌人每帧 nav_next_point 拿下一个路点，朝它移动即可
//
// 【纯逻辑零依赖】不认识 flecs/GL——场景扫描由 Lua API 层做（喂障碍），
//   本类只管网格与算法 → 可完整单元测试。
// ============================================================
class NavGrid
{
public:
    static constexpr int kHalf = 32;        // 覆盖 ±32m
    static constexpr int kSize = kHalf * 2; // 64x64 格，1m/格

    static NavGrid &instance()
    {
        static NavGrid g;
        return g;
    }

    /// 清空全部障碍（重新烘焙前调）
    void clear() { std::memset(m_blocked, 0, sizeof(m_blocked)); }

    /// 标记障碍：世界坐标 AABB [minX,maxX]×[minZ,maxZ] 覆盖的格子全堵上
    void blockRect(float minX, float minZ, float maxX, float maxZ)
    {
        int gx0 = toGrid(minX), gx1 = toGrid(maxX);
        int gz0 = toGrid(minZ), gz1 = toGrid(maxZ);
        for (int z = gz0; z <= gz1; ++z)
            for (int x = gx0; x <= gx1; ++x)
                if (inside(x, z))
                    m_blocked[z * kSize + x] = 1;
    }

    bool isBlocked(float wx, float wz) const
    {
        int x = toGrid(wx), z = toGrid(wz);
        return !inside(x, z) || m_blocked[z * kSize + x];
    }

    /// A* 寻路 + 视线平滑。返回路点序列（世界坐标格中心，不含起点）。
    /// 找不到路/起终点被堵 → 空
    std::vector<std::pair<float, float>> findPath(float fromX, float fromZ,
                                                  float toX, float toZ) const
    {
        std::vector<std::pair<float, float>> out;
        int sx = toGrid(fromX), sz = toGrid(fromZ);
        int tx = toGrid(toX), tz = toGrid(toZ);
        if (!inside(sx, sz) || !inside(tx, tz))
            return out;
        if (m_blocked[tz * kSize + tx] || m_blocked[sz * kSize + sx])
            return out;
        if (sx == tx && sz == tz)
        {
            out.push_back({toX, toZ}); // 同格：直达目标点
            return out;
        }

        // ---- A*（8 方向；对角要求两侧直格都通，防"穿墙角"）----
        struct Node
        {
            int idx;
            float f;
            bool operator<(const Node &o) const { return f > o.f; } // 小顶堆
        };
        std::vector<float> g(kSize * kSize, 1e30f);
        std::vector<int> parent(kSize * kSize, -1);
        std::priority_queue<Node> open;
        auto hDist = [&](int x, int z) { // 对角距离启发
            float dx = (float)std::abs(x - tx), dz = (float)std::abs(z - tz);
            return (dx + dz) + (1.41421f - 2.0f) * std::fmin(dx, dz);
        };
        int start = sz * kSize + sx, target = tz * kSize + tx;
        g[start] = 0;
        open.push({start, hDist(sx, sz)});

        const int dx8[8] = {1, -1, 0, 0, 1, 1, -1, -1};
        const int dz8[8] = {0, 0, 1, -1, 1, -1, 1, -1};
        while (!open.empty())
        {
            int cur = open.top().idx;
            open.pop();
            if (cur == target)
                break;
            int cx = cur % kSize, cz = cur / kSize;
            for (int d = 0; d < 8; ++d)
            {
                int nx = cx + dx8[d], nz = cz + dz8[d];
                if (!inside(nx, nz) || m_blocked[nz * kSize + nx])
                    continue;
                if (d >= 4) // 对角：两个相邻直格必须都通
                {
                    if (m_blocked[cz * kSize + nx] || m_blocked[nz * kSize + cx])
                        continue;
                }
                float cost = (d >= 4) ? 1.41421f : 1.0f;
                int ni = nz * kSize + nx;
                if (g[cur] + cost < g[ni])
                {
                    g[ni] = g[cur] + cost;
                    parent[ni] = cur;
                    open.push({ni, g[ni] + hDist(nx, nz)});
                }
            }
        }
        if (parent[target] < 0)
            return out; // 不可达

        // 回溯格子路径（起点→终点顺序）
        std::vector<int> cells;
        for (int c = target; c != -1; c = parent[c])
            cells.push_back(c);
        for (size_t i = 0, j = cells.size() - 1; i < j; ++i, --j)
            std::swap(cells[i], cells[j]);

        // ---- 视线平滑（string pulling 简版）：从当前点尽量直连最远可见格 ----
        size_t anchor = 0;
        while (anchor + 1 < cells.size())
        {
            size_t far = anchor + 1;
            for (size_t probe = cells.size() - 1; probe > anchor + 1; --probe)
            {
                if (lineClear(cells[anchor], cells[probe]))
                {
                    far = probe;
                    break;
                }
            }
            out.push_back(center(cells[far]));
            anchor = far;
        }
        if (!out.empty())
            out.back() = {toX, toZ}; // 末点贴到真实目标（不卡格中心）
        return out;
    }

private:
    NavGrid() { clear(); }

    static int toGrid(float w) { return (int)std::floor(w) + kHalf; }
    static bool inside(int x, int z) { return x >= 0 && x < kSize && z >= 0 && z < kSize; }
    static std::pair<float, float> center(int idx)
    {
        return {(float)(idx % kSize - kHalf) + 0.5f, (float)(idx / kSize - kHalf) + 0.5f};
    }

    /// 两格中心连线是否无障碍（DDA 步进采样，步长 1/4 格防漏检）
    bool lineClear(int a, int b) const
    {
        float ax = (float)(a % kSize) + 0.5f, az = (float)(a / kSize) + 0.5f;
        float bx = (float)(b % kSize) + 0.5f, bz = (float)(b / kSize) + 0.5f;
        float dx = bx - ax, dz = bz - az;
        int steps = (int)(std::fmax(std::fabs(dx), std::fabs(dz)) * 4.0f) + 1;
        for (int i = 1; i < steps; ++i)
        {
            float t = (float)i / steps;
            int x = (int)(ax + dx * t), z = (int)(az + dz * t);
            if (!inside(x, z) || m_blocked[z * kSize + x])
                return false;
        }
        return true;
    }

    unsigned char m_blocked[kSize * kSize];
};

#endif
