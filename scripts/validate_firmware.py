Import("env")

from pathlib import Path


ESP_IMAGE_MAGIC = 0xE9
ESP_APP_DESC_MAGIC = bytes((0x32, 0x54, 0xCD, 0xAB))
ESP_APP_DESC_OFFSET = 0x20
OTA_SLOT_SIZE = 0x640000


def validate_firmware(source, target, env):
    firmware_path = Path(str(target[0]))
    firmware = firmware_path.read_bytes()

    if len(firmware) >= OTA_SLOT_SIZE:
        raise RuntimeError(
            f"Application image is {len(firmware)} bytes; OTA slot limit is "
            f"{OTA_SLOT_SIZE - 1} bytes"
        )

    if not firmware or firmware[0] != ESP_IMAGE_MAGIC:
        raise RuntimeError("Artifact is not an ESP application image")

    descriptor_end = ESP_APP_DESC_OFFSET + len(ESP_APP_DESC_MAGIC)
    if firmware[ESP_APP_DESC_OFFSET:descriptor_end] != ESP_APP_DESC_MAGIC:
        raise RuntimeError("ESP application descriptor is missing at offset 0x20")

    print(
        f"TamaInk artifact validated: application-only image, "
        f"{len(firmware)} / {OTA_SLOT_SIZE} bytes"
    )


env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", validate_firmware)

