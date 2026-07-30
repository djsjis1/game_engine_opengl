---@meta
-- ============================================================
-- 引擎注入的 Lua API 存根（自动生成，勿手改）
-- 由 ScriptApiStub 从 ScriptApiRegistry 生成，供 VSCode
-- lua-language-server 提供补全/参数提示/文档/报错。
-- ============================================================

--- 打印到控制台（Lua 内置）
function print(...) end

--- 本帧时间增量（秒）
---@return number
function delta() end

--- 按键是否按住（持续），支持 "W"/"space"/"left"/"shift" 等
---@param key any
---@return boolean
function is_key_pressed(key) end

--- 按键本帧刚按下（边沿，跳跃/开枪用）
---@param key any
---@return boolean
function is_key_just_pressed(key) end

--- 按键本帧刚松开（边沿）
---@param key any
---@return boolean
function is_key_just_released(key) end

--- 动作是否按住（如 "jump"，键位可配）
---@param action any
---@return boolean
function is_action_pressed(action) end

--- 动作本帧刚触发（边沿）
---@param action any
---@return boolean
function is_action_just_pressed(action) end

--- 鼠标键是否按住（"left"/"middle"/"right"）
---@param btn any
---@return boolean
function is_mouse_pressed(btn) end

--- 鼠标键本帧刚按下（边沿，点击/射击用）
---@param btn any
---@return boolean
function is_mouse_just_pressed(btn) end

--- 鼠标键本帧刚松开（边沿，拖拽结束用）
---@param btn any
---@return boolean
function is_mouse_just_released(btn) end

--- 鼠标视口像素坐标（配合拾取）
---@return number x, number y
function get_mouse_position() end

--- 本帧鼠标相对位移（FPS 视角用）
---@return number dx, number dy
function get_mouse_delta() end

--- 本帧滚轮格数（上正下负，缩放/切武器用）
---@return number
function get_mouse_wheel() end

--- 光标模式："captured"锁中心(FPS) / "visible"正常
---@param mode any
function set_mouse_mode(mode) end

--- 按名字查实体 ID
---@param name any
---@return number
function get_node(name) end

--- 创建实体，返回其 ID
---@param name any
---@return number
function create_entity(name) end

--- 加可见网格（"cube"），不加不可见
---@param id any
---@param type any
function add_mesh(id, type) end

--- 设置漫反射颜色（0~1）
---@param id any
---@param r any
---@param g any
---@param b any
function add_material(id, r, g, b) end

--- 加刚体（0=静态 1=运动学 2=动态）
---@param id any
---@param motion any
function add_rigidbody(id, motion) end

--- 加盒碰撞体（半尺寸，默认0.5）
---@param id any
---@param hx any
---@param hy any
---@param hz any
function add_collider(id, hx, hy, hz) end

--- 挂行为脚本（相对路径如 "scripts/x.lua"）
---@param id any
---@param path any
function add_script(id, path) end

--- 加游戏相机（active）
---@param id any
---@param px any
---@param py any
---@param pz any
---@param yaw any
---@param pitch any
function add_camera(id, px, py, pz, yaw, pitch) end

--- 自转装饰（度/秒）
---@param id any
---@param sx any
---@param sy any
---@param sz any
function add_rotate(id, sx, sy, sz) end

--- 缩放任意实体
---@param id any
---@param x any
---@param y any
---@param z any
function set_node_scale(id, x, y, z) end

--- 删除实体（资源自动回收）
---@param id any
function destroy_entity(id) end

--- 获取实体世界坐标
---@return number x, number y, number z
function get_position() end

--- 设置实体世界坐标
---@param x any
---@param y any
---@param z any
function set_position(x, y, z) end

--- 获取实体旋转（欧拉角度）
---@return number x, number y, number z
function get_rotation() end

--- 设置实体旋转（欧拉角度）
---@param x any
---@param y any
---@param z any
function set_rotation(x, y, z) end

--- 获取缩放
---@return number x, number y, number z
function get_scale() end

--- 设置缩放
---@param x any
---@param y any
---@param z any
function set_scale(x, y, z) end

