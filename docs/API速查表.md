# OpenGL × Qt API 速查表

> 按主题分类，附最简代码片段。所有函数前缀：`gl*` 来自 OpenGL，`Q*` 来自 Qt。

---

## 1. 上下文与函数加载

| API | 作用 |
|---|---|
| `QSurfaceFormat::setVersion(3,3)`            | 设置 OpenGL 主/次版本 |
| `QSurfaceFormat::setProfile(CoreProfile)`    | 核心模式 |
| `QSurfaceFormat::setDepthBufferSize(24)`     | 24 位深度缓冲 |
| `QSurfaceFormat::setDefaultFormat(fmt)`      | 应用为默认（**必须在 QApplication 之前或之后立刻调用，并先于 Widget 创建**） |
| `initializeOpenGLFunctions()`                | 加载函数指针，**`initializeGL` 第一行** |
| `glGetString(GL_VERSION)`                    | 拿到驱动汇报的 GL 版本 |
| `glGetString(GL_SHADING_LANGUAGE_VERSION)`   | GLSL 版本 |

---

## 2. 视口与清屏

| API | 作用 |
|---|---|
| `glViewport(x, y, w, h)`                     | 设置渲染区域，通常在 `resizeGL` 调用 |
| `glClearColor(r, g, b, a)`                   | 清屏颜色，0~1 |
| `glClear(mask)`                              | 清缓冲，常用 `GL_COLOR_BUFFER_BIT \| GL_DEPTH_BUFFER_BIT` |

---

## 3. 缓冲对象 VBO / VAO / EBO

| API | 作用 |
|---|---|
| `glGenBuffers(n, &id)`                       | 生成 buffer |
| `glBindBuffer(target, id)`                   | 绑定，target = `GL_ARRAY_BUFFER` / `GL_ELEMENT_ARRAY_BUFFER` |
| `glBufferData(target, size, data, usage)`    | 上传数据，usage = `GL_STATIC_DRAW` / `GL_DYNAMIC_DRAW` |
| `glBufferSubData(target, offset, size, data)`| 局部更新 |
| `glGenVertexArrays(n, &id)`                  | 生成 VAO |
| `glBindVertexArray(id)`                      | 绑定 VAO |
| `glVertexAttribPointer(loc, size, type, normalized, stride, offset)` | 顶点属性布局 |
| `glEnableVertexAttribArray(loc)`             | 启用属性槽 |
| `glDeleteBuffers / glDeleteVertexArrays`     | 删除 |

---

## 4. 着色器与程序

| API | 作用 |
|---|---|
| `glCreateShader(type)`                       | type = `GL_VERTEX_SHADER` / `GL_FRAGMENT_SHADER` |
| `glShaderSource(s, 1, &src, NULL)`           | 设置源码 |
| `glCompileShader(s)`                         | 编译 |
| `glGetShaderiv(s, GL_COMPILE_STATUS, &ok)`   | 检查编译 |
| `glGetShaderInfoLog(s, 1024, NULL, log)`     | 编译错误日志 |
| `glCreateProgram()`                          | 创建程序 |
| `glAttachShader(p, s)`                       | 关联 shader |
| `glLinkProgram(p)`                           | 链接 |
| `glGetProgramiv(p, GL_LINK_STATUS, &ok)`     | 检查链接 |
| `glUseProgram(p)`                            | 激活程序 |
| `glDeleteShader / glDeleteProgram`           | 删除 |

---

## 5. Uniform 变量

| API | 类型 |
|---|---|
| `glGetUniformLocation(p, "name")`            | 取位置（-1 = 不存在） |
| `glUniform1i(loc, v)`                        | int / sampler |
| `glUniform1f(loc, v)`                        | float |
| `glUniform3f(loc, x, y, z)`                  | vec3 |
| `glUniform4f(loc, x, y, z, w)`               | vec4 |
| `glUniform3fv(loc, 1, ptr)`                  | vec3 数组形式 |
| `glUniformMatrix4fv(loc, 1, GL_FALSE, ptr)`  | mat4 |

> 设值前 **必须** 先 `glUseProgram(p)`。

---

## 6. 绘制

| API | 作用 |
|---|---|
| `glDrawArrays(mode, first, count)`           | 直接画顶点 |
| `glDrawElements(mode, count, type, offset)`  | 用 EBO 索引画 |
| `glPolygonMode(GL_FRONT_AND_BACK, GL_LINE)`  | 线框模式 |
| `glPolygonMode(GL_FRONT_AND_BACK, GL_FILL)`  | 恢复填充模式 |

`mode` 常用：`GL_POINTS / GL_LINES / GL_LINE_STRIP / GL_TRIANGLES / GL_TRIANGLE_STRIP / GL_TRIANGLE_FAN`

---

## 7. 纹理

| API | 作用 |
|---|---|
| `glGenTextures(1, &t)`                       | 生成 |
| `glBindTexture(GL_TEXTURE_2D, t)`            | 绑定 |
| `glActiveTexture(GL_TEXTURE0 + i)`           | 激活第 i 个纹理单元 |
| `glTexImage2D(target, lvl, ifmt, w, h, 0, fmt, type, data)` | 上传 |
| `glTexParameteri(target, pname, val)`        | 参数 |
| `glGenerateMipmap(target)`                   | 生成 mipmap |
| `glDeleteTextures(1, &t)`                    | 删除 |

