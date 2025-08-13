#!/bin/bash

# Camera Check Script
# Verifies that both cameras are properly connected and accessible
# Usage: ./cam_check.sh [--verbose] [--test-capture]

# -e: exit if any command fails
# -u: treat unset variables as errors
# pipefail: fail if any command in a pipeline fails
set -euo pipefail

# Configuration
FRONT_CAM="/dev/cam_front"
SIDE_CAM="/dev/cam_right"
VERBOSE=false
TEST_CAPTURE=false
TIMEOUT=10  # seconds for capture test

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --verbose|-v)
            VERBOSE=true
            # removes this argument from $1, moving to the next one
            shift
            ;;
        --test-capture|-t)
            TEST_CAPTURE=true
            shift
            ;;
        --help|-h)
            echo "Usage: $0 [OPTIONS]"
            echo "Options:"
            echo "  --verbose, -v      Show detailed camera information"
            echo "  --test-capture, -t Test actual image capture (takes ~5 seconds)"
            echo "  --help, -h         Show this help message"
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            echo "Use --help for usage information"
            exit 1
            ;;
    esac
done

# Function to print colored output
print_status() {
    local color=$1
    local message=$2
    echo -e "${color}${message}${NC}"
}

print_info() {
    print_status "$BLUE" "ℹ️  $1"
}

print_success() {
    print_status "$GREEN" "✅ $1"
}

print_warning() {
    print_status "$YELLOW" "⚠️  $1"
}

print_error() {
    print_status "$RED" "❌ $1"
}

# Function to check if a device exists and is a video device
# In Linux, there are different types of files:
# - Regular files - normal files with data
# - Directory files - folders
# - Block devices - storage devices like hard drives (data in blocks)
# - Character devices - devices that handle data as a stream of characters

# Character devices include:
# /dev/video0 - cameras (video stream)
# /dev/ttyUSB0 - serial ports
# /dev/null - the null device
# Keyboards, mice, etc.

check_device_exists() {
    local device=$1
    local name=$2
    
    if [ ! -e "$device" ]; then
        print_error "$name: Device $device does not exist"
        return 1
    fi
    
    if [ ! -c "$device" ]; then
        print_error "$name: $device is not a character device"
        return 1
    fi
    
    print_success "$name: Device $device exists"
    return 0
}

# Function to check device permissions
check_device_permissions() {
    local device=$1
    local name=$2
    
    if [ ! -r "$device" ]; then
        print_error "$name: Cannot read from $device (permission denied)"
        print_info "Try: sudo chmod 666 $device"
        print_info "Or add your user to the 'video' group: sudo usermod -a -G video $USER"
        return 1
    fi
    
    if [ ! -w "$device" ]; then
        print_warning "$name: Cannot write to $device (may be read-only)"
    fi
    
    print_success "$name: Device permissions OK"
    return 0
}

# Function to get camera info using v4l2-ctl
get_camera_info() {
    local device=$1
    local name=$2
    
    print_info "Getting $name information..."
    
    # Check if v4l2-ctl is available
    if ! command -v v4l2-ctl &> /dev/null; then
        print_error "v4l2-ctl not found. Install with: sudo apt install v4l-utils"
        return 1
    fi
    
    # Get basic device info
    local info_output
    if ! info_output=$(v4l2-ctl --device="$device" --info 2>/dev/null); then
        print_error "$name: Failed to get device info from $device"
        return 1
    fi
    
    local driver_name=$(echo "$info_output" | grep "Driver name" | cut -d':' -f2- | xargs)
    local card_type=$(echo "$info_output" | grep "Card type" | cut -d':' -f2- | xargs)
    local bus_info=$(echo "$info_output" | grep "Bus info" | cut -d':' -f2- | xargs)
    
    print_success "$name: Camera detected successfully"
    
    if [ "$VERBOSE" = true ]; then
        echo "    Driver: $driver_name"
        echo "    Card: $card_type"
        echo "    Bus: $bus_info"
        
        # Get supported formats
        echo "    Supported formats:"
        v4l2-ctl --device="$device" --list-formats-ext 2>/dev/null | grep -E "(Index|Size|Interval)" | sed 's/^/        /' || echo "        Could not retrieve formats"
    fi
    
    return 0
}

