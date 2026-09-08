-- scripts/enemy_ai.lua — 敌人 AI（工作在另外线程的模拟）
--
-- 架构：C++ 工作线程跑 AI 逻辑，emit 信号给主线程消费
-- 主线程的 processDeferredSignals() 在每帧开始时调用回调

local hp = 100
local attack_timer = 0

function ready()
    print("[Enemy] AI 初始化完成")
end

function update(dt)
    attack_timer = attack_timer + dt
    hp = hp - dt * 3 -- 每秒掉 3 滴血

    -- 每 2 秒攻击一次
    if attack_timer > 2.0 then
        attack_timer = 0
        emit("attack", 10) -- 发出攻击信号，伤害值 10
    end

    -- 血量归零 → 死亡信号
    if hp <= 0 then
        emit("death", "Enemy")
        hp = 100 -- 复活（演示用）
    end
end
