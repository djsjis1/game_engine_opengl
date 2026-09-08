-- scripts/demo.lua —— 脚本测试（Godot @export 风格）
--
-- export() 定义的变量会在编辑器里可见、可编辑、可保存！

function ready()
    -- ★ 定义脚本变量（编辑器会自动显示这些）
    export("speed", 5.0)
    export("jump_power", 10.0)

    -- 也可以在 update 里动态读写
    print("speed = " .. get_prop("speed"))
end

function update(dt)
    -- ★ 从编辑器变量读取速度（or 5.0 = 默认值兑底，防 nil 报错）
    local s = get_prop("speed") or 5.0

    local x, y, z = get_position()
    if is_key_pressed("W") then z = z - s * dt end
    if is_key_pressed("S") then z = z + s * dt end
    if is_key_pressed("A") then x = x - s * dt end
    if is_key_pressed("D") then x = x + s * dt end
    set_position(x, y, z)
end
