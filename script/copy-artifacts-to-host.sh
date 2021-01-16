#!/bin/bash

if [[ -z "${MUSIKCUBE_BUILD_HOST_IP}" ]]; then
    exit
fi

if [[ -z "${MUSIKCUBE_BUILD_HOST_PW}" ]]; then
    exit
fi

sshpass -p "${MUSIKCUBE_BUILD_HOST_PW}" scp /root/rpms/* build@${MUSIKCUBE_BUILD_HOST_IP}:/home/build/ 2> /dev/null
sshpass -p "${MUSIKCUBE_BUILD_HOST_PW}" scp /root/debs/* build@${MUSIKCUBE_BUILD_HOST_IP}:/home/build/ 2> /dev/null
