-- ============================================================
-- arena_player.lua —— 第一人称玩家控制器（学 Godot FPS Tutorial）
--
-- Godot 的核心设计：
--   1. 鼠标控制视角（yaw 水平转 + pitch 俯仰，clamp 防翻转）
--   2. WASD 相对摄像机方向移动（W=你看着的方向，不是世界固定轴）
--   3. 摄像机在眼睛位置（角色中心 + 眼睛高度）
--   4. 跳跃用 is_on_floor() 判定
--
-- 与 Godot 的区别：Godot 用子节点层级自动跟随，
-- 我们用 set_camera() 每帧手动设置（效果等价，实现不同）
-- ============================================================

local hp = 100
local maxHp = 100
local myId = 0
local enemyId = 0
local hpBar = 0
local atkCooldown = 0
local dead = false

-- 视角状态（Godot 用节点旋转存，我们用变量存）
local yaw = -90.0        -- 水平朝向（度），-90=朝 -Z 方向
local pitch = 0.0        -- 俯仰（度），0=水平，正=抬头，负=低头
local SENSITIVITY = 0.15 -- 鼠标灵敏度

local function updateHpBar()
    -- 第一人称下自己的 HP 条已在 ready() 缩到 0 隐藏，无需更新
end

local function takeDamage(dmg)
    if dead then return end
    hp = hp - dmg
    updateHpBar()
    if myId ~= 0 then
        tween(myId, "scale", 0.7, 0.7, 0.7, 0.08, "ease_out")
        tween(myId, "scale", 1.0, 1.0, 1.0, 0.25, "elastic")
    end
end

function ready()
    myId = get_node("Player")
    enemyId = get_node("Enemy")
    hpBar = get_node("Player_hp")
    -- ★ 第一人称：相机就在自己身体里，隐藏自己的网格（对标 player.lua，
    --   Godot FPS 同款做法），头顶的 HP 条也缩到 0（set_visible 只作用于
    --   自身实体，HP 条是子节点，用缩放隐藏，不然它在视野里晃）
    set_visible(false)
    local hpBg = get_node("Player_hpbg")
    if hpBar ~= 0 then set_node_scale(hpBar, 0, 0, 0) end
    if hpBg ~= 0 then set_node_scale(hpBg, 0, 0, 0) end
    if myId ~= 0 and enemyId ~= 0 then
        connect_to(enemyId, "attack_hit", function(target, dmg)
            if target == myId then takeDamage(dmg) end
        end)
    end
    -- 锁定鼠标（Godot: Input.set_mouse_mode(Input.MOUSE_MODE_CAPTURED)）
    set_mouse_mode("captured")
    updateHpBar()
end

function update(dt)
    if hp <= 0 and not dead then
        dead = true
        emit("died")
        set_visible(false)
        set_mouse_mode("visible") -- 死亡释放鼠标
    end
    if dead then return end
    if atkCooldown > 0 then atkCooldown = atkCooldown - dt end

    -- ① 鼠标视角控制（Godot: _input 里处理 InputEventMouseMotion）
    local dx, dy = get_mouse_delta()
    yaw = yaw + dx * SENSITIVITY
    pitch = pitch - dy * SENSITIVITY
    -- clamp 防翻转（Godot: clamp(camera_rot.x, -70, 70)）
    if pitch > 80 then pitch = 80 end
    if pitch < -80 then pitch = -80 end

    -- ② 移动：WASD 相对摄像机方向（Godot: cam_xform.basis.z/x）
    -- 计算摄像机前方和右方（忽略 pitch，只在水平面移动）
    local yr = math.rad(yaw)
    local forward_x = math.cos(yr)             -- 摄像机前方的 X 分量
    local forward_z = math.sin(yr)             -- 摄像机前方的 Z 分量
    local right_x = math.cos(yr + math.pi / 2) -- 摄像机右方
    local right_z = math.sin(yr + math.pi / 2)

    local mx, mz = 0, 0
    if is_key_pressed("W") then
        mx = mx + forward_x; mz = mz + forward_z
    end
    if is_key_pressed("S") then
        mx = mx - forward_x; mz = mz - forward_z
    end
    if is_key_pressed("A") then
        mx = mx - right_x; mz = mz - right_z
    end
    if is_key_pressed("D") then
        mx = mx + right_x; mz = mz + right_z
    end

    local len = math.sqrt(mx * mx + mz * mz)
    local speed = is_key_pressed("SHIFT") and 8.5 or 5.0
    local _, vy, _ = get_velocity()
    if len > 0 then
        mx, mz = mx / len * speed, mz / len * speed
        set_velocity(mx, vy, mz)
    else
        set_velocity(0, vy, 0)
    end

    -- ③ 跳跃（Godot: if is_on_floor() and Input.is_action_just_pressed("jump")）
    if is_key_just_pressed("SPACE") and is_on_floor() then
        apply_impulse(0, 7.5, 0)
    end

    -- ④ 摄像机跟随（第一人称：眼睛位置 + 视角方向）
    -- get_position() 返回的是立方体中心（1×1×1，头顶 = +0.5），
    -- 眼睛放在略低于头顶处（+0.45），否则相机会悬在头顶上方产生偏移感
    local px, py, pz = get_position()
    set_camera(px, py + 0.45, pz, yaw, pitch)
    -- 玩家实体始终面向摄像机方向（影子/其他玩家看到你的朝向正确）
    set_rotation(0, yaw, 0)

    -- ⑤ 攻击：J 键
    if atkCooldown <= 0 and is_key_just_pressed("J") then
        atkCooldown = 0.6
        if enemyId ~= 0 then
            local ex, ey, ez = get_node_position(enemyId)
            if ex then
                local ddx, ddz = ex - px, ez - pz
                local dist = math.sqrt(ddx * ddx + ddz * ddz)
                if dist > 0.01 and myId ~= 0 then
                    tween(myId, "position", px + ddx / dist * 0.9, py, pz + ddz / dist * 0.9, 0.1, "ease_out")
                end
                if dist < 2.2 then
                    emit("attack_hit", enemyId, 18)
                end
            end
        end
    end
end
