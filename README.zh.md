# VAR 熔池缺陷检测系统源码仓库

简体中文 | [English](README.md)

> VAR 熔池视频分析系统的完整源码仓库。

仓库地址：`https://github.com/jjhhyyg/VAR-melting-defect-detection-source-code.git`

## 项目概览

本仓库采用“主仓库 + 3 个 Git submodule”的组织方式：

- `frontend`：基于 Nuxt 4、Vue 3、TypeScript 的前端应用
- `backend`：基于 Spring Boot 3、PostgreSQL、Redis、RabbitMQ 的后端服务
- `ai-processor`：基于 Flask、PyTorch、YOLO11 的 AI 分析模块

系统主链路如下：

1. 前端上传视频并创建任务
2. 后端保存任务元数据和视频文件
3. 后端向 RabbitMQ 投递分析任务
4. AI 模块消费消息，执行预处理、检测、追踪和结果导出，并回调后端
5. 后端持久化结果，并通过 WebSocket 将实时状态推送给前端

## 仓库结构

```text
codes/
├── backend/                  # Git submodule：Spring Boot 后端
├── frontend/                 # Git submodule：Nuxt 前端
├── ai-processor/             # Git submodule：Flask + YOLO 分析引擎
├── docs/                     # 交接、开发、测试、部署文档
├── env/                      # 环境变量模板与映射说明
├── scripts/                  # 环境切换与辅助脚本
├── storage/                  # 原视频、预处理视频、结果视频等共享存储
├── docker-compose.dev.yml    # 开发环境，仅启动中间件
├── docker-compose.prod.yml   # 生产部署（GPU）
├── docker-compose.prod.cpu.yml
└── docker-compose.yml        # 生成产物，禁止手工修改
```

## 正确拉取方式

本项目使用 Git submodule。直接 `git clone` 不够。

```bash
git clone --recurse-submodules https://github.com/jjhhyyg/VAR-melting-defect-detection-source-code.git
cd VAR-melting-defect-detection-source-code
```

如果你已经忘记拉 submodule：

```bash
git submodule update --init --recursive
```

如果你修改了 `backend`、`frontend` 或 `ai-processor` 中的代码，必须先在对应 submodule 内提交，再回到主仓库提交更新后的 submodule 指针。

## 必须区分的三个阶段

不要把“服务启动了”当成“可以部署了”。本项目必须严格分为三个阶段：

1. **开发阶段**
   搭建本地环境、生成 `.env`、启动各模块并具备调试能力。
2. **测试阶段**
   验证健康检查、上传链路、任务启动、MQ 消费、结果回传、前端展示。
3. **部署阶段**
   只有测试通过后才允许进入生产部署。

## 快速开始

### 1. 生成环境变量文件

```bash
./scripts/use-env.sh dev
```

脚本会生成：

- 根目录 `.env`，供 Docker Compose 使用
- `backend/.env`
- `frontend/.env`
- `ai-processor/.env`

### 2. 启动开发阶段中间件

```bash
docker compose -f docker-compose.dev.yml up -d
```

开发阶段只建议用 Docker 启动 PostgreSQL、Redis、RabbitMQ；应用层服务建议本地直跑。

### 3. 本地启动应用服务

后端：

```bash
cd backend
./mvnw spring-boot:run
```

前端：

```bash
cd frontend
npm install
npm run dev
```

AI 模块：

```bash
cd ai-processor
pip install -r requirements.txt
python app.py
```

默认本地访问地址：

- 前端：`http://localhost:3000`
- 后端：`http://localhost:8080`
- AI 模块：`http://localhost:5000`
- RabbitMQ 管理界面：`http://localhost:15672`

## 部署说明

- GPU 生产部署：`docker-compose.prod.yml`
- CPU 生产部署：`docker-compose.prod.cpu.yml`
- `docker-compose.yml` 是自动生成文件，不能手工维护
- `backend/Dockerfile` 默认要求先构建本地 JAR
- `backend/Dockerfile.build` 支持在 Docker 内完成构建

生产部署前必须先：

1. 执行 `./scripts/use-env.sh prod`
2. 确认 `ai-processor/weights/best.pt` 存在
3. 先完成本地测试并通过清单
4. 明确选择 GPU 路径还是 CPU 路径

## 文档导航

- 项目接手、开发、测试、部署主文档：
  [`docs/项目接手、开发测试与部署指南.md`](docs/项目接手、开发测试与部署指南.md)
- macOS 桌面端发布指南：
  [`docs/macOS桌面端发布指南.md`](docs/macOS桌面端发布指南.md)
- 环境配置说明：
  [`env/README.md`](env/README.md)
- 后端 Docker 构建说明：
  [`backend/DOCKER.md`](backend/DOCKER.md)

子模块文档：

- [`backend/README.zh.md`](backend/README.zh.md)
- [`frontend/README.zh.md`](frontend/README.zh.md)
- [`ai-processor/README.zh.md`](ai-processor/README.zh.md)

## 强制要求

未完成本地测试清单之前，禁止部署到生产环境。详细清单见 [`docs/项目接手、开发测试与部署指南.md`](docs/项目接手、开发测试与部署指南.md)。

## 许可证

本项目采用 GNU Affero General Public License v3.0 (AGPL-3.0) 许可证。
