# CAN-Dash 量产级汽车仪表

通过修改 YAML 配置完成 90% 的后期迭代，C++ 代码基本不需要改动。

## 技术栈

- **UI**: Qt/QML
- **仿真引擎**: Python + python-can + Unix Socket
- **配置驱动**: YAML → C 代码生成（编译时预处理，零开机延迟）
- **架构**: Layer 1(C生成) → Layer 2(纯C++业务) → Layer 3(Qt适配) → Layer 4(QML)

## 性能基线

数据流热路径（shm + 28 字段 + 18 alarm rules）的端到端耗时：

- **dash tick 总计: 3.6 µs (0.022% of 16ms 预算)**
- shm read+checksum: 1.2 µs
- AlarmRuntime 22 keys eval: 1.2 µs
- 端到端 (含 processor msync): 596 µs (3.73%)

→ 99.98% 时间留给 QML 渲染 + 事件循环。详见 [PERFORMANCE.md](docs/PERFORMANCE.md)。

## 快速开始

```bash
# 1. 安装依赖（Ubuntu/Debian）
sudo apt install qt6-base-dev qt6-declarative-dev qt6-svg-dev
pip install pyyaml jsonschema pydantic cantools python-can

# 2. 校验配置 + 生成 C 代码
python tools/validate.py
python tools/yaml_to_c.py

# 3. 编译（CMake）
./build.sh                         # 默认 = 全量构建 + 跑测试
./build.sh ut                      # 只跑 Layer 1/2 单元测试（无 Qt，< 30s）
./build.sh gui                     # 只编 Qt UI

# 或手动：
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel

# 4. PC 仿真（两终端）
./can-dash                          # 终端1：启动仪表
python can_sim/engine.py            # 终端2：启动CAN仿真
```

## 目录结构

```
config/              YAML 配置（AI 主要修改位置）
schemas/             JSON Schema（AI 理解 YAML 字段语义）
src/generated/       ⚠️ 由 tools/yaml_to_c.py 自动生成
src/layer2/          纯 C++ 业务逻辑（无 Qt）
src/layer3/          Qt 适配层
src/ui/              QML 界面
tools/               yaml_to_c.py / validate.py
tests/               Layer 2 单元测试
can_sim/             PC 仿真引擎
```

## 核心工作流

| 场景 | 操作 |
|------|------|
| 调整报警阈值 | 改 `config/alarm_rules.yaml` → `validate.py` → `yaml_to_c.py` → `make` |
| 新增报警灯 | 改 `config/alarm_rules.yaml` + `config/indicators.yaml` |
| 新增 CAN 信号 | 改 `config/can_ids.yaml` |
| 改界面布局 | 改 `config/display_layout.yaml` |

详见 [ARCHITECTURE.md](ARCHITECTURE.md)

## AI 开发

AI 请先阅读 `ARCHITECTURE.md`，修改配置前运行 `python tools/validate.py` 校验 YAML。

## 贡献流程

请阅读 [CONTRIBUTING.md](CONTRIBUTING.md)。要点：
- commit 信息遵守 Conventional Commits（`feat:` `fix:` `refactor:` `test:` `docs:` `ci:` `chore:`），git hook 会强制
- 连续 3 个 `fix:` 会被 hook 拒绝 — 鼓励用 `refactor:` 找根因
- 改完跑 `./build.sh ut`（无 Qt，< 30s）
- 安装本地 hook：`python3 tools/install-hooks.sh`

## CI

GitHub Actions 跑 4 个 job：lint（header 守卫 + cppcheck）、validate-yaml、ut（无 Qt 单元测试）、build（main 分支全量 Qt 构建）。详见 `.github/workflows/ci.yml`。

## 许可证

MIT
