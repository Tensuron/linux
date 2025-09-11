// SPDX-License-Identifier: GPL-2.0-only
/*
 * Kernel-space neural network implementation for AI-powered security
 *
 * Copyright (c) 2025 First Person
 */

#ifndef _UAPI_NEURAL_MODULE_H
#define _UAPI_NEURAL_MODULE_H

#include <linux/types.h>

/* Public API function declarations */
_Bool neural_validate_input(const s32 *input, u32 size);
_Bool neural_validate_weights(const s32 *weights, u32 size);

/* Internal kernel API */
#ifdef __KERNEL__
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/atomic.h>
#include <linux/file.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/random.h>
#include <linux/ktime.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/mutex.h>
#include <linux/workqueue.h>
#include <linux/crc32.h>
#include <linux/printk.h>
#include <linux/version.h>
#include <linux/overflow.h>
#include <linux/numa.h>
#include <linux/cpu.h>
#include <linux/cpumask.h>
#include <linux/percpu.h>
#include <linux/moduleparam.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <asm/fpu/api.h>
#include <asm/simd.h>
#endif /* __KERNEL__ */

/* Neural network configuration */
#define NEURAL_MAX_LAYERS 16
#define NEURAL_MAX_BATCH_SIZE 64
#define NEURAL_WEIGHT_SCALE 1000
#define NEURAL_LEARNING_RATE_FP INT_TO_FP(1) / 1000  /* 0.001 in fixed point */
#define NEURAL_CACHE_LINE_SIZE 64
#define NEURAL_ALIGN __aligned(NEURAL_CACHE_LINE_SIZE)

/* Model version */
#define NEURAL_MODEL_VERSION 2
#define NEURAL_MAGIC 0x4E455552  /* 'NEUR' in ASCII */

/* Error codes */
#define NEURAL_SUCCESS 0
#define NEURAL_ERROR_INVALID_INPUT -1
#define NEURAL_ERROR_MEMORY -2
#define NEURAL_ERROR_INVALID_LAYER -3
#define NEURAL_ERROR_INVALID_MODEL -4
#define NEURAL_ERROR_SIMD_UNAVAILABLE -5
#define NEURAL_ERROR_NUMA_FAILURE -6
#define NEURAL_ERROR_SECURITY_VIOLATION -7

/* Security limits */
#define NEURAL_MAX_INPUT_SIZE 4096
#define NEURAL_MAX_OUTPUT_SIZE 1024
#define NEURAL_MAX_WEIGHT_VALUE INT_TO_FP(100)
#define NEURAL_MIN_WEIGHT_VALUE INT_TO_FP(-100)

/* Performance tuning */
#define NEURAL_SIMD_THRESHOLD 64
#define NEURAL_CACHE_TIMEOUT_NS (1000000000ULL)  /* 1 second */
#define NEURAL_MAX_NUMA_NODES 8

/* Fixed-point arithmetic for kernel space (Q16.16 format) */
#define FP_SHIFT 16
#define FP_ONE (1 << FP_SHIFT)
#define FP_MUL(a, b) (((s64)(a) * (b)) >> FP_SHIFT)
#define FP_DIV(a, b) (((s64)(a) << FP_SHIFT) / (b))
#define INT_TO_FP(x) ((x) << FP_SHIFT)
#define FP_TO_INT(x) ((x) >> FP_SHIFT)
#define FP_SQRT(x) neural_fp_sqrt(x)

/* Function declarations */
void *neural_alloc_numa(size_t size, int node);
void *neural_alloc_interleaved(size_t size);
void neural_vector_add_simd(const s32 *a, const s32 *b, s32 *result, u32 size);
s32 neural_vector_dot_simd(const s32 *a, const s32 *b, u32 size);
_Bool neural_validate_input(const s32 *input, u32 size);
_Bool neural_validate_weights(const s32 *weights, u32 size);
s32 neural_fp_sqrt(s32 x);
s32 neural_fp_exp(s32 x);

/* Advanced performance statistics with per-CPU counters */
typedef struct neural_stats {
    atomic64_t predictions_made;
    atomic64_t total_inference_time_ns;
    atomic64_t cache_hits;
    atomic64_t cache_misses;
    atomic64_t errors_encountered;
    atomic64_t simd_operations;
    atomic64_t numa_allocations;
    atomic64_t security_violations;
    u32 avg_inference_time_us;
    u32 peak_memory_usage_kb;
    u32 min_batch_time_us;
    u32 max_batch_time_us;
    u64 last_error_ts;  /* Timestamp of last error */
    char last_error[128]; /* Description of last error */
    
    /* Per-CPU statistics */
    struct {
        u64 predictions;
        u64 cache_hits;
        u64 errors;
    } __percpu *per_cpu_stats;
} neural_stats_t __aligned(NEURAL_CACHE_LINE_SIZE);

