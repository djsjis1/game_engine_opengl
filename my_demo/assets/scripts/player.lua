-- ============================================================
-- player.lua —— 多场景跳跳（两关 + 传送门 + 跨场景存档）
--
-- 操作：
--   W/A/S/D 移动    空格 跳跃（落地才能跳）
--   走近旋转的传送门 → 切换关卡（金门→第2关，绿门→回第1关）
--   K 存档（记住当前关卡+位置）  L 读档（跨关卡也能精确回去）
--
-- 【多场景要点】
--   change_scene 是"帧末生效"（脚本执行中删所有实体会崩，Godot 同款延迟）
--   get_scene() 判断自己在哪关 → 同一个脚本适用所有关卡
--
-- 【跨场景读档流程】（两段式，关键思想）
--   L 按下 → save_load 读盘 → 存档记录的关卡 ≠ 当前关？
--     → 草稿里放个"待恢复"标记 → change_scene 切过去
--     → 新场景的 player ready() 看到标记 → 恢复位置、清标记
--   （SaveGame 草稿在内存里，不随场景销毁——正好当跨场景信使）
-- ============================================================

local MOVE_SPEED = 5
local JUMP_IMPULSE = 6

function ready()
    print("当前关卡: " .. get_scene())
    -- 跨场景读档第二段：如果是读档触发的场景切换，恢复存档位置
    if save_get("__restore", false) then
        set_position(save_get("px", 0), save_get("py", 0.55), save_get("pz", 0))
        set_velocity(0, 0, 0)
        save_set("__restore", false) -- 标记只用一次
        print("读档完成，回到存档点")
    end
end

function update(dt)
    -- ① 移动：改水平速度、保留竖直速度（重力/跳跃不被抹掉）
    local vx, vy, vz = get_velocity()
    local mx, mz = 0, 0
    if is_key_pressed("W") then mz = -MOVE_SPEED end
    if is_key_pressed("S") then mz = MOVE_SPEED end
    if is_key_pressed("A") then mx = -MOVE_SPEED end
    if is_key_pressed("D") then mx = MOVE_SPEED end
    set_velocity(mx, vy, mz)

    -- ② 跳跃：边沿检测 + 落地射线
    --   射线从自身中心向下（raycast 自动忽略发射者，打不中自己）
    if is_key_just_pressed("space") then
        local x, y, z = get_position()
        if raycast(x, y, z, 0, -1, 0, 0.65) then
            apply_impulse(0, JUMP_IMPULSE, 0)
        end
    end

    -- ③ 传送门：靠近 Portal（距离 < 1.5）→ 切换关卡
    local portal = get_node("Portal")
    if portal and portal ~= 0 then
        local px, py, pz = get_node_position(portal)
        local x, y, z = get_position()
        local dx, dz = x - px, z - pz
        if dx * dx + dz * dz < 2.25 then -- 1.5^2（平方比较省开方）
            -- 同一脚本两关通用：按当前关决定去向
            if string.find(get_scene(), "level1") then
                change_scene("scenes/level2.json") -- 金门 → 第 2 关
            else
                change_scene("scenes/level1.json") -- 绿门 → 回第 1 关
            end
            return                                 -- 本帧别再动了（场景帧末就换）
        end
    end

    -- ④ 存档（K）：记录关卡 + 位置 → 落盘
    if is_key_just_pressed("K") then
        local x, y, z = get_position()
        save_set("level", get_scene())
        save_set("px", x)
        save_set("py", y)
        save_set("pz", z)
        if save_write(1) then
            print("已存档: " .. get_scene() .. " @ ("
                .. string.format("%.1f", x) .. ", " .. string.format("%.1f", z) .. ")")
        end
    end

    -- ⑤ 读档（L）：跨场景两段式（见文件头注释）
    if is_key_just_pressed("L") then
        if save_load(1) then
            local lv = save_get("level", "")
            if lv ~= "" and lv ~= get_scene() then
                -- 存档在别的关：标记待恢复 → 切场景 → 那边 ready() 恢复位置
                save_set("__restore", true)
                change_scene(lv)
            else
                -- 就在本关：直接瞬移
                set_position(save_get("px", 0), save_get("py", 0.55), save_get("pz", 0))
                set_velocity(0, 0, 0)
                print("读档完成（本关内）")
            end
        else
            print("没有存档（先按 K 存一个）")
        end
    end

    -- ⑥ 掉出场地 → 回出生点（贴地 y=0.55，不从空中掉）
    local _, py2, _ = get_position()
    if py2 < -10 then
        set_position(0, 0.55, 0)
        set_velocity(0, 0, 0)
    end
end
