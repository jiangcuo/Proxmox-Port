#!/bin/bash

HARBOR="harbor.lierfang.com/pxvirt"
SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
PUSH=1
date=$(date +%Y%m%d)

TRIXIE_ARCHS="loong64 arm64 amd64 riscv64"
BOOKWORM_ARCHS="loong64 arm64 amd64"

usage() {
    echo "Usage: $0 [--no-push] <trixie|bookworm|all> [arch]"
    echo ""
    echo "Examples:"
    echo "  $0 trixie              # build + push trixie all archs"
    echo "  $0 trixie arm64        # build + push trixie arm64 only"
    echo "  $0 --no-push trixie    # build trixie all archs, no push"
    echo "  $0 all                 # build + push everything"
    exit 1
}

if [ "$1" = "--no-push" ]; then
    PUSH=0
    shift
fi

SUITE=${1:-"all"}
ARCH=${2:-""}

get_archs() {
    local suite=$1
    case "${suite}" in
        trixie)   echo "${TRIXIE_ARCHS}" ;;
        bookworm) echo "${BOOKWORM_ARCHS}" ;;
    esac
}

build_suite() {
    local suite=$1
    shift
    local archs="$@"
    echo "========== Building ${suite}: ${archs} =========="

    for arch in $archs; do
        local harbor_image="${HARBOR}/pxvirt-builder-${suite}:${arch}-${date}"
        (
            echo "[${suite}/${arch}] Building..."
            docker buildx build \
                --platform "linux/${arch}" \
                --build-arg SUITE="${suite}" \
                -f "${SCRIPT_DIR}/Dockerfile" \
                -t "${harbor_image}" \
                --load \
                "${SCRIPT_DIR}"

            if [ "${PUSH}" -eq 1 ]; then
                echo "[${suite}/${arch}] Pushing..."
                docker push "${harbor_image}"
            fi
            echo "[${suite}/${arch}] done"
        ) &
    done

    wait
    echo "========== ${suite} build finished =========="

    if [ "${PUSH}" -eq 1 ]; then
        local harbor_images=""
        for arch in $archs; do
            harbor_images="${harbor_images} ${HARBOR}/pxvirt-builder-${suite}:${arch}-${date}"
        done

        echo "Creating harbor manifest: ${HARBOR}/pxvirt-builder-${suite}:${date}"
        docker manifest rm "${HARBOR}/pxvirt-builder-${suite}:${date}" 2>/dev/null || true
        docker manifest create "${HARBOR}/pxvirt-builder-${suite}:${date}" ${harbor_images}
        docker manifest push "${HARBOR}/pxvirt-builder-${suite}:${date}"

        echo "Creating harbor manifest: ${HARBOR}/pxvirt-builder-${suite}:latest"
        docker manifest rm "${HARBOR}/pxvirt-builder-${suite}:latest" 2>/dev/null || true
        docker manifest create "${HARBOR}/pxvirt-builder-${suite}:latest" ${harbor_images}
        docker manifest push "${HARBOR}/pxvirt-builder-${suite}:latest"
    fi
}

run_suite() {
    local suite=$1
    if [ -n "${ARCH}" ]; then
        build_suite "${suite}" "${ARCH}"
    else
        build_suite "${suite}" $(get_archs "${suite}")
    fi
}

case "${SUITE}" in
    trixie)
        run_suite trixie
        ;;
    bookworm)
        run_suite bookworm
        ;;
    all)
        run_suite trixie
        run_suite bookworm
        ;;
    *)
        usage
        ;;
esac

echo "Done"
