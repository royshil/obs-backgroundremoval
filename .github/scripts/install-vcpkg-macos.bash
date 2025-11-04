#!/bin/bash

# Exit the script if a command fails, an undefined variable is used,
# or a command in a pipe fails.
set -euo pipefail

#================================================================
# Configuration
#================================================================

# Project root directory (parent directory of this script)
readonly CMAKE_SOURCE_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." &> /dev/null && pwd)

# vcpkg root directory (uses VCPKG_ROOT env var, falling back to VCPG_INSTALL_ROOT)
readonly VCPKG_ROOT="${VCPKG_ROOT:-$VCPKG_INSTALLATION_ROOT}"

# Names of the triplets for each architecture
readonly TRIPLET_ARM64="arm64-osx-obs"
readonly TRIPLET_X64="x64-osx-obs"

# Root directory where vcpkg will install the build artifacts
readonly VCPKG_BUILD_OUTPUT_DIR="${CMAKE_SOURCE_DIR}/vcpkg_installed"

# Output directory for the final universal libraries and binaries
readonly DIR_UNIVERSAL="${VCPKG_BUILD_OUTPUT_DIR}/universal-osx-obs"

#================================================================
# Logging Helpers
#================================================================

# Constants for colored output
readonly COLOR_BLUE='\033[1;34m'
readonly COLOR_GREEN='\033[1;32m'
readonly COLOR_YELLOW='\033[1;33m'
readonly COLOR_NONE='\033[0m'

log_info() {
    echo -e "${COLOR_BLUE}==> ${1}${COLOR_NONE}"
}

log_success() {
    echo -e "${COLOR_GREEN}--> ${1}${COLOR_NONE}"
}

log_warn() {
    echo -e "${COLOR_YELLOW}!!> ${1}${COLOR_NONE}"
}

#================================================================
# Script Functions
#================================================================

# A robust function to process a directory:
# - Combines *.a files using lipo.
# - Copies all other file types.
# - Preserves subdirectory structure.
# Arg1: The subdirectory name relative to the install root (e.g., "lib", "debug/lib", "tools")
process_directory() {
    local subdir="${1}"
    log_info "Processing directory: '${subdir}'..."

    local dir_arm64="${VCPKG_BUILD_OUTPUT_DIR}/${TRIPLET_ARM64}/${TRIPLET_ARM64}/${subdir}"
    local dir_x64="${VCPKG_BUILD_OUTPUT_DIR}/${TRIPLET_X64}/${TRIPLET_X64}/${subdir}"
    local dir_universal_out="${DIR_UNIVERSAL}/${subdir}"

    if [ ! -d "${dir_arm64}" ]; then
        log_warn "Source directory '${dir_arm64}' not found, skipping."
        return
    fi

    # Find all files in the subdirectory, using the arm64 directory as the reference
    find "${dir_arm64}" -type f | while read -r file_arm64; do
        # Get the relative path from the subdirectory base (e.g., "pkgconfig/libfoo.pc")
        local relative_path="${file_arm64#${dir_arm64}/}"
        
        # Construct the path for the x64 and universal files
        local file_x64="${dir_x64}/${relative_path}"
        local file_universal="${dir_universal_out}/${relative_path}"
        
        # Ensure the destination subdirectory exists
        mkdir -p "$(dirname "${file_universal}")"

        # Check if the file is a static library (*.a)
        if [[ "${file_arm64}" == *.a ]]; then
            # This is a static library, try to combine it with lipo
            if [ -f "${file_x64}" ]; then
                log_success "Combining: ${relative_path}"
                lipo -create -output "${file_universal}" "${file_arm64}" "${file_x64}"
            else
                log_warn "Matching .a file for '${relative_path}' not found in x64 dir. Copying arm64 version."
                cp -a "${file_arm64}" "${file_universal}"
            fi
        else
            # Not a static library, so just copy it from the arm64 source
            log_success "Copying:   ${relative_path}"
            cp -a "${file_arm64}" "${file_universal}"
        fi
    done
}

#================================================================
# Main Logic
#================================================================

main() {
    if [ ! -f "${VCPKG_ROOT}/vcpkg" ]; then
        echo "Error: vcpkg not found at '${VCPKG_ROOT}'." >&2
        echo "Please set the VCPKG_ROOT environment variable or adjust the script." >&2
        exit 1
    fi

    log_info "Starting vcpkg builds for required architectures..."

    "${VCPKG_ROOT}/vcpkg" install --triplet "${TRIPLET_ARM64}" --x-install-root="${VCPKG_BUILD_OUTPUT_DIR}/${TRIPLET_ARM64}"
    sleep 1
    "${VCPKG_ROOT}/vcpkg" install --triplet "${TRIPLET_X64}" --x-install-root="${VCPKG_BUILD_OUTPUT_DIR}/${TRIPLET_X64}"
    log_success "vcpkg builds completed."

    log_info "Setting up universal directory structure at ${DIR_UNIVERSAL}..."
    rm -rf "${DIR_UNIVERSAL}"
    mkdir -p "${DIR_UNIVERSAL}"

    local arm64_source_dir="${VCPKG_BUILD_OUTPUT_DIR}/${TRIPLET_ARM64}/${TRIPLET_ARM64}"
    
    log_info "Copying top-level architecture-independent directories..."
    # cp -a preserves permissions, links, and timestamps, similar to rsync -a
    # The '/.' at the end of the source path copies the contents of the directory
    cp -a "${arm64_source_dir}/include/." "${DIR_UNIVERSAL}/include/"
    if [ -d "${arm64_source_dir}/share" ]; then
        cp -a "${arm64_source_dir}/share/." "${DIR_UNIVERSAL}/share/"
    fi
    log_success "Directories copied."

    # Process directories containing binaries and libraries
    process_directory "lib"
    process_directory "debug/lib"
    process_directory "tools"

    log_success "All universal files have been created successfully in ${DIR_UNIVERSAL}"
}

# Start script execution
main "$@"
