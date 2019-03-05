#define NAPI_EXPERIMENTAL
#include <assert.h>
#include <stdio.h>

#include <node_api.h>
#include <uv.h>

#include "evmc/evmc.h"
#include "evmc/loader.h"


struct evmc_js_context
{
    /** The Host interface. */
    const struct evmc_host_interface* host;

    /** The EVMC instance. */
    struct evmc_instance* instance;

    /** callbacks **/
    napi_threadsafe_function account_exists_fn;
    napi_threadsafe_function get_storage_fn;
    napi_threadsafe_function set_storage_fn;
    napi_threadsafe_function get_balance_fn;
    napi_threadsafe_function get_code_size_fn;
    napi_threadsafe_function copy_code_fn;
    napi_threadsafe_function selfdestruct_fn;
    napi_threadsafe_function call_fn;
    napi_threadsafe_function get_tx_context_fn;
    napi_threadsafe_function get_block_hash_fn;
    napi_threadsafe_function emit_log_fn;
    napi_threadsafe_function completer;

    /** if freed */
    bool released;
};


void create_bigint_from_evmc_bytes32(napi_env env, const evmc_bytes32* bytes, napi_value* out) {
  uint64_t temp[4];
  temp[3] = __builtin_bswap64(*(uint64_t*)bytes->bytes);
  temp[2] = __builtin_bswap64(*(uint64_t*)(bytes->bytes + 8));
  temp[1] = __builtin_bswap64(*(uint64_t*)(bytes->bytes + 16));
  temp[0] = __builtin_bswap64(*(uint64_t*)(bytes->bytes + 24));

  napi_status status;
  status = napi_create_bigint_words(env, 0, 4, temp, out);
  assert(status == napi_ok);
}

void create_bigint_from_evmc_address(napi_env env, const evmc_address* address, napi_value* out) {
  uint64_t temp[3];
  // top 4 bytes, special case
  temp[2] = __builtin_bswap32(*(uint32_t*)(address->bytes));
  // remaining bytes
  temp[1] = __builtin_bswap64(*(uint64_t*)(address->bytes + 4));
  temp[0] = __builtin_bswap64(*(uint64_t*)(address->bytes + 12));

  napi_status status;
  status = napi_create_bigint_words(env, 0, 3, temp, out);
  assert(status == napi_ok);
}

void get_evmc_bytes32_from_bigint(napi_env env, napi_value in, evmc_bytes32* out) {
  uint64_t temp[4];
  size_t result_word_count = 4;
  int sign_bit = 0;

  // generate status
  napi_status status;
  status = napi_get_value_bigint_words(env, in, &sign_bit, &result_word_count, temp);
  assert(status == napi_ok);

  // generate evmc32
  *((uint64_t*)out->bytes) = result_word_count > 3 ? __builtin_bswap64(temp[3]) : 0;
  *((uint64_t*)(out->bytes + 8)) = result_word_count > 2 ? __builtin_bswap64(temp[2]) : 0;
  *((uint64_t*)(out->bytes + 16)) = result_word_count  > 1 ? __builtin_bswap64(temp[1]) : 0;
  *((uint64_t*)(out->bytes + 24)) = __builtin_bswap64(temp[0]);
}

void get_evmc_address_from_bigint(napi_env env, napi_value in, evmc_address* out) {
  uint64_t temp[3];
  temp[2] = 0;
  temp[1] = 0;
  temp[0] = 0;
  uint8_t* tempAsBytes = (uint8_t*) temp;
  size_t result_word_count = 3;
  int sign_bit = 0;

  // generate status
  napi_status status;
  status = napi_get_value_bigint_words(env, in, &sign_bit, &result_word_count, temp);
  assert(status == napi_ok);
   
  // generate account 
  *((uint64_t*)(out->bytes)) = __builtin_bswap64(*(uint64_t*)(tempAsBytes + 12));
  *((uint64_t*)(out->bytes + 8)) = __builtin_bswap64(*(uint64_t*)(tempAsBytes + 4));
  *((uint32_t*)(out->bytes + 16)) = __builtin_bswap32(*(uint32_t*)(tempAsBytes));
}

struct js_set_storage_call {
  uv_sem_t sem;
  const evmc_address* address;
  const evmc_bytes32* key;
  const evmc_bytes32* value;
  enum evmc_storage_status result;
};


napi_value set_storage_js_promise_success(napi_env env, napi_callback_info info) {
    napi_value argv[1];
    napi_status status;
    struct js_set_storage_call* data;
    size_t argc = 1;

    status = napi_get_cb_info(env, info, &argc, argv, NULL, (void**) &data);
    assert(status == napi_ok);

    int64_t enumVal;
    status = napi_get_value_int64(env, argv[0], &enumVal);
    assert(status == napi_ok);
    data->result = enumVal;

    uv_sem_post(&data->sem);

    return NULL;
}

void set_storage_js(napi_env env, napi_value js_callback, void* context, struct js_set_storage_call* data) {
    napi_status status;
    napi_value global;

    status = napi_get_global(env, &global);
    assert(status == napi_ok);

    napi_value values[3];

    create_bigint_from_evmc_address(env, data->address, &values[0]);
    create_bigint_from_evmc_bytes32(env, data->key, &values[1]);
    create_bigint_from_evmc_bytes32(env, data->value, &values[2]);

    napi_value result;
    status = napi_call_function(env, global, js_callback, 3, values, &result);
    assert(status == napi_ok);

    bool isPromise = false;
    status = napi_is_promise(env, result, &isPromise);
    assert(status == napi_ok);

    if (!isPromise) {
      int64_t enumVal;
      status = napi_get_value_int64(env, result, &enumVal);
      assert(status == napi_ok);
      data->result = enumVal;

      uv_sem_post(&data->sem);
    } else {
      napi_value then_callback;
      status = napi_get_named_property(env, result, "then", &then_callback);
      assert(status == napi_ok);

      napi_value success_callback;
      status = napi_create_function(env, NULL, 0, set_storage_js_promise_success, data, &success_callback);
      assert(status == napi_ok);

      napi_value args[1];
      args[0] = success_callback;
      status = napi_call_function(env, result, then_callback, 1, args, NULL);
      assert(status == napi_ok);
    }
}

enum evmc_storage_status set_storage(struct evmc_js_context* context,
                                            const evmc_address* address,
                                            const evmc_bytes32* key,
                                            const evmc_bytes32* value) {
     napi_status status;

     struct js_set_storage_call callinfo;
     callinfo.address = address;
     callinfo.key = key;
     callinfo.value = value;

     status = napi_acquire_threadsafe_function(context->set_storage_fn);
     assert(status == napi_ok);

     int uv_status;
     uv_status = uv_sem_init(&callinfo.sem, 0);
     assert(uv_status == 0);

     status = napi_call_threadsafe_function(context->set_storage_fn, &callinfo, napi_tsfn_blocking); 
     assert(status == napi_ok);

     uv_sem_wait(&callinfo.sem);

     status = napi_release_threadsafe_function(context->set_storage_fn, napi_tsfn_release);
     assert(status == napi_ok);

     return callinfo.result;
}


struct js_storage_call {
  uv_sem_t sem;
  const evmc_address* address;
  const evmc_bytes32* key;
  evmc_bytes32 result;
};

napi_value get_storage_js_promise_success(napi_env env, napi_callback_info info) {
    napi_value argv[1];
    napi_status status;
    struct js_storage_call* data;
    size_t argc = 1;

    status = napi_get_cb_info(env, info, &argc, argv, NULL, (void**) &data);
    assert(status == napi_ok);

    get_evmc_bytes32_from_bigint(env, argv[0], &data->result);
    uv_sem_post(&data->sem);

    return NULL;
}

