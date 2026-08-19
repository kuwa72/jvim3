#!/bin/sh
#
# Build and test JVim on a BSD, from a Linux machine, using Docker.
#
#   scripts/test-bsd-docker.sh                  FreeBSD: build and run the tests
#   scripts/test-bsd-docker.sh netbsd           the same on NetBSD
#   scripts/test-bsd-docker.sh all              both, one after the other
#   scripts/test-bsd-docker.sh freebsd build    build only
#   scripts/test-bsd-docker.sh netbsd shell     leave the guest up for poking at
#   scripts/test-bsd-docker.sh freebsd clean    throw the cached guest disk away
#
# Docker cannot run a FreeBSD or NetBSD container: containers share the host
# kernel, and a BSD binary needs a BSD kernel. So the container here is only a
# place to keep QEMU and its tools; the BSD in it is a real virtual machine,
# booted from the project's own image and accelerated with KVM.
#
# Needs: docker, /dev/kvm, about 12 GB of disk, and network on the first run.
#
# The first run of each system installs it far enough to be usable over ssh and
# keeps the result as prepared-<os>.qcow2; later runs overlay that and are up in
# under a minute.
#
# The work directory has to be somewhere the Docker daemon can see. A session
# private /tmp is not, which is why this defaults to ~/.cache.

set -eu

root=$(cd "$(dirname "$0")/.." && pwd)
work=${JVIM_BSD_WORK:-$HOME/.cache/jvim-bsd}
runner=jvim-bsdrunner

os=freebsd
target=test
for a in "$@"; do
	case $a in
	freebsd|netbsd|all)		os=$a ;;
	test|build|shell|clean)	target=$a ;;
	*) echo "usage: $0 [freebsd|netbsd|all] [test|build|shell|clean]" >&2; exit 2 ;;
	esac
done

if [ "$os" = all ]; then
	rc=0
	sh "$0" freebsd "$target" || rc=$?
	sh "$0" netbsd "$target" || rc=$?
	exit $rc
fi

container=jvim-$os
prepared=prepared-$os.qcow2

case $os in
freebsd)
	release=${JVIM_FREEBSD_RELEASE:-14.3-RELEASE}
	image=FreeBSD-$release-amd64-BASIC-CLOUDINIT-ufs.qcow2
	url=https://download.freebsd.org/releases/VM-IMAGES/$release/amd64/Latest/$image.xz
	guest_user=freebsd
	# The build and the tests run with /usr/local/bin on the path anyway.
	guest_path=/usr/local/bin
	disk_size=20G
	;;
netbsd)
	release=${JVIM_NETBSD_RELEASE:-10.1}
	image=NetBSD-$release-amd64-live.img
	url=https://cdn.netbsd.org/pub/NetBSD/images/$release/$image.gz
	# The live image has no serial console and no way to be configured from
	# outside, so the install CD, which does talk to a serial line, is booted
	# once to set it up. boot-com.iso is that CD.
	iso=boot-com.iso
	iso_url=https://cdn.netbsd.org/pub/NetBSD/NetBSD-$release/amd64/installation/cdrom/$iso
	guest_user=root
	# pkgsrc puts bash in /usr/pkg/bin, which is not on root's default path.
	guest_path=/usr/pkg/bin
	# The live image's disklabel is the size of the image; see prepare_netbsd.
	disk_size=
	;;
esac

say() { printf '\n=== %s\n' "$1"; }
dexec() { docker exec "$container" "$@"; }
guest() { docker exec "$container" sh /work/gssh.sh "$@"; }

if [ "$target" = clean ]; then
	docker rm -f "$container" 2>/dev/null || true
	rm -f "$work/$prepared" "$work/run-$os.qcow2" "$work/console-$os.log"
	echo "cleaned $os in $work (the downloaded image is kept)"
	exit 0
fi

[ -e /dev/kvm ] || { echo "no /dev/kvm: this needs hardware virtualisation" >&2; exit 2; }
mkdir -p "$work"

