# Demo02 — UE 5.7 纯蓝图游戏机制练习项目

## 环境

- Unreal Engine 5.7
- 插件：Modeling Tools Editor Mode
- 纯蓝图项目，无 C++ 依赖

---

## 场景（5 关卡）

| 关卡 | 玩法 | 核心技术 |
|------|------|----------|
| **BallAdventure_Level1Map** | 小球冒险 — 收集水晶、开启传送门、火炬照明、躲避销毁触发器 | 蓝图接口解耦交互、Enhanced Input、Pawn 物理控制、GameMode 状态管理 |
| **EatFoodMap** | 食物收集 — 场景中收集食物，碰撞墙壁判定 | 碰撞检测驱动得分、GameMode 规则、蓝图接口交互 |
| **HitCubeMap** | 射击打靶 — 发射投掷物命中立方体 | Projectile 蓝图、碰撞命中判定、命中反馈 |
| **02-Map** | 方块交互 — 与场景立方体触发逻辑 | Actor 蓝图事件绑定、简单交互反馈 |
| **01-Map** | 基础搭建 — 几何体关卡原型 | 关卡搭建基础 |

---

## 蓝图体系

### 1. BallAdventure 关卡 — 核心蓝图接口解耦

#### 1.1 角色系统

| 蓝图 | 功能 | 关键 UE 知识点 |
|------|------|---------------|
| **BP_BallAdventurePlayerPawn** | 小球 Pawn — 物理移动/跳跃/翻滚 | Pawn 类，Sphere Collision 物理体驱动移动，AddForce / AddImpulse 实现跳跃与翻滚，弹簧臂 Camera 跟随 |
| **BP_BallAdventurePlayerController** | 控制器 — 增强输入映射 | Player Controller 子类，Set Input Mode (Game Only)，IMC 绑定 |
| **BP_BallAdventureMode** | 游戏模式 — 规则与状态管理 | GameMode 子类，指定 Default Pawn / Controller / HUD，收集进度判定 |

#### 1.2 蓝图接口体系（核心解耦设计）

| 接口 | 函数签名 | 实现者 | 调用者 |
|------|---------|--------|--------|
| **BPI_Interact** | Interact() | BP_Door01、BP_Portal | 玩家触发器 |
| **BPI_Deliver** | Deliver(Item) | BP_Portal | BP_Crystal（收集完成时） |
| **BPI_ControllerCommands** | Command(CommandType) | BP_BallAdventurePlayerController | 各交互蓝图 |
| **BPI_PlayerState** | GetScore() / AddScore() | BP_BallAdventurePlayerState | 各交互蓝图 |

**BPI 解耦架构示例：**

```
玩家触碰触发器
    → 调用 BPI_Interact.Interact()
        → BP_Door01（实现）：开门/关门
        → BP_Portal（实现）：传送玩家
            （调用侧无需知道具体实现类型）
```

```
水晶收集完成
    → 调用 BPI_Deliver.Deliver()
        → BP_Portal：激活传送
            （水晶不需要知道传送门的内部逻辑）
```

**UE 知识点：** Blueprint Interface 实现接口隔离（Interface Segregation Principle）、多态调用解耦交互方与实现方、GameMode 作为 Mediator 协调子系统。

#### 1.3 交互机制

| 蓝图 | 功能 | 关键 UE 知识点 |
|------|------|---------------|
| **BP_Crystal** | 可收集水晶 — 触碰拾取、计分 | OnComponentBeginOverlap 事件、DestroyActor + Spawn Pickup Effect、通过 BPI_PlayerState / BPI_Deliver 通知 GameMode |
| **BP_Door01** | 可开关门 | Timeline 驱动旋转、BPI_Interact 接口实现、条件触发（全收集后才开门/可随时开关） |
| **BP_Portal** | 传送门 | BPI_Interact + BPI_Deliver 双重接口实现、Teleport 位置跳转、进入条件判定 |
| **BP_Torch** | 火炬照明 | Point Light 组件 + 动态材质参数、碰撞触发光照切换 |
| **BP_VanityDestruction** | 销毁触发器 — 玩家重置 | OnComponentBeginOverlap → Destroy/Reset 玩家，重启关卡 |