# Function to test actual image capture
test_capture() {
    local device=$1
    local name=$2
    
    print_info "Testing capture from $name..."
    
    # Create temporary file for capture test
    local temp_file=$(mktemp --suffix=.jpg)
    
    # Try to capture a single frame
    if timeout "$TIMEOUT" v4l2-ctl --device="$device" --set-fmt-video=width=640,height=480,pixelformat=MJPG --stream-mmap --stream-count=1 --stream-to="$temp_file" &>/dev/null; then
        if [ -f "$temp_file" ] && [ -s "$temp_file" ]; then
            local file_size=$(stat -f%z "$temp_file" 2>/dev/null || stat -c%s "$temp_file" 2>/dev/null)
            print_success "$name: Capture test successful (${file_size} bytes)"
            rm -f "$temp_file"
            return 0
        else
            print_error "$name: Capture test failed - empty file"
            rm -f "$temp_file"
            return 1
        fi
    else
        print_error "$name: Capture test failed - timeout or error"
        rm -f "$temp_file"
        return 1
    fi
}

# Main execution
main() {
    echo "===========================================" 
    echo "         Camera Bringup Check"
    echo "===========================================" 
    echo ""
    
    print_info "Checking camera devices..."
    echo ""
    
    local front_ok=true
    local side_ok=true
    
    # Check front camera
    echo "--- Front Camera Check ---"
    if ! check_device_exists "$FRONT_CAM" "Front Camera"; then
        front_ok=false
    elif ! check_device_permissions "$FRONT_CAM" "Front Camera"; then
        front_ok=false
    elif ! get_camera_info "$FRONT_CAM" "Front Camera"; then
        front_ok=false
    elif [ "$TEST_CAPTURE" = true ]; then
        if ! test_capture "$FRONT_CAM" "Front Camera"; then
            front_ok=false
        fi
    fi
    
    echo ""
    
    # Check side camera  
    echo "--- Side Camera Check ---"
    if ! check_device_exists "$SIDE_CAM" "Side Camera"; then
        side_ok=false
    elif ! check_device_permissions "$SIDE_CAM" "Side Camera"; then
        side_ok=false
    elif ! get_camera_info "$SIDE_CAM" "Side Camera"; then
        side_ok=false
    elif [ "$TEST_CAPTURE" = true ]; then
        if ! test_capture "$SIDE_CAM" "Side Camera"; then
            side_ok=false
        fi
    fi
    
    echo ""
    echo "--- Summary ---"
    
    if [ "$front_ok" = true ] && [ "$side_ok" = true ]; then
        print_success "Both cameras are ready!"
        echo ""
        print_info "Camera assignments:"
        echo "  Front Camera: $FRONT_CAM"
        echo "  Side Camera:  $SIDE_CAM"
        echo ""
        print_info "You can now use these devices in your applications"
        exit 0
    else
        print_error "Camera setup is incomplete"
        echo ""
        print_info "Troubleshooting steps:"
        
        if [ "$front_ok" = false ]; then
            echo "  1. Check front camera connection and udev rules"
        fi
        
        if [ "$side_ok" = false ]; then
            echo "  2. Check side camera connection and udev rules"  
        fi
        
        echo "  3. Verify udev rules in /etc/udev/rules.d/99-cameras.rules"
        echo "  4. Reload udev rules: sudo udevadm control --reload-rules && sudo udevadm trigger"
        echo "  5. Check dmesg for USB/camera errors: dmesg | tail -20"
        echo "  6. Re-run identify_cameras.sh to verify device detection"
        
        exit 1
    fi
}

# Check if running as part of a larger script
if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then
    main "$@"
fi
