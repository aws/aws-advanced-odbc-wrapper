#!/bin/bash
# Automates the full macOS signing pipeline:
#   1. Package dylibs in dummy .app → upload to S3 → wait for signed output
#   2. Download signed dylibs → rebuild .pkg → upload to S3 → wait for signed output
#   3. Download signed .pkg → extract final artifact
#
# Usage:
#   ./scripts/macOS/sign_artifacts_mac.sh <version>
#
# Required environment variables:
#   MAC_SIGNER_S3_BUCKET        - S3 bucket name
#   MAC_SIGNER_PRESIGNED_PREFIX - prefix for unsigned uploads
#   MAC_SIGNER_SIGNED_PREFIX    - prefix for signed outputs 
#   MAC_SIGNER_DYLIBS_DIR       - subdirectory for dylib signing
#   MAC_SIGNER_PKG_DIR          - subdirectory for pkg signing
#
# Requires: AWS CLI configured with appropriate credentials, gnu-tar (gtar), jq

set -euo pipefail

VERSION="${1:?Usage: $0 <version>}"

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
BUILD_DRIVER_DIR="$PROJECT_ROOT/build/driver"
OUTPUT_DIR="$PROJECT_ROOT/build/signing"
PRODUCT_NAME="aws-advanced-odbc-wrapper"

# S3 config from environment
S3_BUCKET="${MAC_SIGNER_S3_BUCKET:?MAC_SIGNER_S3_BUCKET is required}"
PRESIGNED="${MAC_SIGNER_PRESIGNED_PREFIX:?MAC_SIGNER_PRESIGNED_PREFIX is required}"
SIGNED="${MAC_SIGNER_SIGNED_PREFIX:?MAC_SIGNER_SIGNED_PREFIX is required}"
DYLIBS_DIR="${MAC_SIGNER_DYLIBS_DIR:?MAC_SIGNER_DYLIBS_DIR is required}"
PKG_DIR="${MAC_SIGNER_PKG_DIR:?MAC_SIGNER_PKG_DIR is required}"

POLL_INTERVAL=5
POLL_TIMEOUT=300

mkdir -p "$OUTPUT_DIR"

# ── Helper functions ─────────────────────────────────────────────────────────

get_tar_cmd() {
    if ! command -v gtar &>/dev/null; then
        echo "ERROR: gtar not found. Install it with: brew install gnu-tar" >&2
        exit 1
    fi
    echo "gtar"
}

upload_to_s3() {
    # Uploads a file via s3api put-object and prints the VersionId.
    local src="$1"
    local bucket="$2"
    local key="$3"

    local response
    if ! response=$(aws s3api put-object \
        --bucket "$bucket" \
        --key "$key" \
        --body "$src" \
        --output json); then
        echo "ERROR: Failed to upload $src to s3://$bucket/$key" >&2
        exit 1
    fi

    local vid
    vid=$(echo "$response" | jq -r '.VersionId // empty')
    if [ -z "$vid" ]; then
        echo "WARNING: No VersionId returned — bucket versioning may be disabled" >&2
    fi
    echo "$vid"
}

wait_for_s3_object() {
    local bucket="$1"
    local key="$2"
    local dest="$3"
    local elapsed=0

    echo "==> Waiting for s3://$bucket/$key ..."
    while [ $elapsed -lt $POLL_TIMEOUT ]; do
        if aws s3api head-object --bucket "$bucket" --key "$key" &>/dev/null; then
            echo "    Found after ${elapsed}s. Downloading..."
            if ! aws s3 cp "s3://$bucket/$key" "$dest"; then
                echo "ERROR: Failed to download s3://$bucket/$key" >&2
                exit 1
            fi
            echo "    Downloaded to $dest"
            return 0
        fi
        sleep $POLL_INTERVAL
        elapsed=$((elapsed + POLL_INTERVAL))
        echo "    Waiting... (${elapsed}s / ${POLL_TIMEOUT}s)"
    done

    echo "ERROR: Timed out waiting for s3://$bucket/$key after ${POLL_TIMEOUT}s"
    exit 1
}

extract_archive() {
    # Extracts a tar.gz or zip archive
    local archive="$1"
    local dest="$2"

    local tar_cmd
    tar_cmd=$(get_tar_cmd)
    cd "$dest"

    # Item of the signer that we get back is of type zip in the outer layer.
    local file_type
    file_type=$(file -b "$archive")
    if echo "$file_type" | grep -qi "zip"; then
        unzip -o "$archive"
    else
        $tar_cmd -xzf "$archive"
    fi

    if [ -f "$dest/artifact.gz" ]; then
        $tar_cmd -xzf "$dest/artifact.gz"
        rm -f "$dest/artifact.gz"
    fi
}

