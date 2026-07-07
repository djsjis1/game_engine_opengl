
#include "entt/entt.hpp"
#include <iostream>
#include <string>

// ============================================================
// Meta 反射系统 — 用于演示的类
// ============================================================

struct Health
{
    float current = 100.0f;
    float max = 100.0f;

    void heal(float amount)
    {
        current = (current + amount > max) ? max : current + amount;
    }

    void takeDamage(float amount)
    {
        current -= amount;
    }

    void print() const
    {
        std::cout << "  HP: " << current << "/" << max << "\n";
    }
};

struct Player
{
    std::string name;
    int level = 1;

    void levelUp() { ++level; }
    void setName(const std::string &n) { name = n; }
};

// ---- 注册所有反射类型 ----
void registerMetaTypes()
{
    using namespace entt::literals;

    // 注册 Health 类
    entt::meta_factory<Health>{}
        .type("Health"_hs)
        .data<&Health::current>("current"_hs) // 读写成员
        .data<&Health::max>("max"_hs)         // 读写成员
        .func<&Health::heal>("heal"_hs)       // 成员函数
        .func<&Health::takeDamage>("damage"_hs)
        .func<&Health::print>("print"_hs);

    // 注册 Player 类
    entt::meta_factory<Player>{}
        .type("Player"_hs)
        .data<&Player::name>("name"_hs)
        .data<&Player::level>("level"_hs)
        .func<&Player::levelUp>("levelUp"_hs)
        .func<&Player::setName>("setName"_hs);
}

int main()
{
    // ---- 注册类型 ----
    registerMetaTypes();

    std::cout << "=== EnTT Meta 反射系统 ===\n\n";

    using namespace entt::literals;

    // ================================================================
    // 1. 运行时按名字查找类型
    // ================================================================
    auto healthType = entt::resolve<Health>();     // 编译期查
    auto healthType2 = entt::resolve("Health"_hs); // 运行时按名字查

    std::cout << "1. 查找类型:\n";
    std::cout << "   同一类型: " << (healthType == healthType2) << "\n\n";

    // ================================================================
    // 2. 运行时创建实例
    // ================================================================
    std::cout << "2. 运行时创建实例:\n";

    auto healthAny = healthType.construct(); // 返回 meta_any
    if (auto *hp = healthAny.try_cast<Health>())
    {
        hp->print();
    }
    std::cout << "\n";

    // ================================================================
    // 3. 运行时按名字读写成员变量
    // ================================================================
    std::cout << "3. 运行时读写成员:\n";

    healthType.set("current"_hs, healthAny, 50.0f);
    healthType.set("max"_hs, healthAny, 200.0f);

    auto cv = healthType.get("current"_hs, healthAny);
    auto mv = healthType.get("max"_hs, healthAny);
    std::cout << "   current = " << cv.cast<float>() << "\n";
    std::cout << "   max     = " << mv.cast<float>() << "\n\n";

    // ================================================================
    // 4. 运行时按名字调用成员函数
    // ================================================================
    std::cout << "4. 运行时调用函数:\n";

    healthType.invoke("heal"_hs, healthAny, 30.0f);
    std::cout << "   heal(30) → ";
    healthType.invoke("print"_hs, healthAny);

    healthType.invoke("damage"_hs, healthAny, 80.0f);
    std::cout << "   damage(80) → ";
    healthType.invoke("print"_hs, healthAny);

    std::cout << "\n";

    // ================================================================
    // 5. 遍历类型的所有反射成员（编辑器/序列化的基础）
    // ================================================================
    std::cout << "5. Health 的反射成员列表:\n";

    std::cout << "   [数据成员]\n";
    for (auto &&[id, data] : healthType.data())
    {
        std::cout << "      " << data.name() << " (id: " << id << ")\n";
    }

    std::cout << "   [函数成员]\n";
    for (auto &&[id, func] : healthType.func())
    {
        std::cout << "      " << func.name() << " (id: " << id << ")\n";
    }
    std::cout << "\n";

    // ================================================================
    // 6. 遍历所有已注册类型（全局反射表）
    // ================================================================
    std::cout << "6. 全部已注册类型:\n";
    for (auto &&[id, type] : entt::resolve())
    {
        std::cout << "   - " << type.name() << "\n";
    }
    std::cout << "\n";

    // ================================================================
    // 7. Player 类型演示（含字符串成员）
    // ================================================================
    std::cout << "7. Player 反射演示:\n";

    auto playerAny = entt::resolve<Player>().construct();
    entt::resolve<Player>().set("name"_hs, playerAny, std::string{"Hero"});
    entt::resolve<Player>().set("level"_hs, playerAny, 5);

    std::cout << "   名字: " << entt::resolve<Player>().get("name"_hs, playerAny).cast<std::string>() << "\n";
    std::cout << "   等级: " << entt::resolve<Player>().get("level"_hs, playerAny).cast<int>() << "\n";

    entt::resolve<Player>().invoke("levelUp"_hs, playerAny);
    std::cout << "   升级: " << entt::resolve<Player>().get("level"_hs, playerAny).cast<int>() << "\n";

    // ================================================================
    // 8. 核心演示：同一类型、两个不同实例，互不影响
    // ================================================================
    std::cout << "\n8. 两个独立实例演示:\n";

    // 创建两个不同的 Health 对象
    auto objA = healthType.construct();
    auto objB = healthType.construct();

    // 用反射分别给两个对象设不同的值
    healthType.set("current"_hs, objA, 10.0f);
    healthType.set("max"_hs, objA, 100.0f);

    healthType.set("current"_hs, objB, 999.0f);
    healthType.set("max"_hs, objB, 9999.0f);

    // 分别读取 —— 互不影响
    std::cout << "   对象A: current=" << healthType.get("current"_hs, objA).cast<float>()
              << ", max=" << healthType.get("max"_hs, objA).cast<float>() << "\n";
    std::cout << "   对象B: current=" << healthType.get("current"_hs, objB).cast<float>()
              << ", max=" << healthType.get("max"_hs, objB).cast<float>() << "\n";

    std::cout << "\n   → 反射表只存\"字段名→偏移量\"的映射\n";
    std::cout << "   → 对象(objA/objB)是你自己传入的, 系统只是\"拿着地图找到该对象里字段的位置\"\n";
    std::cout << "   → 传入不同的对象, 就操作不同对象的数据\n";
}