#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "models/model_runtime.h"

typedef struct test_sample_t
{
    int32_t features_raw[ECG_MODEL_FEATURE_DIM]; // Q16.16
    int label;
} test_sample_t;

static int try_parse_model_id_int8_only(const char *s, ecg_model_id_t *out_model_id)
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

    // INT8-only mode: accept 1..10
    if (v >= (long)ECG_MODEL_KNN_1 && v <= (long)ECG_MODEL_MIX3)
    {
        *out_model_id = (ecg_model_id_t)v;
        return 1;
    }
    return 0;
}

static int is_flag(const char *s, const char *flag)
{
    return s && flag && strcmp(s, flag) == 0;
}

static int load_test_samples(const char *data_path,
                             test_sample_t **out_samples,
                             size_t *out_count,
                             size_t *out_parse_fail)
{
    if (!data_path || !out_samples || !out_count)
    {
        return -1;
    }

    *out_samples = NULL;
    *out_count = 0;
    if (out_parse_fail)
        *out_parse_fail = 0;

    FILE *fp = fopen(data_path, "r");
    if (!fp)
    {
        return -1;
    }

    size_t cap = 256;
    size_t n = 0;
    test_sample_t *samples = (test_sample_t *)malloc(cap * sizeof(test_sample_t));
    if (!samples)
    {
        fclose(fp);
        return -1;
    }

    char line[512];
    size_t parse_fail = 0;
    while (fgets(line, (int)sizeof(line), fp))
    {
        char *p = line;
        while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')
            p++;
        if (*p == '\0')
            continue;

        test_sample_t s = {0};
        int label = 0;
        int32_t x_raw_q16[ECG_MODEL_FEATURE_DIM];
        if (!ecg_parse_sample_line(p, x_raw_q16, &label))
        {
            parse_fail++;
            continue;
        }
        memcpy(s.features_raw, x_raw_q16, sizeof(x_raw_q16));
        s.label = label;

        if (n == cap)
        {
            cap *= 2;
            test_sample_t *ns = (test_sample_t *)realloc(samples, cap * sizeof(test_sample_t));
            if (!ns)
            {
                free(samples);
                fclose(fp);
                return -1;
            }
            samples = ns;
        }
        samples[n++] = s;
    }
    fclose(fp);

    if (out_parse_fail)
        *out_parse_fail = parse_fail;
    *out_samples = samples;
    *out_count = n;
    return 0;
}

static int eval_model_on_samples(ecg_model_id_t model_id,
                                 const test_sample_t *samples,
                                 size_t count)
{
    ecg_model_runtime_t model_runtime = {0};
    int ret = ecg_model_runtime_init(&model_runtime, model_id);
    if (ret != 0)
    {
        fprintf(stderr, "Failed to init model runtime (model=%d): %d\n", (int)model_id, ret);
        return ret;
    }

    size_t total = 0;
    size_t correct = 0;
    size_t infer_fail = 0;

    size_t tp = 0, tn = 0, fp_ = 0, fn_ = 0;
    int has_binary_cm = 1;

    for (size_t i = 0; i < count; i++)
    {
        ecg_model_result_t infer_out = {.label = 0, .score_q = 0};
        ret = ecg_model_runtime_infer(&model_runtime, samples[i].features_raw, &infer_out);
        if (ret != 0)
        {
            infer_fail++;
            continue;
        }

        total++;
        if (infer_out.label == samples[i].label)
        {
            correct++;
        }

        if (!((samples[i].label == 0 || samples[i].label == 1) && (infer_out.label == 0 || infer_out.label == 1)))
        {
            has_binary_cm = 0;
        }
        else if (has_binary_cm)
        {
            if (samples[i].label == 1 && infer_out.label == 1)
                tp++;
            else if (samples[i].label == 0 && infer_out.label == 0)
                tn++;
            else if (samples[i].label == 0 && infer_out.label == 1)
                fp_++;
            else if (samples[i].label == 1 && infer_out.label == 0)
                fn_++;
        }
    }

    if (total == 0)
    {
        fprintf(stderr, "No valid test samples (infer_fail=%zu)\n", infer_fail);
        return -1;
    }

    // Avoid FP math: accuracy in basis points (0..10000) -> print XX.XX%
    uint32_t acc_bp = (uint32_t)((correct * 10000ULL + (total / 2ULL)) / total);
    printf("Model: id=%d name=%s\n", (int)model_id, ecg_model_runtime_name(model_id));
    printf("Samples: total=%zu correct=%zu accuracy=%u.%02u%% (infer_fail=%zu)\n",
           total, correct, (unsigned)(acc_bp / 100U), (unsigned)(acc_bp % 100U), infer_fail);
    if (has_binary_cm)
    {
        printf("Confusion matrix (label rows, pred cols):\n");
        printf("  TN=%zu  FP=%zu\n", tn, fp_);
        printf("  FN=%zu  TP=%zu\n", fn_, tp);
    }
    printf("\n");
    return 0;
}

int main(int argc, char **argv)
{
    ecg_model_id_t model_id = ECG_MODEL_MIX3;
    const char *data_path = "TestSetData.txt";
    int run_all = 0;

    // Usage:
    //   prog.exe [model_id|all] [data_path]
    //   model_id:
    //     - 1..10 mapped to INT8 models (11..20)
    //     - 11..20 INT8 models directly
    if (argc >= 2 && argv[1] && argv[1][0] != '\0')
    {
        if (is_flag(argv[1], "all"))
        {
            run_all = 1;
        }
        else if (!try_parse_model_id_int8_only(argv[1], &model_id))
        {
            // argv[1] is not a model id; treat it as data path
            data_path = argv[1];
        }
    }

    int argi = 2;
    if (argc > argi && argv[argi] && argv[argi][0] != '\0')
    {
        data_path = argv[argi];
    }

    // fallback to the absolute path the project uses
    FILE *fp_probe = fopen(data_path, "r");
    if (!fp_probe)
    {
        const char *fallback = "D:\\FYPcode_nrf\\fyp_integrate\\TestSetData.txt";
        fp_probe = fopen(fallback, "r");
        if (!fp_probe)
        {
            fprintf(stderr, "Failed to open test data: '%s'\n", data_path);
            return -1;
        }
        fclose(fp_probe);
        data_path = fallback;
    }
    else
    {
        fclose(fp_probe);
    }

    test_sample_t *samples = NULL;
    size_t sample_count = 0;
    size_t parse_fail = 0;
    if (load_test_samples(data_path, &samples, &sample_count, &parse_fail) != 0 || sample_count == 0)
    {
        fprintf(stderr, "No valid test samples read from '%s' (parse_fail=%zu)\n", data_path, parse_fail);
        free(samples);
        return -1;
    }

    printf("Test summary: file=%s samples=%zu (parse_fail=%zu)\n\n", data_path, sample_count, parse_fail);

    int ret = 0;
    if (run_all)
    {
        int base = (int)ECG_MODEL_KNN_1;
        for (int i = 0; i < 10; i++)
        {
            ecg_model_id_t mid = (ecg_model_id_t)(base + i);
            int r = eval_model_on_samples(mid, samples, sample_count);
            if (r != 0)
            {
                ret = r;
            }
        }
    }
    else
    {
        ret = eval_model_on_samples(model_id, samples, sample_count);
    }

    free(samples);
    return ret;
}
