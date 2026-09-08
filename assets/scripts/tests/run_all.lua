-- run_all.lua - Engine headless test (frame-based state machine)
-- No recursive test_run_frames calls. Uses frame counter instead.

local frame = 0
local testBox = 0
local testPlayer = 0
local tweenTarget = 0
local signalReceived = false
local tweenDone = false
local phase = 0 -- 0=setup, 1=wait_physics, 2=check_physics, 3=api_checks, 4=signal, 5=tween, 6=done

function ready()
    -- Phase 0: Setup entities
    testBox = create_entity("TestBox")
    set_node_position(testBox, 0, 5, 0)
    add_mesh(testBox, "cube")
    add_rigidbody(testBox, 2)
    add_collider(testBox)

    testPlayer = create_entity("TestPlayer")
    set_node_position(testPlayer, 0, 0.5, 0)
    add_mesh(testPlayer, "cube")
    add_rigidbody(testPlayer, 2)
    add_collider(testPlayer)

    tweenTarget = create_entity("TweenTarget")
    set_node_position(tweenTarget, 0, 0, 0)

    -- Signal test
    connect("test_signal", function()
        signalReceived = true
    end)

    -- Tween test (done signal fires on the TWEENED entity, not self)
    connect_to(tweenTarget, "tween_done", function()
        tweenDone = true
    end)
    tween(tweenTarget, "position", 5, 0, 0, 0.5, "linear", "tween_done")

    phase = 1
end

function update(dt)
    frame = frame + 1

    if phase == 1 then
        -- Wait 60 frames for physics (gravity)
        if frame >= 60 then
            phase = 2
        end
    elseif phase == 2 then
        -- Phase 2: Check physics results
        test_section("ECS + Physics")

        local x, y, z = test_get_position("TestBox")
        test_assert(y < 5.0, "Gravity: Y < 5 after 1s (got " .. string.format("%.2f", y) .. ")")
        test_assert(y > -1.0, "Gravity: Y > -1 (not fallen through floor)")

        local vx, vy, vz = test_get_velocity("TestBox")
        test_assert_near(vx, 0.0, 0.1, "No horizontal drift")

        -- Script system verification
        test_section("Script System")
        test_assert(true, "Lua script loaded and running")
        test_assert(type(create_entity) == "function", "create_entity available")
        test_assert(type(set_node_position) == "function", "set_node_position available")
        test_assert(type(add_rigidbody) == "function", "add_rigidbody available")
        test_assert(type(is_key_pressed) == "function", "is_key_pressed available")
        test_assert(type(test_assert) == "function", "test_assert injected")

        -- Signal test
        test_section("Signal System")
        emit("test_signal")
        test_assert(signalReceived, "emit -> connect callback fired")

        -- Input test (just verify no crash)
        test_section("Input System")
        test_simulate_key("W", 5)
        test_assert(true, "Simulate key W: no crash")

        -- NavGrid test
        test_section("NavGrid")
        nav_bake()
        local path = nav_find_path(0, 0, 5, 5)
        test_assert(path ~= nil, "Path found (no obstacles)")
        if path then
            test_assert(#path >= 1, "Path has waypoints")
        end

        phase = 3
    elseif phase == 3 then
        -- Wait for tween to complete (0.5s = 30 frames)
        if frame >= 90 then
            phase = 4
        end
    elseif phase == 4 then
        -- Phase 4: Check tween
        test_section("Tween")
        test_assert(tweenDone, "Tween completed (done signal fired)")
        local tx, _, _ = test_get_position("TweenTarget")
        test_assert_near(tx, 5.0, 0.1, "Tween moved X to 5")

        -- Summary
        test_summary()
        phase = 5
    end
    -- phase 5: do nothing (test complete)
end
