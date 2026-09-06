#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
"${project_dir}/build/bin/protocol_tests" -v1
"${project_dir}/build/bin/phase1_tests" -v1
"${project_dir}/build/bin/database_repository_tests" -v1
