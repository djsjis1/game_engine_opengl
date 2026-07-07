# Game Engine Editor — API 参考

## 1. 组件 (Components)

### TransformComponent

```cpp
struct TransformComponent {
    QVector3D position{0,0,0};  // 世界坐标
    QVector3D rotation{0,0,0};  // 欧拉角（度），绕 X/Y/Z
    QVector3D scale{1,1,1};     // 缩放比例
    QMatrix4x4 getModelMatrix() const;      // TRS 顺序 Model 矩阵
    QMatrix3x3 getNormalMatrix() const;     // 法线矩阵（逆转置）
};
```

### MeshComponent

```cpp
struct MeshComponent {
    GLuint vao=0, vbo=0, ebo=0;    // OpenGL 缓冲对象
    GLsizei vertexCount=0, indexCount=0;
    GLenum primitiveType=GL_TRIANGLES;
    bool hasIndices=false;
    char resourcePath[256]="";      // .obj 文件路径（序列化用）
    bool isValid() const;            // vao != 0
};
```

### MaterialComponent

```cpp
struct MaterialComponent {
    QVector3D ambient{0.2,0.2,0.2};     // 环境光颜色
    QVector3D diffuse{0.8,0.8,0.8};     // 漫反射颜色
    QVector3D specular{1,1,1};          // 镜面光颜色
    float shininess=32.0f;              // 反光度 (1~256)
    GLuint diffuseTexture=0;            // 漫反射贴图 ID (0=无)
    GLuint specularTexture=0;           // 镜面光贴图 ID
    char shaderName[64]="default";      // Shader 名称
};
```

### LightComponent

```cpp
enum class LightType : int { Directional=0, Point=1, Spot=2 };

struct LightComponent {
    LightType type=LightType::Point;
    QVector3D ambient{0.2,0.2,0.2};     // 环境光
    QVector3D diffuse{0.5,0.5,0.5};     // 漫反射
    QVector3D specular{1,1,1};          // 镜面光
    float constant=1.0f;                // 衰减常数
    float linear=0.09f;                 // 一次衰减
    float quadratic=0.032f;             // 二次衰减
    float cutOff=0.91f;                 // 聚光灯内锥 cos
    float outerCutOff=0.82f;            // 聚光灯外锥 cos
};
```

| 衰减距离 | constant | linear | quadratic |
|----------|----------|--------|-----------|
| 7        | 1.0      | 0.7    | 1.8       |
| 13       | 1.0      | 0.35   | 0.44      |
| 20       | 1.0      | 0.22   | 0.20      |
| 50       | 1.0      | 0.09   | 0.032     |

### CameraComponent

```cpp
struct CameraComponent {
    QVector3D position{0,0,5};     // 位置
    QVector3D front{0,0,-1};       // 前方向
    QVector3D up{0,1,0};           // 上方向
    float fov=45.0f;               // 垂直视野（度）
    float nearPlane=0.1f;          // 近裁剪面
    float farPlane=100.0f;         // 远裁剪面
    bool active=true;              // 是否为活跃摄像机
};
```

### TagComponent

```cpp
struct TagComponent {
    char name[64]="Entity";        // 实体名称
};
```

## 2. 核心类

### Engine (engine.h)

```cpp
class Engine {
    bool init(const QString &shaderPath);  // 加载默认着色器
    EngineWorld& world();                   // ECS 世界
    Renderer& renderer();                   // 渲染器
    MeshFactory& meshFactory();             // 网格工厂

    ecs_entity_t createEntity(const QString &name);
    void setComponent(ecs_entity_t, const TransformComponent&);
    void setComponent(ecs_entity_t, const MeshComponent&);
    void setComponent(ecs_entity_t, const MaterialComponent&);
    void setComponent(ecs_entity_t, const LightComponent&);
    void setComponent(ecs_entity_t, const CameraComponent&);
    void cleanupGL();                       // 释放所有 VAO/VBO
};
```

### EngineWorld (world.h)

```cpp
class EngineWorld {
    ecs_world_t* raw();                             // Flecs C world 指针
    ecs_entity_t createEntity(const QString &name); // 创建实体+Tag
    void destroyEntity(ecs_entity_t);
    std::vector<ecs_entity_t> getAllEntities();     // 所有带 Tag 的实体
    QString exportToJson();                         // 场景→JSON
    bool importFromJson(const QString&);            // JSON→场景
};
```

### Renderer (renderer.h)

```cpp
class Renderer : protected QOpenGLFunctions_3_3_Core {
    bool initShaders(const QString &basePath);   // 加载着色器
    void beginFrame(ecs_world_t*);               // 收集光源+摄像机
    void renderEntities(ecs_world_t*);           // 渲染所有实体
    void setAspectRatio(float);
    MeshFactory& meshFactory();
};
```

内部组件 ID 通过 `ecs_id(Type)` 宏获取（需 `ECS_COMPONENT_DEFINE` 注册）。

