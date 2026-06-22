<h1 align="center">UE5 物理驱动游戏机制实践</h1>

<p align="center">
  <samp>
    Unreal Engine 5.7 · 蓝图 + C++ · Chaos 物理 · BPI 解耦 · 增强输入
  </samp>
</p>

---

## 项目概述

一个以物理驱动的 3D 小球冒险为核心，延伸出食物收集、射击打靶、方块交互等五个玩法关卡的机制实践项目。核心目标不是关卡数量，而是每一关背后刻意练习的引擎机制——Chaos 物理引擎、Blueprint Interface 多态解耦、Enhanced Input 生命周期、PlayerController 复活握手协议。

个人独立完成，共编写 26 个自定义蓝图、5 套 BPI 接口、4 个 UMG 控件蓝图、16 个关卡专属静态网格体，全部从零搭建。

---

## 核心关卡

| 关卡 | 玩法 | 核心技术 |
|------|------|---------|
| **BallAdventure** | 小球物理冒险 — 收集水晶、开启传送门、躲避销毁触发器 | 向量叉乘扭矩计算、Chaos 双端驱动、BPI 多态交互、Teleport 跃迁、假死复活流水线 |
| **EatFood** | 食物收集 — 场景中收集食物 | 碰撞驱动得分、GameMode 集中规则管理 |
| **HitCube** | 射击打靶 — 发射投掷物命中立方体 | Projectile 碰撞通道、物理投掷物、OnHit 反馈链 |
| **02-Map** | 方块交互 — 与场景立方体触发逻辑 | 蓝图父子继承、OnClicked/OnBeginOverlap 事件绑定 |
| **01-Map** | 基础搭建 — 几何体关卡原型 | BSP/静态网格体灰盒设计 |

---

## 架构设计哲学

### MVC 三层解耦

严格遵循 Controller-Pawn-GameMode 三位一体分层：

- **Controller** 是所有系统级命令的唯一受理者。实现 `BPI_ControllerCommands` 全套接口（ChangePawn 切换肉体、NotifyPlayerDeath 假死上报、RequestPawnRespawn 复活请求、NotifyRespawnReady 亮屏回调）。
- **Pawn** 只负责物理响应——接收扭矩、受力移动、射线检测地面。不持有任何 UI 引用，不直接调用 Destroy。
- **GameMode** 承担规则判定与状态管理，不参与交互逻辑。

### BPI 接口隔离

5 套 Blueprint Interface 按能力拆分，杜绝滥用 Cast To：

- `BPI_Interact` — 通用交互（开门/传送/上车）
- `BPI_Deliver` — 收集交付通知
- `BPI_ControllerCommands` — 系统级命令申报
- `BPI_PlayerState` — 玩家属性读写
- `BPI_BallInteraction` — 食物拾取三函数协议

### 死亡复活握手协议

死亡不是简单 Destroy——而是一套双向握手流程：

```
碰撞触发 → Pawn 调用 BPI_ControllerCommands::NotifyPlayerDeath
→ Controller 受理 → 播放黑屏淡出 → 生成 DeathUI
→ 玩家点击复活 → UI 调用 RequestPawnRespawn
→ Controller 验证 → 重置 Pawn 状态 → 调用 NotifyRespawnReady
→ Start Camera Fade 亮屏 → 世界恢复
```

---

## 物理驱动详解

### 地面滚动

通过向量叉乘将 WASD 键盘输入转换为球体旋转扭矩轴：`Add Torque(In Radians) + Accel Change` 实现地面物理滚动。Line Trace 实时检测地面状态，射线长度 = 球体半径 + 10cm 容差，防止地形起伏导致的判定抖动。

### 空中双端驱动

`Add Force + Accel Change` 提供空中水平推力，`Add Torque` 保持旋转惯性。在物理真实感和操控手感之间取得平衡——让玩家有足够控制力又不会失去物理驱动的本质。

### 性能意识

Tick Interval 锁定 0.033s（约 30 FPS），相比默认每帧执行，物理计算开销减半。

---

## 蓝图统计

| 指标 | 数值 |
|------|------|
| 自定义蓝图 | 26 个（22 Blueprint + 4 Widget） |
| BPI 接口 | 5 套 |
| 最大蓝图 | `BP_BallAdventurePlayerPawn` — 227 节点 / 175 连线 |
| 开发笔记 | 18 篇（涵盖从坐标系到网络同步的完整知识链） |

---

## 学习路径

随项目开发同步记录了 18 篇技术笔记，形成从基础到架构的完整知识链：

角色坐标系 → Gameplay 框架 → 物理控制 → 蓝图通信(BPI) → 射线检测 → 向量叉乘 → BPI 开门 → 传送门 → 角度调控 → 死亡复活 → BPI 终极分类 → Enum 使用 → 函数复用 → UI 广播绑定 → 函数/宏/事件 → 网络同步与 UObject 体系 → PlayerController 生命周期与 RAII

---

**引擎**: Unreal Engine 5.7 · **开发方式**: Blueprint + C++ 插件 · **周期**: 约 4 个月