void get_storage_js(napi_env env, napi_value js_callback, void* context, struct js_storage_call* data) {
    napi_status status;
    napi_value global;

    status = napi_get_global(env, &global);
    assert(status == napi_ok);

    napi_value values[2];

    create_bigint_from_evmc_address(env, data->address, &values[0]);
    create_bigint_from_evmc_bytes32(env, data->key, &values[1]);

    napi_value result;
    status = napi_call_function(env, global, js_callback, 2, values, &result);
    assert(status == napi_ok);

    bool isPromise = false;
    status = napi_is_promise(env, result, &isPromise);
    assert(status == napi_ok);

    if (!isPromise) {
      get_evmc_bytes32_from_bigint(env, result, &data->result);
      uv_sem_post(&data->sem);
    } else {
      napi_value then_callback;
      status = napi_get_named_property(env, result, "then", &then_callback);
      assert(status == napi_ok);

      napi_value success_callback;
      status = napi_create_function(env, NULL, 0, get_storage_js_promise_success, data, &success_callback);
      assert(status == napi_ok);

      napi_value args[1];
      args[0] = success_callback;
      status = napi_call_function(env, result, then_callback, 1, args, NULL);
      assert(status == napi_ok);
    }
}

 evmc_bytes32 get_storage(struct evmc_js_context* context,
                                            const evmc_address* address,
                                            const evmc_bytes32* key) {
     napi_status status;

     struct js_storage_call callinfo;
     callinfo.address = address;
     callinfo.key = key;

     status = napi_acquire_threadsafe_function(context->get_storage_fn);
     assert(status == napi_ok);

     int uv_status;
     uv_status = uv_sem_init(&callinfo.sem, 0);
     assert(uv_status == 0);

     status = napi_call_threadsafe_function(context->get_storage_fn, &callinfo, napi_tsfn_blocking); 
     assert(status == napi_ok);

     uv_sem_wait(&callinfo.sem);

     status = napi_release_threadsafe_function(context->get_storage_fn, napi_tsfn_release);
     assert(status == napi_ok);

     return callinfo.result;
}

struct js_account_exists_call {
  uv_sem_t sem;
  const evmc_address* address;
  bool result;
};

napi_value account_exists_js_promise_success(napi_env env, napi_callback_info info) {
    napi_value argv[1];
    napi_status status;
    struct js_account_exists_call* data;
    size_t argc = 1;

    status = napi_get_cb_info(env, info, &argc, argv, NULL, (void**) &data);
    assert(status == napi_ok);

    status = napi_get_value_bool(env, argv[0], &data->result);
    assert(status == napi_ok);

    uv_sem_post(&data->sem);

    return NULL;
}

void account_exists_js(napi_env env, napi_value js_callback, void* context, struct js_account_exists_call* data) {
    napi_status status;
    napi_value global;

    status = napi_get_global(env, &global);
    assert(status == napi_ok);

    napi_value values[1];

    create_bigint_from_evmc_address(env, data->address, &values[0]);

    napi_value result;
    status = napi_call_function(env, global, js_callback, 1, values, &result);
    assert(status == napi_ok);

    bool isPromise = false;
    status = napi_is_promise(env, result, &isPromise);
    assert(status == napi_ok);

    if (!isPromise) {
      status = napi_get_value_bool(env, result, &data->result);
      assert(status == napi_ok);

      uv_sem_post(&data->sem);
    } else {
      napi_value then_callback;
      status = napi_get_named_property(env, result, "then", &then_callback);
      assert(status == napi_ok);

      napi_value success_callback;
      status = napi_create_function(env, NULL, 0, account_exists_js_promise_success, data, &success_callback);
      assert(status == napi_ok);

      napi_value args[1];
      args[0] = success_callback;
      status = napi_call_function(env, result, then_callback, 1, args, NULL);
      assert(status == napi_ok);
    }
}

bool account_exists(struct evmc_js_context* context,
  const evmc_address* address) {
    napi_status status;
    
    struct js_account_exists_call callinfo;
    callinfo.address = address;
  
    status = napi_acquire_threadsafe_function(context->account_exists_fn);
    assert(status == napi_ok);

    int uv_status;
    uv_status = uv_sem_init(&callinfo.sem, 0);
    assert(uv_status == 0);

    status = napi_call_threadsafe_function(context->account_exists_fn, &callinfo, napi_tsfn_blocking); 
    assert(status == napi_ok);

    uv_sem_wait(&callinfo.sem);

    status = napi_release_threadsafe_function(context->account_exists_fn, napi_tsfn_release);
    assert(status == napi_ok);

    return callinfo.result;
}


struct js_get_balance_call {
  uv_sem_t sem;
  const evmc_address* address;
  evmc_bytes32 result;
};


napi_value get_balance_js_promise_success(napi_env env, napi_callback_info info) {
    napi_value argv[1];
    napi_status status;
    struct js_get_balance_call* data;
    size_t argc = 1;

    status = napi_get_cb_info(env, info, &argc, argv, NULL, (void**) &data);
    assert(status == napi_ok);

    get_evmc_bytes32_from_bigint(env, argv[0], &data->result);
    uv_sem_post(&data->sem);

    return NULL;
}

void get_balance_js(napi_env env, napi_value js_callback, void* context, struct js_get_balance_call* data) {
    napi_status status;
    napi_value global;

    status = napi_get_global(env, &global);
    assert(status == napi_ok);

    napi_value values[1];

    create_bigint_from_evmc_address(env, data->address, &values[0]);

    napi_value result;
    status = napi_call_function(env, global, js_callback, 1, values, &result);
    assert(status == napi_ok);

    bool isPromise = false;
    status = napi_is_promise(env, result, &isPromise);
    assert(status == napi_ok);

    if (!isPromise) {
      get_evmc_bytes32_from_bigint(env, result, &data->result);
      uv_sem_post(&data->sem);
    } else {
      napi_value then_callback;
      status = napi_get_named_property(env, result, "then", &then_callback);
      assert(status == napi_ok);

      napi_value success_callback;
      status = napi_create_function(env, NULL, 0, get_balance_js_promise_success, data, &success_callback);
      assert(status == napi_ok);

      napi_value args[1];
      args[0] = success_callback;
      status = napi_call_function(env, result, then_callback, 1, args, NULL);
      assert(status == napi_ok);
    }
}

evmc_bytes32 get_balance(struct evmc_js_context* context,
  const evmc_address* address) {
    napi_status status;
    
    struct js_get_balance_call callinfo;
    callinfo.address = address;
  
    status = napi_acquire_threadsafe_function(context->get_balance_fn);
    assert(status == napi_ok);

    int uv_status;
    uv_status = uv_sem_init(&callinfo.sem, 0);
    assert(uv_status == 0);

    status = napi_call_threadsafe_function(context->get_balance_fn, &callinfo, napi_tsfn_blocking); 
    assert(status == napi_ok);

    uv_sem_wait(&callinfo.sem);

    status = napi_release_threadsafe_function(context->get_balance_fn, napi_tsfn_release);
    assert(status == napi_ok);

    return callinfo.result;
}

struct js_get_code_size_call {
  uv_sem_t sem;
  const evmc_address* address;
  size_t result;
};


napi_value get_code_size_js_promise_success(napi_env env, napi_callback_info info) {
    napi_value argv[1];
    napi_status status;
    struct js_get_code_size_call* data;
    size_t argc = 1;

    status = napi_get_cb_info(env, info, &argc, argv, NULL, (void**) &data);
    assert(status == napi_ok);

    bool lossless;
    status = napi_get_value_bigint_uint64(env, argv[0], (uint64_t*) &data->result, &lossless);
    assert(status == napi_ok);

    uv_sem_post(&data->sem);

    return NULL;
}

