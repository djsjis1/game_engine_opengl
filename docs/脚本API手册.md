# Lua 脚本 API 手册

> 用本引擎做游戏 = 写 Lua 脚本调用这些 API。
> 本手册是"给游戏开发者看的"；API 的 C++ 实现见 `Scripting/LuaApi.cpp` 的 `kApiTable`（单一事实来源）。
> 写脚本时用 VSCode 打开 `scripts/` 目录，`engine_api.lua`（自动生成）提供补全/参数提示/悬停文档。

---

## 一、脚本的生命周期（两个魔法函数）

引擎只认脚本里这两个全局函数（都可选，不写就是没有）：

```lua
function ready()
    -- 实体第一次运行时调用 1 次（初始化：连信号、读存档、建世界）
end

function update(dt)
    -- 之后每帧调用，dt = 本帧秒数（约 0.016）
    -- 做匀速运动务必乘 dt（帧率无关）
end
```

**要点**：

- 只在**运行模式**（编辑器 ▶ 或 my_game.exe）跑，编辑模式脚本静止。
- 每个挂脚本的实体有**独立沙盒**——脚本间不共享 `local` 变量，通信走信号或存档。
- 脚本里的 `is_key_pressed` / `get_position` 等**隐式作用于"脚本所在实体"**。

---

## 二、API 全表（按用途分类）

### 输入 —— 键盘

| API | 返回 | 说明 |
|-----|------|------|
| `is_key_pressed(key)` | bool | 按住（持续）。key 支持 `"W"` / `"space"` / `"left"` / `"shift"` / `"f1"` 等 |
| `is_key_just_pressed(key)` | bool | **本帧刚按下**（边沿）——跳跃/开枪/开门用 |
| `is_key_just_released(key)` | bool | 本帧刚松开（边沿） |
| `is_action_pressed(action)` | bool | 动作是否按住（如 `"jump"`，键位可在 C++ 配） |
| `is_action_just_pressed(action)` | bool | 动作本帧刚触发 |

> **持续 vs 边沿**：移动用 `is_key_pressed`（每帧都要响应）；一次性动作用 `is_key_just_pressed`（按住只触发一次，否则每帧狂触发）。

### 输入 —— 鼠标

| API | 返回 | 说明 |
|-----|------|------|
| `is_mouse_pressed(btn)` | bool | 按住。btn = `"left"`/`"middle"`/`"right"` |
| `is_mouse_just_pressed(btn)` | bool | 本帧刚按下（点击/射击） |
| `is_mouse_just_released(btn)` | bool | 本帧刚松开（拖拽结束） |
| `get_mouse_position()` | x, y | 视口像素坐标 |
| `get_mouse_delta()` | dx, dy | 本帧相对位移（FPS 视角转动用） |
| `get_mouse_wheel()` | number | 本帧滚轮格数（上正下负，缩放/切武器） |
| `set_mouse_mode(mode)` | — | `"captured"`=隐藏光标锁中心(FPS) / `"visible"`=正常 |

### 变换（当前实体）

| API | 返回 | 说明 |
|-----|------|------|
| `get_position()` | x, y, z | 世界坐标 |
| `set_position(x, y, z)` | — | 设位置（带刚体的实体也已同步物理，见坑 21） |
| `get_rotation()` / `set_rotation(x,y,z)` | 欧拉角度 | 旋转 |
| `get_scale()` / `set_scale(x,y,z)` | — | 缩放 |
| `set_visible(v)` / `is_visible()` | — / bool | 显隐 |

### 变换（任意实体，靠 id）

| API | 说明 |
|-----|------|
| `get_node(name)` → id | 按名字查实体 ID（找不到返回 0） |
| `get_node_position(id)` → x,y,z | 读指定实体位置 |
| `set_node_position(id, x,y,z)` | 设指定实体位置（同步物理） |
| `set_node_scale(id, x,y,z)` | 缩放指定实体 |

### 物理（Jolt）

| API | 说明 |
|-----|------|
| `apply_impulse(x, y, z)` | 施加冲量（瞬间推力，如跳跃）。**注意质量**：Δv = 冲量/质量 |
| `set_velocity(x, y, z)` | 直接设速度（绕过质量，最可控） |
| `get_velocity()` → vx,vy,vz | 读速度。移动标准写法：改水平、保留 y（`set_velocity(mx, vy, mz)`） |
| `raycast(ox,oy,oz, dx,dy,dz [,maxDist])` | 射线，命中返回 `{entity,x,y,z,dist}` 否则 nil。**自动忽略发射者**（坑 19） |

### 建造（纯代码造世界）

