@echo off
cd /d "%~dp0"
docker compose exec -T search-server search %*
