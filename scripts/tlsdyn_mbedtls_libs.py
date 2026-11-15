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

import shutil
from pathlib import Path

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
