# can-repeater-firmware

## Setup
1. Clone this repository (`git clone https://github.com/sufst/can-repeater-firmware`)
2. Init submodules (`git submodule init`)
3. Update submodules (`git submodule update`)

## How to flash
### New ECU
If this is a pre-existing board, skip to the next section.

1. Go to [can-defs](https://github.com/sufst/can-defs) and add a heartbeat message for the board. 
2. You can copy one of the exiting ones ([example](https://github.com/sufst/can-defs/blob/5e8123e9d285b7290c080ce90582ca8007638b85/dbc/CAN-S.dbc#L454-L466)), then change the prefix of the message name and signals, and pick another message ID.
3. Then, run tsgen (`uv run tsgen/main.py`)
4. Commit and push your changes
5. In this repository (can-repeater-firmware), go to SUFST/Middlewares/can-defs/, `git pull` the commit you just pushed (if you get any errors, you might need to `git checkout main` first)
6. Then, go back to the root of can-repeater-firmware, run `git status` then add the new can-defs commit, then commit and push it to this repository

### Existing ECU
1. Edit `SUFST/Inc/can_config.h`, setting `STATUS_CAN_ID` to the ID of the message you just added (e.g. `0x7E1`)
2. Run `make -j8` to build the project
3. Connect the board using an ST-Link and run `make flash`
