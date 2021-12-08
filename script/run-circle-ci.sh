#!/bin/bash

IP=$1
PORT=$2
PW=$3
JOB_COUNT=$4

if [[ -z "${IP}" ]]; then
  echo "no ip address specified."
  echo "invoke with 'run-circle-ci.sh <ip> <port> <pw> <job_count>'"
  exit
fi

if [[ -z "${PORT}" ]]; then
  echo "no file port address specified."
  echo "invoke with 'run-circle-ci.sh <ip> <port> <pw> <job_count>'"
  exit
fi

if [[ -z "${PW}" ]]; then
  echo "no scp password address specified."
  echo "invoke with 'run-circle-ci.sh <ip> <port> <pw> <job_count>'"
  exit
fi

if [[ -z "${JOB_COUNT}" ]]; then
  echo "no job count specified."
  echo "invoke with 'run-circle-ci.sh <ip> <port> <pw> <job_count>'"
  exit
fi

# pre-process the yml file, and change max processes from 2 to 7
circleci config process .circleci/config.yml > local-circle-ci.yml
sed -i "s/-j2/-j${JOB_COUNT}/g" local-circle-ci.yml

ALL_JOBS=(
    "build_ubuntu_bionic"
    "build_ubuntu_focal"
    "build_ubuntu_hirsute"
    "build_ubuntu_impish"
    "build_fedora_32"
    "build_fedora_33"
    "build_fedora_34"
    "build_fedora_35"
    "build_mint_uma"
    "build_debian_buster"
    "build_debian_bullseye"
)

BRANCH="clangen/local-circle-ci"
REPO="https://github.com/clangen/musikcube"

for JOB in ${ALL_JOBS[@]}; do
  circleci local execute \
      -c local-circle-ci.yml \
      -e CIRCLE_BRANCH=${BRANCH} \
      -e CIRCLE_REPOSITORY_URL=${REPO} \
      -e MUSIKCUBE_BUILD_HOST_IP=${IP} \
      -e MUSIKCUBE_BUILD_HOST_PORT=${PORT} \
      -e MUSIKCUBE_BUILD_HOST_PW=${PW} \
      -e MUSIKCUBE_BUILD_JOB_COUNT=${JOB_COUNT} \
      --job ${JOB}
done

