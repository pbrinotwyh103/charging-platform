#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
"${project_dir}/build/bin/protocol_tests" -v1
"${project_dir}/build/bin/phase1_tests" -v1
"${project_dir}/build/bin/admin_controller_tests" -v1
QT_QPA_PLATFORM="${QT_QPA_PLATFORM:-offscreen}" \
    "${project_dir}/build/bin/admin_ui_tests" -v1
