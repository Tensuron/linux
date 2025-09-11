// SPDX-License-Identifier: GPL-2.0-only
/*
 * Kernel-space neural network implementation for AI-powered security
 *
 * Copyright (c) 2025 First Person
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/seq_file.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/cpu.h>
#include <linux/string.h>
#include <linux/numa.h>
#include <linux/crc32.h>
#include <linux/random.h>
#include <linux/proc_fs.h>
#include <linux/mutex.h>
#include <linux/io.h>
#include <linux/module.h>
#include <asm/fpu/api.h>
#include <asm/simd.h>

#include <uapi/linux/neural.h>

#define DEVICE_NAME "neural"

/* Function implementations */
int neural_layer_validate(neural_layer_t *layer)
{
    if (!layer)
        return -EINVAL;

    if (layer->input_size > NEURAL_MAX_INPUT_SIZE ||
        layer->output_size > NEURAL_MAX_OUTPUT_SIZE)
        return -EINVAL;

    if (!neural_validate_weights(layer->weights, 
                               layer->input_size * layer->output_size))
        return -EINVAL;

    return 0;
}

int neural_network_validate(neural_network_t *nn)
{
    int i;
    
    if (!nn)
        return -EINVAL;

    if (nn->num_layers > NEURAL_MAX_LAYERS)
        return -EINVAL;

    for (i = 0; i < nn->num_layers; i++) {
        if (neural_layer_validate(&nn->layers[i]) < 0)
            return -EINVAL;
    }

    return 0;
}

/* Layer initialization and cleanup */
int init_neural_layer(neural_layer_t *layer, u32 input_size, u32 output_size, u8 activation_type)
{
    if (!layer || input_size == 0 || output_size == 0)
        return -EINVAL;

    memset(layer, 0, sizeof(*layer));

    layer->input_size = input_size;
    layer->output_size = output_size;
    layer->activation_type = activation_type;
    layer->weights_size = input_size * output_size;
    layer->biases_size = output_size;

    /* Allocate arrays */
    layer->weights = kzalloc(layer->weights_size * sizeof(s32), GFP_KERNEL);
    if (!layer->weights)
        goto err_weights;

    layer->biases = kzalloc(layer->biases_size * sizeof(s32), GFP_KERNEL);
    if (!layer->biases)
        goto err_biases;

    layer->neurons = kzalloc(output_size * sizeof(s32), GFP_KERNEL);
    if (!layer->neurons)
        goto err_neurons;

    /* Initialize rwlock */
    rwlock_init(&layer->lock);

    /* Initialize weights with small random values */
    get_random_bytes(layer->weights, layer->weights_size * sizeof(s32));
    get_random_bytes(layer->biases, layer->biases_size * sizeof(s32));

    /* Scale the random values to be between -1 and 1 in fixed point */
    for (size_t i = 0; i < layer->weights_size; i++) {
        layer->weights[i] = (layer->weights[i] % (2 * FP_ONE)) - FP_ONE;
        /* Xavier initialization using fixed-point arithmetic */
        /* We use a pre-computed scaling factor instead of sqrt for better performance */
        if (input_size <= 1)
            layer->weights[i] = FP_MUL(layer->weights[i], INT_TO_FP(1));
        else if (input_size <= 4)
            layer->weights[i] = FP_MUL(layer->weights[i], INT_TO_FP(1) / 2);  /* 1/sqrt(4) */
        else if (input_size <= 16)
            layer->weights[i] = FP_MUL(layer->weights[i], INT_TO_FP(1) / 4);  /* 1/sqrt(16) */
        else
            layer->weights[i] = FP_MUL(layer->weights[i], INT_TO_FP(1) / 8);  /* 1/sqrt(64) - good enough */
    }

    for (size_t i = 0; i < layer->biases_size; i++)
        layer->biases[i] = 0;  /* Initialize biases to zero */

    /* Set up NUMA and SIMD configuration */
    layer->numa_node = numa_node_id();
    layer->use_simd = IS_ENABLED(CONFIG_AS_AVX2);

    /* Calculate checksum for security validation */
    layer->checksum = crc32(0, layer->weights, layer->weights_size * sizeof(s32));
    layer->weights_validated = true;

    return 0;

err_neurons:
    kfree(layer->biases);
err_biases:
    kfree(layer->weights);
err_weights:
    return -ENOMEM;
}