/* NUMA-aware neural network layer structure */
typedef struct neural_layer {
    s32 *weights NEURAL_ALIGN;      /* Fixed-point weights matrix */
    s32 *biases NEURAL_ALIGN;       /* Fixed-point bias vector */
    s32 *neurons NEURAL_ALIGN;      /* Neuron outputs */
    s32 *gradients NEURAL_ALIGN;    /* Gradients for training */
    s32 *weight_momentum NEURAL_ALIGN; /* Momentum for optimization */
    u32 input_size;                 /* Number of inputs */
    u32 output_size;                /* Number of outputs */
    u8 activation_type;             /* 0=ReLU, 1=Sigmoid, 2=Linear, 3=Tanh, 4=Leaky_ReLU */
    u8 padding[3];                  /* Padding for alignment */
    s32 dropout_rate;               /* Dropout probability in fixed point format */
    bool batch_norm;                /* Batch normalization enabled */
    s32 *bn_gamma NEURAL_ALIGN;     /* Batch norm scale parameters */
    s32 *bn_beta NEURAL_ALIGN;      /* Batch norm shift parameters */
    u32 weights_size;               /* Size of weights array */
    u32 biases_size;                /* Size of biases array */
    rwlock_t lock;                  /* Read-write lock for thread safety */
    
    /* NUMA and performance optimization */
    int numa_node;                  /* NUMA node for this layer */
    bool use_simd;                  /* Enable SIMD for this layer */
    u64 computation_count;          /* Number of computations performed */
    ktime_t last_access_time;       /* Last access timestamp */
    
    /* Security and validation */
    u32 checksum;                   /* CRC32 checksum of weights */
    bool weights_validated;         /* Weights have been validated */
} neural_layer_t;

/* Advanced neural network structure with NUMA and security features */
typedef struct {
    u32 INPUT_LAYER;
    u32 HIDDEN_LAYER;
    u32 OUTPUT_LAYER;
    u32 num_layers;
    neural_layer_t *layers;
    
    /* Synchronization */
    spinlock_t lock;                /* Coarse-grained lock for whole network */
    struct mutex training_mutex;    /* For training operations */
    atomic_t refcount;              /* Reference counting */
    struct completion ready;        /* For async operations */
    struct rw_semaphore config_sem; /* Configuration changes */
    
    /* State management */
    bool initialized;
    bool training_mode;
    u32 epoch_count;
    u32 flags;                      /* Runtime flags */
    
    /* Performance monitoring */
    neural_stats_t stats;
    ktime_t last_prediction_time;
    
    /* Memory management with NUMA awareness */
    size_t total_memory_usage;
    u32 max_batch_size;
    struct kmem_cache *cache;       /* Slab cache for allocations */
    int preferred_numa_node;        /* Preferred NUMA node */
    cpumask_t allowed_cpus;         /* CPUs allowed for computation */
    
    /* Advanced features */
    s32 learning_rate;
    s32 momentum;
    s32 weight_decay;
    bool use_batch_norm;
    bool adaptive_learning;         /* Adaptive learning rate */
    
    /* Security features */
    u64 creation_time;              /* Network creation timestamp */
    u32 security_token;             /* Security validation token */
    bool secure_mode;               /* Enhanced security checks */
    
    /* Prediction cache with advanced features */
    struct {
        u32 input_hash;
        s32 *cached_output;
        ktime_t cache_time;
        bool valid;
        u32 output_size;            /* Size of cached output */
        spinlock_t lock;            /* Cache-specific lock */
        atomic_t hit_count;         /* Cache hit counter */
        u64 timeout_ns;             /* Cache timeout in nanoseconds */
    } prediction_cache;
    
    /* Debug and profiling */
    struct dentry *debug_dir;       /* DebugFS directory */
    bool profiling_enabled;         /* Performance profiling */
    struct {
        u64 forward_pass_time;
        u64 activation_time;
        u64 memory_access_time;
        u32 cache_efficiency;
    } profiling_data;
} neural_network_t;

/* Adaptive learning rate structure */
typedef struct {
    s32 base_rate;                  /* Base learning rate */
    s32 decay_factor;               /* Decay factor */
    s32 min_rate;                   /* Minimum learning rate */
    s32 max_rate;                   /* Maximum learning rate */
    u32 patience;                   /* Patience for rate adjustment */
    u32 steps_without_improvement;  /* Steps without improvement */
    s32 best_loss;                  /* Best loss seen so far */
    bool enabled;                   /* Adaptive learning enabled */
} neural_adaptive_lr_t;

/* Performance profiler */
typedef struct {
    ktime_t start_time;
    ktime_t end_time;
    u64 cycles_start;
    u64 cycles_end;
    u32 cache_misses;
    u32 branch_misses;
    bool active;
} neural_profiler_t;

