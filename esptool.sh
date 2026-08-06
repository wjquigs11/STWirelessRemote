#!/bin/bash

# Default port, build, and partition
port="/dev/ttyUSB0"
build="diamond"
partition="app0"
run_miniterm=false
flash_all=false
ota_host=""

# Display help message
function show_help {
    echo "Usage: $0 [options]"
    echo "Options:"
    echo "  -p PORT     Specify the port (default: /dev/ttyUSB0)"
    echo "  -b BUILD    Specify the build name: diamond, turnout, esp32dev, feather, ota (default: diamond)"
    echo "  -t TYPE     Specify partition type to flash (app0, app1, littlefs, all) (default: app0)"
    echo "  -ota HOST   Flash via ArduinoOTA to HOST (IP or hostname)"
    echo "              Use with -t littlefs to flash SPIFFS/LittleFS partition OTA"
    echo "  -m          Run miniterm after flashing"
    echo "  -h          Show this help message"
    exit 0
}

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -h)
            show_help
            ;;
        -m)
            run_miniterm=true
            shift
            ;;
        -p)
            if [[ -n $2 ]]; then
                port="$2"
                shift 2
            else
                echo "Error: -p requires a port argument"
                exit 1
            fi
            ;;
        -b)
            if [[ -n $2 ]]; then
                build="$2"
                shift 2
            else
                echo "Error: -b requires a build argument"
                exit 1
            fi
            ;;
        -ota)
            if [[ -n $2 ]]; then
                ota_host="$2"
                shift 2
            else
                echo "Error: -ota requires a host argument (IP or hostname)"
                exit 1
            fi
            ;;
        -h)
            show_help
            ;;
        -t)
            if [[ -n $2 ]]; then
                partition="$2"
                if [ "$partition" = "all" ]; then
                    flash_all=true
                fi
                shift 2
            else
            echo "Error: -t requires a partition argument (app0, app1, littlefs, all)"
                exit 1
            fi
            ;;
        *)
            echo "Unknown option: $1"
            exit 1
            ;;
    esac
done

# Function to get partition file from platformio.ini
get_partition_file() {
    local partition_file=""
    if [ -f "platformio.ini" ]; then
        # Look for board_build.partitions setting (uncommented)
        partition_file=$(grep -E "^[[:space:]]*board_build\.partitions[[:space:]]*=" platformio.ini | head -1 | cut -d'=' -f2 | xargs)
    fi
    echo "$partition_file"
}

# Default ESP32 partition offsets (used when no custom partition file is specified)
DEFAULT_APP0_OFFSET="0x10000"
DEFAULT_LITTLEFS_OFFSET="0x290000"

# Get partition file from platformio.ini
partition_file=$(get_partition_file)
use_default_layout=false

# Initialize partition offsets
app0_offset=""
app1_offset=""
littlefs_offset=""

