#!/usr/bin/env bash
# Generate API clients from protocol/openapi (placeholder)
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

echo "gen-api: scaffold only"
echo "  1. Expand protocol/openapi/*.yaml from docs/api/"
echo "  2. Install openapi-generator-cli"
echo "  3. Generate TS → client/web/src/api/"
echo "  4. Optional C++ stubs for ccvWriter"
echo ""
echo "OpenAPI root: ${ROOT}/protocol/openapi/common.yaml"
