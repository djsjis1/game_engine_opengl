-- ============================================================
-- input_demo.lua —— 输入系统升级演示（边沿检测 + 特殊键 + 动作映射）
--
-- 展示三种输入查询的区别（这次升级补上的能力）：
--   持续检测 is_key_pressed  ：按住每帧都触发（移动用）
--   边沿检测 is_key_just_*   ：只在按下/松开那一帧触发（跳跃/开枪用）★新增
--   动作映射 is_action_*     ：按动作名而非物理键（键位可配）★新增
--
-- 挂到实体上运行，按键看控制台输出对比差异。
-- ============================================================

function ready()
    print("输入演示：")
    print("  W(持续) / space(边沿) / jump动作 / 方向键 / shift")
end

function update(dt)
    -- ① 持续检测：按住 W 每帧都打印（移动类操作用这个）
    if is_key_pressed("W") then
        print("W 持续按住中...")
    end

    -- ② 边沿检测：space 只在"刚按下那一帧"触发一次 ★以前做不到
    if is_key_just_pressed("space") then
        print(">>> 空格刚按下（跳跃！只触发一次）")
    end
    if is_key_just_released("space") then
        print("<<< 空格刚松开")
    end

    -- ③ 特殊键：方向键、shift ★以前 char 截断用不了
    if is_key_pressed("left") then print("← 左") end
    if is_key_pressed("right") then print("→ 右") end
    if is_key_pressed("shift") then print("⚡ 加速(shift)") end

    -- ④ 动作映射：按"动作名"而非物理键（jump 默认映射到 space，可改键位）★新增
    if is_action_just_pressed("jump") then
        print("### 动作[jump]触发（等价 space，但脚本不写死物理键）")
    end
end