/* Batch processing structure */
typedef struct neural_batch {
    s32 **inputs;           /* Array of input vectors */
    s32 **outputs;          /* Array of output vectors */
    u32 batch_size;         /* Number of samples in batch */
    u32 input_dim;          /* Input dimensionality */
    u32 output_dim;         /* Output dimensionality */
} neural_batch_t;

/* Model serialization header */
typedef struct neural_model_header {
    u32 magic;              /* Magic number for validation */
    u32 version;            /* Model format version */
    u32 num_layers;         /* Number of layers */
    u32 total_weights;      /* Total weight count */
    u32 checksum;           /* CRC32 checksum */
    u64 timestamp;          /* Creation timestamp */
} neural_model_header_t;

/* Forward declarations for compatibility */
typedef struct neural_network NeuralNetwork;

/* Activation function declarations */
s32 neural_relu(s32 x);
s32 neural_leaky_relu(s32 x);
s32 neural_sigmoid(s32 x);
s32 neural_tanh(s32 x);
s32 neural_linear(s32 x);
s32 neural_softmax_component(s32 x, const s32 *inputs, u32 size);
void neural_softmax(s32 *inputs, u32 size);

/* DebugFS function declarations */
int neural_stats_show(struct seq_file *m, void *v);
int neural_stats_open(struct inode *inode, struct file *file);
ssize_t neural_config_write(struct file *file, const char __user *buf,
                           size_t count, loff_t *ppos);

/* Core neural network function declarations */
int neural_debugfs_init(neural_network_t *nn);
void neural_debugfs_cleanup(neural_network_t *nn);
void neural_record_error(neural_network_t *nn, const char *error_msg);
int neural_self_test(neural_network_t *nn);
int neural_recovery_attempt(neural_network_t *nn);
void neural_profiler_start(neural_profiler_t *prof);
void neural_profiler_end(neural_profiler_t *prof);
u64 neural_profiler_get_ns(neural_profiler_t *prof);
s32 apply_activation(s32 x, u8 activation_type);

/* Main API functions */
int neural_network_init(neural_network_t *nn, u32 input_size, u32 hidden_size, 
                       u32 output_size, bool use_batch_norm, s32 dropout_rate);
void neural_network_cleanup(neural_network_t *nn);
neural_network_t *neural_network_ref(neural_network_t *nn);
void neural_network_unref(neural_network_t *nn);

/* Utility functions */
u32 neural_hash_input(const s32 *input, u32 size);
void neural_update_stats(neural_network_t *nn, ktime_t start_time);
size_t neural_calculate_layer_memory(neural_layer_t *layer);
int init_neural_layer(neural_layer_t *layer, u32 input_size, u32 output_size, u8 activation_type);
void free_neural_layer(neural_layer_t *layer);
void neural_batch_normalize(neural_layer_t *layer, s32 *inputs, u32 batch_size);
int neural_layer_forward_enhanced(neural_layer_t *layer, s32 *input, bool training_mode);
int neural_layer_forward(neural_layer_t *layer, const s32 *input);

/* Validation and debugging functions */
int neural_layer_validate(neural_layer_t *layer);
int neural_network_validate(neural_network_t *nn);
void neural_network_print_stats(neural_network_t *nn);

/* Legacy compatibility functions */
neural_network_t* neural_network_create(u32 INPUT_LAYER, u32 HIDDEN_LAYER, u32 OUTPUT_LAYER);
void neural_network_destroy(neural_network_t *nn);
int neural_network_predict(neural_network_t *nn, const s32 *input, s32 *output);
int neural_network_set_weights(neural_network_t *nn, u32 layer_idx, s32 *weights, s32 *biases);
u32 neural_network_get_confidence(neural_network_t *nn);

/* Batch processing functions */
neural_batch_t* neural_batch_create(u32 batch_size, u32 input_dim, u32 output_dim);
void neural_batch_destroy(neural_batch_t *batch);

/* Model serialization functions */
int neural_network_save_model(neural_network_t *nn, u8 **model_data, size_t *model_size);
int neural_network_load_model(neural_network_t *nn, const u8 *model_data, size_t model_size);

/* Advanced functions */
int neural_network_predict_cached(neural_network_t *nn, const s32 *input, s32 *output);
neural_network_t* neural_network_create_advanced(u32 input_size, u32 hidden_size, u32 output_size,
                                                  bool use_batch_norm, s32 dropout_rate);/* Legacy constructor function */
neural_network_t neural_network_constructor(int INPUT_LAYER, int HIDDEN_LAYER, int OUTPUT_LAYER);
#endif /* _UAPI_NEURAL_MODULE_H */