say "runner image"
docker build -t "$runner" - >/dev/null <<'EOF'
FROM debian:trixie-slim
RUN apt-get update && apt-get install -y --no-install-recommends \
        qemu-system-x86 qemu-utils cloud-image-utils \
        openssh-client xz-utils curl ca-certificates \
        expect socat \
    && rm -rf /var/lib/apt/lists/*
EOF
echo "$runner ready"

if [ ! -f "$work/$image" ]; then
	say "downloading $image"
	case $url in
	*.xz)	curl -fL --progress-bar -o "$work/$image.xz" "$url"; xz -T0 -d "$work/$image.xz" ;;
	*.gz)	curl -fL --progress-bar -o "$work/$image.gz" "$url"; gunzip "$work/$image.gz" ;;
	esac
fi
if [ "$os" = netbsd ] && [ ! -f "$work/$iso" ]; then
	say "downloading $iso"
	curl -fL --progress-bar -o "$work/$iso" "$iso_url"
fi

[ -f "$work/id_ed25519" ] || ssh-keygen -t ed25519 -N '' -C jvim-bsd -f "$work/id_ed25519" >/dev/null

cat > "$work/gssh.sh" <<EOF
#!/bin/sh
exec ssh -p 2222 -i /work/id_ed25519 -o StrictHostKeyChecking=no \\
	-o UserKnownHostsFile=/dev/null -o LogLevel=ERROR -o ConnectTimeout=5 \\
	$guest_user@127.0.0.1 "\$@"
EOF

cat > "$work/boot.sh" <<'EOF'
#!/bin/sh
set -eu
rm -f /work/mon.sock
exec qemu-system-x86_64 \
	-machine q35,accel=kvm -cpu host -smp 2 -m 2048 \
	-drive file=/work/$DISK,if=virtio,format=$FMT \
	$EXTRA \
	-netdev user,id=n0,hostfwd=tcp:127.0.0.1:2222-:22 \
	-device virtio-net-pci,netdev=n0 \
	-display none -monitor unix:/work/mon.sock,server,nowait \
	-serial $SERIAL
EOF

qimg() { docker run --rm -v "$work":/work "$runner" qemu-img "$@"; }

# boot <disk> <format> <extra qemu args> <serial spec> <tries>
boot() {
	docker rm -f "$container" >/dev/null 2>&1 || true
	rm -f "$work/console-$os.log"
	docker run -d --name "$container" --device /dev/kvm \
		-e DISK="$1" -e FMT="$2" -e EXTRA="$3" -e SERIAL="$4" \
		-v "$work":/work "$runner" sh /work/boot.sh >/dev/null
}

wait_ssh() {	# wait_ssh <tries, 10s apart>
	printf 'waiting for the guest'
	i=0
	until guest true 2>/dev/null; do
		i=$((i + 1))
		if [ "$i" -gt "$1" ]; then
			echo " gave up. Tail of the console:" >&2
			tail -30 "$work/console-$os.log" >&2
			exit 1
		fi
		docker ps --filter "name=$container" --format '{{.ID}}' | grep -q . || {
			echo " qemu stopped:" >&2; tail -30 "$work/console-$os.log" >&2; exit 1; }
		printf .
		sleep 10
	done
	echo " up (${i}0s)"
}

halt() {
	# Pulling the plug loses whatever the guest had not written out yet, and both
	# systems then boot into trouble: NetBSD's fsck stops and asks for help, and
	# sshd finds its host key files there but empty. So the virtual power button
	# is pressed instead — both guests shut down cleanly on it — and qemu is
	# given time to go away with the guest. Doing this through the monitor rather
	# than over ssh also works when the guest never came up.
	dexec sh -c "printf 'system_powerdown\n' | socat -T5 - UNIX-CONNECT:/work/mon.sock" \
		>/dev/null 2>&1 || true
	i=0
	while docker ps --filter "name=$container" --format '{{.ID}}' | grep -q .; do
		i=$((i + 1))
		if [ "$i" -gt 60 ]; then
			echo "the guest did not power off; killing it" >&2
			break
		fi
		sleep 2
	done
	docker rm -f "$container" >/dev/null 2>&1 || true
}

# --- first run: install the guest -----------------------------------------

prepare_freebsd() {
	# The cloud image takes its ssh key from a cloud-init seed, so there is
	# nothing to type at the console.
	cat > "$work/user-data" <<EOF
#cloud-config
ssh_pwauth: false
ssh_authorized_keys:
  - $(cat "$work/id_ed25519.pub")
EOF
	printf 'instance-id: jvim-bsd\nlocal-hostname: jvim-bsd\n' > "$work/meta-data"
	docker run --rm -v "$work":/work "$runner" \
		cloud-localds /work/seed.iso /work/user-data /work/meta-data
	qimg create -f qcow2 -b "/work/$image" -F qcow2 "/work/$prepared" 20G >/dev/null
	boot "$prepared" qcow2 \
		"-drive file=/work/seed.iso,if=virtio,format=raw,readonly=on" \
		"file:/work/console-$os.log"
	# The very first boot applies every security patch since the release and
	# reboots, which takes several minutes.
	wait_ssh 100
	guest 'uname -sr; cc --version | head -1'
	echo "installing bash, which scripts/test-encoding.sh needs"
	guest "su -m root -c 'ASSUME_ALWAYS_YES=yes pkg install -y bash'" >/dev/null
}

prepare_netbsd() {
	# The live image boots to a VGA console this side cannot see or type at,
	# and it has no cloud-init. So the install CD is booted first — it talks to
	# a serial line — and its "Exit Install System" shell is used to configure
	# the live image's filesystem from outside. After that the disk boots on
	# the serial line by itself and everything else goes over ssh.
	# Not resized: the install kernel on the CD hangs in "waiting for devices:
	# ld0" when the disk is bigger than the disklabel the live image came with.
	qimg create -f qcow2 -b "/work/$image" -F raw "/work/$prepared" >/dev/null
	boot "$prepared" qcow2 \
		"-drive file=/work/$iso,media=cdrom -boot d" \
		"unix:/work/ser.sock,server,nowait"
	# qemu makes its serial socket a moment after it starts, and connecting
	# before it is there loses everything the CD says.
	i=0
	until dexec test -S /work/ser.sock; do
		i=$((i + 1))
		[ "$i" -gt 30 ] && { echo "qemu made no serial socket" >&2; exit 1; }
		sleep 1
	done
	dexec expect -f /work/netbsd-setup.exp > "$work/console-$os.log" 2>&1 || true
	# The markers share their line with whatever the guest had just echoed.
	grep -oE -- '-- [a-z].*' "$work/console-$os.log" || true
	grep -q -- '-- guest configured' "$work/console-$os.log" || {
		echo "could not set the NetBSD guest up; see $work/console-$os.log" >&2
		exit 1
	}
	# Boot it again, without the CD, to finish over ssh. The install CD can be
	# stopped the rough way: it has nothing of ours mounted any more, and it has
	# no daemon listening for the power button.
	docker rm -f "$container" >/dev/null 2>&1 || true
	boot "$prepared" qcow2 "" "file:/work/console-$os.log"
	wait_ssh 30
	guest 'uname -sr; cc --version | head -1'
	echo "installing bash, which scripts/test-encoding.sh needs"
	guest "PKG_PATH=http://cdn.NetBSD.org/pub/pkgsrc/packages/NetBSD/amd64/$release/All/ /usr/sbin/pkg_add bash" >/dev/null
}

if [ ! -f "$work/$prepared" ]; then
	if [ "$os" = netbsd ]; then
		cat > "$work/netbsd-setup.exp" <<EXP
#!/usr/bin/expect -f
#
# Set a NetBSD live image up for ssh, over the serial line.
#
# The live image itself boots to a VGA console this side can neither read nor
# reliably type at, so the install CD is what is booted here: boot-com.iso puts
# its console on the serial line. Its menus are walked as far as the shell that
# sysinst offers, and from there the live image's filesystem, sitting on the
# first disk, is mounted and edited. Nothing is installed.
#
set timeout 300
spawn socat - UNIX-CONNECT:/work/ser.sock

expect {
	"Terminal type (just hit ENTER for 'vt220'):" { send "\r" }
	timeout { puts "-- the install CD never asked for a terminal type"; exit 1 }
}
# The menus want the letter and then Return.
expect {
	"Installation messages in English" { after 500; send "a\r" }
	timeout { puts "-- no language menu"; exit 1 }
}
expect {
	-re {([a-z]): Exit Install System} { after 500; send "\$expect_out(1,string)\r" }
	timeout { puts "-- no main menu"; exit 1 }
}
expect {
	"# " {}
	timeout { puts "-- sysinst gave no shell"; exit 1 }
}

# The live image ships with one inconsistency of its own, an unallocated inode
# for /etc/openssl/certs. fsck -p cannot fix that class of problem, so if the
# guest is ever shut down dirty its next boot stops and asks for help. Fixed
# here, once, while nothing has it mounted.
send "fsck_ffs -y /dev/rld0a > /dev/null 2>&1; echo FSCK-DONE\r"
expect {
	"FSCK-DONE" {}
	timeout { puts "-- fsck of the live image did not finish"; exit 1 }
}
expect "# "
# ld0 is the virtio disk; ld0a is the live image's root.
send "mkdir -p /mnt2 && mount /dev/ld0a /mnt2 && echo MOUNT-OK\r"
expect {
	"MOUNT-OK" {}
	timeout { puts "-- could not mount the live image"; exit 1 }
}
expect "# "
# consdev puts later boots on the serial line, so the CD is only needed once.
send "echo dhcpcd=YES >> /mnt2/etc/rc.conf; echo sshd=YES >> /mnt2/etc/rc.conf; echo fsck_flags=-y >> /mnt2/etc/rc.conf; echo consdev=com0 >> /mnt2/boot.cfg; echo RC-DONE\r"
expect "RC-DONE"
expect "# "
send "mkdir -p /mnt2/root/.ssh; echo '$(cat "$work/id_ed25519.pub")' > /mnt2/root/.ssh/authorized_keys; chmod 700 /mnt2/root/.ssh; chmod 600 /mnt2/root/.ssh/authorized_keys; echo KEY-DONE\r"
expect "KEY-DONE"
expect "# "
# sshd takes the first setting it sees, so this has to go on top.
send "(echo PermitRootLogin prohibit-password; cat /mnt2/etc/ssh/sshd_config) > /tmp/sc && cp /tmp/sc /mnt2/etc/ssh/sshd_config; echo SSHD-DONE\r"
expect "SSHD-DONE"
expect "# "
send "umount /mnt2 && echo UMOUNT-OK\r"
expect {
	"UMOUNT-OK" {}
	timeout { puts "-- the live image would not unmount"; exit 1 }
}
expect "# "
puts "-- guest configured"
exit 0
EXP
	fi
	say "preparing a $os guest (first run only, several minutes)"
	prepare_$os
	halt
	echo "kept $work/$prepared"
fi

# --- every run ------------------------------------------------------------

say "booting $os"
rm -f "$work/run-$os.qcow2"
qimg create -f qcow2 -b "/work/$prepared" -F qcow2 "/work/run-$os.qcow2" $disk_size >/dev/null
boot "run-$os.qcow2" qcow2 "" "file:/work/console-$os.log"
wait_ssh 30
guest 'uname -srm; cc --version | head -1'

say "copying the working tree"
rm -rf "$work/src"
mkdir -p "$work/src"
# Tracked files plus anything new that is not ignored, so that a file which has
# not been committed yet still reaches the guest; build products stay behind.
(cd "$root" && git ls-files -z --cached --others --exclude-standard \
	| tar --null -cf - -T -) | tar xf - -C "$work/src"
dexec sh -c 'tar cf - -C /work/src . | sh /work/gssh.sh "rm -rf jvim3 && mkdir jvim3 && tar xf - -C jvim3"'

if [ "$target" = shell ]; then
	say "the guest is up"
	cat <<EOF
  docker exec -it $container sh /work/gssh.sh
  docker exec $container sh /work/gssh.sh 'cd jvim3 && sh scripts/build-unix.sh'
  docker rm -f $container        # when done
EOF
	exit 0
fi

say "building on $os"
rc=0
# The compiler lines and the K&R prototype warnings are the same as on Linux and
# there are thousands of them, so only the interesting parts are shown; the whole
# log is copied back below.
guest "cd jvim3 && PATH=\$PATH:$guest_path sh scripts/build-unix.sh > build.log 2>&1; rc=\$?;
	sed -n 1,9p build.log; grep -E ': error|\\*\\*\\* Error|^built ' build.log;
	printf 'warnings: %s\n' \"\$(grep -c 'warning:' build.log)\"; exit \$rc" || rc=$?
dexec sh /work/gssh.sh 'cat jvim3/build.log' > "$work/build-$os.log" 2>/dev/null || true
echo "full log: $work/build-$os.log"

if [ $rc -eq 0 ] && [ "$target" = test ]; then
	say "running the encoding tests on $os"
	guest "cd jvim3 && PATH=\$PATH:$guest_path scripts/test-encoding.sh src/jvim3" || rc=$?
fi

halt
say "$os run finished with status $rc"
exit $rc