### Shader (shader.h)

```cpp
class Shader : protected QOpenGLFunctions_3_3_Core {
    bool load(const QString &vert, const QString &frag);  // 编译链接
    void use();                                            // glUseProgram
    bool isValid();
    void setInt(name, int);
    void setFloat(name, float);
    void setVec3(name, x, y, z);
    void setMat3(name, const float*);   // 3x3 矩阵
    void setMat4(name, const float*);   // 4x4 矩阵（列优先）
};
```

⚠️ 构造函数不初始化 GL——必须 GL 上下文就绪后调用 `load()`。

### MeshFactory (mesh_factory.h)

```cpp
class MeshFactory : protected QOpenGLFunctions_3_3_Core {
    MeshComponent createCube(float size=1.0f);
    MeshComponent createQuad();                         // NDC [-1,1]
    MeshComponent createPlane(float size=10.0f);        // XZ 平面
    MeshComponent createSphere(float r, int sectors=36, int stacks=18);
    MeshComponent loadOBJ(const QString &path);          // .obj 加载
    void destroyMesh(MeshComponent&);                    // 释放 VAO/VBO
};
```

顶点格式：pos(3) + normal(3) + texcoord(2) = 8 floats，stride=32B。

## 3. 编辑器面板

### Viewport (viewport.h)

```cpp
class Viewport : public QOpenGLWidget, protected QOpenGLFunctions_3_3_Core {
    Q_OBJECT
    void setCameraEntity(ecs_entity_t);      // 设置被控制的摄像机
    float fps() const;
signals:
    void glReady();                           // GL 上下文就绪
public slots:
    void onEntitySelected(ecs_entity_t);      // 高亮选中实体
};
```

- 右键拖拽：旋转视角（yaw/pitch）
- WASD：移动摄像机
- 滚轮：缩放 FOV
- Q/E：上/下移动

### HierarchyPanel (hierarchy_panel.h)

```cpp
class HierarchyPanel : public QWidget {
    Q_OBJECT
    void refresh();                           // 重新加载实体列表
    void deleteSelected();                    // 删除选中实体
signals:
    void entitySelected(ecs_entity_t);
};
```

### InspectorPanel (inspector_panel.h)

```cpp
class InspectorPanel : public QWidget {
    Q_OBJECT
public slots:
    void onEntitySelected(ecs_entity_t);      // 选中实体 → 显示属性
signals:
    void componentChanged();                  // 属性修改 → 刷新视口
};
```

动态显示选中实体的所有组件属性（Transform/Light/Camera/Material）。

### AssetBrowser (asset_browser.h)

```cpp
class AssetBrowser : public QWidget {
    Q_OBJECT
    // "Import Model (.obj)" → loadOBJ → 创建实体
    // "Import Texture" → 暂未实现
};
```

### ExportDialog (export_dialog.h)

```cpp
class ExportDialog : public QDialog {
    // 输入：项目名称 + 导出路径
    // 输出：main.cpp + scene.json + CMakeLists.txt
};
```

## 4. Flecs C API 使用模式

```cpp
// 注册组件（文件作用域声明 + 构造函数初始化）
inline ecs_entity_t ecs_id(TransformComponent);  // world.h 文件作用域
ECS_COMPONENT_DEFINE(m_world, TransformComponent);  // 构造函数内

// 创建实体
ecs_entity_t e = ecs_new(world);
ecs_set_ptr(world, e, TransformComponent, &value);

// 查询
ecs_query_desc_t qd = {};
qd.terms[0].id = ecs_id(MeshComponent);
ecs_query_t *q = ecs_query_init(world, &qd);
ecs_iter_t it = ecs_query_iter(world, q);
while (ecs_query_next(&it)) {
    auto *data = ecs_field(&it, MeshComponent, 1);
    for (int i = 0; i < it.count; ++i) {
        ecs_entity_t entity = it.entities[i];
        // ...
    }
}
ecs_query_fini(q);

// 获取组件
auto *t = (TransformComponent*)ecs_get(world, entity, TransformComponent);
auto *m = (TransformComponent*)ecs_get_mut(world, entity, TransformComponent);
```

⚠️ **注意**：Flecs 使用 `memcpy` 复制组件。组件中**禁止**使用含堆分配指针的类型（如 `QString`、`std::string`、`std::vector`）。使用固定大小 `char[N]` 数组替代。

## 5. 添加新组件步骤

1. 在 `components.h` 定义 struct（仅用平凡可复制类型）
2. 在 `world.h` 添加 `inline ecs_entity_t ecs_id(NewType);`
3. 在 `EngineWorld` 构造函数添加 `ECS_COMPONENT_DEFINE(m_world, NewType);`
4. 在 `engine.h` 添加 `setComponent` 重载
5. 在 `inspector_panel.h` 添加属性编辑器
6. 在 `renderer.h` 添加渲染逻辑（如需要）