void free_neural_layer(neural_layer_t *layer)
{
    if (!layer)
        return;

    kfree(layer->weights);
    kfree(layer->biases);
    kfree(layer->neurons);
    kfree(layer->gradients);
    kfree(layer->weight_momentum);
    kfree(layer->bn_gamma);
    kfree(layer->bn_beta);

    memset(layer, 0, sizeof(*layer));
}

void neural_network_print_stats(neural_network_t *nn)
{
    if (!nn)
        return;

    pr_info("Neural Network Stats:\n");
    pr_info("  Predictions made: %lld\n", 
            atomic64_read(&nn->stats.predictions_made));
    pr_info("  Cache hits: %lld\n", 
            atomic64_read(&nn->stats.cache_hits));
    pr_info("  Cache misses: %lld\n", 
            atomic64_read(&nn->stats.cache_misses));
    pr_info("  Errors encountered: %lld\n", 
            atomic64_read(&nn->stats.errors_encountered));
    pr_info("  Last error: %s\n", nn->stats.last_error);
}

int neural_network_init(neural_network_t *nn, u32 input_size, u32 hidden_size, 
                       u32 output_size, bool use_batch_norm, s32 dropout_rate)
{
    if (!nn || input_size > NEURAL_MAX_INPUT_SIZE || 
        output_size > NEURAL_MAX_OUTPUT_SIZE)
        return -EINVAL;

    memset(nn, 0, sizeof(*nn));

    nn->INPUT_LAYER = input_size;
    nn->HIDDEN_LAYER = hidden_size;
    nn->OUTPUT_LAYER = output_size;
    nn->num_layers = 3;  /* Input, hidden, output */
    nn->use_batch_norm = use_batch_norm;

    nn->layers = kzalloc(sizeof(neural_layer_t) * nn->num_layers, GFP_KERNEL);
    if (!nn->layers)
        return -ENOMEM;

    /* Initialize each layer */
    if (init_neural_layer(&nn->layers[0], input_size, hidden_size, 0) < 0 ||
        init_neural_layer(&nn->layers[1], hidden_size, hidden_size, 0) < 0 ||
        init_neural_layer(&nn->layers[2], hidden_size, output_size, 0) < 0) {
        kfree(nn->layers);
        return -ENOMEM;
    }

    /* Initialize synchronization primitives */
    spin_lock_init(&nn->lock);
    mutex_init(&nn->training_mutex);
    atomic_set(&nn->refcount, 1);
    init_completion(&nn->ready);
    init_rwsem(&nn->config_sem);

    nn->creation_time = ktime_get_ns();
    nn->initialized = true;

    return 0;
}

void neural_network_cleanup(neural_network_t *nn)
{
    int i;

    if (!nn)
        return;

    /* Clean up each layer */
    if (nn->layers) {
        for (i = 0; i < nn->num_layers; i++)
            free_neural_layer(&nn->layers[i]);
        kfree(nn->layers);
    }

    /* Clean up debugfs entries */
    neural_debugfs_cleanup(nn);

    /* Clean up prediction cache */
    if (nn->prediction_cache.cached_output)
        kfree(nn->prediction_cache.cached_output);

    /* Clean up per-CPU stats if allocated */
    if (nn->stats.per_cpu_stats)
        free_percpu(nn->stats.per_cpu_stats);

    memset(nn, 0, sizeof(*nn));
}

void neural_update_stats(neural_network_t *nn, ktime_t start_time)
{
    ktime_t end_time;
    s64 duration;

    if (!nn)
        return;

    end_time = ktime_get();
    duration = ktime_to_ns(ktime_sub(end_time, start_time));

    atomic64_inc(&nn->stats.predictions_made);
    atomic64_add(duration, &nn->stats.total_inference_time_ns);

    /* Update min/max batch times */
    if (duration < nn->stats.min_batch_time_us || nn->stats.min_batch_time_us == 0)
        nn->stats.min_batch_time_us = duration / 1000;  /* Convert to us */
    if (duration > nn->stats.max_batch_time_us)
        nn->stats.max_batch_time_us = duration / 1000;

    /* Calculate average inference time */
    nn->stats.avg_inference_time_us = 
        div_u64(atomic64_read(&nn->stats.total_inference_time_ns),
                atomic64_read(&nn->stats.predictions_made) * 1000);

    nn->last_prediction_time = end_time;
}

/* Implementation of the API functions */

void *neural_alloc_numa(size_t size, int node)
{
    if (node < 0 || node >= MAX_NUMNODES)
        return NULL;
    return kmalloc_node(size, GFP_KERNEL, node);
}

