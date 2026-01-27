# SPDX-License-Identifier: MIT
# shellcheck shell=bash

local_fs()
{
    clear_and_print_title
    SOURCE_IMAGE=$(gum file /mnt \
		       --file \
                       --header="Select Image" \
                       --header.foreground="$COLOR_TITLE" \
                       --cursor "${CURSOR}" \
                       --cursor.foreground="$COLOR_FOREGROUND" \
		       --selected.foreground="$COLOR_FOREGROUND")
}

select_image()
{
    local SELECTED_IMG
    local PROCESSED_DEVICES
    local OLD_PATH
    local IMAGE_LIST="Provide URL\nUse file selection\n"

    mkdir -p "${TEMP_DIR}/mounts"

    # lsblk "PATH" should be "DEVICE" to avoid a conflict
    local OLD_PATH=$PATH
    declare -A PROCESSED_DEVICES
    while read -r line; do
	# Evaluate the line to set variables: $PATH, $LABEL, $MOUNTPOINT
	eval "$line"
	DEVICE=$PATH
	PATH=$OLD_PATH
	local IS_TEMP_MOUNTED=false

	# Skip if we have already processed this device path
	if [[ -n "${PROCESSED_DEVICES[$DEVICE]}" ]]; then
            continue
	fi
	PROCESSED_DEVICES[$DEVICE]=1

	# ${LABEL,,} converts the label to lowercase for comparison
	if [[ "${LABEL,,}" == "images" ]]; then
            echo "Found target partition: $DEVICE (Label: $LABEL)"

            SEARCH_DIR=""
            TEMP_MOUNT_DIR=""

            if [ -n "$MOUNTPOINT" ]; then
		echo "Partition is already mounted at: $MOUNTPOINT"
		SEARCH_DIR="$MOUNTPOINT"
            else
		TEMP_MOUNT_DIR=$(mktemp -d --tmpdir="${TEMP_DIR}/mounts" XXXXXXXXXX)

		# Try to mount the device to the temp directory
		if mount -r "$DEVICE" "$TEMP_MOUNT_DIR" 2>/dev/null ; then
                    SEARCH_DIR="$TEMP_MOUNT_DIR"
                    IS_TEMP_MOUNTED=true
		else
                    rmdir "$TEMP_MOUNT_DIR"
                    continue
		fi
            fi

            echo "Scanning for .raw and .img files..."
	    while IFS= read -r -d '' file; do
		if [ "$IS_TEMP_MOUNTED" = true ]; then
		    IMAGE_LIST+="$(basename "$file") ($DEVICE)\n"
		else
		    IMAGE_LIST+="$file\n"
		fi
	    done < <(find "$SEARCH_DIR" -type f \( -name "*.raw" -o -name "*.img" \) -print0)

	    if [ "$IS_TEMP_MOUNTED" = true ]; then
		echo "Unmounting temporary mount..."
		umount "$TEMP_MOUNT_DIR"
		rmdir "$TEMP_MOUNT_DIR"
            fi
        fi
    done < <(lsblk -P -o PATH,LABEL,MOUNTPOINT)
    PATH=$OLD_PATH

    clear_and_print_title
    SELECTED_IMG=$(echo -e "$IMAGE_LIST" | \
                       gum choose \
                           --header="Select Source Image" \
                           --header.foreground="$COLOR_TITLE" \
                           --cursor="${CURSOR} " \
                           --cursor.foreground="$COLOR_FOREGROUND")

    if [ -z "$SELECTED_IMG" ]; then
        gum style --foreground="$COLOR_TEXT" "Cancelled."
        return
    fi

    case "$SELECTED_IMG" in
	"Provide URL")
	    echo "Missing"
	    ;;
	"Use file selection")
	    local_fs
	    ;;
	*)
	    SOURCE_IMAGE=$SELECTED_IMG
	    ;;
    esac
}
