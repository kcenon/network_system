#!/bin/bash

# network_system 헤더 구조 재구성 스크립트
# network_system/*.h -> kcenon/network/*.h

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
INCLUDE_DIR="$PROJECT_ROOT/include"

echo "=== Network System 헤더 구조 재구성 ==="
echo "Project Root: $PROJECT_ROOT"
echo "Include Dir: $INCLUDE_DIR"

# 기존 구조 백업
if [ -d "$INCLUDE_DIR/network_system" ]; then
    echo "기존 구조 백업 중..."
    if [ ! -d "$INCLUDE_DIR/network_system.backup" ]; then
        cp -r "$INCLUDE_DIR/network_system" "$INCLUDE_DIR/network_system.backup"
        echo "백업 완료: $INCLUDE_DIR/network_system.backup"
    else
        echo "백업이 이미 존재합니다"
    fi
fi

# kcenon/network 디렉토리 생성 (forward.h는 유지)
echo "디렉토리 구조 생성 중..."
mkdir -p "$INCLUDE_DIR/kcenon/network"

# 헤더 파일 복사 및 구조 재구성
echo "헤더 파일 이동 중..."
if [ -d "$INCLUDE_DIR/network_system" ]; then
    # core 디렉토리
    if [ -d "$INCLUDE_DIR/network_system/core" ]; then
        mkdir -p "$INCLUDE_DIR/kcenon/network/core"
        cp -r "$INCLUDE_DIR/network_system/core/"* "$INCLUDE_DIR/kcenon/network/core/" 2>/dev/null || true
        echo "  - core 디렉토리 이동 완료"
    fi

    # session 디렉토리
    if [ -d "$INCLUDE_DIR/network_system/session" ]; then
        mkdir -p "$INCLUDE_DIR/kcenon/network/session"
        cp -r "$INCLUDE_DIR/network_system/session/"* "$INCLUDE_DIR/kcenon/network/session/" 2>/dev/null || true
        echo "  - session 디렉토리 이동 완료"
    fi

    # integration 디렉토리
    if [ -d "$INCLUDE_DIR/network_system/integration" ]; then
        mkdir -p "$INCLUDE_DIR/kcenon/network/integration"
        cp -r "$INCLUDE_DIR/network_system/integration/"* "$INCLUDE_DIR/kcenon/network/integration/" 2>/dev/null || true
        echo "  - integration 디렉토리 이동 완료"
    fi

    # internal 디렉토리
    if [ -d "$INCLUDE_DIR/network_system/internal" ]; then
        mkdir -p "$INCLUDE_DIR/kcenon/network/internal"
        cp -r "$INCLUDE_DIR/network_system/internal/"* "$INCLUDE_DIR/kcenon/network/internal/" 2>/dev/null || true
        echo "  - internal 디렉토리 이동 완료"
    fi

    # utils 디렉토리
    if [ -d "$INCLUDE_DIR/network_system/utils" ]; then
        mkdir -p "$INCLUDE_DIR/kcenon/network/utils"
        cp -r "$INCLUDE_DIR/network_system/utils/"* "$INCLUDE_DIR/kcenon/network/utils/" 2>/dev/null || true
        echo "  - utils 디렉토리 이동 완료"
    fi

    # config 디렉토리
    if [ -d "$INCLUDE_DIR/network_system/config" ]; then
        mkdir -p "$INCLUDE_DIR/kcenon/network/config"
        cp -r "$INCLUDE_DIR/network_system/config/"* "$INCLUDE_DIR/kcenon/network/config/" 2>/dev/null || true
        echo "  - config 디렉토리 이동 완료"
    fi

    # metrics 디렉토리
    if [ -d "$INCLUDE_DIR/network_system/metrics" ]; then
        mkdir -p "$INCLUDE_DIR/kcenon/network/metrics"
        cp -r "$INCLUDE_DIR/network_system/metrics/"* "$INCLUDE_DIR/kcenon/network/metrics/" 2>/dev/null || true
        echo "  - metrics 디렉토리 이동 완료"
    fi

    # 루트 레벨 파일들
    cp "$INCLUDE_DIR/network_system/"*.h "$INCLUDE_DIR/kcenon/network/" 2>/dev/null || true
    echo "  - 루트 레벨 파일 이동 완료"
fi

# include 경로 업데이트
echo "Include 경로 업데이트 중..."
echo "  새로운 헤더 파일에서 include 경로 수정..."
find "$INCLUDE_DIR/kcenon/network" -type f \( -name "*.h" -o -name "*.hpp" \) -print0 | while IFS= read -r -d '' file; do
    # network_system/xxx -> kcenon/network/xxx
    sed -i.bak 's|#include "network_system/|#include "kcenon/network/|g' "$file"
    sed -i.bak 's|#include <network_system/|#include <kcenon/network/|g' "$file"
    rm "${file}.bak"
done

echo "구조 재구성 완료!"
echo ""
echo "변경 사항:"
echo "  - include/network_system/* -> include/kcenon/network/*"
echo "  - 백업: include/network_system.backup"
echo ""
echo "다음 단계:"
echo "1. 소스 파일들의 include 경로 업데이트 (update_source_includes.sh 실행)"
echo "2. CMakeLists.txt 업데이트"
echo "3. 빌드 테스트"