void get_code_size_js(napi_env env, napi_value js_callback, void* context, struct js_get_code_size_call* data) {
    napi_status status;
    napi_value global;

    status = napi_get_global(env, &global);
    assert(status == napi_ok);

    napi_value values[1];

    create_bigint_from_evmc_address(env, data->address, &values[0]);

    napi_value result;
    status = napi_call_function(env, global, js_callback, 1, values, &result);
    assert(status == napi_ok);

    bool isPromise = false;
    status = napi_is_promise(env, result, &isPromise);
    assert(status == napi_ok);

    if (!isPromise) {
      bool lossless;
      status = napi_get_value_bigint_uint64(env, result, (uint64_t*) &data->result, &lossless);
      assert(status == napi_ok);

      uv_sem_post(&data->sem);
    } else {
      napi_value then_callback;
      status = napi_get_named_property(env, result, "then", &then_callback);
      assert(status == napi_ok);

      napi_value success_callback;
      status = napi_create_function(env, NULL, 0, get_code_size_js_promise_success, data, &success_callback);
      assert(status == napi_ok);

      napi_value args[1];
      args[0] = success_callback;
      status = napi_call_function(env, result, then_callback, 1, args, NULL);
      assert(status == napi_ok);
    }
}

size_t get_code_size(struct evmc_js_context* context,
  const evmc_address* address) {
    napi_status status;

    struct js_get_code_size_call callinfo;
    callinfo.address = address;
  
    status = napi_acquire_threadsafe_function(context->get_code_size_fn);
    assert(status == napi_ok);

    int uv_status;
    uv_status = uv_sem_init(&callinfo.sem, 0);
    assert(uv_status == 0);

    status = napi_call_threadsafe_function(context->get_code_size_fn, &callinfo, napi_tsfn_blocking); 
    assert(status == napi_ok);

    uv_sem_wait(&callinfo.sem);

    status = napi_release_threadsafe_function(context->get_code_size_fn, napi_tsfn_release);
    assert(status == napi_ok);

    return callinfo.result;
}


struct js_copy_code_call {
  uv_sem_t sem;
  const evmc_address* address;
  size_t code_offset;
  uint8_t* buffer_data;
  size_t buffer_size;
  size_t result;
};


napi_value copy_code_js_promise_success(napi_env env, napi_callback_info info) {
    napi_value argv[1];
    napi_status status;
    struct js_copy_code_call* data;
    size_t argc = 1;

    status = napi_get_cb_info(env, info, &argc, argv, NULL, (void**) &data);
    assert(status == napi_ok);

    char* node_buffer;
    size_t node_buffer_length;
    status = napi_get_buffer_info(env, argv[0], (void**) &node_buffer, &node_buffer_length);
    assert(status == napi_ok);

    size_t bytes_written = (node_buffer_length < data->buffer_size) ? node_buffer_length : data->buffer_size;
    memcpy(data->buffer_data, node_buffer, bytes_written);
    data->result = bytes_written;

    uv_sem_post(&data->sem);

    return NULL;
}

void copy_code_js(napi_env env, napi_value js_callback, void* context, struct js_copy_code_call* data) {
    napi_status status;
    napi_value global;

    status = napi_get_global(env, &global);
    assert(status == napi_ok);

    napi_value values[3];

    create_bigint_from_evmc_address(env, data->address, &values[0]);

    status = napi_create_int64(env, data->code_offset, &values[1]);
    assert(status == napi_ok);

    status = napi_create_int64(env, data->buffer_size, &values[2]);
    assert(status == napi_ok);

    napi_value result;
    status = napi_call_function(env, global, js_callback, 3, values, &result);
    assert(status == napi_ok);

    bool isPromise = false;
    status = napi_is_promise(env, result, &isPromise);
    assert(status == napi_ok);

    if (!isPromise) {
      char* node_buffer;
      size_t node_buffer_length;
      status = napi_get_buffer_info(env, result, (void**) &node_buffer, &node_buffer_length);
      assert(status == napi_ok);

      size_t bytes_written = (node_buffer_length < data->buffer_size) ?  node_buffer_length : data->buffer_size;
      memcpy(data->buffer_data, (void**) node_buffer, bytes_written);
      data->result = bytes_written;

      uv_sem_post(&data->sem);
    } else {
      napi_value then_callback;
      status = napi_get_named_property(env, result, "then", &then_callback);
      assert(status == napi_ok);

      napi_value success_callback;
      status = napi_create_function(env, NULL, 0, copy_code_js_promise_success, data, &success_callback);
      assert(status == napi_ok);

      napi_value args[1];
      args[0] = success_callback;
      status = napi_call_function(env, result, then_callback, 1, args, NULL);
      assert(status == napi_ok);
    }
}

size_t copy_code(struct evmc_js_context* context,
    const evmc_address* address,
    size_t code_offset,
    uint8_t* buffer_data,
    size_t buffer_size) {
    napi_status status;
    
    struct js_copy_code_call callinfo;
    callinfo.address = address;
    callinfo.code_offset = code_offset;
    callinfo.buffer_data = buffer_data;
    callinfo.buffer_size = buffer_size;

    status = napi_acquire_threadsafe_function(context->copy_code_fn);
    assert(status == napi_ok);

    int uv_status;
    uv_status = uv_sem_init(&callinfo.sem, 0);
    assert(uv_status == 0);

    status = napi_call_threadsafe_function(context->copy_code_fn, &callinfo, napi_tsfn_blocking); 
    assert(status == napi_ok);

    uv_sem_wait(&callinfo.sem);

    status = napi_release_threadsafe_function(context->copy_code_fn, napi_tsfn_release);
    assert(status == napi_ok);

    return callinfo.result;
}

struct js_selfdestruct_call {
  uv_sem_t sem;
  const evmc_address* address;
  const evmc_address* beneficiary;
};


napi_value selfdestruct_js_promise_success(napi_env env, napi_callback_info info) {
    napi_status status;
    struct js_selfdestruct_call* data;

    status = napi_get_cb_info(env, info, NULL, NULL, NULL, (void**) &data);
    assert(status == napi_ok);

    uv_sem_post(&data->sem);
    return NULL;
}

void selfdestruct_js(napi_env env, napi_value js_callback, void* context, struct js_selfdestruct_call* data) {
    napi_status status;
    napi_value global;

    status = napi_get_global(env, &global);
    assert(status == napi_ok);

    napi_value values[2];

    create_bigint_from_evmc_address(env, data->address, &values[0]);
    create_bigint_from_evmc_address(env, data->beneficiary, &values[1]);

    napi_value result;
    status = napi_call_function(env, global, js_callback, 2, values, &result);
    assert(status == napi_ok);

    bool isPromise = false;
    status = napi_is_promise(env, result, &isPromise);
    assert(status == napi_ok);

    if (!isPromise) {
      uv_sem_post(&data->sem);
    } else {
      napi_value then_callback;
      status = napi_get_named_property(env, result, "then", &then_callback);
      assert(status == napi_ok);

      napi_value success_callback;
      status = napi_create_function(env, NULL, 0, selfdestruct_js_promise_success, data, &success_callback);
      assert(status == napi_ok);

      napi_value args[1];
      args[0] = success_callback;
      status = napi_call_function(env, result, then_callback, 1, args, NULL);
      assert(status == napi_ok);
    }
}

