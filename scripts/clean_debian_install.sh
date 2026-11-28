#!/usr/bin/env bash
# Runs only inside a fresh Ubuntu 26.04 container, never on a developer host.
set -euo pipefail
[[ -f /.dockerenv && $(id -u) == 0 ]] || { echo 'Expected disposable root container' >&2; exit 1; }
. /etc/os-release
[[ "$ID" == ubuntu && "$VERSION_ID" == 26.04 ]]
export DEBIAN_FRONTEND=noninteractive
name=kairo-signal-vault
version=0.1.0
package=/input/SignalVault.deb
[[ $(sha256sum "$package" | cut -d' ' -f1) == "$1" ]]
[[ $(dpkg-deb -f "$package" Package) == "$name" ]]
[[ $(dpkg-deb -f "$package" Version) == "$version" ]]
[[ $(dpkg-deb -f "$package" Architecture) == amd64 ]]
! dpkg-query -W "$name" >/dev/null 2>&1
apt-get update
apt-get install -y --no-install-recommends "$package"
[[ $(dpkg-query -W -f='${Status}' "$name") == 'install ok installed' ]]
base=/usr/lib/kairo/$name
[[ $(sha256sum "$base/project/KairoNotices/notice-index.json" | cut -d' ' -f1) == "$2" ]]
[[ $(sha256sum "$base/project/ThirdPartySourceNotices/source-notices.json" | cut -d' ' -f1) == "$3" ]]
# Hash the root-owned installation before using it as a normal user.
(cd "$base" && find . -type f -print0 | sort -z | xargs -0 sha256sum) >/tmp/kairo-install-before.sha256
useradd -m -s /bin/sh kairo-test
home=/home/kairo-test
export XDG_DATA_HOME="$home/data"
as_user() { runuser -u kairo-test -- env HOME="$home" XDG_DATA_HOME="$XDG_DATA_HOME" "$@"; }
launcher=/usr/bin/$name
as_user "$launcher" --validate
project="$XDG_DATA_HOME/kairo/$name/$version/project"
[[ -f "$project/Project.kproject" ]]
as_user sh -c 'printf retain > "$1/.clean-host-fixture"' sh "$project"
# Graphics drivers and virtual display are host prerequisites, installed separately.
apt-get install -y --no-install-recommends xvfb xauth mesa-vulkan-drivers
as_user xvfb-run -a "$launcher" --renderer vulkan --smoke
as_user "$launcher" --validate
[[ $(cat "$project/.clean-host-fixture") == retain ]]
[[ ! -e "$base/project/.clean-host-fixture" ]]
(cd "$base" && sha256sum -c /tmp/kairo-install-before.sha256)
(cd "$base" && find . -type f -print0 | sort -z | xargs -0 sha256sum) >/tmp/kairo-install-after.sha256
cmp /tmp/kairo-install-before.sha256 /tmp/kairo-install-after.sha256
apt-get remove -y "$name"
[[ ! -e "$launcher" && -f "$project/.clean-host-fixture" ]]
printf 'Clean Ubuntu %s APT install, unprivileged validation, Vulkan smoke, retention and uninstall passed\n' "$VERSION_ID"
