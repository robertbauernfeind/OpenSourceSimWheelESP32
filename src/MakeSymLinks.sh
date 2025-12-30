#!/bin/bash
############################################################################
#
# .SYNOPSYS
#     Copy or link required files from "common" and "include" folders into
#     every Arduino's sketch folder for proper compilation
#
# .USAGE
#     Create a file named "includes.txt" into the sketch's folder.
#     Insert every required file name into "includes.txt",
#     relative to the "src" folder.
#     Header files in "include" (.h/.hpp) are always linked/copied,
#     so there is no need to include them.
#     Symlinks will be created. If not possible,
#     files will be copied instead. In such a case, this script must be run
#     again every time a file is touched at the "common" or "include" folders.
#
# .AUTHORS
#     José Agustin (https://github.com/janc18)
#     Ángel Fernández Pineda. Madrid. Spain. 2022.
##
# .LICENSE
#     Licensed under the EUPL
#############################################################################

THIS_PATH="$(dirname "$(realpath "$0")")"
INCLUDE_PATH="$THIS_PATH/include"
COMMON_PATH="$THIS_PATH/common"
INCLUDES_FILE="includes.txt"

header_files=$(find "$INCLUDE_PATH" -type f -name "*.h??")

get_required_links() {
    local path="$1"
    local links=()
    local uses_hid_ble=false
    local uses_hid_espble=false
    local uses_hid_nimble=false
    local uses_hid_h2zero=false
    local uses_nimble_wrapper=false
    file_content=$(<"$path/$INCLUDES_FILE")
    if [ -n "$file_content" ] && [ "${file_content: -1}" != $'\n' ]; then
        file_content="$file_content"$'\n'
    fi
    while IFS= read -r line; do
        line=$(echo "$line" | sed 's/\r//;s/^[[:space:]]*//;s/[[:space:]]*$//')
        if [[ -n "$line" ]]; then
            if [ "$line" = "hid_ESPBLE.cpp" ]; then
                uses_hid_espble=true
            fi
            if [ "$line" = "hid_BLE.cpp" ]; then
                uses_hid_ble=true
            fi
            if [ "$line" = "hid_NimBLE.cpp" ]; then
                uses_hid_nimble=true
            fi
            if [ "$line" = "hid_h2zero.cpp" ]; then
                uses_hid_h2zero=true
            fi
            if [ "$line" = "NimBLEWrapper.cpp" ]; then
                uses_nimble_wrapper=true
            fi
            if [ "$line" != "hid_NimBLE.cpp" ] && [ "$line" != "hid_ESPBLE.cpp" ]; then
                links+=("$COMMON_PATH/$line")
            fi
        fi
    done <<<"$file_content"
    echo "${links[@]}"
    echo $header_files
    if [ "$uses_hid_nimble" = true ] && [ "$uses_hid_h2zero" = false ]; then
        echo "$COMMON_PATH/hid_h2zero.cpp"
    else
        if [ "$uses_hid_espble" = true ] && [ "$uses_hid_ble" = false ]; then
            echo "$COMMON_PATH/hid_BLE.cpp"
            uses_hid_ble=true
        fi
        if [ "$uses_hid_ble" = true ] && [ "$uses_nimble_wrapper" = false ]; then
            echo "$COMMON_PATH/NimBLEWrapper.cpp"
        fi
    fi
}

get_sketch_folders() {
    find "$THIS_PATH" -type f -name "*.ino" -exec dirname {} \; | sort | uniq
}

echo "🛈 Path: $THIS_PATH"

SKETCH_FOLDERS=$(get_sketch_folders)

for folder in $SKETCH_FOLDERS; do
    INCLUDES_FILE_PATH="$folder/$INCLUDES_FILE"
    if [[ -f "$INCLUDES_FILE_PATH" ]]; then
        echo "⛏ Processing path: $folder"
        req_links=$(get_required_links "$folder")
        find "$folder" -name "*.c??" -exec rm -f {} +
        find "$folder" -name "*.h??" -exec rm -f {} +
        for link_spec in $req_links; do
            target="$folder/$(basename "$link_spec")"
            if test -f "$link_spec"; then
                if ln -s "$link_spec" "$target" 2>/dev/null; then
                    :
                    # echo "Symlink created: $link_spec -> $target"
                else
                    if cp -f "$link_spec" "$target" 2>/dev/null; then
                        chmod -w $target 2>/dev/null
                        # echo "Copied: $link_spec -> $target"
                    else
                        echo "ERROR: unable to link or copy '$target'"
                        exit 2
                    fi
                fi
            else
                echo "ERROR: '$link_spec' (found in 'includes.txt') does not exist"
                exit 1
            fi
        done
    else
        echo "⚠️  No 'includes.txt' found in folder: $folder"
    fi
done