void selfdestruct(struct evmc_js_context* context,
    const evmc_address* address,
    const evmc_address* beneficiary) {
    napi_status status;
    
    struct js_selfdestruct_call callinfo;
    callinfo.address = address;
    callinfo.beneficiary = beneficiary;
  
    status = napi_acquire_threadsafe_function(context->selfdestruct_fn);
    assert(status == napi_ok);

    int uv_status;
    uv_status = uv_sem_init(&callinfo.sem, 0);
    assert(uv_status == 0);

    status = napi_call_threadsafe_function(context->selfdestruct_fn, &callinfo, napi_tsfn_blocking); 
    assert(status == napi_ok);

    uv_sem_wait(&callinfo.sem);


    status = napi_release_threadsafe_function(context->selfdestruct_fn, napi_tsfn_release);
    assert(status == napi_ok);
}

struct js_call_call {
  uv_sem_t sem;
  const struct evmc_message* msg;
  struct evmc_result result;;
};

void call_free_result(const struct evmc_result* result) {
  if (result->output_size > 0) {
    free((void*)result->output_data);
  }
}

napi_value call_js_promise_success(napi_env env, napi_callback_info info) {
    napi_status status;
    struct js_call_call* data;
    size_t argc = 1;
    napi_value argv[1];

    status = napi_get_cb_info(env, info, &argc, argv, NULL, (void**) &data);
    assert(status == napi_ok);

    napi_value node_status_code;
    status = napi_get_named_property(env, argv[0], "statusCode", &node_status_code);
    assert(status == napi_ok);
    int64_t int_status_code;
    status = napi_get_value_int64(env, node_status_code, &int_status_code);
    assert(status == napi_ok);
    data->result.status_code = (enum evmc_status_code) int_status_code;

    napi_value node_gas_left;
    status = napi_get_named_property(env, argv[0], "gasLeft", &node_gas_left);
    assert(status == napi_ok);
    bool gasLeftLossless = true;
    status = napi_get_value_bigint_int64(env, node_gas_left, &data->result.gas_left, &gasLeftLossless);
    assert(status == napi_ok);

    napi_value node_output_data;
    status = napi_get_named_property(env, argv[0], "outputData", &node_output_data);
    assert(status == napi_ok);
    uint8_t* outputData;
    size_t outputData_size;
    status = napi_get_buffer_info(env, node_output_data, (void**) &outputData, &outputData_size);
    assert(status == napi_ok);
    
    data->result.output_size = outputData_size;
    if (outputData_size > 0) {
      data->result.output_data = (uint8_t*) malloc(outputData_size);
      data->result.release = call_free_result;
    }

    napi_value node_create_address;
    status = napi_get_named_property(env, argv[0], "createAddress", &node_create_address);
    assert(status == napi_ok);
    get_evmc_address_from_bigint(env, node_create_address, &data->result.create_address);

    uv_sem_post(&data->sem);
    return NULL;
}

void call_js(napi_env env, napi_value js_callback, void* context, struct js_call_call* data) {
    napi_status status;
    napi_value global;

    status = napi_get_global(env, &global);
    assert(status == napi_ok);

    napi_value values[1];

    // create message object
    status = napi_create_object(env, &values[0]);
    assert(status == napi_ok);

    napi_value node_gas;
    status = napi_create_bigint_int64(env, data->msg->gas, &node_gas);
    assert(status == napi_ok);
    status = napi_set_named_property(env, values[0], "gas", node_gas);
    assert(status == napi_ok);

    napi_value node_depth;
    status = napi_create_int32(env, data->msg->depth, &node_depth);
    assert(status == napi_ok);
    status = napi_set_named_property(env, values[0], "depth", node_depth);
    assert(status == napi_ok);

    napi_value node_flags;
    status = napi_create_uint32(env, data->msg->depth, &node_flags);
    assert(status == napi_ok);
    status = napi_set_named_property(env, values[0], "flags", node_flags);
    assert(status == napi_ok);

    napi_value node_kind;
    status = napi_create_int64(env, data->msg->kind, &node_kind);
    assert(status == napi_ok);
    status = napi_set_named_property(env, values[0], "kind", node_kind);
    assert(status == napi_ok);

    napi_value node_destination;
    create_bigint_from_evmc_address(env, &data->msg->destination, &node_destination);
    status = napi_set_named_property(env, values[0], "destination", node_destination);
    assert(status == napi_ok);

    napi_value node_sender;
    create_bigint_from_evmc_address(env, &data->msg->sender, &node_sender);
    status = napi_set_named_property(env, values[0], "sender", node_sender);
    assert(status == napi_ok);

    if (data->msg->input_size != 0) {
      napi_value node_input;
      status = napi_create_buffer(env, data->msg->input_size, (void**) data->msg->input_data, &node_input);
      assert(status == napi_ok);
      status = napi_set_named_property(env, values[0], "inputData", node_input);
      assert(status == napi_ok);
    }

    napi_value node_value;
    create_bigint_from_evmc_bytes32(env, &data->msg->value, &node_value);
    status = napi_set_named_property(env, values[0], "value", node_value);
    assert(status == napi_ok);

    napi_value result;
    status = napi_call_function(env, global, js_callback, 1, values, &result);
    assert(status == napi_ok);

    bool isPromise = false;
    status = napi_is_promise(env, result, &isPromise);
    assert(status == napi_ok);

    if (!isPromise) {
      napi_value node_status_code;
      status = napi_get_named_property(env, result, "statusCode", &node_status_code);
      assert(status == napi_ok);
      int64_t int_status_code;
      status = napi_get_value_int64(env, node_status_code, &int_status_code);
      assert(status == napi_ok);
      data->result.status_code = (enum evmc_status_code) int_status_code;

      napi_value node_gas_left;
      status = napi_get_named_property(env, result, "gasLeft", &node_gas_left);
      assert(status == napi_ok);
      bool gasLeftLossless = true;
      status = napi_get_value_bigint_int64(env, node_gas_left, &data->result.gas_left, &gasLeftLossless);
      assert(status == napi_ok);

      napi_value node_output_data;
      status = napi_get_named_property(env, result, "outputData", &node_output_data);
      assert(status == napi_ok);
      uint8_t* outputData;
      size_t outputData_size;
      status = napi_get_buffer_info(env, node_output_data, (void**) &outputData, &outputData_size);
      assert(status == napi_ok);
      
      data->result.output_size = outputData_size;
      if (outputData_size > 0) {
        data->result.output_data = (uint8_t*) malloc(outputData_size);
        data->result.release = call_free_result;
      }

      napi_value node_create_address;
      status = napi_get_named_property(env, result, "createAddress", &node_create_address);
      assert(status == napi_ok);
      get_evmc_address_from_bigint(env, node_create_address, &data->result.create_address);

      uv_sem_post(&data->sem);
    } else {
      napi_value then_callback;
      status = napi_get_named_property(env, result, "then", &then_callback);
      assert(status == napi_ok);

      napi_value success_callback;
      status = napi_create_function(env, NULL, 0, call_js_promise_success, data, &success_callback);
      assert(status == napi_ok);

      napi_value args[1];
      args[0] = success_callback;
      status = napi_call_function(env, result, then_callback, 1, args, NULL);
      assert(status == napi_ok);
    }
}

struct evmc_result call(struct evmc_js_context* context,
  const struct evmc_message* msg) {
    napi_status status;
    
    struct js_call_call callinfo;
    callinfo.msg = msg;
  
    status = napi_acquire_threadsafe_function(context->call_fn);
    assert(status == napi_ok);

    int uv_status;
    uv_status = uv_sem_init(&callinfo.sem, 0);
    assert(uv_status == 0);

    status = napi_call_threadsafe_function(context->call_fn, &callinfo, napi_tsfn_blocking); 
    assert(status == napi_ok);

    uv_sem_wait(&callinfo.sem);

    status = napi_release_threadsafe_function(context->call_fn, napi_tsfn_release);
    assert(status == napi_ok);

    return callinfo.result;
}


