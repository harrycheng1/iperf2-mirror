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

import logging
import asyncio
import weakref
import os
import shlex
import time
from typing import List, Optional
from dataclasses import dataclass

logger = logging.getLogger(__name__)

# Standard SSH Options
SSH_OPTS = [
    '-o', 'StrictHostKeyChecking=no',
    '-o', 'UserKnownHostsFile=/dev/null',
    '-o', 'LogLevel=ERROR'
]

@dataclass
class SshResult:
    stdout: str = ""
    stderr: str = ""
    returncode: int = 0

class SshSession:
    """Represents a single execution of an SSH command."""
    def __init__(self, node: 'ssh_node', cmd: str, silent: bool = False, is_anchor: bool = False):
        self.node = node
        self.cmd = cmd
        self.silent = silent
        self.is_anchor = is_anchor
        self.results = bytearray()
        self._process: Optional[asyncio.subprocess.Process] = None

    async def run(self, timeout: Optional[float] = 30.0) -> bytes:
        full_cmd = self._build_ssh_command()
        logging.debug(f"[{self.node.name}] Exec: {shlex.join(full_cmd)}")

        try:
            self._process = await asyncio.create_subprocess_exec(
                *full_cmd,
                stdout=asyncio.subprocess.PIPE,
                stderr=asyncio.subprocess.PIPE
            )

            if timeout is None:
                await asyncio.gather(
                    self._read_stream(self._process.stdout, is_stderr=False),
                    self._read_stream(self._process.stderr, is_stderr=True)
                )
            else:
                await asyncio.wait_for(
                    asyncio.gather(
                        self._read_stream(self._process.stdout, is_stderr=False),
                        self._read_stream(self._process.stderr, is_stderr=True)
                    ),
                    timeout=timeout
                )

            await self._process.wait()
            return self.results

        except asyncio.TimeoutError:
            logging.error(f"[{self.node.name}] Timeout ({timeout}s) executing: {self.cmd}")
            if self._process:
                try:
                    self._process.kill()
                except ProcessLookupError: pass
            return self.results
        except Exception as e:
            logging.error(f"[{self.node.name}] Execution error: {e}")
            return self.results

    async def _read_stream(self, stream: asyncio.StreamReader, is_stderr: bool):
        if not stream: return
        while True:
            line = await stream.readline()
            if not line: break
            self.results.extend(line)
            decoded_line = line.decode('utf-8', errors='ignore').strip()

            if is_stderr:
                if "read kernel buffer failed" in decoded_line:
                    logging.debug(f"[{self.node.name}] {decoded_line}")
                else:
                    logging.warning(f"[{self.node.name}] {decoded_line}")
            elif not self.silent:
                logging.info(f"[{self.node.name}] {decoded_line}")

            if self.is_anchor and not is_stderr:
                self.node.on_console_output(decoded_line)

    def _build_ssh_command(self) -> List[str]:
        base = self.node.base_cmd.copy()
        if self.node.ssh_speedups:
            if self.is_anchor:
                if os.path.exists(self.node.control_socket):
                    try: os.remove(self.node.control_socket)
                    except OSError: pass
                base.extend(['-o', 'ControlMaster=yes', '-o', f'ControlPath={self.node.control_socket}', '-o', 'ControlPersist=yes'])
            else:
                base.extend(['-o', f'ControlPath={self.node.control_socket}'])
        target = f"{self.node.user}@{self.node.ipaddr}" if self.node.user else self.node.ipaddr
        base.append(target)
        base.append(self.cmd)
        return base

    async def close(self):
        """Forcefully closes the process with a timeout."""
        if self._process and self._process.returncode is None:
            try:
                self._process.terminate()
                try:
                    await asyncio.wait_for(self._process.wait(), timeout=1.0)
                except asyncio.TimeoutError:
                    self._process.kill()
                    await self._process.wait()
            except ProcessLookupError: pass

