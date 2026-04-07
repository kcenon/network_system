#!/usr/bin/env bash
# validate_vcpkg_chain.sh - End-to-end validation of kcenon-network-system vcpkg build chain
#
# Usage:
#   ./scripts/validate_vcpkg_chain.sh [OPTIONS]
#
# Options:
#   --vcpkg-root <path>   Path to vcpkg installation (default: auto-detect)
#   --triplet <triplet>   Target triplet (default: auto-detect from platform)
#   --skip-build          Skip test consumer build (install-only validation)
#   --clean               Remove previous validation artifacts before starting
#   --json                Output results as JSON
#   --help                Show this help message

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
CONSUMER_DIR="${PROJECT_ROOT}/tests/vcpkg-consumer"
BUILD_DIR="${PROJECT_ROOT}/build_vcpkg_validation"

# Defaults
VCPKG_ROOT=""
TRIPLET=""
SKIP_BUILD=false
CLEAN=false
JSON_OUTPUT=false

# Ports to validate (network_system + transitive dependencies)
PORTS=(
    "kcenon-common-system"
    "kcenon-thread-system"
    "kcenon-network-system"
)
PORT_COUNT=${#PORTS[@]}

# Results: parallel arrays indexed 0..2 matching PORTS
INSTALL_STATUS=()
FIND_STATUS=()
for (( i=0; i<PORT_COUNT; i++ )); do
    INSTALL_STATUS+=("unknown")
    FIND_STATUS+=("skipped")
done
CONSUMER_BUILD_RESULT="skipped"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

log_info()   { [[ "$JSON_OUTPUT" == true ]] || echo -e "${BLUE}[INFO]${NC} $*"; }
log_ok()     { [[ "$JSON_OUTPUT" == true ]] || echo -e "${GREEN}[PASS]${NC} $*"; }
log_fail()   { [[ "$JSON_OUTPUT" == true ]] || echo -e "${RED}[FAIL]${NC} $*"; }
log_warn()   { [[ "$JSON_OUTPUT" == true ]] || echo -e "${YELLOW}[WARN]${NC} $*"; }
log_header() { [[ "$JSON_OUTPUT" == true ]] || echo -e "\n${BLUE}=== $* ===${NC}"; }

usage() {
    head -14 "$0" | tail -12 | sed 's/^# \{0,1\}//'
    exit 0
}

# Map port name to its index in PORTS array
port_index() {
    local name="$1"
    for (( i=0; i<PORT_COUNT; i++ )); do
        if [[ "${PORTS[$i]}" == "$name" ]]; then
            echo "$i"
            return
        fi
    done
    echo "-1"
}

# Map port name to CMake find_package name
port_to_cmake_pkg() {
    case "$1" in
        kcenon-common-system)  echo "common_system" ;;
        kcenon-thread-system)  echo "thread_system" ;;
        kcenon-network-system) echo "network_system" ;;
    esac
}

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --vcpkg-root)  VCPKG_ROOT="$2"; shift 2 ;;
        --triplet)     TRIPLET="$2"; shift 2 ;;
        --skip-build)  SKIP_BUILD=true; shift ;;
        --clean)       CLEAN=true; shift ;;
        --json)        JSON_OUTPUT=true; shift ;;
        --help|-h)     usage ;;
        *)             echo "Unknown option: $1"; usage ;;
    esac
done

detect_vcpkg() {
    if [[ -n "$VCPKG_ROOT" ]]; then
        if [[ ! -x "${VCPKG_ROOT}/vcpkg" ]]; then
            echo "Error: vcpkg not found at ${VCPKG_ROOT}/vcpkg" >&2
            exit 1
        fi
        return
    fi

    local candidates=(
        "${PROJECT_ROOT}/../vcpkg"
        "${HOME}/vcpkg"
        "/usr/local/vcpkg"
    )

    for dir in "${candidates[@]}"; do
        if [[ -x "${dir}/vcpkg" ]]; then
            VCPKG_ROOT="$(cd "$dir" && pwd)"
            log_info "Auto-detected vcpkg at: ${VCPKG_ROOT}"
            return
        fi
    done

    if command -v vcpkg &>/dev/null; then
        VCPKG_ROOT="$(dirname "$(command -v vcpkg)")"
        log_info "Found vcpkg in PATH: ${VCPKG_ROOT}"
        return
    fi

    echo "Error: Cannot find vcpkg. Use --vcpkg-root to specify." >&2
    exit 1
}

