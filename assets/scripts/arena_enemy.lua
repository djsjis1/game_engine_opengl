-- ============================================================
-- arena_enemy.lua —— AI 恶魔（暗红方块）V4 稳定版
-- 三态 AI：追击（寻路）/ 攻击 / 撤退
-- 去掉了动画状态机避免初始化时序问题
-- ============================================================

local hp = 100
local maxHp = 100
local myId = 0
local playerId = 0
local hpBar = 0
local dead = false

local atkCooldown = 0
local retreatTime = 0
local repathTimer = 0
local path = nil
local pathIdx = 1

local function updateHpBar()
    if hpBar ~= 0 then
        local ratio = math.max(hp / maxHp, 0.001)
        set_node_scale(hpBar, ratio, 0.14, 0.14)
    end
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
    myId = get_node("Enemy")
    playerId = get_node("Player")
    hpBar = get_node("Enemy_hp")
    if playerId ~= 0 then
        connect_to(playerId, "attack_hit", function(target, dmg)
            if target == myId then takeDamage(dmg) end
        end)
    end
    updateHpBar()
end

function update(dt)
    if hp <= 0 and not dead then
        dead = true
        emit("died")
        set_visible(false)
    end
    if dead then return end
    if atkCooldown > 0 then atkCooldown = atkCooldown - dt end
    if retreatTime > 0 then retreatTime = retreatTime - dt end
    repathTimer = repathTimer - dt

    if playerId == 0 then return end
    local px, py, pz = get_position()
    local tx, ty, tz = get_node_position(playerId)
    if not tx then return end
    local dx, dz = tx - px, tz - pz
    local dist = math.sqrt(dx * dx + dz * dz)
    local _, vy, _ = get_velocity()

    if retreatTime > 0 then
        -- 撤退：背身小退
        local rx, rz = -dx / math.max(dist, 0.01), -dz / math.max(dist, 0.01)
        set_velocity(rx * 3.0, vy, rz * 3.0)
    elseif dist > 2.0 then
        -- 追击：寻路绕障碍
        if repathTimer <= 0 or path == nil then
            repathTimer = 0.4
            path = nav_find_path(px, pz, tx, tz)
            pathIdx = 1
        end
        local gx, gz = tx, tz
        if path and path[pathIdx] then
            gx, gz = path[pathIdx].x, path[pathIdx].z
            local wx, wz = gx - px, gz - pz
            if math.sqrt(wx * wx + wz * wz) < 0.6 then
                pathIdx = pathIdx + 1
                if path[pathIdx] then gx, gz = path[pathIdx].x, path[pathIdx].z end
            end
        end
        local mx, mz = gx - px, gz - pz
        local mlen = math.sqrt(mx * mx + mz * mz)
        if mlen > 0.05 then
            set_velocity(mx / mlen * 4.2, vy, mz / mlen * 4.2)
            set_rotation(0, math.deg(math.atan(mx, mz)), 0)
        end
    else
        -- 攻击圈：站定出手
        set_velocity(0, vy, 0)
        set_rotation(0, math.deg(math.atan(dx, dz)), 0)
        if atkCooldown <= 0 then
            atkCooldown = 1.1
            if myId ~= 0 then
                local lx = px + dx / math.max(dist, 0.01) * 0.9
                local lz = pz + dz / math.max(dist, 0.01) * 0.9
                tween(myId, "position", lx, py, lz, 0.12, "ease_out")
            end
            emit("attack_hit", playerId, 12)
            retreatTime = 0.5
        end
    end
end