class ssh_node:
    instances = weakref.WeakSet()
    ANCHOR_CMD = "dmesg -w 2>/dev/null || tail -f /dev/null"

    def __init__(self, name: str, ipaddr: str, user: str = 'root',
                 device: str = None, devip: str = None,
                 ssh_speedups: bool = False, relay: str = None):
        self.name = name
        self.ipaddr = ipaddr
        self.user = user
        self.device = device
        self.devip = devip
        self.ssh_speedups = ssh_speedups
        self.control_socket = f'/tmp/cm_{self.ipaddr}_{self.user}'
        self.master_session: Optional[SshSession] = None
        self.master_task: Optional[asyncio.Task] = None

        # Attributes
        self.link_speed = "-"
        self.kernel = "-"
        self.wifi_stats = {}
        self.clock_source = "Unknown"

        self.base_cmd = ['/usr/bin/ssh'] + SSH_OPTS
        if relay: self.base_cmd.extend(['-J', f'root@{relay}'])
        ssh_node.instances.add(self)

    @property
    def loop(self): return asyncio.get_running_loop()

    def rexec(self, cmd: str, timeout: float = 30.0, run_now: bool = True) -> SshSession:
        session = SshSession(self, cmd)
        if run_now: asyncio.create_task(session.run(timeout))
        return session

    async def rexec_async(self, cmd: str, timeout: float = 30.0) -> bytes:
        session = SshSession(self, cmd)
        return await session.run(timeout)

    def on_console_output(self, line: str): pass

    async def check_time_source(self):
        try:
            cmd = "chronyc sources"
            raw = await self.rexec_async(cmd, timeout=5)
            output = raw.decode('utf-8', errors='ignore')

            has_gps = False
            has_pps = False
            has_ntp = False

            for line in output.split('\n'):
                if "PPS" in line: has_pps = True
                if "GPS" in line or "NMEA" in line or "GPZ" in line: has_gps = True
                if "." in line and "GPS" not in line and "PPS" not in line and "NMEA" not in line: has_ntp = True

            if has_pps and has_gps: self.clock_source = "GPS/PPS"
            elif has_gps: self.clock_source = "GPS"
            elif has_ntp: self.clock_source = "NTP"
            else: self.clock_source = "Unknown"
        except Exception as e: self.clock_source = "Unknown"

    async def clean_multiplex_socket(self):
        if os.path.exists(self.control_socket):
            exit_cmd = ['/usr/bin/ssh', '-O', 'exit', '-o', f'ControlPath={self.control_socket}', 'dummy']
            proc = await asyncio.create_subprocess_exec(*exit_cmd, stdout=asyncio.subprocess.DEVNULL, stderr=asyncio.subprocess.DEVNULL)
            await proc.wait()

        kill_cmd = "pkill -f 'dmesg -w'; pkill -f 'tail -f /dev/null'"
        temp_session = SshSession(self, kill_cmd)
        temp_session.node.ssh_speedups = False
        await temp_session.run(timeout=5)
        self.ssh_speedups = True

    @classmethod
    def get_instances(cls) -> List['ssh_node']: return list(cls.instances)

    @classmethod
    async def open_consoles_async(cls, silent_mode: bool = True):
        nodes = cls.get_instances()
        clean_tasks = []
        logging.info("Cleaning up existing SSH multiplex sockets...")
        for node in nodes:
            if node.ssh_speedups: clean_tasks.append(node.clean_multiplex_socket())
        if clean_tasks: await asyncio.gather(*clean_tasks)

        logging.info("Opening SSH Master connections (speedup)...")
        ready_nodes = []
        for node in nodes:
            if node.ssh_speedups and not node.master_task:
                node.master_session = SshSession(node, cls.ANCHOR_CMD, silent=silent_mode, is_anchor=True)
                node.master_task = asyncio.create_task(node.master_session.run(timeout=None))
                ready_nodes.append(node.name)
        if ready_nodes:
            await asyncio.sleep(2)
            logging.info(f"Consoles ready: {', '.join(ready_nodes)}")

    @classmethod
    def close_consoles(cls):
        try:
            loop = asyncio.get_running_loop()
            loop.create_task(cls.close_consoles_async())
        except RuntimeError:
            asyncio.run(cls.close_consoles_async())

    @classmethod
    async def close_consoles_async(cls):
        logging.info("Closing SSH consoles...")
        nodes = cls.get_instances()
        cleanup_tasks = []
        for node in nodes:
            if node.ssh_speedups:
                cleanup_tasks.append(node.clean_multiplex_socket())

        if cleanup_tasks:
            try: await asyncio.wait_for(asyncio.gather(*cleanup_tasks), timeout=5)
            except asyncio.TimeoutError: logging.warning("Timeout cleaning up SSH sockets.")

        tasks = []
        for node in nodes:
            if node.master_session: tasks.append(node.master_session.close())
        if tasks: await asyncio.gather(*tasks)
        logging.info("Consoles closed.")

