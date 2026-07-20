SHELL := /bin/bash

COMPOSE_FILE := server/deploy/docker-compose.yml
ENV_FILE     := server/deploy/.env

.PHONY: dev-up dev-down dev-logs dev-ps srs-test init-keycloak-db chatlive-build dev-up-full smoke-p2 tls-cert tls-renew help

help:
	@echo "Chat-Live-OK Monorepo"
	@echo ""
	@echo "  make dev-up           Start dev stack (Postgres, Redis, MinIO, SRS, Keycloak, coturn, nginx)"
	@echo "  make dev-up-full      Build chatlive-server image and start full stack"
	@echo "  make chatlive-build   Build chatlive-server Docker image only"
	@echo "  make smoke-p2         Run P2 IM smoke test (needs jq, running stack)"
	@echo "  make clear-stale-calls  End zombie ringing/connected calls (fix busy line)"
	@echo "  make tls-cert           Obtain Let's Encrypt cert (set TLS_DOMAIN in .env)"
	@echo "  make tls-renew          Renew Let's Encrypt cert"
	@echo "  make dev-down         Stop dev stack"
	@echo "  make dev-logs         Follow compose logs"
	@echo "  make dev-ps           Show running services"
	@echo "  make init-keycloak-db Create keycloak DB on existing Postgres volume"
	@echo "  make srs-test         RTMP test pattern push (requires ffmpeg)"
	@echo ""
	@echo "First run: cp server/deploy/.env.example server/deploy/.env"

dev-up:
	@test -f $(ENV_FILE) || (echo "Copy server/deploy/.env.example to server/deploy/.env first" && exit 1)
	@bash server/deploy/nginx/gen-dev-cert.sh
	@bash server/deploy/nginx/resolve-nginx-tls.sh
	@bash server/deploy/coturn/resolve-turn-conf.sh
	docker compose -f $(COMPOSE_FILE) --env-file $(ENV_FILE) up -d
	@sleep 3 && bash server/deploy/scripts/sync-keycloak-https.sh || true

chatlive-build:
	@test -f $(ENV_FILE) || (echo "Copy server/deploy/.env.example to server/deploy/.env first" && exit 1)
	docker compose -f $(COMPOSE_FILE) --env-file $(ENV_FILE) build chatlive

dev-up-full: chatlive-build
	@test -f $(ENV_FILE) || (echo "Copy server/deploy/.env.example to server/deploy/.env first" && exit 1)
	@bash server/deploy/nginx/gen-dev-cert.sh
	@bash server/deploy/nginx/resolve-nginx-tls.sh
	@bash server/deploy/coturn/resolve-turn-conf.sh
	docker compose -f $(COMPOSE_FILE) --env-file $(ENV_FILE) up -d
	@sleep 3 && bash server/deploy/scripts/sync-keycloak-https.sh || true

tls-cert:
	@test -f $(ENV_FILE) || (echo "Copy server/deploy/.env.example to server/deploy/.env first" && exit 1)
	@bash server/deploy/nginx/obtain-tls-cert.sh
	@bash server/deploy/scripts/sync-keycloak-https.sh || true

tls-renew:
	@bash server/deploy/nginx/renew-tls-cert.sh

smoke-p2:
	@bash server/scripts/smoke-p2.sh

clear-stale-calls:
	@bash server/deploy/scripts/clear-stale-calls.sh

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
