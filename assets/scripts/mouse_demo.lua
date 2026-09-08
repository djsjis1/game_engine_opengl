-- ============================================================
-- mouse_demo.lua —— 鼠标输入全功能演示（对标 Godot Input）
--
-- 展示本次升级的鼠标能力：
--   三键三态：is_mouse_pressed / just_pressed / just_released
--   坐标：get_mouse_position（视口像素）
--   位移：get_mouse_delta（FPS 视角用）
--   滚轮：get_mouse_wheel（缩放实体演示）
--   光标捕获：set_mouse_mode("captured")（按 C 切换，FPS 锁中心）
--
-- 挂到实体上 ▶ 运行：
--   左键点击 → 打印点击坐标（边沿，只触发一次）
--   滚轮 → 实体缩放变大变小
--   按 C → 切换光标捕获（隐藏+锁中心，移动鼠标打印视角增量）
-- ============================================================

local captured = false

function ready()
    print("鼠标演示：左键点击 / 滚轮缩放 / C 键切换光标捕获")
end

function update(dt)
    -- ① 左键边沿：点击一次只触发一次（射击/选取的正解）
    if is_mouse_just_pressed("left") then
        local x, y = get_mouse_position()
        print(">>> 左键点击 @ (" .. x .. ", " .. y .. ")")
    end
    if is_mouse_just_released("left") then
        print("<<< 左键松开（拖拽结束）")
    end

    -- ② 滚轮：缩放实体（上滚变大下滚变小）
    local wheel = get_mouse_wheel()
    if wheel ~= 0 then
        local sx, sy, sz = get_scale()
        local factor = 1 + wheel * 0.1
        set_scale(sx * factor, sy * factor, sz * factor)
        print("滚轮 " .. wheel .. " → 缩放 " .. string.format("%.2f", sx * factor))
    end

    -- ③ 按 C 切换光标捕获（FPS 模式：隐藏光标锁中心）
    if is_key_just_pressed("C") then
        captured = not captured
        set_mouse_mode(captured and "captured" or "visible")
        print("光标捕获: " .. tostring(captured))
    end

    -- ④ 捕获时打印视角增量（FPS 视角控制就用这两个值转相机）
    if captured then
        local dx, dy = get_mouse_delta()
        if dx ~= 0 or dy ~= 0 then
            print("视角增量 dx=" .. dx .. " dy=" .. dy)
        end
    end
end
