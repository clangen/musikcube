#!/bin/bash

IP=$1
PW=$2

if [[ -z "${IP}" ]]; then
  echo "no file ip address specified."
  exit
fi

if [[ -z "${PW}" ]]; then
  echo "no scp password address specified."
  exit
fi

# pre-process the yml file, and change max processes from 2 to 7
circleci config process .circleci/config.yml > local-circle-ci.yml
sed -i 's/-j2/-j7/g' local-circle-ci.yml

ALL_JOBS=(
    "build_ubuntu_bionic"
    "build_ubuntu_focal"
    "build_ubuntu_groovy"
    "build_fedora_33"
    "build_fedora_32"
    "build_fedora_31"
)

BRANCH="clangen/local-circle-ci"
REPO="https://github.com/clangen/musikcube"

for JOB in ${ALL_JOBS[@]}; do
  circleci local execute \
      -c .circleci/config.yml \
      -e CIRCLE_BRANCH=${BRANCH} \
      -e CIRCLE_REPOSITORY_URL=${REPO} \
      -e MUSIKCUBE_BUILD_HOST_IP=${IP} \
      -e MUSIKCUBE_BUILD_HOST_PW=${PW} \
      --job ${JOB}
done