detect_triplet() {
    if [[ -n "$TRIPLET" ]]; then
        return
    fi

    local arch os
    arch="$(uname -m)"
    os="$(uname -s)"

    case "${os}" in
        Darwin)
            case "${arch}" in
                arm64)  TRIPLET="arm64-osx" ;;
                x86_64) TRIPLET="x64-osx" ;;
            esac
            ;;
        Linux)   TRIPLET="x64-linux" ;;
        MINGW*|MSYS*|CYGWIN*) TRIPLET="x64-windows" ;;
    esac

    if [[ -z "$TRIPLET" ]]; then
        echo "Error: Cannot detect triplet for ${os}/${arch}. Use --triplet." >&2
        exit 1
    fi

    log_info "Auto-detected triplet: ${TRIPLET}"
}

check_prerequisites() {
    log_header "Checking Prerequisites"

    local missing=()
    command -v cmake &>/dev/null || missing+=("cmake")
    command -v git &>/dev/null   || missing+=("git")

    if ! command -v c++ &>/dev/null && ! command -v g++ &>/dev/null && ! command -v clang++ &>/dev/null; then
        missing+=("C++ compiler")
    fi

    if [[ ${#missing[@]} -gt 0 ]]; then
        echo "Error: Missing prerequisites: ${missing[*]}" >&2
        exit 1
    fi

    log_ok "All prerequisites found"
    log_info "vcpkg: ${VCPKG_ROOT}/vcpkg"
    log_info "triplet: ${TRIPLET}"
    log_info "cmake: $(cmake --version | head -1)"
}

setup_validation_dir() {
    log_header "Setting Up Validation Environment"

    if [[ "$CLEAN" == true ]] && [[ -d "$BUILD_DIR" ]]; then
        log_info "Cleaning previous build artifacts..."
        rm -rf "$BUILD_DIR"
    fi

    mkdir -p "$BUILD_DIR"

    # Manifest listing network_system (transitive deps resolved automatically)
    cat > "${BUILD_DIR}/vcpkg.json" << 'MANIFEST'
{
  "name": "vcpkg-chain-validation",
  "version-string": "0.0.1",
  "description": "End-to-end validation of kcenon-network-system vcpkg build chain",
  "dependencies": [
    "kcenon-network-system"
  ]
}
MANIFEST

    # Copy registry configuration from project root
    cp "${PROJECT_ROOT}/vcpkg-configuration.json" "${BUILD_DIR}/vcpkg-configuration.json"

    log_ok "Validation directory ready: ${BUILD_DIR}"
}

install_ports() {
    log_header "Installing Ports (Manifest Mode)"
    log_info "Installing kcenon-network-system and transitive dependencies..."

    local install_log="${BUILD_DIR}/vcpkg_install.log"
    local install_ok=true
    local rc=0

    if [[ "$JSON_OUTPUT" == true ]]; then
        "${VCPKG_ROOT}/vcpkg" install \
            --triplet "${TRIPLET}" \
            --x-manifest-root="${BUILD_DIR}" \
            --x-install-root="${BUILD_DIR}/vcpkg_installed" \
            --overlay-ports="${PROJECT_ROOT}/vcpkg-ports" \
            > "$install_log" 2>&1 || rc=$?
    else
        "${VCPKG_ROOT}/vcpkg" install \
            --triplet "${TRIPLET}" \
            --x-manifest-root="${BUILD_DIR}" \
            --x-install-root="${BUILD_DIR}/vcpkg_installed" \
            --overlay-ports="${PROJECT_ROOT}/vcpkg-ports" \
            2>&1 | tee "$install_log" || rc=$?
    fi
    if [[ $rc -eq 0 ]]; then
        log_ok "vcpkg install completed successfully"
    else
        log_fail "vcpkg install failed. See ${install_log}"
        install_ok=false
    fi

    # Verify each port
    for (( i=0; i<PORT_COUNT; i++ )); do
        local port="${PORTS[$i]}"
        local status_file="${BUILD_DIR}/vcpkg_installed/vcpkg/status"
        local info_dir="${BUILD_DIR}/vcpkg_installed/vcpkg/info"

        if [[ -f "$status_file" ]] && grep -q "Package: ${port}" "$status_file"; then
            INSTALL_STATUS[$i]="pass"
            log_ok "  ${port}: installed"
        elif [[ -d "$info_dir" ]] && ls "${info_dir}/${port}"_* &>/dev/null 2>&1; then
            INSTALL_STATUS[$i]="pass"
            log_ok "  ${port}: installed"
        elif [[ "$install_ok" == false ]] && grep -q "Error:.*${port}" "$install_log" 2>/dev/null; then
            INSTALL_STATUS[$i]="fail"
            log_fail "  ${port}: build error"
        else
            INSTALL_STATUS[$i]="unknown"
            log_warn "  ${port}: status unknown"
        fi
    done
}

build_consumer() {
    if [[ "$SKIP_BUILD" == true ]]; then
        log_info "Skipping test consumer build (--skip-build)"
        CONSUMER_BUILD_RESULT="skipped"
        return
    fi

    log_header "Building Test Consumer"

    local consumer_build="${BUILD_DIR}/consumer_build"
    mkdir -p "$consumer_build"

    local cmake_log="${BUILD_DIR}/cmake_configure.log"
    local build_log="${BUILD_DIR}/cmake_build.log"

    log_info "Configuring test consumer..."
    rc=0
    if [[ "$JSON_OUTPUT" == true ]]; then
        cmake \
            -S "${CONSUMER_DIR}" \
            -B "${consumer_build}" \
            -DCMAKE_TOOLCHAIN_FILE="${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" \
            -DVCPKG_MANIFEST_MODE=OFF \
            -DVCPKG_INSTALLED_DIR="${BUILD_DIR}/vcpkg_installed" \
            -DVCPKG_TARGET_TRIPLET="${TRIPLET}" \
            -DCMAKE_CXX_STANDARD=20 \
            > "$cmake_log" 2>&1 || rc=$?
    else
        cmake \
            -S "${CONSUMER_DIR}" \
            -B "${consumer_build}" \
            -DCMAKE_TOOLCHAIN_FILE="${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" \
            -DVCPKG_MANIFEST_MODE=OFF \
            -DVCPKG_INSTALLED_DIR="${BUILD_DIR}/vcpkg_installed" \
            -DVCPKG_TARGET_TRIPLET="${TRIPLET}" \
            -DCMAKE_CXX_STANDARD=20 \
            2>&1 | tee "$cmake_log" || rc=$?
    fi
    if [[ $rc -eq 0 ]]; then
        log_ok "CMake configure succeeded"

        # Check find_package results
        for (( i=0; i<PORT_COUNT; i++ )); do
            local port="${PORTS[$i]}"
            local pkg_name
            pkg_name="$(port_to_cmake_pkg "$port")"

            if grep -q "Found ${pkg_name}" "$cmake_log" 2>/dev/null || \
               grep -q "${pkg_name}_FOUND" "$cmake_log" 2>/dev/null; then
                FIND_STATUS[$i]="pass"
                log_ok "  find_package(${pkg_name}): found"
            elif grep -qi "not find.*${pkg_name}\|Could NOT find ${pkg_name}" "$cmake_log" 2>/dev/null; then
                FIND_STATUS[$i]="fail"
                log_fail "  find_package(${pkg_name}): NOT found"
            else
                FIND_STATUS[$i]="unknown"
                log_warn "  find_package(${pkg_name}): status unclear"
            fi
        done
    else
        log_fail "CMake configure failed. See ${cmake_log}"
        CONSUMER_BUILD_RESULT="configure_failed"
        return
    fi

    log_info "Building test consumer..."
    rc=0
    if [[ "$JSON_OUTPUT" == true ]]; then
        cmake --build "${consumer_build}" > "$build_log" 2>&1 || rc=$?
    else
        cmake --build "${consumer_build}" 2>&1 | tee "$build_log" || rc=$?
    fi
    if [[ $rc -eq 0 ]]; then
        log_ok "Test consumer build succeeded"
        CONSUMER_BUILD_RESULT="pass"
    else
        log_fail "Test consumer build failed. See ${build_log}"
        CONSUMER_BUILD_RESULT="build_failed"
    fi
}

report_results() {
    if [[ "$JSON_OUTPUT" == true ]]; then
        report_json
        return
    fi

    log_header "Validation Results"

    echo ""
    echo "Platform: $(uname -s) $(uname -m)"
    echo "Triplet:  ${TRIPLET}"
    echo "vcpkg:    ${VCPKG_ROOT}"
    echo ""

    printf "%-30s %-12s %-12s\n" "Port" "Install" "find_package"
    printf "%-30s %-12s %-12s\n" "------------------------------" "------------" "------------"

    local all_pass=true
    for (( i=0; i<PORT_COUNT; i++ )); do
        local port="${PORTS[$i]}"
        local ist="${INSTALL_STATUS[$i]}"
        local fst="${FIND_STATUS[$i]}"

        local id fd
        case "$ist" in
            pass) id="${GREEN}PASS${NC}" ;;
            fail) id="${RED}FAIL${NC}"; all_pass=false ;;
            *)    id="${YELLOW}UNKNOWN${NC}"; all_pass=false ;;
        esac
        case "$fst" in
            pass)    fd="${GREEN}PASS${NC}" ;;
            fail)    fd="${RED}FAIL${NC}"; all_pass=false ;;
            skipped) fd="-" ;;
            *)       fd="${YELLOW}UNKNOWN${NC}" ;;
        esac

        printf "%-30s %-24b %-24b\n" "$port" "$id" "$fd"
    done

    echo ""
    case "$CONSUMER_BUILD_RESULT" in
        pass)             echo -e "Consumer Build: ${GREEN}PASS${NC}" ;;
        skipped)          echo -e "Consumer Build: ${YELLOW}SKIPPED${NC}" ;;
        configure_failed) echo -e "Consumer Build: ${RED}CONFIGURE FAILED${NC}"; all_pass=false ;;
        build_failed)     echo -e "Consumer Build: ${RED}BUILD FAILED${NC}"; all_pass=false ;;
    esac

    echo ""
    echo "Logs: ${BUILD_DIR}/"

    if [[ "$all_pass" == true ]] && [[ "$CONSUMER_BUILD_RESULT" != "skipped" ]]; then
        echo ""
        echo -e "${GREEN}All validations passed for ${TRIPLET}${NC}"
        return 0
    elif [[ "$all_pass" == true ]]; then
        echo ""
        echo -e "${YELLOW}Install passed; consumer build was skipped${NC}"
        return 0
    else
        echo ""
        echo -e "${RED}Some validations failed. Check logs for details.${NC}"
        return 1
    fi
}