void *neural_alloc_interleaved(size_t size)
{
    return vzalloc(size);
}

void neural_vector_add_simd(const s32 *a, const s32 *b, s32 *result, u32 size)
{
    u32 i;
    if (size < NEURAL_SIMD_THRESHOLD || !IS_ENABLED(CONFIG_AS_AVX2)) {
        for (i = 0; i < size; i++)
            result[i] = a[i] + b[i];
        return;
    }

    kernel_fpu_begin();
    // Implement SIMD addition here using inline assembly
    kernel_fpu_end();
}

s32 neural_vector_dot_simd(const s32 *a, const s32 *b, u32 size)
{
    s32 result = 0;
    u32 i;

    if (size < NEURAL_SIMD_THRESHOLD || !IS_ENABLED(CONFIG_AS_AVX2)) {
        for (i = 0; i < size; i++)
            result += FP_MUL(a[i], b[i]);
        return result;
    }

    kernel_fpu_begin();
    // Implement SIMD dot product here using inline assembly
    kernel_fpu_end();
    return result;
}

bool neural_validate_input(const s32 *input, u32 size)
{
    u32 i;
    
    if (!input || size > NEURAL_MAX_INPUT_SIZE)
        return false;

    for (i = 0; i < size; i++) {
        if (input[i] > NEURAL_MAX_WEIGHT_VALUE || input[i] < NEURAL_MIN_WEIGHT_VALUE)
            return false;
    }
    return true;
}

bool neural_validate_weights(const s32 *weights, u32 size)
{
    u32 i;
    
    if (!weights || size == 0)
        return false;
        
    for (i = 0; i < size; i++) {
        if (weights[i] > NEURAL_MAX_WEIGHT_VALUE || weights[i] < NEURAL_MIN_WEIGHT_VALUE)
            return false;
    }
    return true;
}

s32 neural_fp_sqrt(s32 x)
{
    s32 result = 0;
    s32 temp = x;
    s32 i;

    if (x <= 0)
        return 0;

    for (i = 0; i < 16; i++) {  // Fixed-point precision iterations
        result = (result + FP_DIV(temp, result)) >> 1;
        if (result == 0)
            break;
    }

    return result;
}

s32 neural_fp_exp(s32 x)
{
    // Implement fixed-point exponential approximation
    // This is a simplified version - consider using a lookup table or better approximation
    if (x > INT_TO_FP(5))
        return INT_TO_FP(148);  // e^5
    if (x < INT_TO_FP(-5))
        return 0;

    s32 result = INT_TO_FP(1);
    s32 term = x;
    s32 i;

    for (i = 1; i <= 8; i++) {  // Use 8 terms of Taylor series
        result += term;
        term = FP_MUL(term, FP_DIV(x, INT_TO_FP(i + 1)));
    }

    return result;
}

s32 neural_relu(s32 x)
{
    return x > 0 ? x : 0;
}

s32 neural_leaky_relu(s32 x)
{
    /* Use fixed-point constant 0.01 (655 in Q16.16 format) */
    return x > 0 ? x : FP_MUL(x, 655);
}

s32 neural_sigmoid(s32 x)
{
    return FP_DIV(INT_TO_FP(1), INT_TO_FP(1) + neural_fp_exp(-x));
}

s32 neural_tanh(s32 x)
{
    s32 exp_pos = neural_fp_exp(x);
    s32 exp_neg = neural_fp_exp(-x);
    return FP_DIV(exp_pos - exp_neg, exp_pos + exp_neg);
}

s32 neural_linear(s32 x)
{
    return x;
}

s32 neural_softmax_component(s32 x, const s32 *inputs, u32 size)
{
    s32 max_val = x;
    s32 sum = 0;
    u32 i;

    // Find max value for numerical stability
    for (i = 0; i < size; i++)
        if (inputs[i] > max_val)
            max_val = inputs[i];

    // Calculate sum of exponentials
    for (i = 0; i < size; i++)
        sum += neural_fp_exp(inputs[i] - max_val);

    // Return normalized probability
    return FP_DIV(neural_fp_exp(x - max_val), sum);
}

void neural_softmax(s32 *inputs, u32 size)
{
    u32 i;
    for (i = 0; i < size; i++)
        inputs[i] = neural_softmax_component(inputs[i], inputs, size);
}

