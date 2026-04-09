# VAR Melting Defect Detection Source Code

[简体中文](README.zh.md) | English

> Source code repository for the VAR molten pool video analysis system.

Repository URL: `https://github.com/jjhhyyg/VAR-melting-defect-detection-source-code.git`

## Overview

This repository contains the full source code for a VAR molten pool video analysis system. It is organized as a main repository plus three Git submodules:

- `frontend`: Nuxt 4 + Vue 3 + TypeScript web application
- `backend`: Spring Boot 3 + PostgreSQL + Redis + RabbitMQ backend
- `ai-processor`: Flask + PyTorch + YOLO11 analysis engine

The system flow is:

1. The frontend uploads a video and creates a task.
2. The backend stores metadata and video files.
3. The backend sends an analysis message to RabbitMQ.
4. The AI processor consumes the message, preprocesses and analyzes the video, then pushes results back to the backend.
5. The backend persists results and pushes real-time updates to the frontend through WebSocket.

## Repository Structure

```text
codes/
├── backend/                  # Git submodule: Spring Boot backend
├── frontend/                 # Git submodule: Nuxt frontend
├── ai-processor/             # Git submodule: Flask + YOLO analysis engine
├── docs/                     # Handover and operation documents
├── env/                      # Environment templates and environment mapping
├── scripts/                  # Environment switching and helper scripts
├── storage/                  # Shared storage for uploaded and generated files
├── docker-compose.dev.yml    # Development infrastructure only
├── docker-compose.prod.yml   # Production deployment (GPU)
├── docker-compose.prod.cpu.yml
└── docker-compose.yml        # Generated file, do not edit manually
```

## Clone Correctly

This project uses Git submodules. A plain `git clone` is not enough.

```bash
git clone --recurse-submodules https://github.com/jjhhyyg/VAR-melting-defect-detection-source-code.git
cd VAR-melting-defect-detection-source-code
```

If you already cloned the repository without submodules:

```bash
git submodule update --init --recursive
```

When you modify code inside `backend`, `frontend`, or `ai-processor`, you must commit inside that submodule first, then commit the updated submodule pointer in the main repository.

## Three Required Stages

Do not treat "service starts successfully" as "ready for production". This project must be handled in three stages:

1. **Development**
   Set up the local environment, generate `.env` files, and run the services locally.
2. **Testing**
   Verify infrastructure, health checks, upload flow, task start, MQ consumption, result callback, and frontend display.
3. **Deployment**
   Deploy to production only after local testing passes.

## Quick Start

### 1. Generate environment files

```bash
./scripts/use-env.sh dev
```

This script generates:

- `.env` for Docker Compose
- `backend/.env`
- `frontend/.env`
- `ai-processor/.env`

### 2. Start development infrastructure

```bash
docker compose -f docker-compose.dev.yml up -d
```

This starts PostgreSQL, Redis, and RabbitMQ for local development. Application services are expected to run locally.

### 3. Run application services locally

Backend:

```bash
cd backend
./mvnw spring-boot:run
```

Frontend:

```bash
cd frontend
npm install
npm run dev
```

AI processor:

```bash
cd ai-processor
pip install -r requirements.txt
python app.py
```

Default local endpoints:

- Frontend: `http://localhost:3000`
- Backend: `http://localhost:8080`
- AI processor: `http://localhost:5000`
- RabbitMQ management UI: `http://localhost:15672`

## Deployment Notes

- GPU production deployment uses `docker-compose.prod.yml`
- CPU production deployment uses `docker-compose.prod.cpu.yml`
- `docker-compose.yml` is an auto-generated file and must not be edited manually
- `backend/Dockerfile` expects a prebuilt JAR by default
- `backend/Dockerfile.build` performs the build inside Docker when needed

Before production deployment:

1. switch to production environment with `./scripts/use-env.sh prod`
2. confirm `ai-processor/weights/best.pt` exists
3. complete local testing successfully
4. choose GPU or CPU deployment path deliberately

## Documentation

- Handover, development, testing, and deployment guide:
  [`docs/项目接手、开发测试与部署指南.md`](docs/项目接手、开发测试与部署指南.md)
- Environment configuration guide:
  [`env/README.md`](env/README.md)
- Backend Docker build details:
  [`backend/DOCKER.md`](backend/DOCKER.md)

Submodule READMEs:

- [`backend/README.md`](backend/README.md)
- [`frontend/README.md`](frontend/README.md)
- [`ai-processor/README.md`](ai-processor/README.md)

## Critical Rule

Do not deploy this project to production before the local testing checklist has passed. The detailed checklist is defined in [`docs/项目接手、开发测试与部署指南.md`](docs/项目接手、开发测试与部署指南.md).

## License

This project is licensed under the GNU Affero General Public License v3.0 (AGPL-3.0).
