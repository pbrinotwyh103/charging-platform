#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="${project_dir}/build"

mkdir -p "${build_dir}"
cd "${build_dir}"
qmake6 "${project_dir}/charging-platform.pro"

if [[ -n "${BUILD_JOBS:-}" ]]; then
    build_jobs="${BUILD_JOBS}"
elif command -v nproc >/dev/null 2>&1; then
    build_jobs="$(nproc)"
elif command -v sysctl >/dev/null 2>&1; then
    build_jobs="$(sysctl -n hw.ncpu)"
else
    build_jobs=2
fi

make -j"${build_jobs}"

printf 'Build complete: %s/bin\n' "${build_dir}"
