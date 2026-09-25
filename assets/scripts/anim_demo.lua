-- ============================================================
-- anim_demo.lua —— Lua 脚本设计动画 + 控制播放（Godot 风格）
--
-- 用法：给实体挂 ScriptComponent，脚本路径填 scripts/anim_demo.lua
--       Tab/▶ 进入运行模式 → ready() 里程序化创建动画并播放
--
-- 按键（运行模式）：
--   1 = 播放 lua_spin（脚本设计的旋转）
--   2 = 播放 lua_jump（脚本设计的弹跳，带缓动）
--   3 = 停止
-- ============================================================

function ready()
    -- ① 绑定动画文件（绝对路径，不依赖工作目录；文件不存在也行，内存库立即可用）
    anim_set_file("D:/code/.std/opengl/anims/lua_made.anim")

    -- ② 设计动画 A：水平旋转 2 秒一圈（对标 Godot track_insert_key）
    anim_create("lua_spin", 2.0)
    anim_add_key("lua_spin", "rotation", 0.0, 0, 0, 0)
    anim_add_key("lua_spin", "rotation", 2.0, 0, 360, 0)

    -- ③ 设计动画 B：弹跳 + 落地挤压（多轨道 + 缓动）
    --    缓动参数：0=线性 2=慢出 10=弹跳落地 11=弹簧回弹
    anim_create("lua_jump", 1.0)
    anim_add_key("lua_jump", "position", 0.0, 0, 0, 2, 2)  -- 起跳：慢出
    anim_add_key("lua_jump", "position", 0.5, 0, 2, 2, 10) -- 下落：弹跳缓动
    anim_add_key("lua_jump", "position", 1.0, 0, 0, 2)
    anim_add_key("lua_jump", "scale", 0.0, 1, 1, 1)
    anim_add_key("lua_jump", "scale", 0.5, 0.8, 1.3, 0.8) -- 空中拉伸
    anim_add_key("lua_jump", "scale", 1.0, 1, 1, 1)
    -- ★ Property Track：颜色轨道（跳跃时变亮）—— 任意 REFLECT 字段都能动画
    anim_add_key("lua_jump", "MaterialComponent/diffuse", 0.0, 0.3, 0.3, 1.0)
    anim_add_key("lua_jump", "MaterialComponent/diffuse", 0.5, 1.0, 1.0, 0.3)
    anim_add_key("lua_jump", "MaterialComponent/diffuse", 1.0, 0.3, 0.3, 1.0)
    -- ★ Method Track：事件轨道（播到指定时刻发信号）
    anim_add_event("lua_jump", 0.5, "jump_apex")  -- 最高点
    anim_add_event("lua_jump", 0.98, "jump_land") -- 落地

    -- 事件接收：动画只报告"什么时候"，逻辑在这里（音效/伤害/特效...）
    connect("jump_apex", function() print("📢 到达最高点！") end)
    connect("jump_land", function() print("📢 落地！（这里可播音效/震屏/伤害判定）") end)

    -- ④ 可选：存成 .anim 文件（之后编辑器里也能打开继续调）
    anim_save()

    -- ⑤ 立即播放脚本刚设计的动画
    anim_set_loop(true)
    anim_play("lua_spin")
    print("lua 动画已创建：lua_spin / lua_jump（按 1/2 切换，3 停止）")
end

function update(dt)
    -- 运行时按键切换动画（控制播放）
    if is_key_pressed("1") then
        anim_play("lua_spin")
    end
    if is_key_pressed("2") then
        anim_play("lua_jump")
    end
    if is_key_pressed("3") then
        anim_stop()
    end
end
