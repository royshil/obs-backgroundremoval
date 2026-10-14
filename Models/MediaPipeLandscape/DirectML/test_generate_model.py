# SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
# SPDX-License-Identifier: GPL-3.0-or-later

"""Regression checks against the cached, pinned model (no network access)."""

import ast
import re
import pathlib
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
    lower_model, main,
)


class GeneratorTests(unittest.TestCase):
    def generate(self, model):
        with tempfile.TemporaryDirectory() as directory:
            paths = [pathlib.Path(directory) / name for name in ("graph.cpp", "weights.bin")]
            with (mock.patch("sys.argv", ["GenerateModel.py", "--offline", *map(str, paths)]),
                  mock.patch("GenerateModel.hf_hub_download", return_value="unused.onnx"),
                  mock.patch("GenerateModel.onnx.load", return_value=model),
                  mock.patch("GenerateModel.lower_model", return_value=model)):
                main()
            source, weights = paths[0].read_text(), paths[1].read_bytes()
            literal = re.search(r'constexpr char embeddedWeights\[\] =\s*(.*?);', source, re.S).group(1)
            encoded = "".join(re.findall(r'"([^"\n]*)"', literal))
            decoded = bytes(int(value, 16) for value in re.findall(r'\\x([0-9a-f]{2})', encoded))
            self.assertEqual(decoded, weights)
            return source, weights

    def value(self, name, element_type=onnx.TensorProto.FLOAT, shape=(1, 1, 1, 3)):
        return onnx.helper.make_tensor_value_info(name, element_type, shape)

    def test_multiple_inputs_and_shared_weight(self):
        model = onnx.helper.make_model(onnx.helper.make_graph(
            [onnx.helper.make_node("Add", ["b", "w"], ["first"]),
             onnx.helper.make_node("Add", ["a", "w"], ["second"]),
             onnx.helper.make_node("Mul", ["first", "second"], ["out"])],
            "multiple", [self.value("a"), self.value("b")], [self.value("out")],
            initializer=[onnx.numpy_helper.from_array(np.ones((1, 1, 1, 3), dtype=np.float32), "w")],
            value_info=[self.value("first"), self.value("second")],
        ))
        source, weights = self.generate(model)
        self.assertEqual(source.count("ItemType::RuntimeInput"), 2)
        self.assertEqual(source.count("ItemType::Weight"), 1)
        self.assertEqual(source.count("InputItem{2}"), 1)
        self.assertIn("Add{DataType::Float32, 1, 2, 4, {1u, 1u, 1u, 3u}}", source)
        self.assertIn("Add{DataType::Float32, 0, 2, 4, {1u, 1u, 1u, 3u}}", source)
        self.assertIn("3, inputSpecs,", source)
        self.assertEqual(weights, np.ones(3, dtype="<f4").tobytes())

    def test_all_defined_types_preserve_packed_weight_bytes(self):
        for tensor_type, numpy_type, name in (
            (onnx.TensorProto.FLOAT, "<f4", "Float32"),
            (onnx.TensorProto.FLOAT16, "<f2", "Float16"),
            (onnx.TensorProto.DOUBLE, "<f8", "Float64"),
            (onnx.TensorProto.INT8, "i1", "Int8"),
            (onnx.TensorProto.UINT8, "u1", "UInt8"),
            (onnx.TensorProto.INT16, "<i2", "Int16"),
            (onnx.TensorProto.UINT16, "<u2", "UInt16"),
            (onnx.TensorProto.INT32, "<i4", "Int32"),
            (onnx.TensorProto.UINT32, "<u4", "UInt32"),
            (onnx.TensorProto.INT64, "<i8", "Int64"),
            (onnx.TensorProto.UINT64, "<u8", "UInt64"),
        ):
            with self.subTest(data_type=name):
                array = np.array([1, 2, 3], dtype=numpy_type).reshape(1, 1, 1, 3)
                model = onnx.helper.make_model(onnx.helper.make_graph(
                    [onnx.helper.make_node("Add", ["x", "w"], ["out"])], "typed",
                    [self.value("x", tensor_type)], [self.value("out", tensor_type)],
                    initializer=[onnx.numpy_helper.from_array(array, "w")],
                ))
                source, weights = self.generate(model)
                self.assertIn(f"Add{{DataType::{name},", source)
                self.assertIn(f"DataType::{name}, ItemType::Weight", source)
                self.assertEqual(weights, array.tobytes())

    def test_mixed_weight_offsets_have_no_gpu_padding(self):
        a = np.array([1, 2, 3], dtype=np.uint8).reshape(1, 1, 1, 3)
        b = np.array([4, 5, 6], dtype=np.float64).reshape(1, 1, 1, 3)
        model = onnx.helper.make_model(onnx.helper.make_graph(
            [onnx.helper.make_node("Add", ["x", "a"], ["unused"]),
             onnx.helper.make_node("Relu", ["b"], ["out"])], "mixed",
            [self.value("x", onnx.TensorProto.UINT8)], [self.value("out", onnx.TensorProto.DOUBLE)],
            initializer=[onnx.numpy_helper.from_array(a, "a"), onnx.numpy_helper.from_array(b, "b")],
            value_info=[self.value("unused", onnx.TensorProto.UINT8)],
        ))
        source, weights = self.generate(model)
        self.assertIn("DataType::Float64, ItemType::Weight, 4, {1u, 1u, 1u, 3u}, 3LL", source)
        self.assertEqual(weights, a.tobytes() + b.tobytes())

    def test_literal_boundaries_and_empty_weights(self):
        for length in (0, 12287, 12288, 12289, 12290, 24576):
            with self.subTest(length=length):
                shape = (max(1, length),)
                initializers = []
                operation = onnx.helper.make_node("Relu", ["x"], ["out"])
                if length:
                    array = np.arange(length, dtype=np.uint8)
                    initializers = [onnx.numpy_helper.from_array(array, "w")]
                    operation = onnx.helper.make_node("Add", ["x", "w"], ["out"])
                model = onnx.helper.make_model(onnx.helper.make_graph(
                    [operation], "chunked", [self.value("x", onnx.TensorProto.UINT8, shape)],
                    [self.value("out", onnx.TensorProto.UINT8, shape)], initializer=initializers,
                ))
                source, weights = self.generate(model)
                self.assertEqual(len(weights), length)
                self.assertNotIn("LoadResource", source)
                self.assertNotIn("decodeBase64", source)
                if not length:
                    self.assertIn('constexpr char embeddedWeights[] =\n\t""', source)

    def test_unsupported_operators_rejected_before_writing(self):
        for operation in ("Constant", "Reshape", "Transpose", "Concat", "MatMul"):
            with self.subTest(operation=operation), tempfile.TemporaryDirectory() as directory:
                model = onnx.helper.make_model(onnx.helper.make_graph(
                    [onnx.helper.make_node(operation, ["x"], ["out"])], operation,
                    [self.value("x")], [self.value("out")],
                ))
                paths = [pathlib.Path(directory) / name for name in ("graph.cpp", "weights.bin")]
                with (mock.patch("sys.argv", ["GenerateModel.py", "--offline", *map(str, paths)]),
                      mock.patch("GenerateModel.hf_hub_download", return_value="unused.onnx"),
                      mock.patch("GenerateModel.onnx.load", return_value=model),
                      mock.patch("GenerateModel.lower_model", return_value=model)):
                    with self.assertRaisesRegex(RuntimeError, "outside the pinned Landscape variants"):
                        main()
                self.assertFalse(any(path.exists() for path in paths))

    def test_unknown_type_and_invalid_shape_are_rejected(self):
        for tensor_type, shape in ((onnx.TensorProto.BOOL, (1,)), (onnx.TensorProto.FLOAT, (1, 0))):
            model = onnx.helper.make_model(onnx.helper.make_graph(
                [onnx.helper.make_node("Relu", ["x"], ["out"])], "invalid",
                [self.value("x", tensor_type, shape)], [self.value("out", tensor_type, shape)],
            ))
            with self.assertRaises(RuntimeError):
                self.generate(model)


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
