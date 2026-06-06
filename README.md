# CAN-Dash 量产级汽车仪表

> ⚠️ **许可证: AGPL-3.0 + 商业化限制**
> 本项目以 [GNU Affero General Public License v3](https://www.gnu.org/licenses/agpl-3.0.html) 发布。
> **任何商业化使用** — 销售、SaaS、收费集成、咨询服务等 — **必须事先获得版权所有者书面授权**。
> 详见 [LICENSE](LICENSE)。如需商业授权联系 [980464102@qq.com](mailto:980464102@qq.com)。

通过修改 YAML 配置完成 90% 的后期迭代，C++ 代码基本不需要改动。

## 技术栈

- **UI**: Qt/QML
- **仿真引擎**: Python + python-can + Unix Socket
- **配置驱动**: YAML → C 代码生成（编译时预处理，零开机延迟）
- **架构**: Layer 1(C生成) → Layer 2(纯C++业务) → Layer 3(Qt适配) → Layer 4(QML)

## 性能基线

数据流热路径（shm + 28 字段 + 18 alarm rules）的端到端耗时：

- **dash tick 总计: 4.1 µs (0.026% of 16ms 预算)** ← read + convert + alarm + trip_computer + view_manager + settings_manager (PR 66/67 加进 L3 接入, view_current / settings_units / settings_brightness 5 字段无 shm L1 镜像, 16ms 必经 L2 snapshot)
- shm read+checksum: 1.2 µs
- AlarmRuntime 22 keys eval (18 rules): 1.6 µs
- TripComputer 派生指标 (PR 1-4): <0.1 µs
- ViewManager 视图状态机 (PR 11/13, 16ms hot path 必经): <0.1 µs
- SettingsManager 公制/英制 + 0-100 亮度 (PR 14, 16ms hot path 必经): <0.1 µs
- 端到端 (含 processor msync): 600 µs (3.75%)
- display 旁路总开销: ~0.78 µs (PR 7/9/14/17/43/51/61/62 L2 runtime 状态查询, 8 个 runtime, 含 PR 17 SelfTestRuntime 14 onValueChanged + 1Hz tick 节流 366 ns)

→ 99.97% 时间留给 QML 渲染 + 事件循环。详见 [PERFORMANCE.md](docs/PERFORMANCE.md)。

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

**[AGPL-3.0 + 商业化限制](LICENSE)** (Copyright © 2026 CAN-Dash contributors)

- ✅ 个人学习/研究/非商业开发/开源贡献: **允许** (AGPL-3.0)
- ⚠️ 商业化使用(销售/SaaS/收费集成/咨询): **需要书面授权** (见 [LICENSE](LICENSE) 第 2 节)
