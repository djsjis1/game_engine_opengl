---
trigger: always_on
---
# 测试规范

## 核心原则

AI 生成代码时必须同步输出测试方案，禁止只交付代码不交付测试。

新增业务逻辑必须有对应的测试覆盖。

---

# AI 测试输出要求

AI 在 Step 3（设计方案）阶段必须同时给出测试策略：

```markdown
## 测试方案

### 单元测试
- [测试类/函数名] — [覆盖场景]
- ...

### 集成测试
- [模块间交互] — [覆盖场景]
- ...

### 是否需要压测
- 是/否 — [原因]

### 是否需要异常测试
- 是/否 — [原因]
```

编码完成后必须输出测试结果总结。

---

# 测试框架

统一使用 GoogleTest + GoogleMock：

* 测试文件命名：`<被测文件名>_test.cpp`
* 测试目录：`tests/unit/<模块名>/`、`tests/integration/<模块名>/`
* 测试 fixtures 数据：`tests/fixtures/`

---

# 覆盖率要求

| 模块类型 | 最低覆盖率 | 说明 |
|----------|-----------|------|
| 核心业务逻辑 | ≥ 80% | 如：运动控制算法、协议解析 |
| 工具类 / 数据结构 | ≥ 90% | 如：RingBuffer、ThreadPool |
| UI 层 | ≥ 50% | 核心交互逻辑 |
| 第三方封装层 | ≥ 70% | 如：FFmpeg 封装 |

---

# 四类必测场景

## 1. 单元测试

只测试单一类/函数，使用 Mock 隔离外部依赖：

```cpp
// GoogleTest + GoogleMock 示例
class MockNetworkClient : public NetworkClient {
public:
    MOCK_METHOD(bool, send, (const Packet&), (override));
    MOCK_METHOD(Packet, receive, (), (override));
};

TEST(MotionControllerTest, SendCommand_NetworkOk_ReturnsTrue) {
    MockNetworkClient mock;
    EXPECT_CALL(mock, send(testing::_)).WillOnce(testing::Return(true));

    MotionController controller(&mock);
    Command cmd{CommandType::kMoveAbsolute, {100, 200}};
    EXPECT_TRUE(controller.execute(cmd));
}
```

每个用例执行时间必须 < 100ms。

## 2. 集成测试

测试模块间协作，允许使用真实依赖：

```
测试场景示例：
- 解码器 → 渲染器完整链路（输入 MP4 文件 → 验证帧输出）
- 网络模块 → 协议解析 → 命令执行
- 运动控制卡 → 电机驱动 → 状态反馈
```

每个集成测试执行时间必须 < 5s。

## 3. 压力测试

以下场景必须进行压力测试：

* 长时间运行稳定性（≥ 24 小时）
* 高并发连接（≥ 100 客户端）
* 高频指令下发（≥ 1000 条/秒）
* 大量数据吞吐（如 4K 视频解码）

需要输出：

* 内存泄漏检测结果（valgrind / ASAN 无泄漏）
* CPU 使用率曲线
* 延迟分布（P50 / P95 / P99）

## 4. 异常测试

以下异常场景必须覆盖：

* 断网 / 网络抖动（模拟丢包、高延迟）
* 设备掉线 / 超时
* 资源不足（内存分配失败、磁盘满）
* 非法输入（空指针、越界、格式错误）
* 并发竞争（多线程同时读写共享数据）

```cpp
// 异常测试示例
TEST(ProtocolParserTest, ParsePacket_TruncatedData_ReturnsError) {
    ProtocolParser parser;
    // 传入截断的数据包
    std::vector<uint8_t> truncated{0xAA, 0xBB};  // 不完整的包头
    auto result = parser.parse(truncated);
    EXPECT_EQ(result.status, ParseStatus::kIncomplete);
    EXPECT_FALSE(result.packet.has_value());
}
```

---

# 测试用例命名规范

格式：`TEST(类名Test, 方法名_场景_期望结果)`

```cpp
// 正确
TEST(VideoDecoderTest, DecodeFrame_NullPacket_ReturnsNullopt)
TEST(VideoDecoderTest, DecodeFrame_ValidH264Packet_ReturnsFrameWithCorrectPts)
TEST(VideoDecoderTest, DecodeFrame_CorruptedPacket_ReturnsError)

// 禁止
TEST(VideoDecoderTest, test1)              // 无意义
TEST(VideoDecoderTest, test_decode)        // 不描述场景
TEST(VideoDecoderTest, itShouldWork)       // 不说期望结果
```

---

# 禁止行为

禁止：

* 测试之间存在执行顺序依赖
* 测试用例中包含业务逻辑（测试应直接断言结果）
* 忽略测试失败（`EXPECT` / `ASSERT` 不得注释掉）
* 在测试中 `sleep()` 等待异步结果（使用条件变量或 callback）
* 测试覆盖重复的简单 Getter/Setter（仅测有逻辑的方法）
* 测试未隔离导致在其他机器上无法运行（禁止引用绝对路径/特定环境）

---

# 测试运行规范

提交前必须执行：

```bash
# 单元测试
ctest --output-on-failure

# 内存检测（Debug 构建）
# ASAN + UBSAN 无错误输出
```

CI 流水线中测试失败必须阻塞合并。

---

# 最终检查

✓ 新增逻辑有对应测试用例

✓ 覆盖 Happy Path + 边界值 + 错误路径

✓ 测试命名符合 TEST(类Test, 方法_场景_期望) 格式

✓ 单元测试执行时间 < 100ms

✓ 测试无顺序依赖

✓ 无 `sleep()` 等待异步结果

✓ ASAN / UBSAN 无错误
