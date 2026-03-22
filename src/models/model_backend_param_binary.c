#include <errno.h>
#include <zephyr/logging/log.h>

#include "model_backends.h"
#include "model_params.h"

LOG_MODULE_REGISTER(model_param_binary, LOG_LEVEL_INF);

static void standardize_features(const double features[ECG_MODEL_FEATURE_DIM],
                                 float features_norm[ECG_MODEL_FEATURE_DIM])
{
    for (int i = 0; i < ECG_MODEL_FEATURE_DIM; i++) {
        const float feature = (float)features[i];
        const float mean = mean_all[i];
        const float std = std_all[i];
        if (std_all[i] != 0.0f) {
            features_norm[i] = (feature - mean) / std;
        } else {
            features_norm[i] = feature;
        }
    }
}

int ecg_model_backend_param_binary_infer(const double features[ECG_MODEL_FEATURE_DIM],
                                         ecg_model_result_t *out)
{
    if (!features || !out) {
        return -EINVAL;
    }

    float features_norm[ECG_MODEL_FEATURE_DIM];
    standardize_features(features, features_norm);

    int node = 0;
    int depth = 0;

    while (1) {
        if (!is_branch[node]) {
            out->label = node_class[node];
            out->score = (float)out->label;
            out->supported = true;
            return 0;
        }

        int feature_index = cut_feature[node];
        if (feature_index < 0 || feature_index >= ECG_MODEL_FEATURE_DIM) {
            LOG_WRN("Invalid feature index %d at node %d", feature_index, node);
            out->label = node_class[node];
            out->score = (float)out->label;
            out->supported = true;
            return 0;
        }

        float split_value = cut_value[node];
        int next_node = (features_norm[feature_index] < split_value)
                            ? left_child[node]
                            : right_child[node];

        if (next_node < 0 || next_node >= N_NODES) {
            LOG_WRN("Invalid child node %d from node %d", next_node, node);
            out->label = node_class[node];
            out->score = (float)out->label;
            out->supported = true;
            return 0;
        }

        node = next_node;
        depth++;

        if (depth > N_NODES) {
            LOG_ERR("Tree traversal depth overflow");
            return -ELOOP;
        }
    }
}
