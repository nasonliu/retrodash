PY ?= python3
.PHONY: test project fw

test:
	PYTHON=$(PY) sh tests/run_retrodash.sh

project:
	$(PY) tools/build_project.py projects/peaceful-days
	$(PY) tools/pack_ui.py
	$(PY) tools/gen_spc_firmware.py

fw:
	PYTHON=$(PY) sh build.sh
