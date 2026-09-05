# SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
# SPDX-License-Identifier: GPL-3.0-or-later

"""Regression checks against the cached, pinned model (no network access)."""

import ast
import itertools
import pathlib
import re
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

import numpy as np
import onnx
from huggingface_hub import hf_hub_download
import onnxruntime as ort

from GenerateModel import (
    INPUT_SHAPE, OUTPUT_SHAPE, MODEL_FILENAME, MODEL_REPOSITORY, MODEL_REVISION,
    emit_concat, emit_transpose, lower_model, main,
)


class TransposeTests(unittest.TestCase):
    def test_rank_specific_permutations(self):
        for rank in (2, 3, 4):
            shape = list(range(2, rank + 2))
            for permutation in itertools.permutations(range(rank)):
                with self.subTest(rank=rank, permutation=permutation):
                    entries = ["Input{0}"]
                    output = [shape[axis] for axis in permutation]
                    self.assertEqual(emit_transpose(entries, 0, shape, list(permutation), output), 1)
                    axes = ", ".join(f"{axis}u" for axis in permutation)
                    self.assertEqual(entries[1], f"TransposeRank{rank}{{0, {{{axes}}}}}")

    def test_default_permutation_and_rank_one_alias(self):
        for rank in (1, 2, 3, 4):
            shape = list(range(2, rank + 2))
            entries = ["Input{0}"]
            result = emit_transpose(entries, 0, shape, None, shape[::-1])
            self.assertEqual(result, 0 if rank == 1 else 1)
            if rank == 1:
                self.assertEqual(entries, ["Input{0}"])
            else:
                axes = ", ".join(f"{axis}u" for axis in reversed(range(rank)))
                self.assertEqual(entries[1], f"TransposeRank{rank}{{0, {{{axes}}}}}")
        entries = ["Input{0}"]
        self.assertEqual(emit_transpose(entries, 0, [3], [0], [3]), 0)
        self.assertEqual(entries, ["Input{0}"])

    def test_invalid_transpose_is_rejected(self):
        cases = [
            ([], None, []),
            ([1] * 5, None, [1] * 5),
            ([2, 3], [0, 0], [2, 2]),
            ([2, 3], [0], [2]),
            ([2, 3], [-1, 0], [3, 2]),
            ([2, 3], [0, 2], [2, 3]),
            ([2, 3], [1, 0], [2, 3]),
        ]
        for shape, permutation, output in cases:
            with self.subTest(shape=shape, permutation=permutation):
                entries = ["Input{0}"]
                with self.assertRaises(RuntimeError):
                    emit_transpose(entries, 0, shape, permutation, output)
                self.assertEqual(entries, ["Input{0}"])

    def test_generator_connects_constant_transpose_and_following_node(self):
        for rank in (1, 2, 3, 4):
            with self.subTest(rank=rank):
                shape = list(range(2, rank + 2))
                def value(name, sizes):
                    return onnx.helper.make_tensor_value_info(name, onnx.TensorProto.FLOAT, sizes)

                model = onnx.helper.make_model(onnx.helper.make_graph(
                    [onnx.helper.make_node("Transpose", ["weight"], ["transposed"]),
                     onnx.helper.make_node("Relu", ["transposed"], ["out"])],
                    "transpose", [value("x", INPUT_SHAPE)], [value("out", shape[::-1])],
                    initializer=[onnx.numpy_helper.from_array(np.ones(shape, dtype=np.float32), "weight")],
                    value_info=[value("transposed", shape[::-1])],
                ))
                with tempfile.TemporaryDirectory() as directory:
                    paths = [pathlib.Path(directory) / name for name in ("graph.cpp", "graph.rc", "weights.bin")]
                    with (mock.patch("sys.argv", ["GenerateModel.py", "--offline", *map(str, paths)]),
                          mock.patch("GenerateModel.hf_hub_download", return_value="unused.onnx"),
                          mock.patch("GenerateModel.onnx.load", return_value=model),
                          mock.patch("GenerateModel.lower_model", return_value=model)):
                        main()
                    source = paths[0].read_text()
                if rank == 1:
                    self.assertNotIn("TransposeRank", source)
                    self.assertIn("Relu{1}", source)
                    self.assertIn("3LL, graphNodes,\n\t2,", source)
                else:
                    axes = ", ".join(f"{axis}u" for axis in reversed(range(rank)))
                    self.assertIn(f"TransposeRank{rank}{{1, {{{axes}}}}}", source)
                    self.assertIn("Relu{2}", source)
                    self.assertIn("4LL, graphNodes,\n\t3,", source)