--- 显示/隐藏实体
---@param visible any
function set_visible(visible) end

--- 是否可见
---@return boolean
function is_visible() end

--- 获取指定实体位置
---@param id any
---@return number x, number y, number z
function get_node_position(id) end

--- 设置指定实体位置
---@param id any
---@param x any
---@param y any
---@param z any
function set_node_position(id, x, y, z) end

--- 施加冲量（瞬间推力，如跳跃）
---@param x any
---@param y any
---@param z any
function apply_impulse(x, y, z) end

--- 直接设置刚体线速度
---@param x any
---@param y any
---@param z any
function set_velocity(x, y, z) end

--- 读取刚体速度（移动时保留 y：set_velocity(mx, vy, mz)）
---@return number vx, number vy, number vz
function get_velocity() end

--- 写存档草稿（number/string/bool）
---@param key any
---@param value any
function save_set(key, value) end

--- 读存档草稿（无此键返回 default）
---@param key any
---@param default any
---@return any
function save_get(key, default) end

--- 草稿落盘到槽位（默认1），玩家按保存时调
---@param slot any
---@return boolean
function save_write(slot) end

--- 读档到草稿（默认槽1），继续游戏时调
---@param slot any
---@return boolean
function save_load(slot) end

--- 存档是否存在（主菜单'继续'判断）
---@param slot any
---@return boolean
function save_exists(slot) end

--- 切换场景（帧末生效），如 change_scene("scenes/level2.json")
---@param path any
function change_scene(path) end

--- 当前场景相对路径（判断自己在哪关）
---@return string
function get_scene() end

--- 物理射线，命中返回 {entity,x,y,z,dist}，未命中 nil
---@param ox any
---@param oy any
---@param oz any
---@param dx any
---@param dy any
---@param dz any
---@param maxDist any
---@return table|nil
function raycast(ox, oy, oz, dx, dy, dz, maxDist) end

--- 设置父子关系（child_of）
---@param child any
---@param parent any
function set_parent(child, parent) end

--- 获取父实体 ID（无父返回 0）
---@param id any
---@return number
function get_parent(id) end

--- 监听本实体信号
---@param signal any
---@param callback any
function connect(signal, callback) end

--- 发射信号（可带参数）
---@param signal any
---@param ... any
function emit(signal, ...) end

--- 监听指定实体的信号
---@param id any
---@param signal any
---@param callback any
function connect_to(id, signal, callback) end

--- 断开信号监听
---@param signal any
---@param ref any
function disconnect(signal, ref) end

--- 异步执行计算，完成后回调
---@param fn any
---@param callback any
function async(fn, callback) end

--- 声明可导出变量（编辑器可调）
---@param name any
---@param default any
function export(name, default) end

--- 读取脚本变量
---@param name any
---@return any
function get_prop(name) end

--- 设置脚本变量
---@param name any
---@param value any
function set_prop(name, value) end

--- 从头播放指定动画
---@param clip any
function anim_play(clip) end

--- 带过渡切换到新动画（默认0.3s，走→跑无缝混合）
---@param clip any
---@param duration any
function anim_crossfade(clip, duration) end

--- 停止播放（保持当前姿态）
function anim_stop() end

--- 是否正在播放
---@return boolean
function anim_is_playing() end

--- 播放速度倍率（1=正常 2=两倍 负=倒放）
---@param speed any
function anim_set_speed(speed) end

--- 循环开关（true/false）
---@param loop any
function anim_set_loop(loop) end

--- 绑定动画文件（无组件时自动挂）
---@param path any
function anim_set_file(path) end

--- 新建动画（同名覆盖）
---@param clip any
---@param duration any
function anim_create(clip, duration) end

--- 打关键帧。track 如 "position" 或 "组件名/字段名"，easing 可选
---@param clip any
---@param track any
---@param t any
---@param x any
---@param y any
---@param z any
---@param easing any
function anim_add_key(clip, track, t, x, y, z, easing) end

--- 事件轨道：播到 t 时发信号
---@param clip any
---@param t any
---@param signal any
function anim_add_event(clip, t, signal) end

--- 把动画库写回 .anim 文件
---@return boolean
function anim_save() end

