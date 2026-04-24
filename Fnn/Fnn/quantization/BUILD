load("@rules_python//python:defs.bzl", "py_binary")
load("@tflm_pip_deps//:requirements.bzl", "requirement")

py_binary(
    name = "ptq",
    srcs = ["ptq.py"], 
    deps = [
        requirement("absl_py"),
        requirement("numpy"),
        requirement("tensorflow"),
        "//python/tflite_micro:runtime",  
    ],
)