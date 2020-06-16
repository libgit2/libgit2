#!/bin/bash

set -e

DOCKERFILE_PATH=$1

if [ "${DOCKERFILE_PATH}" = "" ]; then
	echo "usage: $0 dockerfile"
	exit 1
fi

if [ "${DOCKER_REGISTRY}" = "" ]; then
	echo "DOCKER_REGISTRY environment variable is unset."
	echo "Not running inside GitHub Actions or misconfigured?"
	exit 1
fi

DOCKER_CONTAINER="${GITHUB_REPOSITORY}/$(basename ${DOCKERFILE_PATH})"
DOCKER_REGISTRY_CONTAINER="${DOCKER_REGISTRY}/${DOCKER_CONTAINER}"

echo "::set-env name=docker-container::${DOCKER_CONTAINER}"
echo "::set-env name=docker-registry-container::${DOCKER_REGISTRY_CONTAINER}"

# Identify the last git commit that touched the Dockerfiles
# Use this as a hash to identify the resulting docker containers
DOCKER_SHA=$(git log -1 --pretty=format:"%h" -- "${DOCKERFILE_PATH}")
echo "::set-env name=docker-sha::${DOCKER_SHA}"

DOCKER_REGISTRY_CONTAINER_SHA="${DOCKER_REGISTRY_CONTAINER}:${DOCKER_SHA}"

echo "::set-env name=docker-registry-container-sha::${DOCKER_REGISTRY_CONTAINER_SHA}"
echo "::set-env name=docker-registry-container-latest::${DOCKER_REGISTRY_CONTAINER}:latest"

exists="true"
docker login https://${DOCKER_REGISTRY} -u ${GITHUB_ACTOR} -p ${GITHUB_TOKEN} || exists="false"

if [ "${exists}" != "false" ]; then
	docker pull ${DOCKER_REGISTRY_CONTAINER_SHA} || exists="false"
fi

if [ "${exists}" = "true" ]; then
	echo "::set-env name=docker-container-exists::true"
else
	echo "::set-env name=docker-container-exists::false"
fi
