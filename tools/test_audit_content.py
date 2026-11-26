import importlib.util
from pathlib import Path
import tempfile
import unittest

spec = importlib.util.spec_from_file_location("audit_content", Path(__file__).with_name("audit_content.py"))
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)

class ContentInventoryTests(unittest.TestCase):
    def test_missing_content_fails(self):
        with tempfile.TemporaryDirectory() as folder:
            with self.assertRaises(ValueError):
                module.inventory(folder)

    def test_inventory_is_deterministic_read_only_and_not_license_approval(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            content = root / "Content"
            content.mkdir()
            (content / "b.uasset").write_bytes(b"model")
            (content / "a.png").write_bytes(b"texture")
            (content / "notes.md").write_text("not an asset")
            result = module.inventory(root)
            self.assertEqual(result, module.inventory(root))
            self.assertEqual(result["asset_count"], 2)
            self.assertEqual(result["total_bytes"], 12)
            self.assertEqual(result["assets"][0]["path"], "Content/a.png")
            self.assertTrue(all(len(a["sha256"]) == 64 for a in result["assets"]))
            self.assertTrue(all(a["license_status"] == "requires_documented_review" for a in result["assets"]))
            self.assertEqual((content / "b.uasset").read_bytes(), b"model")

if __name__ == "__main__":
    unittest.main()