| API | 说明 |
|-----|------|
| `create_entity(name)` → id | 创建光杆实体（Tag+Transform），下面这组装配它 |
| `add_mesh(id, "cube")` | 加可见网格（不加=不可见逻辑实体） |
| `add_material(id, r,g,b [,metallic,roughness])` | PBR 材质：基础色 + 金属度(0=非金属 1=金属) + 粗糙度(0=镜面 1=粗糙) |
| `add_rigidbody(id, motion)` | 刚体：0=静态 1=运动学 2=动态 |
| `add_collider(id [,hx,hy,hz])` | 盒碰撞体半尺寸（默认 0.5） |
| `add_sphere_collider(id [,radius])` | 球形碰撞体（半径默认 0.5） |
| `add_capsule_collider(id [,radius,height])` | 胶囊碰撞体（总高默认 2.0，包含两端半球） |
| `add_script(id, "scripts/x.lua")` | 挂行为脚本（相对路径，导出后有效） |
| `add_camera(id, px,py,pz [,yaw,pitch])` | 游戏相机 |
| `add_rotate(id, sx,sy,sz)` | 自转（度/秒，传送门那种） |
| `add_light(id, r,g,b [,intensity,type])` | 光源：type 0=方向光 1=点光 2=聚光灯（位置取实体 Transform） |
| `add_particles(id [, preset])` | 粒子发射器：`"fire"`(火,默认)/`"smoke"`(烟)/`"sparkle"`(星光)，细参 Inspector 可调 |
| `particles_emit(id, on)` | 开/关粒子发射（已有粒子自然飘完，Godot 同行为） |
| `tween(id, target, x,y,z, dur [,easing [,signal]])` | 补间：target=`"position/rotation/scale"`，easing=`"linear/ease_in/ease_out/ease_in_out/bounce/elastic"`，完成可发信号 |
| `destroy_entity(id)` | 删除实体（资源自动回收） |

### 寻路（NavGrid，对标 NavigationAgent）

| API | 说明 |
|-----|------|
| `nav_bake()` | 烘焙导航网格（扫静态碰撞体为障碍），进关卡 ready() 调一次 |
| `nav_find_path(fx,fz,tx,tz)` | A* 寻路 → `{{x=..,z=..},...}` 路点序列（视线平滑），不可达 nil |
| `nav_is_blocked(x,z)` | 该位置是否障碍 |

### 动画状态机（anim_fsm_*，对标 AnimationTree StateMachine）

```lua
-- ready() 声明状态图（状态名 = 动画 clip 名）：
anim_fsm_add(id, "idle", "walk", "speed", ">", 0.5, 0.2)
anim_fsm_add(id, "walk", "idle", "speed", "<", 0.1, 0.2)
anim_fsm_start(id, "idle")
-- update() 只喂参数，切换（带 crossfade）自动发生：
anim_fsm_param(id, "speed", 当前速度)
-- 查询：anim_fsm_state(id) → "walk"
```

### 触发器（Area3D 同款）

Box、Sphere、Capsule 碰撞体都可勾选 `isTrigger`（或加组件后在 Inspector 勾选）→ 不阻挡只发信号。同一实体只保留一种碰撞体；通过 Lua 添加新形状时会替换旧形状：

```lua
connect("trigger_enter", function(other) end)  -- 物体进入区域
connect("collision_end", function(other) end)  -- 分离/离开区域
```

### 音频总线

`set_bus_volume("music"/"sfx"/"master", v)` — 音乐/音效独立音量（设置界面滑块直接对接）。

### 存档（玩家进度）

| API | 说明 |
|-----|------|
| `save_set(key, value)` | 写存档草稿（内存），value 支持 number/string/bool |
| `save_get(key, default)` | 读草稿，无此键返回 default |
| `save_write([slot])` | 草稿落盘到 `saves/save{slot}.json`（默认 1）——玩家按"保存"时调 |
| `save_load([slot])` | 磁盘 → 草稿（读档/继续游戏） |
| `save_exists([slot])` | 存档是否存在（主菜单"继续"按钮判断） |

> **草稿两层**：`save_set` 只写内存，`save_write` 才落盘。游戏中随时 set 记状态，玩家明确保存才 write（单机存档标准语义）。

### 多场景

| API | 说明 |
|-----|------|
| `change_scene("scenes/level2.json")` | 切换场景（**帧末生效**，脚本执行中删实体会崩） |
| `get_scene()` → string | 当前场景相对路径（判断自己在哪关） |

### 音频（miniaudio 后端，对标 Godot AudioServer）

| API | 说明 |
|-----|------|
| `play_sound("sounds/x.wav" [,vol])` | 播一次性音效（爆炸/UI 点击/开枪，不跟随实体） |
| `play_music("music/bgm.mp3" [,vol])` | 播/切背景音乐（循环，单例——新曲自动停旧曲） |
| `stop_music()` | 停止背景音乐 |
| `set_master_volume(v)` | 主音量 0~1（影响全部声音） |