#### 1.4 输入系统

| 蓝图 | 类型 | 功能 |
|------|------|------|
| **IA_Jump_BallAdventure** | Input Action (Bool) | 跳跃触发 |
| **IA_Look_BallAdventure** | Input Action (Axis2D) | 视角控制 |
| **IA_Roll_BallAdventure** | Input Action (Bool) | 翻滚技能 |
| **IA_Interact_Fkeybord** | Input Action (Bool) | 交互触发（F 键） |
| **IMC_BallAdventure** | Input Mapping Context | 绑定上述 IA 到键盘/鼠标按键 |

**UE 知识点：** Enhanced Input System 的 Action 类型（Bool/Axis2D）、IMC 优先级与按键映射、Input Action 与蓝图函数直接绑定。

#### 1.5 UI 系统

| 蓝图 | 功能 | 关键 UE 知识点 |
|------|------|---------------|
| **WBP_BallHealthBar** | 血量条 | Progress Bar 绑定玩家血量变量、Bind Widget 事件驱动更新 |
| **WBP_BallMainHDU** | 主游戏内 HUD | Canvas Panel 布局、得分/提示文本实时绑定 |
| **WBP_BallScoreBoard** | 计分板 | Vertical Box 列表、收集进度显示 |
| **WBP_DeathScreen** | 死亡界面 | 全屏覆盖、按钮 → 重新开始/返回主菜单 |

**UE 知识点：** Widget Blueprint 控件绑定（Bind Widget）、`IsValid` 节点避免空指针、Event Construct 初始化绑定、Level 跳转（Open Level）。

---

### 2. EatFood 关卡

| 蓝图 | 功能 | 关键 UE 知识点 |
|------|------|---------------|
| **BP_Player** | 玩家角色 | Pawn 类，Physics 物理碰撞驱动，键盘控制移动 |
| **BP_MyController** | 玩家控制器 | 输入绑定与视角管理 |
| **BP_Food** | 食物物品 | OnBeginOverlap → 增加分数 → DestroyActor，随机位置生成 |
| **BP_Wall** | 墙壁障碍 | Static Mesh 碰撞阻挡，边界划定 |
| **BP_GameMode** | 游戏模式 | 得分累计、游戏结束判定、食物生成管理 |
| **BPI_BallInteraction** | 球体交互接口 | 统一食物拾取与碰撞响应 |

**UE 知识点：** 碰撞事件驱动游戏逻辑、GameMode 管理运行时状态、Interface 统一交互协议。

---

### 3. HitCube 关卡

| 蓝图 | 功能 | 关键 UE 知识点 |
|------|------|---------------|
| **BP_Bullet** | 子弹投掷物 | Projectile 运动：初始速度 AddImpulse / SetPhysicsLinearVelocity、碰撞通道预设（Projectile Channel）、OnHit 事件 → 销毁自身 |
| **BP_HitCube** | 目标立方体 | OnHit 接收 → 命中反馈（材质切换/颜色变化/销毁）、计分更新 |

**UE 知识点：** 物理投掷物实现（Physics Projectile）、碰撞通道（Collision Channel）筛选命中对象、OnHit 事件驱动反馈循环。

---

### 4. 方块交互关卡

| 蓝图 | 功能 | 关键 UE 知识点 |
|------|------|---------------|
| **Cube4_Blueprint** | 可交互立方体 | 蓝图 Actor，OnClicked/OnBeginOverlap 事件 → 触发逻辑与视觉反馈 |
| **Cube4_Blueprint_Blueprint_zi** | 交互子蓝图 | 继承/复用 Cube4_Blueprint |

**UE 知识点：** 蓝图继承（Blueprint Parent/Child）、Actor 点击事件绑定。