class WiFiDut(ssh_node):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.wifi_stats = {"chip": "N/A", "driver": "N/A", "ssid": "N/A", "rssi": "N/A"}
        self.device_type = "Unknown"
        self.link_ready_event = asyncio.Event()

    def on_console_output(self, line: str):
        if self.device not in line: return
        if "link becomes ready" in line or "link up" in line or "associated" in line:
             if not self.link_ready_event.is_set():
                 self.link_ready_event.set()
                 logging.info(f"[{self.name}] EVENT: Link Up detected via dmesg.")

    async def connect(self, ssid: str, psk: str, band: str = None, timeout: int = 45) -> bool:
        """Connects using BSSID locking to ensure correct band."""
        self.link_ready_event.clear()

        freq_cond = "1"
        if band == "2.4G": freq_cond = "$2 < 3000"
        elif band == "5G": freq_cond = "$2 > 3000 && $2 < 5900"
        elif band == "6G": freq_cond = "$2 > 5900"

        cmd = (
            f"TARGET_BSSID=$(sudo nmcli -f BSSID,FREQ,SSID dev wifi list | "
            f"grep '{ssid}' | "
            f"awk '$2 ~ /^[0-9]+$/ {{if ({freq_cond}) {{print $1; exit}}}}' | head -n 1); "
            f"if [ ! -z \"$TARGET_BSSID\" ]; then "
            f"  sudo nmcli device wifi connect '{ssid}' password '{psk}' bssid \"$TARGET_BSSID\"; "
            f"else "
            f"  sudo nmcli device wifi connect '{ssid}' password '{psk}'; "
            f"fi"
        )

        result = await self.rexec_async(cmd, timeout=timeout)
        output = result.decode('utf-8', errors='ignore')
        return "successfully activated" in output

    async def update_stats(self):
        await asyncio.sleep(2)
        # Tightened Sed: "AMD Ryzen Threadripper PRO" -> "RyzenTR", remove " 16-Cores"
        cmd_model = (
            r"if [ -f /proc/device-tree/model ]; then "
            r"cat /proc/device-tree/model | tr -d '\0' | sed 's/Raspberry Pi/RPi/g'; "
            r"else grep -m1 'model name' /proc/cpuinfo | sed 's/model name\s*: //; s/AMD Ryzen Threadripper PRO/RyzenTR/g; s/ 16-Cores//g; s/Raspberry Pi/RPi/g'; fi"
        )

        cmd_chip = (
            f"bus=$(sudo ethtool -i {self.device} 2>/dev/null | grep bus-info | awk '{{print $2}}'); "
            f"drv=$(sudo ethtool -i {self.device} 2>/dev/null | grep driver | awk '{{print $2}}'); "
            f"[ -z \"$drv\" ] && drv=$(basename $(readlink /sys/class/net/{self.device}/device/driver) 2>/dev/null); "
            f"chip=$(lspci -s $bus 2>/dev/null | sed 's/^.*: //' | sed 's/Intel Corporation/INTC/g'); "
            f"if [ -z \"$chip\" ]; then echo \"$drv\"; else echo \"$chip ($drv)\"; fi"
        )

        cmd = (
            f"echo \"$({cmd_model})|"
            f"$({cmd_chip})|"
            f"$(sudo iw dev {self.device} link 2>/dev/null | grep SSID | cut -d: -f2 | sed 's/^ *//g')|"
            f"$(sudo iw dev {self.device} link 2>/dev/null | grep signal | awk '{{print $2}}')|"
            f"$(uname -r)\""
        )

        try:
            raw = await self.rexec_async(cmd, timeout=10)
            result = raw.decode('utf-8', errors='ignore').strip().split('\n')[-1]
            parts = result.split('|')
            if len(parts) >= 5:
                self.device_type = parts[0]
                self.wifi_stats = {
                    "chip": parts[1] if parts[1] else "Unknown",
                    "ssid": parts[2] if parts[2] else "Not Assoc",
                    "rssi": parts[3] + " dBm" if parts[3] else "N/A"
                }
                self.kernel = parts[4]
            else: self.device_type = "Parse Error"
        except Exception as e: self.device_type = f"Error: {e}"