struct js_tx_context_call {
  uv_sem_t sem;
  struct evmc_tx_context result;
};


napi_value get_tx_context_js_promise_success(napi_env env, napi_callback_info info) {
    napi_status status;
    struct js_tx_context_call* data;
    size_t argc = 1;
    napi_value argv[1];

    status = napi_get_cb_info(env, info, &argc, argv, NULL, (void**) &data);
    assert(status == napi_ok);

      napi_value node_tx_gas_price;
      status = napi_get_named_property(env, argv[0], "txGasPrice", &node_tx_gas_price);
      assert(status == napi_ok);
      get_evmc_bytes32_from_bigint(env, node_tx_gas_price, &data->result.tx_gas_price);

      napi_value node_tx_origin;
      status = napi_get_named_property(env, argv[0], "txOrigin", &node_tx_origin);
      assert(status == napi_ok);
      get_evmc_address_from_bigint(env, node_tx_origin, &data->result.tx_origin);

      napi_value node_blockcoinbase;
      status = napi_get_named_property(env, argv[0], "blockCoinbase", &node_blockcoinbase);
      assert(status == napi_ok);
      get_evmc_address_from_bigint(env, node_blockcoinbase, &data->result.block_coinbase);

      napi_value node_block_number;
      status = napi_get_named_property(env, argv[0], "blockNumber", &node_block_number);
      assert(status == napi_ok);
      bool block_number_lossless = true;
      status = napi_get_value_bigint_int64(env, node_block_number, &data->result.block_number, &block_number_lossless);
      assert(status == napi_ok);

      napi_value node_timestamp;
      status = napi_get_named_property(env, argv[0], "blockTimestamp", &node_timestamp);
      assert(status == napi_ok);
      bool block_timestamp_lossless = true;
      status = napi_get_value_bigint_int64(env, node_timestamp, &data->result.block_timestamp, &block_timestamp_lossless);
      assert(status == napi_ok);

      napi_value node_gas_limit;
      status = napi_get_named_property(env, argv[0], "blockGasLimit", &node_gas_limit);
      assert(status == napi_ok);
      bool block_gas_limit_lossless = true;
      status = napi_get_value_bigint_int64(env, node_gas_limit, &data->result.block_gas_limit, &block_gas_limit_lossless);
      assert(status == napi_ok);

      napi_value node_block_difficulty;
      status = napi_get_named_property(env, argv[0], "blockDifficulty", &node_block_difficulty);
      assert(status == napi_ok);
      get_evmc_bytes32_from_bigint(env, node_block_difficulty, &data->result.block_difficulty);

    uv_sem_post(&data->sem);
    return NULL;
}

void get_tx_context_js(napi_env env, napi_value js_callback, void* context, struct js_tx_context_call* data) {
    napi_status status;
    napi_value global;

    status = napi_get_global(env, &global);
    assert(status == napi_ok);

    napi_value result;
    status = napi_call_function(env, global, js_callback, 0, NULL, &result);
    assert(status == napi_ok);

    bool isPromise = false;
    status = napi_is_promise(env, result, &isPromise);
    assert(status == napi_ok);

    if (!isPromise) {
      napi_value node_tx_gas_price;
      status = napi_get_named_property(env, result, "txGasPrice", &node_tx_gas_price);
      assert(status == napi_ok);
      get_evmc_bytes32_from_bigint(env, node_tx_gas_price, &data->result.tx_gas_price);

      napi_value node_tx_origin;
      status = napi_get_named_property(env, result, "txOrigin", &node_tx_origin);
      assert(status == napi_ok);
      get_evmc_address_from_bigint(env, node_tx_origin, &data->result.tx_origin);

      napi_value node_blockcoinbase;
      status = napi_get_named_property(env, result, "blockCoinbase", &node_blockcoinbase);
      assert(status == napi_ok);
      get_evmc_address_from_bigint(env, node_blockcoinbase, &data->result.block_coinbase);

      napi_value node_block_number;
      status = napi_get_named_property(env, result, "blockNumber", &node_block_number);
      assert(status == napi_ok);
      bool block_number_lossless = true;
      status = napi_get_value_bigint_int64(env, node_block_number, &data->result.block_number, &block_number_lossless);
      assert(status == napi_ok);

      napi_value node_timestamp;
      status = napi_get_named_property(env, result, "blockTimestamp", &node_timestamp);
      assert(status == napi_ok);
      bool block_timestamp_lossless = true;
      status = napi_get_value_bigint_int64(env, node_timestamp, &data->result.block_timestamp, &block_timestamp_lossless);
      assert(status == napi_ok);

      napi_value node_gas_limit;
      status = napi_get_named_property(env, result, "blockGasLimit", &node_gas_limit);
      assert(status == napi_ok);
      bool block_gas_limit_lossless = true;
      status = napi_get_value_bigint_int64(env, node_gas_limit, &data->result.block_gas_limit, &block_gas_limit_lossless);
      assert(status == napi_ok);

      napi_value node_block_difficulty;
      status = napi_get_named_property(env, result, "blockDifficulty", &node_block_difficulty);
      assert(status == napi_ok);
      get_evmc_bytes32_from_bigint(env, node_block_difficulty, &data->result.block_difficulty);

      uv_sem_post(&data->sem);
    } else {
      napi_value then_callback;
      status = napi_get_named_property(env, result, "then", &then_callback);
      assert(status == napi_ok);

      napi_value success_callback;
      status = napi_create_function(env, NULL, 0, get_tx_context_js_promise_success, data, &success_callback);
      assert(status == napi_ok);

      napi_value args[1];
      args[0] = success_callback;
      status = napi_call_function(env, result, then_callback, 1, args, NULL);
      assert(status == napi_ok);
    }
}


struct evmc_tx_context get_tx_context(struct evmc_js_context* context) {
    napi_status status;
    
    struct js_tx_context_call callinfo;
  
    status = napi_acquire_threadsafe_function(context->get_tx_context_fn);
    assert(status == napi_ok);

    int uv_status;
    uv_status = uv_sem_init(&callinfo.sem, 0);
    assert(uv_status == 0);

    status = napi_call_threadsafe_function(context->get_tx_context_fn, &callinfo, napi_tsfn_blocking); 
    assert(status == napi_ok);

    uv_sem_wait(&callinfo.sem);


    status = napi_release_threadsafe_function(context->get_tx_context_fn, napi_tsfn_release);
    assert(status == napi_ok);

    return callinfo.result;
}


struct js_get_block_hash_call {
  uv_sem_t sem;
  uint64_t number;
  evmc_bytes32 result;
};

napi_value get_block_hash_js_promise_success(napi_env env, napi_callback_info info) {
    napi_value argv[1];
    napi_status status;
    struct js_get_block_hash_call* data;
    size_t argc = 1;

    status = napi_get_cb_info(env, info, &argc, argv, NULL, (void**) &data);
    assert(status == napi_ok);

    get_evmc_bytes32_from_bigint(env, argv[0], &data->result);
    uv_sem_post(&data->sem);

    return NULL;
}

