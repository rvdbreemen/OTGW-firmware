# TASK-1974 (research/cloud-recording): copy the rebuilt mbedTLS archives into the Arduino libs package.
#
# The pioarduino hybrid compile (custom_sdkconfig) rebuilds every IDF component, then copies only the archives that
# sit directly in each component's build folder into framework-arduinoespressif32-libs. mbedTLS keeps its real
# libraries one level deeper (mbedtls/mbedtls/library/...), so the link kept using the prebuilt mbedTLS of the
# Arduino release: none of the env's MBEDTLS_* settings (dynamic buffers, session tickets) reached the firmware,
# and esp-tls, rebuilt with them, failed to link (esp_mbedtls_dynamic_set_rx_buf_static undefined).
# This post action runs before pioarduino's own copy step (a pre: script registers its action first) and copies
# those archives under the names the Arduino link line uses.
Import("env")  # noqa: F821 (SCons)

import os
import shutil
from pathlib import Path

from SCons.Errors import UserError


# Fail before either SDK copy action can run, including on direct pio invocations.
isolated = os.environ.get("OTGW_TLSDYN_PACKAGES")
platform = env.PioPlatform()  # noqa: F821
for name in ("framework-arduinoespressif32", "framework-arduinoespressif32-libs"):
    package = platform.get_package_dir(name)
    if not isolated or not package or Path(package).resolve().parent != Path(isolated):
        raise UserError("Build esp32-combo-tlsdyn through build.bat/build.sh to isolate the SDK packages")

NESTED = {
    "mbedtls/mbedtls/library/libmbedtls.a": "libmbedtls_2.a",
    "mbedtls/mbedtls/library/libmbedcrypto.a": "libmbedcrypto.a",
    "mbedtls/mbedtls/library/libmbedx509.a": "libmbedx509.a",
    "mbedtls/mbedtls/3rdparty/everest/libeverest.a": "libeverest.a",
    "mbedtls/mbedtls/3rdparty/p256-m/libp256m.a": "libp256m.a",
}


def copy_nested_mbedtls(source, target, env):
    idf = Path(env.subst("$BUILD_DIR")) / "esp-idf"
    if not (idf / "mbedtls").is_dir():
        return                                    # the Arduino pass: nothing was rebuilt in this run
    pkg = env.PioPlatform().get_package_dir("framework-arduinoespressif32-libs")
    dst = Path(pkg) / env.BoardConfig().get("build.mcu") / "lib"
    for src, name in NESTED.items():
        if (idf / src).is_file():
            shutil.copyfile(idf / src, dst / name)
            print(f"*** tlsdyn: {src} -> {name} ***")


env.AddPostAction("checkprogsize", copy_nested_mbedtls)  # noqa: F821

# TASK-1963: the env no longer uses dynamic mbedTLS buffers, so the link carries none of IDF's
# CONFIG_MBEDTLS_DYNAMIC_BUFFER --wrap flags (they would point at __wrap_ functions that are not built).
