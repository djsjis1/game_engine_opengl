// ============================================================
// reflect_gen — mini-UHT：组件反射注册代码生成器
//
// 【做什么】（对标 Unreal Header Tool 的极简版）
//   扫描 Components/*.h 中带 REFLECT 宏前缀的字段，
//   自动生成 flecs 反射注册代码（member<T>(名字, 1, offsetof...)）
//
// 【用法】reflect_gen <Components目录> <输出.inl路径>
//   CMake 在每次构建 my_demo 前自动执行（内容不变则不写文件，避免无谓重编）
//
// 【标记语法】字段声明前加 REFLECT 宏前缀：
//   struct HealthComponent {
//       REFLECT float hp = 100;   // 进反射（编辑器+序列化自动支持）
//       float cachedValue;         // 不标记 = 运行时字段，反射不可见
//   };
//   REFLECT 定义在 Components/Reflect.h，对编译器展开为空（零开销）
//
// 【支持类型】float / bool / int / int32_t / glm::vec3
//   （新类型：下面 kTypeMap 加一行即可）
//
// 【为什么不用完整 C++ 解析器】
//   只需支持本项目的组件写法（POD struct + 单行字段声明），
//   正则逐行扫描足够——UHT 的复杂度来自要支持任意 C++，我们不需要
// ============================================================

#include <cstdio>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <regex>
#include <filesystem>

namespace fs = std::filesystem;

struct Field
{
    std::string type; // member<T> 里的 T
    std::string name;
};

struct Component
{
    std::string name;
    std::vector<Field> fields;
};

// 源码类型 → flecs member<T> 类型映射（int 统一成 int32_t）
static const char *mapType(const std::string &src)
{
    if (src == "int")
        return "int32_t";
    return src.c_str(); // float/bool/int32_t/glm::vec3 原样
}

int main(int argc, char **argv)
{
    if (argc < 3)
    {
        fprintf(stderr, "用法: reflect_gen <Components目录> <输出.inl>\n");
        return 1;
    }
    fs::path compDir = argv[1];
    fs::path outPath = argv[2];

    // 字段行匹配： REFLECT 类型 名字 ...
    // 例：  REFLECT glm::vec3 position{0,0,0};
    //       REFLECT float mass = 1.0f;
    //       REFLECT bool visible = true;
    // 用宏前缀而非注释标记：拼错编译报错、注释恢复自由（对标 Unreal UPROPERTY）
    std::regex fieldRe(
        R"(^\s*REFLECT\s+(glm::vec3|float|bool|int32_t|int)\s+(\w+))");
    std::regex structRe(R"(^\s*struct\s+(\w+))");
    // struct 结束：必须是行首的 "};"。不能用 find("};")：
    // 字段的花括号初始化 vec3 pos{0,0,0}; 也含此子串，
    // 会把 struct 提前"结束"导致后续字段全丢（测试抓出的真实 bug）
    std::regex structEndRe(R"(^\s*\};)");

    std::vector<Component> comps;

    // 逐文件逐行扫描（按文件名排序，保证生成结果稳定可重现）
    std::vector<fs::path> headers;
    for (auto &entry : fs::directory_iterator(compDir))
        if (entry.path().extension() == ".h")
            headers.push_back(entry.path());
    std::sort(headers.begin(), headers.end());

    for (auto &h : headers)
    {
        std::ifstream in(h);
        std::string line;
        Component cur;
        bool inStruct = false;

        while (std::getline(in, line))
        {
            std::smatch m;
            if (std::regex_search(line, m, structRe))
            {
                // 新 struct 开始（前一个若有字段先收下）
                if (inStruct && !cur.fields.empty())
                    comps.push_back(cur);
                cur = Component{};
                cur.name = m[1];
                inStruct = true;
                continue;
            }
            if (inStruct && std::regex_search(line, m, fieldRe))
                cur.fields.push_back({mapType(m[1]), m[2]});
            // struct 结束（仅行首 "};" 才算）
            if (inStruct && std::regex_search(line, m, structEndRe))
            {
                if (!cur.fields.empty())
                    comps.push_back(cur);
                cur = Component{};
                inStruct = false;
            }
        }
        if (inStruct && !cur.fields.empty())
            comps.push_back(cur);
    }

    // 生成注册代码
    std::ostringstream out;
    out << "#pragma once\n";
    out << "// =====================================================\n";
    out << "// \u26a0 \u81ea\u52a8\u751f\u6210\u6587\u4ef6\uff0c\u52ff\u624b\u6539\uff01\n";
    out << "// \u7531 tools/reflect_gen.cpp \u626b\u63cf Components/*.h \u7684 @reflect \u6807\u8bb0\u751f\u6210\n";
    out << "// \u52a0\u5b57\u6bb5\uff1a\u7ec4\u4ef6\u5934\u6587\u4ef6\u91cc\u5b57\u6bb5\u884c\u5c3e\u52a0 // @reflect\uff0c\u91cd\u65b0\u6784\u5efa\u5373\u751f\u6548\n";
    out << "// =====================================================\n\n";
    out << "inline void registerGeneratedReflection(flecs::world &w)\n{\n";
    for (auto &c : comps)
    {
        out << "    w.component<" << c.name << ">()";
        for (auto &f : c.fields)
            out << "\n        .member<" << f.type << ">(\"" << f.name
                << "\", 1, offsetof(" << c.name << ", " << f.name << "))";
        out << ";\n\n";
    }
    out << "}\n";

    std::string newContent = out.str();

    // 内容没变则不写（保持时间戳 → 不触发下游重编译）
    {
        std::ifstream old(outPath);
        if (old)
        {
            std::stringstream oldContent;
            oldContent << old.rdbuf();
            if (oldContent.str() == newContent)
            {
                printf("reflect_gen: \u65e0\u53d8\u5316\uff08%d \u4e2a\u7ec4\u4ef6\uff09\n", (int)comps.size());
                return 0;
            }
        }
    }

    fs::create_directories(outPath.parent_path());
    std::ofstream o(outPath);
    if (!o)
    {
        fprintf(stderr, "reflect_gen: \u65e0\u6cd5\u5199\u5165 %s\n", outPath.string().c_str());
        return 1;
    }
    o << newContent;
    printf("reflect_gen: \u5df2\u751f\u6210 %d \u4e2a\u7ec4\u4ef6\u7684\u53cd\u5c04\u6ce8\u518c \u2192 %s\n",
           (int)comps.size(), outPath.string().c_str());
    return 0;
}
