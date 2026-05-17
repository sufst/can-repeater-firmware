import can
import cantools
import time
import os
import argparse

# --- CONFIGURATION ---
# Get the exact folder path where this python script lives
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))

# Build the absolute paths to the DBC files
DBC_FILE_CAN1 = os.path.join(SCRIPT_DIR, "can1_fast.dbc")
DBC_FILE_CAN2 = os.path.join(SCRIPT_DIR, "can2_slow.dbc")

# Constants matching our STM32 firmware
CONFIG_CAN_ID = 0x7E0
CMD_CLEAR_LIST = 0x00
CMD_ADD_ID = 0x01
CMD_SET_STATE = 0x03

TARGET_LIST_CAN1_TO_2 = 0x01
TARGET_LIST_CAN2_TO_1 = 0x02

STATE_NORMAL = 0x00
STATE_BLOCK_ALL = 0x01


# ---------------------

def send_config_message(bus, cmd, target_list, can_id):
    """Packs the 8-byte payload and sends it over CAN"""
    data = [
        cmd,
        target_list,
        (can_id >> 24) & 0xFF,
        (can_id >> 16) & 0xFF,
        (can_id >> 8) & 0xFF,
        can_id & 0xFF,
        0x00, 0x00  # Padding
    ]

    msg = can.Message(arbitration_id=CONFIG_CAN_ID, data=data, is_extended_id=False)
    bus.send(msg)
    # STM32 batches saves now, no delay needed


def set_repeater_state(bus, state):
    print(f"Setting repeater state to: {state}")
    data = [
        CMD_SET_STATE,
        0x00,
        state,
        0x00, 0x00, 0x00, 0x00, 0x00
    ]
    msg = can.Message(arbitration_id=CONFIG_CAN_ID, data=data, is_extended_id=False)
    bus.send(msg)


def configure_repeater(args):
    # 1. Verify files exist before starting (only if not blocking)
    if not args.block and not args.normal and (not os.path.exists(DBC_FILE_CAN1) or not os.path.exists(DBC_FILE_CAN2)):
        print("Error: Could not find the .dbc files. Check the filenames!")
        return

    # 2. Connect to the CAN bus
    try:
        if args.channel.isdigit():
            channel = int(args.channel)
        else:
            channel = args.channel
        bus = can.interface.Bus(interface=args.interface, channel=channel, bitrate=args.bitrate)
        print(f"Connected to CAN bus (interface: {args.interface}, channel: {args.channel}, bitrate: {args.bitrate}).")
    except Exception as e:
        print(f"Failed to connect to CAN bus: {e}")
        return

    if args.block:
        set_repeater_state(bus, STATE_BLOCK_ALL)
        print("SUCCESS: Repeater set to Block All mode.")
        bus.shutdown()
        return
        
    if args.normal:
        set_repeater_state(bus, STATE_NORMAL)
        print("SUCCESS: Repeater set to Normal mode.")
        bus.shutdown()
        return

    # 3. Load the DBC files
    print(f"Loading {DBC_FILE_CAN1}...")
    db_can1 = cantools.database.load_file(DBC_FILE_CAN1)

    print(f"Loading {DBC_FILE_CAN2}...")
    db_can2 = cantools.database.load_file(DBC_FILE_CAN2)

    # 4. Clear BOTH lists on the repeater
    print("\nWiping old configuration from STM32 Flash...")
    send_config_message(bus, CMD_CLEAR_LIST, TARGET_LIST_CAN1_TO_2, 0)
    send_config_message(bus, CMD_CLEAR_LIST, TARGET_LIST_CAN2_TO_1, 0)

    # 5. Program CAN1 -> CAN2 Routing
    print("\n--- Programming CAN1 to CAN2 Routing ---")
    for message in db_can1.messages:
        # Ignore our own config/status messages if they are in the DBC
        if message.frame_id in [CONFIG_CAN_ID, 0x7E1]:
            continue

        print(f"Adding ID: {hex(message.frame_id)} ({message.name}) to CAN1->CAN2")
        send_config_message(bus, CMD_ADD_ID, TARGET_LIST_CAN1_TO_2, message.frame_id)

    # 6. Program CAN2 -> CAN1 Routing
    print("\n--- Programming CAN2 to CAN1 Routing ---")
    for message in db_can2.messages:
        # Ignore our own config/status messages if they are in the DBC
        if message.frame_id in [CONFIG_CAN_ID, 0x7E1]:
            continue

        print(f"Adding ID: {hex(message.frame_id)} ({message.name}) to CAN2->CAN1")
        send_config_message(bus, CMD_ADD_ID, TARGET_LIST_CAN2_TO_1, message.frame_id)

    # Make sure we're in normal mode
    set_repeater_state(bus, STATE_NORMAL)

    print("\nSUCCESS: Repeater update complete!")
    bus.shutdown()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Configure the CAN Repeater dynamically.")
    parser.add_argument("--interface", default="kvaser", help="CAN interface type (e.g., kvaser, pcan, slcan, socketcan).")
    parser.add_argument("--channel", default="0", help="CAN channel (e.g., 0, PCAN_USBBUS1, vcan0).")
    parser.add_argument("--bitrate", type=int, default=1000000, help="Bitrate for the config bus connection.")
    parser.add_argument("--block", action="store_true", help="Instantly block all routing traffic.")
    parser.add_argument("--normal", action="store_true", help="Instantly return to normal routing mode.")
    
    args = parser.parse_args()
    configure_repeater(args)