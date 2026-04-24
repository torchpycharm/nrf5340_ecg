#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "models/model_runtime.h"

static int try_parse_model_id(const char *s, ecg_model_id_t *out_model_id)
{
    if (!s || !out_model_id)
    {
        return 0;
    }

    char *end = NULL;
    long v = strtol(s, &end, 10);
    if (end == s || *end != '\0')
    {
        return 0; // not a pure number
    }
    if (v <= 0 || v >= ECG_MODEL_MAX)
    {
        return 0;
    }
    *out_model_id = (ecg_model_id_t)v;
    return 1;
}

int main(int argc, char **argv)
{
    ecg_model_id_t model_id = ECG_MODEL_MIX3;
    const char *data_path = "TestSetData.txt";

    if (argc >= 2 && argv[1] && argv[1][0] != '\0')
    {
        if (try_parse_model_id(argv[1], &model_id))
        {
            if (argc >= 3 && argv[2] && argv[2][0] != '\0')
            {
                data_path = argv[2];
            }
        }
        else
        {
            // argv[1] is not a model id; treat it as data path
            data_path = argv[1];
        }
    }

    ecg_model_runtime_t model_runtime = {0};
    int ret = ecg_model_runtime_init(&model_runtime, model_id);
    if (ret != 0)
    {
        fprintf(stderr, "Failed to init model runtime: %d\n", ret);
        return ret;
    }

    FILE *fp = fopen(data_path, "r");
    if (!fp)
    {
        // fallback to the absolute path the project uses
        const char *fallback = "D:\\FYPcode_nrf\\fyp_integrate\\TestSetData.txt";
        fp = fopen(fallback, "r");
        if (!fp)
        {
            fprintf(stderr, "Failed to open test data: '%s'\n", data_path);
            return -1;
        }
        data_path = fallback;
    }

    size_t total = 0;
    size_t correct = 0;
    size_t infer_fail = 0;
    size_t parse_fail = 0;

    // confusion matrix for binary labels (0/1)
    size_t tp = 0, tn = 0, fp_ = 0, fn_ = 0;
    int has_binary_cm = 1;

    char line[512];
    while (fgets(line, (int)sizeof(line), fp))
    {
        // skip empty / whitespace-only lines
        char *p = line;
        while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')
        {
            p++;
        }
        if (*p == '\0')
        {
            continue;
        }

        double features[ECG_MODEL_FEATURE_DIM];
        int label = 0;
        int n = sscanf(p, "%lf%lf%lf%lf%lf%lf%d",
                       &features[0], &features[1], &features[2],
                       &features[3], &features[4], &features[5],
                       &label);
        if (n != 7)
        {
            parse_fail++;
            continue;
        }

        ecg_model_result_t infer_out = {.label = 0, .score = 0.0};
        ret = ecg_model_runtime_infer(&model_runtime, features, &infer_out);
        if (ret != 0)
        {
            infer_fail++;
            continue;
        }

        total++;
        if (infer_out.label == label)
        {
            correct++;
        }

        if (!((label == 0 || label == 1) && (infer_out.label == 0 || infer_out.label == 1)))
        {
            has_binary_cm = 0;
        }
        else if (has_binary_cm)
        {
            if (label == 1 && infer_out.label == 1)
                tp++;
            else if (label == 0 && infer_out.label == 0)
                tn++;
            else if (label == 0 && infer_out.label == 1)
                fp_++;
            else if (label == 1 && infer_out.label == 0)
                fn_++;
        }
    }
    fclose(fp);

    if (total == 0)
    {
        fprintf(stderr, "No valid test samples read from '%s' (parse_fail=%zu, infer_fail=%zu)\n",
                data_path, parse_fail, infer_fail);
        return -1;
    }

    double acc = (double)correct / (double)total;
    printf("Test summary: file=%s\n", data_path);
    printf("Model: id=%d name=%s\n",
           (int)model_runtime.active_model,
           ecg_model_runtime_name(model_runtime.active_model));
    printf("Samples: total=%zu correct=%zu accuracy=%.4f%% (parse_fail=%zu infer_fail=%zu)\n",
           total, correct, acc * 100.0, parse_fail, infer_fail);
    if (has_binary_cm)
    {
        printf("Confusion matrix (label rows, pred cols):\n");
        printf("  TN=%zu  FP=%zu\n", tn, fp_);
        printf("  FN=%zu  TP=%zu\n", fn_, tp);
    }
    return 0;
}
