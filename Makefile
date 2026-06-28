.PHONY: all configure build flash clean

BUILD_DIR = build
TOOLCHAIN = cmake/gcc-arm-none-eabi.cmake
CMAKE := $(shell which cmake)

all build: $(BUILD_DIR)/CMakeCache.txt
	$(CMAKE) --build $(BUILD_DIR)

$(BUILD_DIR)/CMakeCache.txt:
	$(CMAKE) -S . -B $(BUILD_DIR) -DCMAKE_TOOLCHAIN_FILE=$(TOOLCHAIN)

flash:
	$(CMAKE) --build $(BUILD_DIR) --target flash

clean:
	rm -rf $(BUILD_DIR)
