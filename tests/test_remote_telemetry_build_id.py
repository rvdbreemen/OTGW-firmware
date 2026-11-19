"""Remote telemetry export decodes retained debug tokens with the exact firmware image."""
import base64
from contextlib import redirect_stderr, redirect_stdout
from io import StringIO
from pathlib import Path
import struct
import sys
from tempfile import TemporaryDirectory
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools" / "remote-telemetry"))
import decode_all
import esp_image
import otrec


class RemoteTelemetryBuildIdTest(unittest.TestCase):
    def test_export_selects_image_per_batch(self):
        with TemporaryDirectory() as tmp:
            root = Path(tmp)
            records = []
            images = []
            addr = esp_image.DROM_BASE
            dbg = otrec.dbg_encode(0, [("println", 0, addr + 256, addr + 252, 10)])
            payload = base64.b64encode(otrec.pack_mix(0, dbg=dbg)).decode()
            for seq, label in enumerate((b"old build", b"new build"), 1):
                image_path = root / f"fw-{seq}.bin"
                sha = bytes([seq]) * 32
                desc = struct.pack("<I", 0xABCD5432) + bytes(140) + sha + bytes(76)
                drom = desc + b"log\x00" + label + b"\x00"
                image_path.write_bytes(b"\xe9\x01" + bytes(22) + struct.pack("<II", addr, len(drom)) + drom)
                image = esp_image.EspImage(image_path)
                self.assertEqual(image.build_id, sha.hex())
                images.append(str(image_path))
                records.append({"seq": seq, "fmt": 4, "t0": 0, "payload": payload, "build_id": sha.hex()})

            out = root / "out"
            records.append({**records[0], "seq": 3, "build_id": ""})
            args = ["decode_all.py", "http://example.invalid", "admin", "pw", "otgw-0123456789ab",
                    str(out), "--tz", "UTC", "--image", images[0], "--image", images[1]]
            with patch.object(decode_all.eb, "api", side_effect=[{"token": "auth"}, {"items": [{"id": "device"}]}] * 2), \
                    patch.object(decode_all.eb, "fetch_batches", return_value=records):
                with patch.object(sys, "argv", args), redirect_stderr(StringIO()) as err:
                    with self.assertRaises(SystemExit):
                        decode_all.main()
                self.assertIn("supply --legacy-image", err.getvalue())
                self.assertFalse(out.exists())
                with patch.object(sys, "argv", args + ["--legacy-image", images[0]]), redirect_stdout(StringIO()):
                    decode_all.main()
            self.assertEqual([line.split(b": ")[-1] for line in (out / "debug.log").read_bytes().splitlines()],
                             [b"old build", b"new build", b"old build"])


if __name__ == "__main__":
    unittest.main()
