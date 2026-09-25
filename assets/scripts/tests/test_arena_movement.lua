-- test_arena_movement.lua - Reproduce arena movement/camera issues
-- Key insight: movement script must be on the PLAYER entity (not TestRunner)
-- and W must be HELD (not press+release) for is_key_pressed to return true

local frame = 0
local phase = 0

function ready()
    -- Create ground (static)
    local ground = create_entity("Ground")
    set_node_position(ground, 0, -0.5, 0)
    set_node_scale(ground, 20, 1, 20)
    add_mesh(ground, "cube")
    add_rigidbody(ground, 0)
    add_collider(ground)

    -- Create player (dynamic) + attach movement script
    local player = create_entity("Player")
    set_node_position(player, 0, 1.0, 6)
    add_mesh(player, "cube")
    add_rigidbody(player, 2)
    add_collider(player)
    -- Attach the actual arena_player.lua to Player entity
    add_script(player, "D:/code/.std/opengl/assets/scripts/arena_player.lua")

    phase = 1
end

function update(dt)
    frame = frame + 1

    if phase == 1 then
        -- Wait 30 frames for player to land
        if frame >= 30 then
            test_section("Arena: Player Landed")
            local _, py, _ = test_get_position("Player")
            test_assert(py < 1.0, "Player fell (Y=" .. string.format("%.2f", py) .. ")")
            test_assert(py > 0.0, "Player above ground (Y=" .. string.format("%.2f", py) .. ")")

            -- HOLD W key (arena_player reads is_key_pressed("W"))
            test_press_key("W")
            phase = 2
            frame = 0
        end
    elseif phase == 2 then
        -- Let player move for 30 frames with W held
        if frame >= 30 then
            test_release_key("W")
            phase = 3
        end
    elseif phase == 3 then
        -- Check results
        test_section("Arena: W Movement")
        local px, py, pz = test_get_position("Player")
        -- Player started at Z=6, W should move in -Z (forward)
        -- After 30 frames at 5 m/s: ~2.5m moved → Z should be ~3.5
        test_assert(pz < 5.5, "Player moved forward (Z=" .. string.format("%.2f", pz) .. " < 5.5)")

        local vx, vy, vz = test_get_velocity("Player")
        test_section("Arena: Velocity Check")
        test_assert(vz < -1.0 or pz < 5.5,
            "Either velocity is -Z or position moved (vz=" .. string.format("%.2f", vz) .. ")")

        -- Test is_key_pressed works when key is held
        test_section("Arena: Input While Held")
        test_press_key("D")
        local dPressed = is_key_pressed("D")
        test_release_key("D")
        test_assert(dPressed, "is_key_pressed('D') returns true while held")

        test_summary()
        phase = 4
    end
end
