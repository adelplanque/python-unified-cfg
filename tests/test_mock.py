import cfg
import cfg.testing
import importlib
import sys
import unittest

if sys.version_info[0] == 2:
    import importlib_resources as resources
else:
    from importlib import resources


data_path = (
    resources.files(importlib.import_module(".".join(__name__.split(".")[:-1])))
    / "data"
    / "test_mock"
)


class Test(unittest.TestCase):

    def test(self):
        cfg.set_config_path(str(data_path))
        with cfg.testing.mock({"config.group.value": "1"}):
            self.assertEqual(cfg.settings.config.group.value, "1")
        self.assertEqual(cfg.settings.config.group.value, "5")