class ConcatTests(unittest.TestCase):
    def test_single_input_is_an_alias(self):
        entries = ["Input{0}"]
        self.assertEqual(emit_concat(entries, [0], [[1, 2, 3, 4]], 1, [1, 2, 3, 4]), 0)
        self.assertEqual(entries, ["Input{0}"])

    def test_binary_and_variadic_preserve_values_and_shapes(self):
        for count in (2, 3, 5):
            for axis in range(-4, 4):
                with self.subTest(count=count, axis=axis):
                    shapes = [[1, 2, 3, 4] for _ in range(count)]
                    for index, shape in enumerate(shapes):
                        shape[axis] = index + 1
                    values = [np.full(shape, index, dtype=np.float32)
                              for index, shape in enumerate(shapes)]
                    expected = np.concatenate(values, axis=axis)
                    entries = [f"Input{{{index}}}" for index in range(count)]
                    result = emit_concat(entries, list(range(count)), shapes, axis, list(expected.shape))
                    self.assertEqual(len(entries), 2 * count - 1)
                    self.assertEqual(result, len(entries) - 1)
                    for entry in entries[count:]:
                        match = re.fullmatch(r"Concat\{(\d+), (\d+), (\d+)u, \{([\d u,]+)\}\}", entry)
                        self.assertIsNotNone(match)
                        left, right, normalized_axis = map(int, match.group(1, 2, 3))
                        self.assertLess(left, len(values))
                        self.assertLess(right, len(values))
                        value = np.concatenate([values[left], values[right]], axis=normalized_axis)
                        shape = tuple(int(size.strip().removesuffix("u"))
                                      for size in match.group(4).split(","))
                        self.assertEqual(value.shape, shape)
                        values.append(value)
                    np.testing.assert_array_equal(values[result], expected)

    def test_invalid_concat_is_rejected_without_emitting_nodes(self):
        cases = [
            ([], [], 0, [1, 2, 3, 4]),
            ([0], [], 0, [1, 2, 3, 4]),
            ([0], [[1, 2, 3]], 0, [1, 2, 3, 4]),
            ([0], [[1, 2, 3, 4]], 4, [1, 2, 3, 4]),
            ([0], [[1, 2, 3, 4]], -5, [1, 2, 3, 4]),
            ([0, 1], [[1, 2, 3, 4], [1, 2, 5, 4]], 1, [1, 4, 3, 4]),
            ([0], [[1, 2, 3, 4]], 1, [1, 3, 3, 4]),
            ([0], [[1, 0, 3, 4]], 1, [1, 0, 3, 4]),
            ([0, 1], [[1, 2**31, 3, 4]] * 2, 1, [1, 2**32, 3, 4]),
        ]
        for inputs, shapes, axis, output in cases:
            with self.subTest(inputs=inputs, shapes=shapes, axis=axis):
                entries = ["Input{0}", "Input{1}"]
                with self.assertRaises(RuntimeError):
                    emit_concat(entries, inputs, shapes, axis, output)
                self.assertEqual(entries, ["Input{0}", "Input{1}"])

    def test_generator_connects_alias_constants_and_following_nodes(self):
        def value(name, channels):
            return onnx.helper.make_tensor_value_info(name, onnx.TensorProto.FLOAT, [1, channels, 144, 256])

        model = onnx.helper.make_model(onnx.helper.make_graph(
            [onnx.helper.make_node("Concat", ["x"], ["alias"], axis=1),
             onnx.helper.make_node("Concat", ["alias", "weight", "x"], ["joined"], axis=-3),
             onnx.helper.make_node("Relu", ["joined"], ["out"])],
            "concat", [value("x", 3)], [value("out", 7)],
            initializer=[onnx.numpy_helper.from_array(np.ones((1, 1, 144, 256), dtype=np.float32), "weight")],
            value_info=[value("alias", 3), value("joined", 7)],
        ))
        with tempfile.TemporaryDirectory() as directory:
            paths = [pathlib.Path(directory) / name for name in ("graph.cpp", "graph.rc", "weights.bin")]
            with (mock.patch("sys.argv", ["GenerateModel.py", "--offline", *map(str, paths)]),
                  mock.patch("GenerateModel.hf_hub_download", return_value="unused.onnx"),
                  mock.patch("GenerateModel.onnx.load", return_value=model),
                  mock.patch("GenerateModel.lower_model", return_value=model)):
                main()
            source = paths[0].read_text()
        self.assertIn("Constant{0LL, 4, {1u, 1u, 144u, 256u}}", source)
        self.assertIn("Concat{0, 1, 1u, {1u, 4u, 144u, 256u}}", source)
        self.assertIn("Concat{2, 0, 1u, {1u, 7u, 144u, 256u}}", source)
        self.assertIn("Relu{3}", source)
        self.assertIn("5LL, graphNodes,\n\t4,", source)


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
