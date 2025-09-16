#!/usr/bin/env bash
set -euo pipefail

set_cpu_performance() {
  local CPU="$1"
  echo performance > "/sys/devices/system/cpu/cpu${CPU}/cpufreq/scaling_governor"
}

tune_nic_cpu() {
  local IFACE="$1"
  local CPU="$2"

  # 1) Find this interface's PCI device path
  local PCIDEV
  PCIDEV=$(readlink -f "/sys/class/net/${IFACE}/device")

  # 2) Pin MSI/MSI-X IRQs for this NIC to the chosen CPU
  for V in "${PCIDEV}"/msi_irqs/*; do
    local IRQ
    IRQ=$(basename "$V")
    echo "$CPU" > "/proc/irq/${IRQ}/smp_affinity_list" || true
  done
}

# Tune the EtherCAT NIC
set_cpu_performance "3"
tune_nic_cpu "enp1s0" "3"
ethtool -C "enp1s0" rx-usecs 0

# Tune the camera NIC
set_cpu_performance "2"
tune_nic_cpu "enp2s0" "2"