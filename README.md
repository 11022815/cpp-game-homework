# 星星人豆豆大冒险 —— C++ 作业版

这是一个**独立的 C++ 控制台游戏工程**，和原来的网页版完全分开。
不需要网页、不需要图形库、不需要外部素材，只用标准 C++ + Windows 控制台 API。

## 文件清单

| 文件 | 作用 |
| --- | --- |
| `StarBeanAdventure.sln` | Visual Studio 解决方案，双击它打开工程 |
| `StarBeanAdventure.vcxproj` | Visual Studio 工程配置 |
| `StarBeanAdventure.vcxproj.filters` | VS 里的文件分类显示 |
| `StarBeanAdventure.cpp` | 全部游戏源码，交作业时主要交这个文件 |
| `README.md` | 本说明文档 |

## 用 Visual Studio 打开和运行

1. 双击 `StarBeanAdventure.sln`。
2. 顶部配置选择 `Debug` 和 `x64`（或 `Release x64`）。
3. 按 `Ctrl + F5` 运行（不调试直接运行）。
4. 生成的 exe 在工程目录下的 `x64\Debug\` 或 `x64\Release\` 里。

> 工程使用 `$(DefaultPlatformToolset)` 自动适配本机已安装的 MSVC 工具集，
> Visual Studio 2019 / 2022 / 2026 都可以直接编译，不需要手动改版本号。
> 如果已经打开过旧版工程，请先关闭 VS，再重新双击 `.sln` 打开一次。

> 也可以用命令行编译：
> `cl /EHsc StarBeanAdventure.cpp` 或 `g++ StarBeanAdventure.cpp -o StarBeanAdventure.exe`

## 操作说明

| 按键 | 功能 |
| --- | --- |
| `W A S D` / 方向键 | 移动，按住可连续走，松开就停 |
| `J` | 击晕技能：眩晕附近怪物；力量状态下可击杀 |
| `K` | 冲刺：短时间内每帧多走一格，带冷却 |
| `P` | 暂停 / 继续 |
| `Q` | 退出 |
| `R` | 游戏结束后重新开始 |

## 游戏规则

- 迷宫每关随机生成，并会打通一些墙形成环路。
- 吃光所有豆子后，出口 `E` 才生效；走到 `E` 进入下一关。
- 共 3 关，全部通过即通关（源码里 `MAX_LEVEL` 可改成 999）。
- 怪物一开始巡逻，靠近后会用 BFS 寻路追击；吃到力量豆后会逃跑。
- 安全屋 `H` 里待满 3 秒，进入隐匿状态 8 秒，怪物检测不到你；离开后隐匿继续倒计时。
- 陷阱 `^` 会扣一条命；星星护盾 `*` 可以挡一次伤害。
- 3 条星星命用完游戏结束。

## 代码结构（作业讲解可用）

- `Pos`：格子坐标。
- `Player`：玩家状态（位置、生命、分数、力量、护盾、技能冷却、隐匿时间等）。
- `Enemy`：怪物状态（位置、出生点、朝向、眩晕、复活倒计时）。
- `Game`：游戏主类。
  - `generate()`：随机迷宫生成、豆豆/陷阱/安全屋/怪物布置。
  - `handleInput()` / `updateHeldDirections()`：键盘输入与按住连续移动。
  - `update()`：主循环逻辑，每 70ms 推进一帧。
  - `movePlayer()` / `collectItem()`：移动与吃豆判定。
  - `chaseStep()` / `fleeStep()` / `patrolStep()`：怪物 BFS 追击、逃跑和巡逻。
  - `updateSafeHouse()`：安全屋 3 秒隐匿逻辑。
  - `render()`：控制台绘制地图、HUD 和状态提示。

## 说明

界面全部使用 ASCII 字符，避免 Windows 控制台中文乱码。
所有迷宫、寻路、动画和音效之外的玩法都由本文件代码生成，没有引用任何第三方库。