**常用 pname / val：**

| pname | val |
|---|---|
| `GL_TEXTURE_WRAP_S/T`   | `GL_REPEAT` / `GL_MIRRORED_REPEAT` / `GL_CLAMP_TO_EDGE` / `GL_CLAMP_TO_BORDER` |
| `GL_TEXTURE_MIN_FILTER` | `GL_NEAREST` / `GL_LINEAR` / `GL_LINEAR_MIPMAP_LINEAR`（推荐） |
| `GL_TEXTURE_MAG_FILTER` | `GL_NEAREST` / `GL_LINEAR` |

---

## 8. 状态开关

| API | 作用 |
|---|---|
| `glEnable / glDisable(GL_DEPTH_TEST)`        | 深度测试 |
| `glEnable / glDisable(GL_BLEND)`             | 混合 |
| `glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA)` | 经典透明 |
| `glEnable / glDisable(GL_CULL_FACE)`         | 面剔除 |
| `glCullFace(GL_BACK / GL_FRONT)`             | 剔除哪面 |
| `glDepthFunc(GL_LESS)`                       | 深度比较函数 |
| `glDepthMask(GL_FALSE)`                      | 关闭深度写入（常用于透明物） |

---

## 9. Qt 矩阵库 `QMatrix4x4`

| 方法 | 作用 |
|---|---|
| `setToIdentity()`                                | 单位矩阵 |
| `translate(x, y, z)`                             | 平移 |
| `rotate(angleDeg, x, y, z)`                      | 绕轴旋转（**角度，不是弧度**） |
| `scale(s)` / `scale(sx, sy, sz)`                 | 缩放 |
| `lookAt(eye, center, up)`                        | 视图矩阵 |
| `perspective(fovDeg, aspect, near, far)`         | 透视投影 |
| `ortho(l, r, b, t, n, f)`                        | 正交投影 |
| `constData()`                                    | 取列优先 16 个 float（喂给 `glUniformMatrix4fv`） |
| `inverted()` / `transposed()`                    | 求逆、转置 |

---

## 10. Qt 输入事件（重写虚函数）

| 函数 | 触发时机 |
|---|---|
| `keyPressEvent(QKeyEvent*)`                  | 按键按下 |
| `keyReleaseEvent(QKeyEvent*)`                | 按键释放 |
| `mousePressEvent(QMouseEvent*)`              | 鼠标按下 |
| `mouseMoveEvent(QMouseEvent*)`               | 鼠标移动（默认仅按下时；调 `setMouseTracking(true)` 后无按下也触发） |
| `wheelEvent(QWheelEvent*)`                   | 滚轮 |

> **若 widget 收不到键盘事件**：调用 `setFocusPolicy(Qt::StrongFocus)`。

---

## 11. Qt 图像与字符串

| API | 作用 |
|---|---|
| `QImage(path)`                               | 从文件加载 |
| `img.convertToFormat(QImage::Format_RGBA8888)` | 转格式（OpenGL 友好） |
| `img.mirrored(false, true)`                  | 垂直翻转（OpenGL Y 轴对齐） |
| `img.bits()`                                 | 取像素裸指针 |
| `QString::toStdString()`                     | 转 `std::string` |

---

## 12. 顶点属性 layout 速查

| 顶点格式 | stride | 偏移 |
|---|---|---|
| pos(3)              | 3 * 4 = 12 | 0 |
| pos(3) + color(3)   | 6 * 4 = 24 | 0 / 12 |
| pos(3) + uv(2)      | 5 * 4 = 20 | 0 / 12 |
| pos(3) + normal(3)  | 6 * 4 = 24 | 0 / 12 |
| pos(3) + normal(3) + uv(2) | 8 * 4 = 32 | 0 / 12 / 24 |

```cpp
int stride = 8 * sizeof(float);
glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)(6 * sizeof(float)));
```

---

## 13. GLSL 常用内建

| 函数 | 说明 |
|---|---|
| `vec*(...)` / `mat*(...)`                | 构造 |
| `length(v)` / `normalize(v)` / `dot(a,b)` / `cross(a,b)` | 向量基础运算 |
| `mix(a, b, t)`                           | 线性插值 |
| `clamp(x, lo, hi)`                       | 截断 |
| `pow(x, y)` / `sqrt(x)` / `abs(x)`       | 数学 |
| `texture(sampler2D, uv)`                 | 采样 |
| `reflect(I, N)`                          | 反射方向（注意 I 是 **入射方向**） |
| `refract(I, N, eta)`                     | 折射方向 |
| `gl_Position`                            | VS 输出位置（裁剪空间） |
| `gl_FragCoord`                           | FS 中拿屏幕像素坐标 |

---

## 14. 调试技巧

| 方法 | 怎么用 |
|---|---|
| 编译/链接日志        | 见第 4 节 `glGetShaderInfoLog` |
| `glGetError()`       | 每个关键步骤后调用，返回 `GL_NO_ERROR(0)` 表示成功 |
| `qDebug() << ...`    | Qt 风格输出，可流式拼接 |
| RenderDoc            | 抓帧看每个 draw call |
| `glPolygonMode(...,GL_LINE)` | 线框模式调试几何 |
