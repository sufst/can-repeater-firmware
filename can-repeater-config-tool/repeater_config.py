import can
import cantools
import time
import os

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

TARGET_LIST_CAN1_TO_2 = 0x01
TARGET_LIST_CAN2_TO_1 = 0x02


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
    time.sleep(0.05)  # Crucial: Give the STM32 50ms to save to Flash!


def configure_repeater():
    # 1. Verify files exist before starting
    if not os.path.exists(DBC_FILE_CAN1) or not os.path.exists(DBC_FILE_CAN2):
        print("Error: Could not find the .dbc files. Check the filenames!")
        return

    # 2. Connect to the CAN bus
    # Note: Adjust 'bustype' and 'channel' for your specific USB-CAN adapter
    try:
        bus = can.interface.Bus(interface='kvaser', channel=0, bitrate=1000000)
        print("Connected to CAN bus.")
    except Exception as e:
        print(f"Failed to connect to CAN bus: {e}")
        # Note: If testing without hardware, change bustype='virtual', channel='vcan0'
        return

    # 3. Load the DBC files
    print(f"Loading {DBC_FILE_CAN1}...")
    db_can1 = cantools.database.load_file(DBC_FILE_CAN1)

    print(f"Loading {DBC_FILE_CAN2}...")
    db_can2 = cantools.database.load_file(DBC_FILE_CAN2)

    # 4. Clear BOTH lists on the repeater
    print("\nWiping old configuration from STM32 Flash...")
    send_config_message(bus, CMD_CLEAR_LIST, TARGET_LIST_CAN1_TO_2, 0)
    time.sleep(0.5)  # Extra time for Flash erase

    send_config_message(bus, CMD_CLEAR_LIST, TARGET_LIST_CAN2_TO_1, 0)
    time.sleep(0.5)

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

    print("\nSUCCESS: Repeater update complete!")
    bus.shutdown()


if __name__ == "__main__":
    configure_repeater()