report_json() {
    echo "{"
    echo "  \"platform\": \"$(uname -s) $(uname -m)\","
    echo "  \"triplet\": \"${TRIPLET}\","
    echo "  \"vcpkg_root\": \"${VCPKG_ROOT}\","

    # install_results
    echo "  \"install_results\": {"
    for (( i=0; i<PORT_COUNT; i++ )); do
        local comma=""
        if (( i < PORT_COUNT - 1 )); then comma=","; fi
        echo "    \"${PORTS[$i]}\": \"${INSTALL_STATUS[$i]}\"${comma}"
    done
    echo "  },"

    # find_package_results
    echo "  \"find_package_results\": {"
    for (( i=0; i<PORT_COUNT; i++ )); do
        local comma=""
        if (( i < PORT_COUNT - 1 )); then comma=","; fi
        echo "    \"${PORTS[$i]}\": \"${FIND_STATUS[$i]}\"${comma}"
    done
    echo "  },"

    echo "  \"consumer_build\": \"${CONSUMER_BUILD_RESULT}\","
    echo "  \"log_dir\": \"${BUILD_DIR}\""
    echo "}"
}

main() {
    detect_vcpkg
    detect_triplet
    check_prerequisites
    setup_validation_dir
    install_ports
    build_consumer
    report_results
}

main
