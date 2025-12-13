#!/usr/bin/env python3
# ---------------------------------------------------------------
# * Copyright (c) 2025
# * Umber Networks
# * All Rights Reserved.
# *---------------------------------------------------------------
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

    except KeyboardInterrupt:
        print("\nDiscovery cancelled by user.")
    except Exception as e:
        print(f"\nDiscovery failed: {e}")
    finally:
        # 5. Cleanup
        print("\nClosing connections...")
        # FIX: Await the async close method
        await rig.close()

    print("Done.")

if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        pass
