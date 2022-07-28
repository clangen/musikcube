#!/bin/bash

# Format and update translations from a source file (en_US.json by default).
# Requires jq (https://stedolan.github.io/jq/)

set -euo pipefail

SOURCE=en_US.json
PREFIX=_TSTR_

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
LOCALES_DIR="${SCRIPT_DIR}/../src/musikcube/data/locales"
TEMPFILE=$(mktemp -p ${TMPDIR:-/tmp} $(basename "${0}").XXXXX)
PREFIXDFILE=$(mktemp -p ${TMPDIR:-/tmp} $(basename "${0}").XXXXX)

trap cleanup 0 1 2 3 5

cleanup() {
  rm -f "${PREFIXDFILE}" "${TEMPFILE}"
}

# Sort $SOURCE translations file alphabetically by keys.
jq -S . "${LOCALES_DIR}/${SOURCE}" > "${TEMPFILE}"
mv "${TEMPFILE}" "${LOCALES_DIR}/${SOURCE}"

# Generate a temporary translations file prefixing all values with ${PREFIX}
# to indicate missing translations.
awk -F'": ' -v prefix="${PREFIX}" \
  '/: "/{sub("\"", "\"" prefix, $2);print $1 "\": " $2;next} {print}' \
  "${LOCALES_DIR}/${SOURCE}" \
  > "${PREFIXDFILE}" \
  # awk > $PREFIXDFILE

# Merge language specific translations fle with temporary translations file,
# adding any missing translations from ${SOURCE}, prefixed with ${PREFIX}.
for JSON in $(git ls-files "${LOCALES_DIR}" | grep -v "${SOURCE}"); do
  jq -Ss '.[0] * .[1]' "${PREFIXDFILE}" "${JSON}" > "${TEMPFILE}"
  mv "${TEMPFILE}" "${JSON}"
done
