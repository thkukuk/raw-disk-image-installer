#!/bin/bash

# Kernel cmdline arguments:
#
# ssh=1             Enable sshd.service
# ssh.password=xyz  Set as root password and enable PermitRootLogin
# ssh.key=base64    Public ssh key base64 encoded

cmdline=$(cat /proc/cmdline)

# Initialize variables
ENABLE_SSH=0
ROOT_PASS=""
SSH_PUB_KEY_B64=""

for ARG in $cmdline; do
    case "$ARG" in
        ssh=1)
            ENABLE_SSH=1
            ;;
        ssh.password=*)
            # Extract value after the first '='
            ROOT_PASS="${ARG#*=}"
            ;;
	ssh.key=*)
            # Extract Base64 string
            SSH_PUB_KEY_B64="${ARG#*=}"
            ;;
    esac
done

if [ "$ENABLE_SSH" -eq 1 ]; then
    echo "Boot argument ssh=1 detected. Configuring SSH..."

    if [ -n "$ROOT_PASS" ]; then
        echo "Setting root password and enabling root login..."

        echo "root:$ROOT_PASS" | chpasswd

	mkdir -p /etc/ssh/sshd_config.d
	echo "PermitRootLogin yes" > /etc/ssh/sshd_config.d/50-permit-root-login.conf
    fi

    if [ -n "$SSH_PUB_KEY_B64" ]; then
        echo "Deploying SSH Public Key..."

        # Create .ssh directory with correct permissions
        mkdir -m 0700 /root/.ssh

        echo "$SSH_PUB_KEY_B64" | base64 -d >> /root/.ssh/authorized_keys
        chmod 600 /root/.ssh/authorized_keys

        echo "SSH Public key deployed."
    fi

    systemctl enable --now sshd.service
    echo "SSH service enabled and started."
fi