package_dylibs() {
    # Packages dylibs in a dummy .app bundle for deep-signing.
    # Output: $OUTPUT_DIR/<product>-<ver>-dylibs.tar.gz
    local app_name="${PRODUCT_NAME}-${VERSION}"

    echo "==> Packaging dylibs in dummy .app bundle for signing..."
    echo "    Version: $VERSION"
    echo "    App name: ${app_name}.app"

    local info_plist="$PROJECT_ROOT/build/Info.plist"
    if [ ! -f "$info_plist" ]; then
        echo "ERROR: $info_plist not found. Run cmake first to generate it."
        exit 1
    fi

    local staging="$OUTPUT_DIR/staging-dylibs"
    rm -rf "$staging"
    mkdir -p "$staging/$app_name.app/Contents/MacOS"
    mkdir -p "$staging/$app_name.app/Contents/Frameworks"

    cp "$info_plist" "$staging/$app_name.app/Contents/"

    # Placeholder executable (required for valid .app bundle)
    printf '#!/bin/true\n' > "$staging/$app_name.app/Contents/MacOS/$app_name"
    chmod +x "$staging/$app_name.app/Contents/MacOS/$app_name"

    # Copy all dylibs into Frameworks
    cp "$BUILD_DRIVER_DIR"/*.dylib "$staging/$app_name.app/Contents/Frameworks/"
    local dylib_count
    dylib_count=$(ls "$staging/$app_name.app/Contents/Frameworks/"*.dylib | wc -l | tr -d ' ')
    echo "    Copied $dylib_count dylibs into Contents/Frameworks/"

    # Compress the dylibs in gz file.
    # Note: We need to double-zip this.
    local tar_cmd
    tar_cmd=$(get_tar_cmd)
    cd "$staging"
    $tar_cmd -czf "$OUTPUT_DIR/artifact.gz" "$app_name.app"
    cd "$OUTPUT_DIR"
    $tar_cmd -czf "$OUTPUT_DIR/${PRODUCT_NAME}-${VERSION}-dylibs.tar.gz" artifact.gz
    rm -f artifact.gz

    echo "==> Packaged: $OUTPUT_DIR/${PRODUCT_NAME}-${VERSION}-dylibs.tar.gz"
}

rebuild_pkg_with_signed_dylibs() {
    # Extracts signed dylibs, rebuilds .pkg via cpack,
    # Args: $1 = path to signed dylibs tar.gz
    # Output: $OUTPUT_DIR/<product>-<ver>-unsigned.tar.gz
    local signed_dylibs_tar="$1"
    local app_name="${PRODUCT_NAME}-${VERSION}"

    echo "==> Extracting signed dylibs from $signed_dylibs_tar..."
    local extract_dir="$OUTPUT_DIR/signed-extract"
    rm -rf "$extract_dir"
    mkdir -p "$extract_dir"

    extract_archive "$signed_dylibs_tar" "$extract_dir"

    local frameworks_dir="$extract_dir/$app_name.app/Contents/Frameworks"
    if [ ! -d "$frameworks_dir" ]; then
        echo "ERROR: Could not find $app_name.app/Contents/Frameworks/ in the archive"
        echo "    Contents of extract dir:"
        ls -R "$extract_dir"
        exit 1
    fi

    # Copy signed dylibs back to build/driver/
    local signed_count
    signed_count=$(ls "$frameworks_dir"/*.dylib | wc -l | tr -d ' ')
    echo "    Found $signed_count signed dylibs"
    cp "$frameworks_dir"/*.dylib "$BUILD_DRIVER_DIR/"
    echo "    Copied signed dylibs to $BUILD_DRIVER_DIR/"

    # Build .pkg with signed dylibs
    echo "==> Running cpack to build .pkg with signed dylibs..."
    cd "$PROJECT_ROOT/build"
    COPYFILE_DISABLE=1 cpack

    local pkg_file
    pkg_file=$(find "$PROJECT_ROOT/build" -maxdepth 1 -name "*.pkg" | head -1)
    if [ -z "$pkg_file" ]; then
        echo "ERROR: cpack did not produce a .pkg"
        exit 1
    fi
    echo "    Built: $(basename "$pkg_file")"

    # Package .pkg
    local staging="$OUTPUT_DIR/staging-pkg"
    rm -rf "$staging"
    mkdir -p "$staging/artifact"
    cp "$pkg_file" "$staging/artifact/${PRODUCT_NAME}-${VERSION}-macos.pkg"

    local tar_cmd
    tar_cmd=$(get_tar_cmd)
    cd "$staging"
    $tar_cmd -czf "$OUTPUT_DIR/artifact.gz" -C artifact .

    # Compress the package to gz file.
    # Note: We need to double-zip this.
    cd "$OUTPUT_DIR"
    $tar_cmd -czf "$OUTPUT_DIR/${PRODUCT_NAME}-${VERSION}-unsigned.tar.gz" artifact.gz
    rm -f artifact.gz

    echo "==> Packaged: $OUTPUT_DIR/${PRODUCT_NAME}-${VERSION}-unsigned.tar.gz"
}

# ── Phase 1: Sign dylibs via dummy .app ──────────────────────────────────────

echo ""
echo "════════════════════════════════════════════════════════════"
echo "  Phase 1: Signing dylibs via dummy .app bundle"
echo "════════════════════════════════════════════════════════════"

package_dylibs

DYLIBS_TAR="${PRODUCT_NAME}-${VERSION}-dylibs.tar.gz"

echo "==> Uploading dylibs package to S3..."
DYLIBS_VID=$(upload_to_s3 "$OUTPUT_DIR/$DYLIBS_TAR" "$S3_BUCKET" "$PRESIGNED/$DYLIBS_DIR/$DYLIBS_TAR")
echo "    Upload VersionId: $DYLIBS_VID"

# Lambda appends the version ID to the signed output key
DYLIBS_SIGNED_TAR="${PRODUCT_NAME}-${VERSION}-dylibs-signed-${DYLIBS_VID}.tar.gz"

wait_for_s3_object "$S3_BUCKET" "$SIGNED/$DYLIBS_DIR/$DYLIBS_SIGNED_TAR" \
    "$OUTPUT_DIR/$DYLIBS_SIGNED_TAR"

# ── Phase 2: Rebuild .pkg with signed dylibs, then sign the .pkg ─────────────

echo ""
echo "════════════════════════════════════════════════════════════"
echo "  Phase 2: Rebuilding .pkg and signing"
echo "════════════════════════════════════════════════════════════"

rebuild_pkg_with_signed_dylibs "$OUTPUT_DIR/$DYLIBS_SIGNED_TAR"

UNSIGNED_TAR="${PRODUCT_NAME}-${VERSION}-unsigned.tar.gz"

echo "==> Uploading pkg package to S3..."
PKG_VID=$(upload_to_s3 "$OUTPUT_DIR/$UNSIGNED_TAR" "$S3_BUCKET" "$PRESIGNED/$PKG_DIR/$UNSIGNED_TAR")
echo "    Upload VersionId: $PKG_VID"

# Lambda appends the version ID to the signed output key
SIGNED_TAR="${PRODUCT_NAME}-${VERSION}-signed-${PKG_VID}.tar.gz"

wait_for_s3_object "$S3_BUCKET" "$SIGNED/$PKG_DIR/$SIGNED_TAR" \
    "$OUTPUT_DIR/$SIGNED_TAR"

# ── Phase 3: Extract final signed .pkg ───────────────────────────────────────

echo ""
echo "════════════════════════════════════════════════════════════"
echo "  Phase 3: Extracting signed .pkg"
echo "════════════════════════════════════════════════════════════"

FINAL_DIR="$OUTPUT_DIR/final"
rm -rf "$FINAL_DIR"
mkdir -p "$FINAL_DIR"

extract_archive "$OUTPUT_DIR/$SIGNED_TAR" "$FINAL_DIR"

PKG_FILE=$(find "$FINAL_DIR" -name "*.pkg" | head -1)
if [ -z "$PKG_FILE" ]; then
    echo "ERROR: No .pkg found in signed output"
    exit 1
fi

# Final pkg keeps the standard name (no version ID suffix)
FINAL_PKG="$PROJECT_ROOT/build/${PRODUCT_NAME}-${VERSION}-macos-arm64.pkg"
cp "$PKG_FILE" "$FINAL_PKG"

echo ""
echo "════════════════════════════════════════════════════════════"
echo "  Signing complete!"
echo "  Signed .pkg: $FINAL_PKG"
echo "════════════════════════════════════════════════════════════"

echo ""
echo "==> Verifying .pkg signature..."
if pkgutil --check-signature "$FINAL_PKG"; then
    echo "==> Signature verification passed."
    exit 0
else
    echo "ERROR: Signature verification failed for $FINAL_PKG"
    exit 1
fi
