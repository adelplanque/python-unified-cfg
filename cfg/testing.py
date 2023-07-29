import cfg
import shutil
import sys
import tempfile

if sys.version_info[0] == 2:
    import pathlib2 as pathlib
else:
    import pathlib


class mock(object):
    """
    Override some configuration values.
    """

    def __init__(self, data):
        files = {}
        for k, v in data.items():
            toks = k.split(".")
            if len(toks) < 3:
                raise ValueError("Invalid key %s" % k)
            if any(
                x in files
                for x in (
                    pathlib.Path(*toks[:i]).with_suffix(".ini")
                    for i in range(1, len(toks) - 2)
                )
            ):
                raise ValueError("Inconsistent keys")
            files.setdefault(
                pathlib.Path(*toks[:-2]).with_suffix(".ini"), {}
            ).setdefault(toks[-2], {}).setdefault(toks[-1], v)
        self._files = files

    def __enter__(self):
        self._config_path = cfg.get_config_path()
        print(self._config_path)
        self._tmp_path = pathlib.Path(tempfile.mkdtemp())
        for filename, data in self._files.items():
            with (self._tmp_path / filename).open("w") as f:
                for group, values in data.items():
                    f.write(u"[%s]\n" % group)
                    for item in values.items():
                        f.write(u"%s = %s\n" % item)
        cfg.set_config_path([str(self._tmp_path)] + self._config_path)

    def __exit__(self, exc_type, exc_value, exc_traceback):
        shutil.rmtree(str(self._tmp_path))
        cfg.set_config_path(self._config_path)