> **路径规则**：传相对资源根路径（如 `"sounds/jump.wav"`），运行时自动拼绝对路径 → 导出后有效。支持 wav/mp3/flac/ogg。
> **实体音源**：想让声音跟随物体（脚步声/引擎声）用 `AudioComponent`（编辑器加组件，带 3D 空间衰减），一次性音效才用 `play_sound`。

### 环境（对标 Godot WorldEnvironment）

| API | 说明 |
|-----|------|
| `set_fog(r, g, b, density)` | 动态雾：density 0=关闭，0.02~0.1 明显（走进山洞/雾区渐起） |
| `set_exposure(v)` | 动态曝光：1=正常，<1 变暗，>1 变亮（做昼夜明暗） |

> **静态环境**：天空色/曝光/暗角/雾也可在编辑器给实体加 `EnvironmentComponent` 调，随场景保存。脚本 API 用于**运行时动态改**（如触发雾区）；首次调用会自动建一个环境实体。

### 动画（AnimationPlayer 风格）

| API | 说明 |
|-----|------|
| `anim_play(clip)` | 从头播放（硬切） |
| `anim_crossfade(clip, duration)` | 带过渡切换（默认 0.3s，走→跑无缝混合） |
| `anim_stop()` / `anim_is_playing()` | 停止 / 查询 |
| `anim_set_speed(s)` / `anim_set_loop(b)` | 速度倍率（负=倒放） / 循环开关 |
| `anim_set_file` / `anim_create` / `anim_add_key` / `anim_add_event` / `anim_save` | 脚本程序化创建动画（见 AnimationLibrary.h） |

### 信号 & 其它

| API | 说明 |
|-----|------|
| `connect(signal, cb)` / `emit(signal, ...)` | 监听/发射本实体信号 |
| `connect_to(id, signal, cb)` / `disconnect(signal, ref)` | 跨实体监听 / 断开 |
| `async(fn, cb)` | 异步执行计算（WorkerThreadPool） |
| `export(name, default)` / `get_prop` / `set_prop` | 可导出变量（编辑器可调，@export 风格） |
| `delta()` → number | 帧间隔秒数（等价 update 的 dt 参数） |
| `print(...)` | 打印（注意：WIN32 GUI 无控制台，看不见——调试用状态栏/颜色，见坑清单方法论） |

---

## 三、三种游戏开发方式（对标 Godot）

| 方式 | 怎么做 | 适合 |
|------|--------|------|
| 纯编辑器 | 场景树摆实体 + Inspector 调参 | 关卡设计、所见即所得 |
| **混合（推荐）** | 编辑器摆场景 + 脚本写行为 | 大多数游戏 |
| 纯代码 | 空场景 + bootstrap 脚本用建造 API 造一切 | 程序化关卡、快速原型 |

演示脚本（都在 `scripts/`）：

- `player.lua` —— 移动/跳跃/传送门/跨场景存读档（混合方式的完整范例）
- `code_world.lua` —— 纯代码造世界（地面/玩家/8箱子/相机全代码生成）
- `crossfade_demo.lua` —— 动画混合
- `input_demo.lua` / `mouse_demo.lua` —— 键鼠全功能演示

---

## 四、最小可玩示例（复制即用）

```lua
-- 挂到一个带 Rigidbody(动态) + BoxCollider 的方块上
local SPEED = 5
local JUMP = 6

function update(dt)
    -- 移动：改水平速度，保留竖直速度（不然重力/跳跃被抹掉）
    local vx, vy, vz = get_velocity()
    local mx, mz = 0, 0
    if is_key_pressed("W") then mz = -SPEED end
    if is_key_pressed("S") then mz = SPEED end
    if is_key_pressed("A") then mx = -SPEED end
    if is_key_pressed("D") then mx = SPEED end
    set_velocity(mx, vy, mz)

    -- 跳跃：边沿检测 + 落地射线（从中心发，自动忽略自己）
    if is_key_just_pressed("space") then
        local x, y, z = get_position()
        if raycast(x, y, z, 0, -1, 0, 0.65) then
            apply_impulse(0, JUMP, 0)
        end
    end
end
```

---

## 五、新手最容易踩的 5 个坑（详见 排障手册.md）

1. **场景 JSON 不能手写**——只能编辑器保存或代码生成（坑 15）
2. **移动要保留 y 速度**——`set_velocity(mx, vy, mz)` 的 vy 来自 get_velocity（否则跳不起来）
3. **一次性动作用 `just_pressed`**——用 `is_key_pressed` 会每帧狂触发
4. **change_scene 是帧末生效**——调用后本帧别再操作实体，直接 return
5. **apply_impulse 看质量**——组件 mass 要配好，冲量效果 = 冲量/质量

---

## 关联文档

- 引擎架构与模块：`../模块文档.md`
- 排障手册（22 个实战坑 + 方法论）：`排障手册.md`
- API 实现（单一事实来源）：`../Scripting/LuaApi.cpp` 的 `kApiTable`
