-- scripts/demo.lua —— 脚本测试
-- 运行模式：WASD 控制移动，自动旋转

local speed = 5.0

function ready()
    print("Script loaded! Use WASD to move in Play mode.")
end

function update(dt)

    -- WASD 移动
    local x, y, z = get_position()
    if is_key_pressed("W") then z = z - speed * dt end
    if is_key_pressed("S") then z = z + speed * dt end
    if is_key_pressed("A") then x = x - speed * dt end
    if is_key_pressed("D") then x = x + speed * dt end
    set_position(x, y, z)
end

function on_signal(name)
    print("Signal: " .. name)
end