---

## 关卡入口

| 关卡文件 | 路径 |
|---------|------|
| BallAdventure_Level1Map.umap | Content/MyMaps/Maps/ |
| EatFoodMap.umap | Content/MyMaps/Maps/ |
| HitCubeMap.umap | Content/MyMaps/Maps/ |
| 01-Map.umap | Content/MyMaps/Maps/ |
| 02-Map.umap | Content/MyMaps/Maps/ |

---

## 目录结构

```
Content/
├── MyMaps/                              # 主要游戏内容
│   ├── BluePrint/
│   │   ├── BP_BallAdventure_Level1/     # ★ 核心项目：完整蓝图接口示例
│   │   │   ├── BP_BallAdventureMode     # GameMode
│   │   │   ├── BP_BallAdventurePlayerController  # PlayerController
│   │   │   ├── BP_BallAdventurePlayerPawn        # 小球 Pawn
│   │   │   ├── BP_Crystal              # 收集品
│   │   │   ├── BP_Door01               # 交互门
│   │   │   ├── BP_Portal               # 传送门
│   │   │   ├── BP_Torch                # 火炬
│   │   │   ├── BP_VanityDestruction    # 销毁触发器
│   │   │   ├── BPI_Interact            # ★ 交互接口
│   │   │   ├── BPI_ControllerCommands  # ★ 控制器命令接口
│   │   │   ├── BPI_PlayerState         # ★ 玩家状态接口
│   │   │   ├── Input/                  # 增强输入动作
│   │   │   └── UI/                     # Widget Blueprint
│   │   ├── BP_EatFood/                 # 食物收集
│   │   ├── BP_HitCubeMap/              # 射击打靶
│   │   └── BP_Map02/                   # 方块交互
│   ├── Maps/                           # 关卡文件（5 umaps）
│   ├── Materials/                      # 材质
│   └── Mesh/                           # 模型
├── FirstPerson/                         # 第一人称模板
├── ImportContent/                       # 导入第三方资源
├── LevelPrototyping/                    # 关卡原型素材
└── StarterContent/                      # UE 初学者内容包
```

---

## UE 知识点汇总

| 类别 | 知识点 | 应用位置 |
|------|--------|---------|
| **蓝图接口** | BPI 接口隔离、多态调用、跨 Actor 解耦 | BPI_Interact / BPI_Deliver / BPI_ControllerCommands / BPI_PlayerState |
| **增强输入** | Input Action (Bool/Axis2D)、Mapping Context 优先级绑定 | IA_Jump / IA_Look / IMC_BallAdventure 等 |
| **物理碰撞** | OnComponentBeginOverlap、OnHit 事件、碰撞通道预设 | BP_Crystal / BP_Bullet / BP_Wall |
| **GameMode** | 游戏规则管理、Default Pawn/Controller/HUD 指定、得分/状态控制 | BP_BallAdventureMode / EatFood BP_GameMode |
| **Pawn/Controller** | Pawn 物理体控制、Controller 输入转发、Character Movement | BP_BallAdventurePlayerPawn / BP_Player |
| **UI Widget** | Progress Bar 绑定、Canvas Panel 布局、Event Construct、Open Level | WBP_BallHealthBar / WBP_DeathScreen |
| **Timeline** | Timeline 驱动旋转/位移连续变化 | BP_Door01 开门动画 |
| **投掷物** | Projectile 物理运动、碰撞命中反馈 | BP_Bullet / BP_HitCube |
| **蓝图继承** | Parent/Child 蓝图复用 | Cube4_Blueprint 继承链 |
| **事件分发** | Event Dispatcher 解耦事件广播 | 各交互蓝图中的触发链 |

---

## 运行

1. 安装 Unreal Engine 5.7
2. 双击 `Demo02.uproject`
3. Content Browser → `Content/MyMaps/Maps/` → 打开任意关卡

推荐从 **BallAdventure_Level1Map** 开始体验最完整的蓝图接口解耦设计。
