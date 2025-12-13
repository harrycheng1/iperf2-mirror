#!/usr/bin/env python3
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

import asyncio
import logging
import sys
import os
import argparse

sys.path.append(os.getcwd())

from test_rig import TestRig
from ssh_nodes import ssh_node

# Configure Logging (suppress low-level noise)
logging.basicConfig(level=logging.WARNING, format='%(name)s: %(message)s')
logger = logging.getLogger("Discovery")
logger.setLevel(logging.INFO)

async def main():
    parser = argparse.ArgumentParser(description='Umber Networks Test Rig Discovery & Association')
    parser.add_argument('--ssid', type=str, help='SSID to associate Wi-Fi nodes to')
    parser.add_argument('--password', type=str, help='Wi-Fi Password (PSK)')
    parser.add_argument('--band', type=str, choices=['2.4G', '5G', '6G'], help='Wi-Fi Band Lock')
    parser.add_argument('--set-hostname', action='store_true', help='Update the system hostname of each node to match its Node Name')
    args = parser.parse_args()

    # 1. Setup Asyncio Loop for ssh_nodes
    loop = asyncio.new_event_loop()
    asyncio.set_event_loop(loop)
    ssh_node._loop = loop

    print("--- Umber Networks Test Rig Discovery ---")

    # 2. Initialize Rig
    try:
        rig = TestRig()
    except Exception as e:
        print(f"Error initializing TestRig: {e}")
        return

    try:
        # 3. Associate (Optional)
        if args.ssid and args.password:
            await rig.associate(args.ssid, args.password, band=args.band)

        elif args.ssid or args.password:
            print("[!] Warning: Both --ssid and --password are required for association. Skipping.")

        # 4. Run Discovery
        await rig.discover(target_ssid=args.ssid)

        # 5. Set Hostnames (Optional)
        if args.set_hostname:
            await rig.set_hostnames()

    except KeyboardInterrupt:
        print("\nDiscovery cancelled by user.")
    except Exception as e:
        print(f"\nDiscovery failed: {e}")
    finally:
        # 6. Cleanup
        print("\nClosing connections...")
        await rig.close()

    print("Done.")

if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        pass