void get_block_hash_js(napi_env env, napi_value js_callback, void* context, struct js_get_block_hash_call* data) {
    napi_status status;
    napi_value global;

    status = napi_get_global(env, &global);
    assert(status == napi_ok);

    napi_value values[1];

    status = napi_create_bigint_int64(env, data->number, &values[0]);
    assert(status == napi_ok);

    napi_value result;
    status = napi_call_function(env, global, js_callback, 1, values, &result);
    assert(status == napi_ok);

    bool isPromise = false;
    status = napi_is_promise(env, result, &isPromise);
    assert(status == napi_ok);

    if (!isPromise) {
      get_evmc_bytes32_from_bigint(env, result, &data->result);
      uv_sem_post(&data->sem);
    } else {
      napi_value then_callback;
      status = napi_get_named_property(env, result, "then", &then_callback);
      assert(status == napi_ok);

      napi_value success_callback;
      status = napi_create_function(env, NULL, 0, get_block_hash_js_promise_success, data, &success_callback);
      assert(status == napi_ok);

      napi_value args[1];
      args[0] = success_callback;
      status = napi_call_function(env, result, then_callback, 1, args, NULL);
      assert(status == napi_ok);
    }
}

evmc_bytes32 get_block_hash(struct evmc_js_context* context, uint64_t number) {
    napi_status status;
    
    struct js_get_block_hash_call callinfo;
    callinfo.number = number;
  
    status = napi_acquire_threadsafe_function(context->get_block_hash_fn);
    assert(status == napi_ok);

    int uv_status;
    uv_status = uv_sem_init(&callinfo.sem, 0);
    assert(uv_status == 0);

    status = napi_call_threadsafe_function(context->get_block_hash_fn, &callinfo, napi_tsfn_blocking); 
    assert(status == napi_ok);

    uv_sem_wait(&callinfo.sem);


    status = napi_release_threadsafe_function(context->get_block_hash_fn, napi_tsfn_release);
    assert(status == napi_ok);

    return callinfo.result;
}


struct js_emit_log_call {
  uv_sem_t sem;
  const evmc_address* address;
  const uint8_t* data;
  size_t data_size;
  const evmc_bytes32* topics;
  size_t topics_count;
};


napi_value emit_log_js_promise_success(napi_env env, napi_callback_info info) {
    napi_value argv[1];
    napi_status status;
    struct js_emit_log_call* data;
    size_t argc = 1;

    status = napi_get_cb_info(env, info, &argc, argv, NULL, (void**) &data);
    assert(status == napi_ok);

    uv_sem_post(&data->sem);

    return NULL;
}

void emit_log_js(napi_env env, napi_value js_callback, void* context, struct js_emit_log_call* data) {
    napi_status status;
    napi_value global;

    status = napi_get_global(env, &global);
    assert(status == napi_ok);

    napi_value values[3];

    create_bigint_from_evmc_address(env, data->address, &values[0]);
  
    uint8_t* buffer;
    status = napi_create_buffer_copy(env, data->data_size, (void*) data->data, (void**) &buffer, &values[1]);
    assert(status == napi_ok);

    status = napi_create_array_with_length(env, data->topics_count, &values[2]);
    assert(status == napi_ok);

    size_t i;
    for (i = 0; i < data->topics_count; i++) {
      napi_value topic;
      create_bigint_from_evmc_bytes32(env, &data->topics[i], &topic);
      status = napi_set_element(env, values[2], i, topic);
      assert(status == napi_ok);
    }

    napi_value result;
    status = napi_call_function(env, global, js_callback, 3, values, &result);
    assert(status == napi_ok);

    bool isPromise = false;
    status = napi_is_promise(env, result, &isPromise);
    assert(status == napi_ok);

    if (!isPromise) {
      uv_sem_post(&data->sem);
    } else {
      napi_value then_callback;
      status = napi_get_named_property(env, result, "then", &then_callback);
      assert(status == napi_ok);

      napi_value success_callback;
      status = napi_create_function(env, NULL, 0, emit_log_js_promise_success, data, &success_callback);
      assert(status == napi_ok);

      napi_value args[1];
      args[0] = success_callback;
      status = napi_call_function(env, result, then_callback, 1, args, NULL);
      assert(status == napi_ok);
    }
}


void emit_log(struct evmc_js_context* context,
                                 const evmc_address* address,
                                 const uint8_t* data,
                                 size_t data_size,
                                 const evmc_bytes32 topics[],
                                 size_t topics_count) {
    napi_status status;
    struct js_emit_log_call callinfo;
    callinfo.address = address;
    callinfo.data = data;
    callinfo.data_size = data_size;
    callinfo.topics = topics;
    callinfo.topics_count = topics_count;
  
    status = napi_acquire_threadsafe_function(context->emit_log_fn);
    assert(status == napi_ok);

    int uv_status;
    uv_status = uv_sem_init(&callinfo.sem, 0);
    assert(uv_status == 0);

    status = napi_call_threadsafe_function(context->emit_log_fn, &callinfo, napi_tsfn_blocking); 
    assert(status == napi_ok);

    uv_sem_wait(&callinfo.sem);

    status = napi_release_threadsafe_function(context->emit_log_fn, napi_tsfn_release);
    assert(status == napi_ok);                      
}

struct js_execution_context {
  struct evmc_js_context* context;
  struct evmc_message message;
  enum evmc_revision revision;
  struct evmc_result result;
  
  uint8_t* code;
  size_t code_size;
  
  napi_deferred deferred;
  napi_value promise;
};

// Forward declaration
void completer_js(napi_env env, napi_value js_callback, void* context, struct js_execution_context* data);

