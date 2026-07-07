# 常见问题 FAQ（避坑指南）

> 按"现象 → 原因 → 解决"组织，遇到问题先 Ctrl+F 关键词。

---

## 一、构建 / 配置类

### Q1：CMake 报错 `Could not find Qt5`

* **原因**：`CMAKE_PREFIX_PATH` 没指到 Qt 安装目录。
* **解决**：编辑顶层 [`CMakeLists.txt`](../CMakeLists.txt)：

```cmake
set(CMAKE_PREFIX_PATH "你的Qt路径/5.x.y/msvc2017_64" ${CMAKE_PREFIX_PATH})
```

或在命令行加：

```powershell
cmake -S . -B build -DCMAKE_PREFIX_PATH="D:/QT/5.14.2/msvc2017_64"
```

---

### Q2：编译时大量乱码 / `error C2018: 未知字符"0xa6"`

* **原因**：MSVC 默认用 GBK 解码源文件，遇到 UTF-8 中文注释报错。
* **解决**：项目已加 `/utf-8`，若你新建文件请确保 **保存为 UTF-8（无 BOM 也可）**。VSCode 右下角点击编码切换。

---

### Q3：构建时 `pdb 文件被占用` / `LNK1318: 意外 PDB 错误`

* **原因**：MSBuild 并发写 PDB。
* **解决**：项目已加 `/FS`。如还报错，关掉 IDE 后再 `cmake --build` 一次即可。

---

### Q4：链接错误 `unresolved external symbol __imp_gl*`

* **原因**：忘了链接 OpenGL 库。
* **解决**：`target_link_libraries(target ... OpenGL::GL)`。

---

## 二、运行黑屏 / 看不到东西

### Q5：窗口能开，但**全黑**或**只剩纯色背景**

按下面顺序排查：

1. **VAO 未绑定**：Core Profile 下没默认 VAO，必须 `glGenVertexArrays + glBindVertexArray`。
2. **顶点属性 location 不匹配**：Shader 里 `layout(location=0)` 与 `glVertexAttribPointer(0,...)` 对不上。
3. **未启用属性**：少调 `glEnableVertexAttribArray(0)`。
4. **Shader 没编过 / 没用**：编译/链接日志一定要打印（项目里 `Shader::checkCompileErrors` 会输出）。
5. **glUseProgram 漏调**：每帧绘制前必须激活程序。
6. **顶点在裁剪空间外**：检查矩阵；试试简单 `gl_Position = vec4(aPos, 1.0)` 看有没有图。
7. **背面剔除剔了正面**：临时 `glDisable(GL_CULL_FACE)`。
8. **深度测试错误**：未 `glClear(GL_DEPTH_BUFFER_BIT)` 导致全部被遮挡。

---

### Q6：3D 物体**穿插错乱 / 后面盖前面**

* **原因 1**：没开深度测试 → `glEnable(GL_DEPTH_TEST)`。
* **原因 2**：没要深度缓冲 → `format.setDepthBufferSize(24)` 在 main 中设置。
* **原因 3**：每帧没清深度 → `glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)`。

---

## 三、Shader 类

### Q7：`Shader compile error: ERROR: 0:1: '' : version '330' is not supported`

* **原因**：上下文不是 3.3 Core，可能是 setDefaultFormat 在 widget 显示之后才调。
* **解决**：在 `main` 创建 widget **之前** 调用 `QSurfaceFormat::setDefaultFormat(...)`。

---

### Q8：`ERROR: Failed to open shader files`

* **原因**：相对路径找不到 `shaders/xxx.vert`。
* **解决**：
  * VS 调试：项目已设 `VS_DEBUGGER_WORKING_DIRECTORY`；
  * 命令行运行：进入课程源码目录再启动，例如：

  ```powershell
  cd lesson/lesson05_texture
  ../../build/lesson/Release/lesson05_texture.exe
  ```

  * 或改用 Qt 资源系统 `:/shaders/xxx.vert`（需要 `.qrc`）。

---

### Q9：`uniform xxx` 设了没效果

排查：

1. **没 `glUseProgram`** 就调 `glUniform*`，会写到错误的程序上。
2. **GLSL 里 uniform 未使用** 会被驱动优化掉，`glGetUniformLocation` 返回 -1。
3. **名字拼错** / 大小写不一致。
4. **struct uniform**：必须逐字段写全名，如 `uMaterial.diffuse`。

---

## 四、纹理类

### Q10：纹理**上下颠倒**

* **原因**：QImage Y 轴朝下，OpenGL Y 轴朝上。
* **解决**：上传前 `image.mirrored(false, true)`。或在 fragment shader 里 `vec2(uv.x, 1.0-uv.y)`。

---