if [ -n "$partition_file" ] && [ -f "$partition_file" ]; then
    echo "Using custom partition file: $partition_file"
    # Read partition offsets from custom CSV file
    while IFS=, read -r name type subtype offset size flags; do
        # Skip comments and empty lines
        [[ $name =~ ^#.*$ || -z $name ]] && continue
        
        # Remove whitespace
        name=$(echo $name | xargs)
        offset=$(echo $offset | xargs)
        
        # Store offsets in separate variables
        if [ "$name" = "app0" ]; then
            app0_offset=$offset
        elif [ "$name" = "app1" ]; then
            app1_offset=$offset
        elif [ "$name" = "spiffs" ] || [ "$name" = "littlefs" ]; then
            littlefs_offset=$offset
        fi
    done < "$partition_file"
else
    if [ -n "$partition_file" ]; then
        echo "Warning: Partition file '$partition_file' specified in platformio.ini but not found."
    fi
    echo "Using default ESP32 partition layout"
    use_default_layout=true
    app0_offset=$DEFAULT_APP0_OFFSET
    littlefs_offset=$DEFAULT_LITTLEFS_OFFSET
    # app1 is not available in default layout
fi

ESPTOOL=~/.platformio/packages/tool-esptoolpy/esptool.py
# Use PlatformIO's bundled Python which has all esptool dependencies (rich_click, etc.)
PYTHON=~/.platformio/penv/bin/python

# Find workspace_dir from platformio.ini (if uncommented)
workspace_dir=""
if [ -f "platformio.ini" ]; then
    workspace_dir=$(grep -E "^[[:space:]]*workspace_dir[[:space:]]*=" platformio.ini | head -1 | cut -d'=' -f2 | xargs)
fi

# Build path: use workspace_dir/build if workspace_dir is set, otherwise .pio/build
if [ -n "$workspace_dir" ]; then
    BUILD_PATH="$workspace_dir/build"
else
    BUILD_PATH=".pio/build"
fi

# Flash based on selected partition
# If OTA mode, use espota.py and skip serial flashing
if [ -n "$ota_host" ]; then
    ESPOTA=~/.platformio/packages/framework-arduinoespressif32/tools/espota.py
    if [ ! -f "$ESPOTA" ]; then
        echo "Error: espota.py not found at $ESPOTA"
        exit 1
    fi

    if [ "$partition" = "littlefs" ]; then
        # OTA flash filesystem (SPIFFS/LittleFS)
        if [ -f "$BUILD_PATH/$build/littlefs.bin" ]; then
            fs_image="$BUILD_PATH/$build/littlefs.bin"
        elif [ -f "$BUILD_PATH/$build/spiffs.bin" ]; then
            fs_image="$BUILD_PATH/$build/spiffs.bin"
        else
            echo "Error: no filesystem image found at $BUILD_PATH/$build/littlefs.bin or spiffs.bin"
            echo "Run: pio run -t buildfs -e $build"
            exit 1
        fi
        echo "Flashing filesystem $fs_image to $ota_host via ArduinoOTA (SPIFFS)..."
        $PYTHON $ESPOTA -i "$ota_host" -p 3232 -s -f "$fs_image"
    else
        # OTA flash firmware
        firmware="$BUILD_PATH/$build/firmware.bin"
        if [ ! -f "$firmware" ]; then
            echo "Error: firmware not found at $firmware"
            exit 1
        fi
        echo "Flashing $firmware to $ota_host via ArduinoOTA..."
        $PYTHON $ESPOTA -i "$ota_host" -p 3232 -f "$firmware"
    fi
    exit $?
fi

case $partition in
    app0)
        if [ -z "$app0_offset" ]; then
            if [ "$use_default_layout" = true ]; then
                echo "Error: app0 partition not available in default layout"
            else
                echo "Error: app0 partition not found in partition file"
            fi
            exit 1
        fi
        echo "Flashing firmware to app0 partition at $app0_offset"
        $PYTHON $ESPTOOL --chip esp32 --port $port --baud 460800 write_flash $app0_offset "$BUILD_PATH/$build/firmware.bin"
        ;;
    app1)
        if [ "$use_default_layout" = true ]; then
            echo "Error: app1 partition is not available in default ESP32 layout"
            echo "To use app1, specify a custom partition file with OTA support in platformio.ini:"
            echo "  board_build.partitions = your_ota_partition_file.csv"
            exit 1
        fi
        if [ -z "$app1_offset" ]; then
            echo "Error: app1 partition not found in partition file"
            exit 1
        fi
        echo "Flashing firmware to app1 partition at $app1_offset"
        $PYTHON $ESPTOOL --chip esp32 --port $port --baud 460800 write_flash $app1_offset "$BUILD_PATH/$build/firmware.bin"
        ;;
    littlefs)
        if [ -z "$littlefs_offset" ]; then
            if [ "$use_default_layout" = true ]; then
                echo "Error: littlefs partition not available in default layout"
            else
                echo "Error: littlefs partition not found in partition file"
            fi
            exit 1
        fi
        echo "Flashing filesystem data to partition at $littlefs_offset"
        # platformio.ini uses board_build.filesystem = spiffs, so image may be spiffs.bin or littlefs.bin
        if [ -f "$BUILD_PATH/$build/littlefs.bin" ]; then
            fs_image="$BUILD_PATH/$build/littlefs.bin"
        elif [ -f "$BUILD_PATH/$build/spiffs.bin" ]; then
            fs_image="$BUILD_PATH/$build/spiffs.bin"
        else
            echo "Error: no filesystem image found at $BUILD_PATH/$build/littlefs.bin or spiffs.bin"
            exit 1
        fi
        echo "Filesystem image: $fs_image"
        ls -la "$fs_image"
        $PYTHON $ESPTOOL --chip esp32 --port $port --baud 460800 --before default_reset --after hard_reset write_flash -z --flash_mode dio --flash_freq 40m --flash_size detect $littlefs_offset "$fs_image"
        ;;
    all)
        # Flash app0 first
        if [ -z "$app0_offset" ]; then
            if [ "$use_default_layout" = true ]; then
                echo "Error: app0 partition not available in default layout"
            else
                echo "Error: app0 partition not found in partition file"
            fi
            exit 1
        fi
        echo "Flashing firmware to app0 partition at $app0_offset"
        $PYTHON $ESPTOOL --chip esp32 --port $port --baud 460800 write_flash $app0_offset "$BUILD_PATH/$build/firmware.bin"
        
        # Then flash filesystem
        if [ -z "$littlefs_offset" ]; then
            if [ "$use_default_layout" = true ]; then
                echo "Error: littlefs partition not available in default layout"
            else
                echo "Error: littlefs partition not found in partition file"
            fi
            exit 1
        fi
        echo "Flashing filesystem data to partition at $littlefs_offset"
        if [ -f "$BUILD_PATH/$build/littlefs.bin" ]; then
            fs_image="$BUILD_PATH/$build/littlefs.bin"
        elif [ -f "$BUILD_PATH/$build/spiffs.bin" ]; then
            fs_image="$BUILD_PATH/$build/spiffs.bin"
        else
            echo "Error: no filesystem image found at $BUILD_PATH/$build/littlefs.bin or spiffs.bin"
            exit 1
        fi
        echo "Filesystem image: $fs_image"
        ls -la "$fs_image"
        $PYTHON $ESPTOOL --chip esp32 --port $port --baud 460800 --before default_reset --after hard_reset write_flash -z --flash_mode dio --flash_freq 40m --flash_size detect $littlefs_offset "$fs_image"
        ;;
    *)
        if [ -z "$app0_offset" ]; then
            if [ "$use_default_layout" = true ]; then
                echo "Error: app0 partition not available in default layout"
            else
                echo "Error: app0 partition not found in partition file"
            fi
            exit 1
        fi
        echo "Unknown partition: $partition. Using default app0 at $app0_offset"
        $PYTHON $ESPTOOL --chip esp32 --port $port --baud 460800 write_flash $app0_offset "$BUILD_PATH/$build/firmware.bin"
        ;;
esac

if [[ "$run_miniterm" == true ]]; then
    $PYTHON -m serial.tools.miniterm $port 115200
fi
