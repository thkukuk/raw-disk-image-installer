# SPDX-License-Identifier: MIT
# shellcheck shell=bash

select_image()
{
    clear_and_print_title
    SOURCE_IMAGE=$(gum file . \
		       --file \
                       --header="Select Image" \
                       --header.foreground="$COLOR_TITLE" \
                       --cursor "➜" \
                       --cursor.foreground="$COLOR_FOREGROUND" \
		       --selected.foreground="$COLOR_FOREGROUND")
}
