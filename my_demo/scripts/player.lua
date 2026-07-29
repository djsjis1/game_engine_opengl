-- scripts/player.lua — 玩家脚本（监听敌人信号 + WASD 移动）
--
-- 多线程信号流程：
--   1. Enemy 实体跑 enemy_ai.lua，emit("attack", 10)
--   2. 如果是主线程 emit → 立刻调用 Player 的回调
--   3. 如果是工作线程 emit → 入队 → 主线程下帧 processDeferredSignals() 消费

local speed = 5.0
local score = 0

function ready()
    print("[Player] 初始化完成，查找 Enemy...")
    local enemy = get_node("Enemy")
    if enemy then
        -- ★ 跨实体信号连接：Player 监听 Enemy 的 attack 信号
        connect_to(enemy, "attack", function(dmg)
            print("[Player] 受到攻击! 伤害: " .. dmg)
            score = score - dmg
        end)
        -- ★ 监听 Enemy 的 death 信号
        connect_to(enemy, "death", function(name)
            print("[Player] " .. name .. " 被击败! +100 分")
            score = score + 100
        end)
        print("[Player] 已连接 Enemy 的信号")
    else
        print("[Player] 未找到 Enemy 实体!")
    end
end

function update(dt)
    -- WASD 移动
    local x, y, z = get_position()
    if is_key_pressed("W") then z = z - speed * dt end
    if is_key_pressed("S") then z = z + speed * dt end
    if is_key_pressed("A") then x = x - speed * dt end
    if is_key_pressed("D") then x = x + speed * dt end
    set_position(x, y, z)

    -- 每 3 秒输出分数
    -- print("[Player] 分数: " .. score)
end
