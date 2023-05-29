#!/bin/bash

# Set the path to ESP-IDF
export IDF_PATH=$HOME/workspace/esp/esp-idf/

# Function to detect ESP32 USB port
detect_port() {
    echo "Detecting ESP32 USB port..."
    ESP_PORTS=($(dmesg | grep cp210x | grep ttyUSB | awk '{print "/dev/"$NF}'))
    if [ -z "$ESP_PORTS" ]; then
        echo "No ESP32 USB port detected. Please connect the device and try again."
        exit 1
    else
        # Display available ESP32 ports
        echo "Available ESP32 ports:"
        for i in "${!ESP_PORTS[@]}"; do
            printf "%s\t%s\n" "$i" "${ESP_PORTS[$i]}"
        done

    fi
}

# Menu options
while true; do
    # clear screen
    clear
    
    echo "Choose an option:"
    echo "0. Delete Build"
    echo "1. Full clean"
    echo "2. Menu config"
    echo "3. Build"
    echo "4. Flash"
    echo "5. Monitor"
    echo "6. Build, flash, and monitor"
    read -p "Enter your choice (1-6): " choice

    case $choice in
    0)
        # remove build folder
        echo "Deleting build..."
        rm -dr build
        ;;
    1)
        # fullclearn
        echo "Running full clean..."
        idf.py fullclean
        ;;
    2)
        # menuconfig
        echo "Running menu config..."
        idf.py menuconfig
        ;;
    3)
        # build
        echo "Running build..."
        idf.py build
        ;;
    4)
        # flash
        detect_port
        read -p "Enter the port number to flash (0-${#ESP_PORTS[@]}): " port_num
        echo "Flashing firmware to $port..."
        idf.py -p ${ESP_PORTS[$port_num]} flash
        ;;
    5)
        # monitor
        detect_port
        read -p "Enter the port number to flash (0-${#ESP_PORTS[@]}): " port_num
        echo "Starting serial monitor on $port..."
        idf.py -p ${ESP_PORTS[$port_num]} monitor
        ;;
    6)
        # build flash monitor
        detect_port
        read -p "Enter the port number to flash (0-${#ESP_PORTS[@]}): " port_num
        echo "Building, flashing and monitoring firmware on $port..."
        idf.py -p ${ESP_PORTS[$port_num]} build flash monitor
        ;;
    *)
        echo "Invalid choice. Please try again."
        ;;
    esac

    read -p "Do you want to continue? (y/n)" yn
    case $yn in
    [Nn]*) exit ;;
    esac
done
