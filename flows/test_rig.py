# ---------------------------------------------------------------
# * Copyright (c) 2025
# * Umber Networks
# * All Rights Reserved.
# *---------------------------------------------------------------
# Redistribution and use in source and binary forms, with or without modification, are permitted
# provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice, this list of conditions
#    and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions
#    and the following disclaimer in the documentation and/or other materials provided with the distribution.
#
# 3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or
#    promote products derived from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
# FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
# IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
# OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
# Author Robert J. McMahon, Umber Networks
# Date December 2025
# Refactored for Modern Python 3.10+ and Asyncio

import asyncio
from ssh_nodes import ssh_node, WiFiDut

# Robust Model Command: Replaced TR with RyzenTR
CMD_GET_MODEL = (
    r"if [ -f /proc/device-tree/model ]; then "
    r"cat /proc/device-tree/model | tr -d '\0' | sed 's/Raspberry Pi/RPi/g'; "
    r"else grep -m1 'model name' /proc/cpuinfo | sed 's/model name\s*: //; s/AMD Ryzen Threadripper PRO/RyzenTR/g; s/ 16-Cores//g; s/Raspberry Pi/RPi/g'; fi"
)

USER = 'rjmcmahon'

class TestRig:
    def __init__(self):
        self.wired_no_gps = ssh_node(name='Wired_NoGPS', ipaddr='192.168.1.101', user=USER, device='eth0', devip='192.168.1.101', ssh_speedups=True)
        self.wired_no_gps.device_type = "Unknown"
        self.wired_gps = ssh_node(name='Wired_GPS', ipaddr='192.168.1.50', user=USER, device='eth0', devip='192.168.1.50', ssh_speedups=True)
        self.wired_gps.device_type = "Unknown"

        self.wifi_1 = WiFiDut(name='WiFi_41', ipaddr='192.168.1.41', user=USER, device='wlan0', devip='192.168.1.41', ssh_speedups=True)
        self.wifi_2 = WiFiDut(name='WiFi_42', ipaddr='192.168.1.42', user=USER, device='wlan0', devip='192.168.1.42', ssh_speedups=True)
        self.wifi_3 = WiFiDut(name='WiFi_44', ipaddr='192.168.1.44', user=USER, device='wlan0', devip='192.168.1.44', ssh_speedups=True)

    def get_all_nodes(self):
        return [self.wired_no_gps, self.wired_gps, self.wifi_1, self.wifi_2, self.wifi_3]

    def _format_speed(self, raw_speed):
        if not raw_speed or "Unknown" in raw_speed or raw_speed == "-" or raw_speed == "": return "-"
        try:
            val_str = raw_speed.replace('Mb/s', '').strip()
            val = int(val_str)
            if val >= 1000:
                if val % 1000 == 0: return f"{val // 1000}G"
                else: return f"{val / 1000}G"
            return f"{val}M"
        except ValueError: return raw_speed

    async def associate(self, ssid, psk, band=None):
        band_msg = f" ({band})" if band else ""
        print(f"[*] Associating Wi-Fi nodes to SSID: '{ssid}'{band_msg}...")
        await ssh_node.open_consoles_async(silent_mode=True)

        connect_tasks = []
        duts = [n for n in self.get_all_nodes() if isinstance(n, WiFiDut)]
        for dut in duts: connect_tasks.append(dut.connect(ssid, psk, band=band))
        await asyncio.gather(*connect_tasks, return_exceptions=True)

        print(f"[*] Waiting for Kernel Link-Up Events (dmesg)...")
        wait_tasks = []
        for dut in duts: wait_tasks.append(asyncio.wait_for(dut.link_ready_event.wait(), timeout=15))
        try: await asyncio.gather(*wait_tasks)
        except asyncio.TimeoutError: pass

        for dut in duts:
            status = "Ready (Event)" if dut.link_ready_event.is_set() else "Timeout/No Event"
            print(f"    {dut.name:<10}: {status}")
        print("")

    async def discover(self, target_ssid=None):
        print(f"[*] Starting Extended Discovery on {len(self.get_all_nodes())} nodes...")
        await ssh_node.open_consoles_async(silent_mode=True)

        tasks = []
        for node in self.get_all_nodes():
            tasks.append(node.check_time_source())

            if isinstance(node, WiFiDut):
                tasks.append(node.update_stats())
            else:
                async def _wired_disc(n):
                    try:
                        cmd_find_iface = f"ip -o addr show to {n.ipaddr} | awk '{{print $2}}'"
                        raw_iface = await n.rexec_async(cmd_find_iface, timeout=5)
                        real_iface = raw_iface.decode('utf-8', errors='ignore').strip()
                        if real_iface: n.device = real_iface

                        cmd_speed = (
                            f"if [ -f /sys/class/net/{n.device}/speed ]; then "
                            f"cat /sys/class/net/{n.device}/speed; "
                            f"else sudo ethtool {n.device} 2>/dev/null | grep Speed | awk '{{print $2}}'; fi"
                        )
                        cmd = f"echo \"$({CMD_GET_MODEL})|$({cmd_speed})|$(uname -r)\""

                        raw = await n.rexec_async(cmd, timeout=10)
                        res = raw.decode('utf-8', errors='ignore').strip().split('\n')[-1]
                        parts = res.split('|')

                        n.device_type = parts[0] if parts[0] else "Unknown"
                        n.link_speed = self._format_speed(parts[1]) if len(parts) > 1 else "-"
                        n.kernel = parts[2] if len(parts) > 2 else "-"

                    except Exception as e:
                        n.device_type = f"Error: {e}"
                        n.link_speed = "Err"
                tasks.append(_wired_disc(node))

        await asyncio.gather(*tasks)

        header = f"{'Node Name':<12} | {'IP Address':<14} | {'Chip (Driver)':<30} | {'SSID':<15} | {'RSSI':<6} | {'Speed':<6} | {'Clock':<10} | {'Kernel':<20} | {'Device Model'}"
        print("\n" + header)
        print("-" * len(header))

        mismatch_found = False
        for node in self.get_all_nodes():
            chip_str = node.wifi_stats.get('chip', '-')[:29]
            ssid_val = node.wifi_stats.get('ssid', '-')
            rssi_val = node.wifi_stats.get('rssi', '-')

            ssid_display = ssid_val
            if target_ssid and ssid_val != target_ssid and ssid_val != "Not Assoc" and ssid_val != "-":
                ssid_display = f"*{ssid_val}"
                mismatch_found = True

            print(f"{node.name:<12} | {node.ipaddr:<14} | {chip_str:<30} | {ssid_display:<15} | {rssi_val:<6} | {node.link_speed:<6} | {node.clock_source:<10} | {node.kernel:<20} | {node.device_type}")

        if mismatch_found:
            print(f"\n* Indicates SSID mismatch (Expected: {target_ssid})")

    async def close(self):
        await ssh_node.close_consoles_async()
