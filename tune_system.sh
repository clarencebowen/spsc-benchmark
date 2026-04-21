#!/bin/bash

# System tuning for benchmarking only.
# Reduces OS scheduling and frequency scaling effects.

set -e

echo "Applying system settings..."

# CPU governor: performance
echo "performance" | sudo tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor > /dev/null

# Energy performance preference (if supported)
for epp in /sys/devices/system/cpu/cpu*/cpufreq/energy_performance_preference; do
    [ -f "$epp" ] && echo "performance" | sudo tee "$epp" > /dev/null
done

# Lock CPU frequency to max (if available)
MAX_FREQ=$(lscpu | awk -F: '/CPU max MHz/ {gsub(/ /,"",$2); print int($2*1000)}')

if [ -n "$MAX_FREQ" ]; then
    echo $MAX_FREQ | sudo tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_min_freq > /dev/null
fi

# Disable NMI watchdog
echo 0 | sudo tee /proc/sys/kernel/nmi_watchdog > /dev/null

# Disable irqbalance
sudo systemctl stop irqbalance 2>/dev/null || true
sudo systemctl disable irqbalance 2>/dev/null || true

echo "Done."
