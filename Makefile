SHELL := /bin/bash

COMPOSE_FILE := server/deploy/docker-compose.yml
ENV_FILE     := server/deploy/.env

.PHONY: dev-up dev-down dev-logs dev-ps srs-test init-keycloak-db help

help:
	@echo "Chat-Live-OK Monorepo"
	@echo ""
	@echo "  make dev-up           Start dev stack (Postgres, Redis, MinIO, SRS, Keycloak, coturn, nginx)"
	@echo "  make dev-down         Stop dev stack"
	@echo "  make dev-logs         Follow compose logs"
	@echo "  make dev-ps           Show running services"
	@echo "  make init-keycloak-db Create keycloak DB on existing Postgres volume"
	@echo "  make srs-test         RTMP test pattern push (requires ffmpeg)"
	@echo ""
	@echo "First run: cp server/deploy/.env.example server/deploy/.env"

dev-up:
	@test -f $(ENV_FILE) || (echo "Copy server/deploy/.env.example to server/deploy/.env first" && exit 1)
	docker compose -f $(COMPOSE_FILE) --env-file $(ENV_FILE) up -d

dev-down:
	docker compose -f $(COMPOSE_FILE) --env-file $(ENV_FILE) down

dev-logs:
	docker compose -f $(COMPOSE_FILE) --env-file $(ENV_FILE) logs -f

dev-ps:
	docker compose -f $(COMPOSE_FILE) --env-file $(ENV_FILE) ps

init-keycloak-db:
	@bash scripts/init-keycloak-db.sh

srs-test:
	@echo "Pushing test pattern to rtmp://127.0.0.1:1935/live/test?token=dev"
	ffmpeg -re -f lavfi -i testsrc=size=1280x720:rate=30 \
		-f lavfi -i sine=frequency=1000:sample_rate=44100 \
		-c:v libx264 -preset veryfast -tune zerolatency -pix_fmt yuv420p -g 60 -bf 0 \
		-c:a aac -b:a 128k -ar 44100 -ac 2 \
		-f flv "rtmp://127.0.0.1:1935/live/test?token=dev" -t 30
