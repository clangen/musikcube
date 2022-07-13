#!/bin/bash

# Format and update translations from a source file (en_US.json by default)
#
# Requires jq (https://stedolan.github.io/jq/) and sponge
# (https://www.putorius.net/linux-sponge-soak-up-standard-input-and-write-to-a-file.html)

set -euo pipefail

SOURCE=en_US.json
PREFIX=_TSTR_

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
LOCALES_DIR="${SCRIPT_DIR}/../src/musikcube/data/locales"
TEMPFILE=$(mktemp -p ${TMPDIR:-/tmp} $(basename "${0}").XXXXX)

trap cleanup 0 1 2 3 5

cleanup() {
  rm -f "${TEMPFILE}"
}

# Sort $SOURCE translations file alphabetically by keys.
jq -S . "${LOCALES_DIR}/${SOURCE}" | sponge "${LOCALES_DIR}/${SOURCE}"

# Generate a temporary translations file prefixing all values with ${PREFIX}
# to indicate missing translations.
awk -F'": ' -v prefix="${PREFIX}" \
  '/: "/{sub("\"", "\"" prefix, $2);print $1 "\": " $2;next} {print}' \
  "${LOCALES_DIR}/${SOURCE}" \
  > "${TEMPFILE}" \
  # awk > $TEMPFILE

# Merge language specific translations fle with temporary translations file,
# adding any missing translations from ${SOURCE}, prefixed with ${PREFIX}.
for JSON in $(git ls-files "${LOCALES_DIR}" | grep -v "${SOURCE}"); do
  jq -Ss '.[0] * .[1]' "${TEMPFILE}" "${JSON}" \
    | sponge "${JSON}" \
    # jq | sponge
done
