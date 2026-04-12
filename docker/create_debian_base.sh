#!/bin/bash
set -euo pipefail

HARBOR="harbor.lierfang.com/pxvirt"
date=$(date +%Y%m%d)
FAILED=0
LOG_DIR=$(mktemp -d /tmp/debian-base-build.XXXXXX)
STATUS_DIR="${LOG_DIR}/status"
mkdir -p "${STATUS_DIR}"

COLOR_RESET="\033[0m"
COLOR_GREEN="\033[32m"
COLOR_RED="\033[31m"
COLOR_YELLOW="\033[33m"
COLOR_CYAN="\033[36m"
COLOR_DIM="\033[2m"

set_status() {
    echo "$2" > "${STATUS_DIR}/$1"
}

build_arch() {
    local arch=$1
    local suite=$2
    local mirror=$3
    local tag=$4
    local label="${tag}-${arch}"
    local dir="./debian-${tag}-${arch}"
    local harbor_image="${HARBOR}/debian-base-${tag}:${arch}-${date}"
    local logfile="${LOG_DIR}/${tag}-${arch}.log"

    set_status "${label}" "debootstrap"
    rm -rf "${dir}"
    if [ "${arch}" == "loong64" ]; then
        debootstrap --arch=${arch} \
            --include=debian-ports-archive-keyring,usrmerge,perl \
            --exclude=exim4,exim4-base,usr-is-merged \
            --no-check-gpg "${suite}" "${dir}" "${mirror}" >> "${logfile}" 2>&1
        chroot "${dir}" apt install -y usr-is-merged >> "${logfile}" 2>&1
        echo "deb [trusted=yes check-valid-until=no] https://mirrors.lierfang.com/debian-ports/${tag} sid main" > ${dir}/etc/apt/sources.list
        echo 'APT { Get { AllowUnauthenticated "1"; }; };' > ${dir}/etc/apt/apt.conf.d/99allow_unauth
    else 
        debootstrap --arch=${arch} "${suite}" "${dir}" "${mirror}" >> "${logfile}" 2>&1
    fi

    set_status "${label}" "importing"
    tar -C "${dir}" -c . | docker import --platform "linux/${arch}" - "${harbor_image}" >> "${logfile}" 2>&1

    set_status "${label}" "pushing"
    docker push "${harbor_image}" >> "${logfile}" 2>&1

    set_status "${label}" "done"
}

show_progress() {
    local total=$1
    local elapsed=0
    local start_time=$(date +%s)

    while true; do
        local now=$(date +%s)
        elapsed=$(( now - start_time ))
        local mins=$(( elapsed / 60 ))
        local secs=$(( elapsed % 60 ))

        # Move cursor up and clear
        printf "\033[2J\033[H"
        printf "${COLOR_CYAN}━━━ debian-base build ━━━ [%02d:%02d]${COLOR_RESET}\n\n" "${mins}" "${secs}"

        local done_count=0
        local fail_count=0
        for f in "${STATUS_DIR}"/*; do
            [ -f "$f" ] || continue
            local name=$(basename "$f")
            local status=$(cat "$f")
            local color
            case "${status}" in
                done)        color="${COLOR_GREEN}"  ; done_count=$((done_count+1)) ;;
                failed)      color="${COLOR_RED}"    ; fail_count=$((fail_count+1)) ;;
                debootstrap) color="${COLOR_YELLOW}" ;;
                importing)   color="${COLOR_YELLOW}" ;;
                pushing)     color="${COLOR_YELLOW}" ;;
                *)           color="${COLOR_DIM}"    ;;
            esac
            printf "  ${color}%-22s %s${COLOR_RESET}\n" "${name}" "${status}"
            local logfile="${LOG_DIR}/${name}.log"
            if [ -f "${logfile}" ]; then
                tail -2 "${logfile}" 2>/dev/null | while IFS= read -r line; do
                    printf "    ${COLOR_DIM}%s${COLOR_RESET}\n" "${line:0:80}"
                done
            fi
            echo ""
        done

        printf "\n${COLOR_DIM}  Logs: ${LOG_DIR}/${COLOR_RESET}\n"

        # Exit when all tasks are done or failed
        if [ $(( done_count + fail_count )) -ge "${total}" ]; then
            break
        fi
        sleep 2
    done
}

push_manifest() {
    local manifest=$1
    shift
    local images="$@"

    echo "Creating multi-arch manifest: ${manifest}"
    docker manifest rm "${manifest}" 2>/dev/null || true
    docker manifest create "${manifest}" ${images}
    docker manifest push "${manifest}"
    echo "Multi-arch manifest pushed: ${manifest}"
}

create_manifests() {
    local tag=$1
    shift
    local archs="$@"

    # harbor: harbor.lierfang.com/pxvirt/debian-base-<tag>:<arch>-<date> -> harbor.lierfang.com/pxvirt/debian-base-<tag>:latest
    local harbor_images=""
    for arch in $archs; do
        harbor_images="${harbor_images} ${HARBOR}/debian-base-${tag}:${arch}-${date}"
    done
    push_manifest "${HARBOR}/debian-base-${tag}:${date}" ${harbor_images}
    push_manifest "${HARBOR}/debian-base-${tag}:latest" ${harbor_images}
}

PIDS=()
LABELS=()
MONITOR_PID=""

cleanup() {
    echo ""
    echo -e "${COLOR_RED}Interrupted, killing all tasks...${COLOR_RESET}"
    for pid in "${PIDS[@]}"; do
        kill "${pid}" 2>/dev/null || true
    done
    [ -n "${MONITOR_PID}" ] && kill "${MONITOR_PID}" 2>/dev/null || true
    wait 2>/dev/null || true
    rm -rf "${LOG_DIR}"
    exit 130
}
trap cleanup INT TERM

start_build() {
    build_arch "$@" || set_status "$4-$1" "failed" &
    PIDS+=($!)
    LABELS+=("$4-$1")
}

start_build loong64  sid      https://mirrors.lierfang.com/debian-ports/trixie   trixie
start_build arm64    trixie   https://mirrors.ustc.edu.cn/debian                 trixie
start_build amd64    trixie   https://mirrors.ustc.edu.cn/debian                 trixie
start_build riscv64  trixie   https://mirrors.ustc.edu.cn/debian                 trixie

start_build loong64  sid      https://mirrors.lierfang.com/debian-ports/bookworm bookworm
start_build arm64    bookworm https://mirrors.ustc.edu.cn/debian                 bookworm
start_build amd64    bookworm https://mirrors.ustc.edu.cn/debian                 bookworm

show_progress ${#PIDS[@]} &
MONITOR_PID=$!

for i in "${!PIDS[@]}"; do
    if ! wait "${PIDS[$i]}"; then
        set_status "${LABELS[$i]}" "failed"
        FAILED=1
    fi
done

# Let monitor finish its final refresh
sleep 3
kill "${MONITOR_PID}" 2>/dev/null || true
wait "${MONITOR_PID}" 2>/dev/null || true

echo ""
if [ "${FAILED}" -eq 1 ]; then
    echo -e "${COLOR_RED}ERROR: Some builds failed, skipping manifest creation${COLOR_RESET}"
    echo "Check logs in: ${LOG_DIR}/"
    exit 1
fi

echo -e "${COLOR_GREEN}All architectures built and pushed successfully${COLOR_RESET}"

create_manifests trixie   loong64 arm64 amd64 riscv64
create_manifests bookworm loong64 arm64 amd64

rm -rf "${LOG_DIR}"
