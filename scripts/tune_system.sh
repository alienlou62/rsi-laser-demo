#!/usr/bin/env bash
set -euo pipefail

IFACE="enp1s0"
CPULIST="3"

echo performance | tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor

# Find this interface's PCI device path
PCIDEV=$(readlink -f /sys/class/net/"$IFACE"/device)

# 1) Pin MSI/MSI-X IRQs for this NIC to CPU 3
for V in "$PCIDEV"/msi_irqs/*; do
  IRQ=$(basename "$V")
  # Prefer smp_affinity_list for readability
  echo "$CPULIST" > /proc/irq/"$IRQ"/smp_affinity_list || true
done

# 2) Coalescing (minimize latency)
ethtool -C "$IFACE" rx-usecs 0

exit 0