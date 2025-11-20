#!/bin/bash

# network_system 소스 파일들의 include 경로 업데이트 스크립트

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

echo "=== Network System 소스 파일 Include 경로 업데이트 ==="
echo "Project Root: $PROJECT_ROOT"

# 업데이트할 디렉토리 목록
DIRS_TO_UPDATE=(
    "$PROJECT_ROOT/src"
    "$PROJECT_ROOT/tests"
    "$PROJECT_ROOT/samples"
    "$PROJECT_ROOT/benchmarks"
    "$PROJECT_ROOT/integration_tests"
)

# 변경 사항 추적
TOTAL_FILES=0
MODIFIED_FILES=0

echo ""
echo "Include 경로 업데이트 중..."

for dir in "${DIRS_TO_UPDATE[@]}"; do
    if [ -d "$dir" ]; then
        echo "처리 중: $dir"

        # 소스 파일 찾기 및 업데이트
        find "$dir" -type f \( -name "*.cpp" -o -name "*.cc" -o -name "*.h" -o -name "*.hpp" \) -print0 | while IFS= read -r -d '' file; do
            TOTAL_FILES=$((TOTAL_FILES + 1))

            # 임시 파일로 변경 사항 적용
            cp "$file" "${file}.tmp"

            # network_system/xxx -> kcenon/network/xxx
            sed -i.bak 's|#include "network_system/|#include "kcenon/network/|g' "${file}.tmp"
            sed -i.bak 's|#include <network_system/|#include <kcenon/network/|g' "${file}.tmp"

            # 변경 사항이 있는지 확인
            if ! cmp -s "$file" "${file}.tmp"; then
                mv "${file}.tmp" "$file"
                rm -f "${file}.bak"
                MODIFIED_FILES=$((MODIFIED_FILES + 1))
                echo "  수정됨: $(basename "$file")"
            else
                rm -f "${file}.tmp" "${file}.bak"
            fi
        done
    fi
done

# 다른 시스템들과의 통합 파일 업데이트
echo ""
echo "다른 시스템들과의 통합 파일 확인 중..."

# unified_system 프로젝트 전체에서 network_system을 참조하는 파일 찾기
UNIFIED_ROOT="$(dirname "$PROJECT_ROOT")"
if [ -d "$UNIFIED_ROOT" ]; then
    OTHER_SYSTEMS=(
        "common_system"
        "thread_system"
        "logger_system"
        "monitoring_system"
        "container_system"
        "database_system"
    )

    for system in "${OTHER_SYSTEMS[@]}"; do
        SYSTEM_DIR="$UNIFIED_ROOT/$system"
        if [ -d "$SYSTEM_DIR" ]; then
            echo "  확인 중: $system"

            # network_system을 참조하는 파일 찾기
            grep -r "network_system/" "$SYSTEM_DIR" --include="*.h" --include="*.hpp" --include="*.cpp" --include="*.cc" 2>/dev/null | while IFS=: read -r file line; do
                if [ -f "$file" ]; then
                    echo "    경고: $file 파일이 network_system을 참조합니다"
                fi
            done || true
        fi
    done
fi

echo ""
echo "=== 업데이트 완료 ==="
echo "총 파일 수: $TOTAL_FILES"
echo "수정된 파일 수: $MODIFIED_FILES"
echo ""
echo "다음 단계:"
echo "1. CMakeLists.txt 업데이트"
echo "2. 빌드 테스트"
echo "3. 테스트 실행"