### Q11：纹理变成**纯白**或**纯黑**

* 纯白：sampler 默认从 0 号单元采样，但你忘了 `glBindTexture` 或忘了 `setInt("uTex", 0)`。
* 纯黑：图像数据格式不对（如 BGRA 当 RGBA 上传），或 mipmap 没生成（设了 MIPMAP 过滤却没 `glGenerateMipmap`）。

---

### Q12：远处纹理**闪烁 / 摩尔纹**

* **原因**：缩小过滤选了 `GL_NEAREST` / `GL_LINEAR`，没用 mipmap。
* **解决**：`GL_LINEAR_MIPMAP_LINEAR` + `glGenerateMipmap`。

---

## 五、矩阵 / 坐标系类

### Q13：物体**变形 / 拉长**

* **原因**：投影矩阵的 aspect 没随窗口更新。
* **解决**：在 `paintGL` 里用 `float(width()) / float(height())`，或在 `resizeGL` 重新算并保存。

---

### Q14：旋转 / 平移看起来"颠倒"

* 在 GLSL 里 `gl_Position = uProjection * uView * uModel * v;`，**乘法顺序**从右往左生效。
* `QMatrix4x4` 的 `translate / rotate / scale` 是 **后乘**（在已有变换之后再做）。

---

### Q15：摄像机看上下翻动后**画面颠倒**

* **原因**：`pitch` 超过 ±90°，`up` 与 `front` 重合。
* **解决**：限制 `pitch` 在 `[-89°, 89°]`。

---

## 六、Qt 集成类

### Q16：键盘事件没响应

* **原因**：`QOpenGLWidget` 默认 `Qt::NoFocus`。
* **解决**：构造里 `setFocusPolicy(Qt::StrongFocus);`。

---

### Q17：鼠标按下才有 mouseMove

* **解决**：`setMouseTracking(true);`。

---

### Q18：定时器 60FPS 看起来还卡

* **原因 1**：`QTimer` 16ms 是 **下限**，事件循环忙时会延迟。
* **原因 2**：渲染开销大，需要在着色器或 draw call 上优化。
* 进阶方案：`QOpenGLWindow` 配合 `swapBuffers` + `requestUpdate`，或用 V-Sync。

---

### Q19：程序退出时**崩溃 / 资源泄漏警告**

* **原因**：在没有上下文的情况下调 `glDelete*`。
* **解决**：

```cpp
~OpenGLWidget() {
    makeCurrent();
    glDelete...;
    doneCurrent();
}
```

---

## 七、性能类

### Q20：FPS 偏低 / GPU 占用高

按从易到难尝试：

1. 把 `glBufferData(..., GL_DYNAMIC_DRAW)` 改 `GL_STATIC_DRAW`（数据不变时）。
2. 减少 draw call：合批 / 实例化（`glDrawElementsInstanced`）。
3. 减少 uniform 设置次数：每帧不变的 uniform 只设一次。
4. 开启面剔除：`glEnable(GL_CULL_FACE)`。
5. 用 RenderDoc 抓帧定位瓶颈。

---

## 八、跨平台 / 显卡兼容

### Q21：Intel 集显报 `GL_VERSION 4.x` 但跑 3.3 仍崩

* 老版本 Intel 驱动对 Core Profile 支持不完美，升级显卡驱动通常可解决。

---

### Q22：N 卡笔记本程序跑在集显上

* 在 NVIDIA 控制面板里给可执行文件指定"高性能 NVIDIA 处理器"。
* 或代码里加：

```cpp
extern "C" {
    __declspec(dllexport) int NvOptimusEnablement = 1;
}
```

---

## 九、调试小工具

| 工具 | 用途 |
|---|---|
| `qDebug() << "OpenGL Version:" << (const char*)glGetString(GL_VERSION);` | 启动时打印版本 |
| `glGetError()` 包成宏每步检查 | 定位错误调用 |
| RenderDoc                    | 抓帧、逐 draw call 看顶点 / 纹理 / uniform |
| Nsight Graphics              | NVIDIA 官方深度调试 |
| 着色器临时输出                | `FragColor = vec4(Normal*0.5+0.5, 1.0);` 可视化法线 |

---

## 十、推荐排错顺序

```
出问题 →
  ① 看 stderr 有无 Shader / Program 错误
  ② glGetError() 在关键步骤后检查
  ③ 临时把 fragment shader 改成 FragColor = vec4(1,0,0,1) 看是否能画出红色
  ④ 关闭深度测试 / 面剔除 看是不是被剔
  ⑤ RenderDoc 抓一帧
```

---

> 还遇到本表没覆盖的问题？记下来，把它扩展进 FAQ —— 这就是你专属的 OpenGL 知识库。