void create_callbacks_from_context(napi_env env, struct evmc_js_context* ctx, napi_value node_context) {
  napi_status status;
    
  napi_value unnamed;
  status = napi_create_string_utf8(env, "unnamed", NAPI_AUTO_LENGTH, &unnamed);
  assert(status == napi_ok);

  napi_value account_exists_callback;
  status = napi_get_named_property(env, node_context, "getAccountExists", &account_exists_callback);
  assert(status == napi_ok); 

  status = napi_create_threadsafe_function(env, account_exists_callback, NULL, unnamed, 0, 1, NULL, NULL, NULL, (napi_threadsafe_function_call_js) account_exists_js, &ctx->account_exists_fn);
  assert(status == napi_ok);

  napi_value storage_callback;
  status = napi_get_named_property(env, node_context, "getStorage", &storage_callback);
  assert(status == napi_ok); 

  status = napi_create_threadsafe_function(env, storage_callback, NULL, unnamed, 0, 1, NULL, NULL, NULL, (napi_threadsafe_function_call_js) get_storage_js, &ctx->get_storage_fn);
  assert(status == napi_ok);

  napi_value set_storage_callback;
  status = napi_get_named_property(env, node_context, "setStorage", &set_storage_callback);
  assert(status == napi_ok); 

  status = napi_create_threadsafe_function(env, set_storage_callback, NULL, unnamed, 0, 1, NULL, NULL, NULL, (napi_threadsafe_function_call_js) set_storage_js, &ctx->set_storage_fn);
  assert(status == napi_ok);
    
  napi_value get_balance_callback;
  status = napi_get_named_property(env, node_context, "getBalance", &get_balance_callback);
  assert(status == napi_ok); 

  status = napi_create_threadsafe_function(env, get_balance_callback, NULL, unnamed, 0, 1, NULL, NULL, NULL, (napi_threadsafe_function_call_js) get_balance_js, &ctx->get_balance_fn);
  assert(status == napi_ok);

  napi_value get_code_size_callback;
  status = napi_get_named_property(env, node_context, "getCodeSize", &get_code_size_callback);
  assert(status == napi_ok); 

  status = napi_create_threadsafe_function(env, get_code_size_callback, NULL, unnamed, 0, 1, NULL, NULL, NULL, (napi_threadsafe_function_call_js) get_code_size_js, &ctx->get_code_size_fn);
  assert(status == napi_ok);

  napi_value copy_code_callback;
  status = napi_get_named_property(env, node_context, "copyCode", &copy_code_callback);
  assert(status == napi_ok); 

  status = napi_create_threadsafe_function(env, copy_code_callback, NULL, unnamed, 0, 1, NULL, NULL, NULL, (napi_threadsafe_function_call_js) copy_code_js, &ctx->copy_code_fn);
  assert(status == napi_ok);

  napi_value self_destruct_callback;
  status = napi_get_named_property(env, node_context, "selfDestruct", &self_destruct_callback);
  assert(status == napi_ok); 

  status = napi_create_threadsafe_function(env, self_destruct_callback, NULL, unnamed, 0, 1, NULL, NULL, NULL, (napi_threadsafe_function_call_js) selfdestruct_js, &ctx->selfdestruct_fn);
  assert(status == napi_ok);

  napi_value call_callback;
  status = napi_get_named_property(env, node_context, "call", &call_callback);
  assert(status == napi_ok); 

  status = napi_create_threadsafe_function(env, call_callback, NULL, unnamed, 0, 1, NULL, NULL, NULL, (napi_threadsafe_function_call_js) call_js, &ctx->call_fn);
  assert(status == napi_ok);

  napi_value tx_context_callback;
  status = napi_get_named_property(env, node_context, "getTxContext", &tx_context_callback);
  assert(status == napi_ok); 

  status = napi_create_threadsafe_function(env, tx_context_callback, NULL, unnamed, 0, 1, NULL, NULL, NULL, (napi_threadsafe_function_call_js) get_tx_context_js, &ctx->get_tx_context_fn);
  assert(status == napi_ok);

  napi_value get_block_hash_callback;
  status = napi_get_named_property(env, node_context, "getBlockHash", &get_block_hash_callback);
  assert(status == napi_ok); 

  status = napi_create_threadsafe_function(env, get_block_hash_callback, NULL, unnamed, 0, 1, NULL, NULL, NULL, (napi_threadsafe_function_call_js) get_block_hash_js, &ctx->get_block_hash_fn);
  assert(status == napi_ok);

  napi_value emit_log_callback;
  status = napi_get_named_property(env, node_context, "emitLog", &emit_log_callback);
  assert(status == napi_ok); 

  status = napi_create_threadsafe_function(env, emit_log_callback, NULL, unnamed, 0, 1, NULL, NULL, NULL, (napi_threadsafe_function_call_js) emit_log_js, &ctx->emit_log_fn);
  assert(status == napi_ok);

  napi_value execute_complete_callback;
  status = napi_get_named_property(env, node_context, "executeComplete", &execute_complete_callback);
  assert(status == napi_ok); 

  status = napi_create_threadsafe_function(env, execute_complete_callback, NULL, unnamed, 0, 1, NULL, NULL, NULL, (napi_threadsafe_function_call_js) completer_js, &ctx->completer);
  assert(status == napi_ok);
}

void release_callbacks_from_context(napi_env env, struct evmc_js_context* ctx) {
  napi_status status;

  status = napi_release_threadsafe_function(ctx->account_exists_fn, napi_tsfn_release);
  assert(status == napi_ok);
  
  status = napi_release_threadsafe_function(ctx->get_storage_fn, napi_tsfn_release);
  assert(status == napi_ok);
  
  status = napi_release_threadsafe_function(ctx->set_storage_fn, napi_tsfn_release);
  assert(status == napi_ok);

  status = napi_release_threadsafe_function(ctx->get_balance_fn, napi_tsfn_release);
  assert(status == napi_ok);

  status = napi_release_threadsafe_function(ctx->get_code_size_fn, napi_tsfn_release);
  assert(status == napi_ok);

  status = napi_release_threadsafe_function(ctx->copy_code_fn, napi_tsfn_release);
  assert(status == napi_ok);

  status = napi_release_threadsafe_function(ctx->selfdestruct_fn, napi_tsfn_release);
  assert(status == napi_ok);

  status = napi_release_threadsafe_function(ctx->call_fn, napi_tsfn_release);
  assert(status == napi_ok);

  status = napi_release_threadsafe_function(ctx->get_tx_context_fn, napi_tsfn_release);
  assert(status == napi_ok);

  status = napi_release_threadsafe_function(ctx->get_block_hash_fn, napi_tsfn_release);
  assert(status == napi_ok);

  status = napi_release_threadsafe_function(ctx->emit_log_fn, napi_tsfn_release);
  assert(status == napi_ok);

  status = napi_release_threadsafe_function(ctx->completer, napi_tsfn_release);
  assert(status == napi_ok);

}

void completer_js(napi_env env, napi_value js_callback, void* context, struct js_execution_context* data) {
  napi_status status;
  napi_value out;

  status = napi_create_object(env, &out);
  assert(status == napi_ok);

  napi_value statusCode;
  status = napi_create_int32(env, data->result.status_code, &statusCode);
  assert(status == napi_ok);
  status = napi_set_named_property(env, out, "statusCode", statusCode);
  assert(status == napi_ok);

  napi_value gasLeft;
  status = napi_create_bigint_int64(env, data->result.gas_left, &gasLeft);
  assert(status == napi_ok);
  status = napi_set_named_property(env, out, "gasLeft", gasLeft);
  assert(status == napi_ok);

  napi_value outputData;
  void* outputDataBuffer;
  status = napi_create_buffer_copy(env, data->result.output_size, data->result.output_data, &outputDataBuffer, &outputData);
  assert(status == napi_ok);
  status = napi_set_named_property(env, out, "outputData", outputData);
  assert(status == napi_ok);

  if (data->result.status_code == EVMC_SUCCESS) {
    napi_value createAddress;
    status = napi_create_bigint_words(env, 0, 4, (uint64_t*) data->result.create_address.bytes, &createAddress);
    assert(status == napi_ok);
    status = napi_set_named_property(env, out, "createAddress", createAddress);
    assert(status == napi_ok);
  }

  status = napi_resolve_deferred(env, data->deferred, out);
  assert(status == napi_ok);
}

void execute_done(uv_work_t* work, int status) {
  free(work);
}

void execute(uv_work_t* work) {
  struct js_execution_context* data = (struct js_execution_context*) work->data;
  data->result = data->context->instance->execute(data->context->instance, (struct evmc_context*) data->context, data->revision, &data->message, data->code, data->code_size);
  if (data->code != NULL) {
    free(data->code);
  }
  if (data->message.input_size != 0) {
    free((void*) data->message.input_data);
  }
  napi_call_threadsafe_function(data->context->completer, data, napi_tsfn_blocking);
}

struct evmc_host_interface host_interface;

