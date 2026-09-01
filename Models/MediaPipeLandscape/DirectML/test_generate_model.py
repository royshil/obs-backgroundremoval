# SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
# SPDX-License-Identifier: GPL-3.0-or-later

"""Regression checks against the cached, pinned model (no network access)."""

import ast
import pathlib
import subprocess
import sys
import tempfile
import unittest

import numpy as np
import onnx
from huggingface_hub import hf_hub_download
import onnxruntime as ort

from GenerateModel import (
    INPUT_SHAPE, OUTPUT_SHAPE, MODEL_FILENAME, MODEL_REPOSITORY, MODEL_REVISION, lower_model,
)


class ModelTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        path = hf_hub_download(
            repo_id=MODEL_REPOSITORY, revision=MODEL_REVISION, filename=MODEL_FILENAME,
            local_files_only=True,
        )
        cls.original = onnx.load(path)
        cls.lowered = lower_model(onnx.load(path))

    def test_same_source_as_coreml(self):
        source = pathlib.Path(__file__).parents[1] / "CoreML/GenerateModel.py"
        constants = {}
        for node in ast.parse(source.read_text()).body:
            if isinstance(node, ast.Assign) and isinstance(node.value, ast.Constant):
                for target in node.targets:
                    if isinstance(target, ast.Name):
                        constants[target.id] = node.value.value
        self.assertEqual(constants["MODEL_REPOSITORY"], MODEL_REPOSITORY)
        self.assertEqual(constants["MODEL_REVISION"], MODEL_REVISION)
        self.assertEqual(constants["MODEL_FILENAME"], MODEL_FILENAME)

    def test_generated_artifacts_are_reproducible(self):
        generator = pathlib.Path(__file__).with_name("GenerateModel.py")
        generated = generator.parents[3] / "src/BackgroundRemoval/windows/generated"
        names = [
            "MediaPipeLandscapeDmlProgram.generated.cpp",
            "MediaPipeLandscapeDmlProgram.generated.rc",
            "MediaPipeLandscapeDmlProgram.weights.bin",
        ]
        with tempfile.TemporaryDirectory() as directory:
            outputs = [pathlib.Path(directory) / name for name in names]
            subprocess.run([sys.executable, str(generator), "--offline", *map(str, outputs)], check=True)
            for name, output in zip(names, outputs):
                self.assertEqual(output.read_bytes(), (generated / name).read_bytes(), name)

    def test_planar_contract(self):
        graph = self.lowered.graph
        self.assertEqual([d.dim_value for d in graph.input[0].type.tensor_type.shape.dim], INPUT_SHAPE)
        self.assertEqual([d.dim_value for d in graph.output[0].type.tensor_type.shape.dim], OUTPUT_SHAPE)
        self.assertEqual(graph.node[0].op_type, "Conv")
        self.assertEqual(graph.node[0].input[0], graph.input[0].name)
        self.assertFalse({"HardSwish", "ReduceMean", "Shape", "Slice"}.intersection(n.op_type for n in graph.node))

    def test_lowering_preserves_alpha(self):
        options = ort.SessionOptions()
        options.graph_optimization_level = ort.GraphOptimizationLevel.ORT_DISABLE_ALL
        options.log_severity_level = 3
        original = ort.InferenceSession(self.original.SerializeToString(), options, providers=["CPUExecutionProvider"])
        lowered = ort.InferenceSession(self.lowered.SerializeToString(), options, providers=["CPUExecutionProvider"])
        rng = np.random.default_rng(42)
        for pixels in (np.zeros(INPUT_SHAPE, dtype=np.float32), rng.random(INPUT_SHAPE, dtype=np.float32)):
            feed = {self.original.graph.input[0].name: pixels}
            expected = original.run(None, feed)[0]
            actual = lowered.run(None, feed)[0]
            self.assertEqual(list(actual.shape), OUTPUT_SHAPE)
            np.testing.assert_allclose(actual, expected, rtol=2e-5, atol=2e-5)


if __name__ == "__main__":
    unittest.main()