s32 apply_activation(s32 x, u8 activation_type)
{
    switch (activation_type) {
    case 0:
        return neural_relu(x);
    case 1:
        return neural_sigmoid(x);
    case 2:
        return neural_linear(x);
    case 3:
        return neural_tanh(x);
    case 4:
        return neural_leaky_relu(x);
    default:
        return x;
    }
}

int neural_debugfs_init(neural_network_t *nn)
{
    if (!nn)
        return -EINVAL;

    nn->debug_dir = debugfs_create_dir(DEVICE_NAME, NULL);
    if (!nn->debug_dir)
        return -ENODEV;

    debugfs_create_u64("predictions_made", 0444, nn->debug_dir,
                      (u64 *)&nn->stats.predictions_made);
    debugfs_create_u64("cache_hits", 0444, nn->debug_dir,
                      (u64 *)&nn->stats.cache_hits);
    debugfs_create_u64("cache_misses", 0444, nn->debug_dir,
                      (u64 *)&nn->stats.cache_misses);

    return 0;
}

void neural_debugfs_cleanup(neural_network_t *nn)
{
    if (nn && nn->debug_dir) {
        debugfs_remove_recursive(nn->debug_dir);
        nn->debug_dir = NULL;
    }
}

void neural_record_error(neural_network_t *nn, const char *error_msg)
{
    if (!nn || !error_msg)
        return;

    atomic64_inc(&nn->stats.errors_encountered);
    nn->stats.last_error_ts = ktime_get_ns();
    strncpy(nn->stats.last_error, error_msg, sizeof(nn->stats.last_error) - 1);
    nn->stats.last_error[sizeof(nn->stats.last_error) - 1] = '\0';
}

int neural_layer_forward(neural_layer_t *layer, const s32 *input)
{
    u32 i, j;
    s32 sum;

    if (!layer || !input)
        return -EINVAL;

    if (!neural_validate_input(input, layer->input_size))
        return -EINVAL;

    write_lock(&layer->lock);

    for (i = 0; i < layer->output_size; i++) {
        sum = layer->biases[i];
        for (j = 0; j < layer->input_size; j++) {
            sum += FP_MUL(input[j], 
                         layer->weights[i * layer->input_size + j]);
        }
        layer->neurons[i] = apply_activation(sum, layer->activation_type);
    }

    write_unlock(&layer->lock);
    return 0;
}

neural_network_t* neural_network_create(u32 INPUT_LAYER, u32 HIDDEN_LAYER, u32 OUTPUT_LAYER)
{
    neural_network_t *nn;
    int ret;

    if (INPUT_LAYER > NEURAL_MAX_INPUT_SIZE || 
        OUTPUT_LAYER > NEURAL_MAX_OUTPUT_SIZE)
        return NULL;

    nn = kzalloc(sizeof(*nn), GFP_KERNEL);
    if (!nn)
        return NULL;

    ret = neural_network_init(nn, INPUT_LAYER, HIDDEN_LAYER, OUTPUT_LAYER,
                            false, 0);
    if (ret < 0) {
        kfree(nn);
        return NULL;
    }

    return nn;
}

void neural_network_destroy(neural_network_t *nn)
{
    if (!nn)
        return;

    neural_network_cleanup(nn);
    kfree(nn);
}

int neural_network_predict(neural_network_t *nn, const s32 *input, s32 *output)
{
    int ret;
    u32 i;
    const s32 *current_input = input;
    ktime_t start_time = ktime_get();

    if (!nn || !input || !output)
        return -EINVAL;

    if (!neural_validate_input(input, nn->layers[0].input_size))
        return -EINVAL;

    mutex_lock(&nn->training_mutex);

    for (i = 0; i < nn->num_layers; i++) {
        ret = neural_layer_forward(&nn->layers[i], current_input);
        if (ret < 0) {
            mutex_unlock(&nn->training_mutex);
            return ret;
        }
        current_input = nn->layers[i].neurons;
    }

    memcpy(output, current_input, nn->layers[nn->num_layers - 1].output_size * sizeof(s32));
    
    neural_update_stats(nn, start_time);
    mutex_unlock(&nn->training_mutex);

    return 0;
}

/* Module init and exit */

/* Module init and exit */
static int __init neural_module_init(void)
{
    pr_info("Neural network module initialized\n");
    return 0;
}

static void __exit neural_module_exit(void)
{
    pr_info("Neural network module unloaded\n");
}

module_init(neural_module_init);
module_exit(neural_module_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("First Person");
MODULE_DESCRIPTION("Neural network acceleration module");