napi_value evmc_execute_evm(napi_env env, napi_callback_info info) {
  napi_value argv[2];
  napi_status status;

  size_t argc = 2;

  status = napi_get_cb_info(env, info, &argc, argv, NULL, NULL);
  assert(status == napi_ok);

  if (argc < 2) {
    napi_throw_error(env, "EINVAL", "Too few arguments");
    return NULL;
  }

  // this needs to run on another thread, apparently, so we need to return a promise
  struct js_execution_context* js_ctx = (struct js_execution_context*) malloc(sizeof(struct js_execution_context));

  status = napi_get_value_external(env, argv[0], (void*) &js_ctx->context);
  assert(status == napi_ok);
 
  napi_value node_revision;
  status = napi_get_named_property(env, argv[1], "revision", &node_revision);
  assert(status == napi_ok);
  status = napi_get_value_int32(env, node_revision, (int32_t*) &js_ctx->revision);
  assert(status == napi_ok);

  napi_value node_message;
  status = napi_get_named_property(env, argv[1], "message", &node_message);
  assert(status == napi_ok);

  napi_value node_message_gas;
  status = napi_get_named_property(env, node_message, "gas", &node_message_gas);
  assert(status == napi_ok);
  bool node_message_gas_lossless;
  status = napi_get_value_bigint_int64(env, node_message_gas, &js_ctx->message.gas, &node_message_gas_lossless);
  assert(status == napi_ok);

  napi_value node_message_depth;
  status = napi_get_named_property(env, node_message, "depth", &node_message_depth);
  assert(status == napi_ok);
  status = napi_get_value_int32(env, node_message_depth, &js_ctx->message.depth);
  assert(status == napi_ok);
  
  napi_value node_message_flags;
  status = napi_get_named_property(env, node_message, "flags", &node_message_flags);
  assert(status == napi_ok);
  status = napi_coerce_to_number(env, node_message_flags, &node_message_flags);
  assert(status == napi_ok);
  status = napi_get_value_uint32(env, node_message_flags, &js_ctx->message.flags);
  assert(status == napi_ok);

  napi_value node_message_destination;
  status = napi_get_named_property(env, node_message, "destination", &node_message_destination);
  assert(status == napi_ok);
  get_evmc_address_from_bigint(env, node_message_destination, &js_ctx->message.destination);

  napi_value node_message_sender;
  status = napi_get_named_property(env, node_message, "sender", &node_message_sender);
  assert(status == napi_ok);
  get_evmc_address_from_bigint(env, node_message_sender, &js_ctx->message.sender);

  napi_value node_message_input_data;
  status = napi_get_named_property(env, node_message, "inputData", &node_message_input_data);
  assert(status == napi_ok);
  uint8_t* input_buffer;
  status = napi_get_buffer_info(env, node_message_input_data, (void**) &input_buffer, &js_ctx->message.input_size);
  assert(status == napi_ok);
  if (js_ctx->message.input_size != 0 ){
    js_ctx->message.input_data = (uint8_t*) malloc(js_ctx->message.input_size);
  } else {
    js_ctx->message.input_data = NULL;
  }

  napi_value node_message_value;
  status = napi_get_named_property(env, node_message, "value", &node_message_value);
  assert(status == napi_ok);
  get_evmc_bytes32_from_bigint(env, node_message_value, &js_ctx->message.value);

  napi_value node_message_kind;
  status = napi_get_named_property(env, node_message, "kind", &node_message_kind);
  assert(status == napi_ok);
  int64_t message_kind;
  status = napi_get_value_int64(env, node_message_kind, &message_kind);
  assert(status == napi_ok);
  js_ctx->message.kind = (enum evmc_call_kind) message_kind;

  size_t code_size;
  uint8_t* code;
  napi_value node_code;

  status = napi_get_named_property(env, argv[1], "code", &node_code);
  assert(status == napi_ok);

  status = napi_get_buffer_info(env, node_code, (void**) &code, &code_size);
  assert(status == napi_ok);

  js_ctx->code_size = code_size;
  js_ctx->code = (uint8_t*) malloc(code_size);
  memcpy(js_ctx->code, code, code_size);

  status = napi_create_promise(env, &js_ctx->deferred, &js_ctx->promise);
  assert(status == napi_ok);

  uv_work_t* work = (uv_work_t*) malloc((sizeof(uv_work_t)));
  work->data = (void*) js_ctx;
  uv_queue_work(uv_default_loop(), work, execute, execute_done);
  
  return js_ctx->promise;
}


void evmc_cleanup_evm(napi_env env, void* finalize_data, void* finalize_hint) {
    struct evmc_js_context* context = (struct evmc_js_context*) finalize_data;

    if (!context->released) {
      context->instance->destroy(context->instance);
      release_callbacks_from_context(env, context);
    }

    free(context);
}

napi_value evmc_create_evm(napi_env env, napi_callback_info info) {
    napi_status status;
    napi_value out;
    
    size_t argc = 2;
    napi_value argv[2];

    status = napi_get_cb_info(env, info, &argc, argv, NULL, NULL);
    assert(status == napi_ok);

    if (argc != 2) {
      status = napi_throw_error(env, "EINVAL", "Expected 2 arguments");
    }

    char* path;
    size_t path_size;

    status = napi_get_value_string_utf8(env, argv[0], NULL, 0, &path_size);
    assert(status == napi_ok);
    // path_size excludes NULL terminator
    path = (char*) malloc(path_size + 1);
    status = napi_get_value_string_utf8(env, argv[0], path, path_size + 1, &path_size);
    assert(status == napi_ok);
    
    enum evmc_loader_error_code error_code;
    struct evmc_instance* instance = evmc_load_and_create(path, &error_code);
    assert(error_code == EVMC_LOADER_SUCCESS);
    free((void*) path);

    struct evmc_js_context* context = (struct evmc_js_context*) malloc(sizeof(struct evmc_js_context));
    context->instance = instance;
    context->host = &host_interface;
    context->released = false;

    create_callbacks_from_context(env, context, argv[1]);

    status = napi_create_external(env, context, evmc_cleanup_evm, NULL, &out);
    assert(status == napi_ok);
    return out;
}


napi_value evmc_release_evm(napi_env env, napi_callback_info info) {
    napi_status status;
    
    size_t argc = 1;
    napi_value argv[1];

    status = napi_get_cb_info(env, info, &argc, argv, NULL, NULL);
    assert(status == napi_ok);

    if (argc != 1) {
      status = napi_throw_error(env, "EINVAL", "Expected 1 argument");
    }

    struct evmc_js_context* context;
    status = napi_get_value_external(env, argv[0], (void**) &context);

    if (!context->released) {
      context->instance->destroy(context->instance);
      release_callbacks_from_context(env, context);
      context->released = true;
    }

    return NULL;
}

napi_value init_all (napi_env env, napi_value exports) {
  napi_value evmc_create_evm_fn;
  napi_value evmc_execute_evm_fn;
  napi_value evmc_release_evm_fn;

  host_interface.account_exists = (evmc_account_exists_fn) account_exists;
  host_interface.get_storage = (evmc_get_storage_fn) get_storage;
  host_interface.set_storage = (evmc_set_storage_fn) set_storage;
  host_interface.get_balance = (evmc_get_balance_fn) get_balance;
  host_interface.get_code_size = (evmc_get_code_size_fn) get_code_size;
  host_interface.copy_code = (evmc_copy_code_fn) copy_code;
  host_interface.selfdestruct = (evmc_selfdestruct_fn) selfdestruct;
  host_interface.call = (evmc_call_fn) call;
  host_interface.get_tx_context = (evmc_get_tx_context_fn) get_tx_context;
  host_interface.get_block_hash = (evmc_get_block_hash_fn) get_block_hash;
  host_interface.emit_log = (evmc_emit_log_fn) emit_log;

  napi_create_function(env, NULL, 0, evmc_create_evm, NULL, &evmc_create_evm_fn);
  napi_create_function(env, NULL, 0, evmc_execute_evm, NULL, &evmc_execute_evm_fn);
  napi_create_function(env, NULL, 0, evmc_release_evm, NULL, &evmc_release_evm_fn);

  napi_set_named_property(env, exports, "createEvmcEvm", evmc_create_evm_fn);
  napi_set_named_property(env, exports, "executeEvmcEvm", evmc_execute_evm_fn);
  napi_set_named_property(env, exports, "releaseEvmcEvm", evmc_release_evm_fn);

  return exports;
}

NAPI_MODULE(NODE_GYP_MODULE_NAME, init_all);