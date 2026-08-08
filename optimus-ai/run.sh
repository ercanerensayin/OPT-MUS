#!/usr/bin/env bash
# OPTIMUS AI — kurulum ve çalıştırma
set -euo pipefail

cd "$(dirname "$0")"

if [ ! -d .venv ]; then
  echo "· sanal ortam kuruluyor"
  python3 -m venv .venv
fi

# shellcheck disable=SC1091
source .venv/bin/activate

echo "· bağımlılıklar yükleniyor"
pip install --quiet --upgrade pip
pip install --quiet -r requirements.txt

if [ ! -f .env ]; then
  cp .env.example .env
  echo "· .env oluşturuldu — ANTHROPIC_API_KEY'i doldur, sonra tekrar çalıştır."
fi

exec python -m optimus_ai.server
