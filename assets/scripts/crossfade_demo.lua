-- ============================================================
-- crossfade_demo.lua —— 动画混合（crossfade）演示
--
-- 对标 Godot AnimationTree 的过渡：按键切换动画不再"啪"地硬切，
-- 而是在一段时间内从旧动画平滑混合到新动画。
--
-- 用法：给实体挂 ScriptComponent 指向本脚本 + AnimationComponent 指向
--   demo.anim（含 spin / bounce 两个动画）。运行后：
--     按 1 → 平滑过渡到 spin（旋转）
--     按 2 → 平滑过渡到 bounce（弹跳）
--   对比 anim_play（硬切）能明显看出混合的顺滑。
-- ============================================================

function ready()
    print("crossfade 演示：按 1=spin  2=bounce（观察平滑过渡）")
    anim_play("spin") -- 初始动画（硬切开场没问题）
end

function update(dt)
    -- 按 1：0.4 秒平滑混合到 spin
    if is_key_pressed("1") then
        anim_crossfade("spin", 0.4)
    end
    -- 按 2：0.4 秒平滑混合到 bounce
    if is_key_pressed("2") then
        anim_crossfade("bounce", 0.4)
    end
end
