import * as path from 'path';

type EvmcHandle = void;
const evmc: EvmcBinding = require('bindings')('evmc');

/**
 * EVM revision.
 *
 *  The revision of the EVM specification based on the Ethereum
 *  upgrade / hard fork codenames.
 */
export enum EvmcRevision {
  /**
   * The Frontier revision.
   *
   * The one Ethereum launched with.
   */
  EVMC_FRONTIER = 0,

  /**
   * The Homestead revision.
   *
   * https://eips.ethereum.org/EIPS/eip-606
   */
  EVMC_HOMESTEAD = 1,

  /**
   * The Tangerine Whistle revision.
   *
   * https://eips.ethereum.org/EIPS/eip-608
   */
  EVMC_TANGERINE_WHISTLE = 2,

  /**
   * The Spurious Dragon revision.
   *
   * https://eips.ethereum.org/EIPS/eip-607
   */
  EVMC_SPURIOUS_DRAGON = 3,

  /**
   * The Byzantium revision.
   *
   * https://eips.ethereum.org/EIPS/eip-609
   */
  EVMC_BYZANTIUM = 4,

  /**
   * The Constantinople revision.
   *
   * https://eips.ethereum.org/EIPS/eip-1013
   */
  EVMC_CONSTANTINOPLE = 5,

  /**
   * The Petersburg revision.
   *
   * Other names: Constantinople2, ConstantinopleFix.
   *
   * https://eips.ethereum.org/EIPS/eip-1716
   */
  EVMC_PETERSBURG = 6,

  /**
   * The Istanbul revision.
   *
   * https://eips.ethereum.org/EIPS/eip-1679
   */
  EVMC_ISTANBUL = 7,

  /**
   * The Berlin revision.
   *
   * https://github.com/ethereum/eth1.0-specs/blob/master/network-upgrades/mainnet-upgrades/berlin.md
   */
  EVMC_BERLIN = 8,

  /**
   * The London revision.
   *
   * https://github.com/ethereum/eth1.0-specs/blob/master/network-upgrades/mainnet-upgrades/london.md
   */
  EVMC_LONDON = 9,

  /**
   * The Shanghai revision.
   *
   * https://github.com/ethereum/eth1.0-specs/blob/master/network-upgrades/mainnet-upgrades/shanghai.md
   */
  EVMC_SHANGHAI = 10,

  /** The maximum EVM revision supported. */
  EVMC_MAX_REVISION = EVMC_SHANGHAI,

  /**
   * The latest known EVM revision with finalized specification.
   *
   * This is handy for EVM tools to always use the latest revision available.
   */
  EVMC_LATEST_STABLE_REVISION = EVMC_LONDON
}

/** The flags for ::evmc_message. */
export enum evmc_flags {
  EVMC_NO_FLAG = 0, /** No Flags. */
  EVMC_STATIC = 1   /** Static call mode. */
}

export enum EvmcStatusCode {
  /** Execution finished with success. */
  EVMC_SUCCESS = 0,

  /** Generic execution failure. */
  EVMC_FAILURE = 1,

  /**
   * Execution terminated with REVERT opcode.
   *
   * In this case the amount of gas left MAY be non-zero and additional output
   * data MAY be provided in ::evmc_result.
   */
  EVMC_REVERT = 2,

  /** The execution has run out of gas. */
  EVMC_OUT_OF_GAS = 3,

  /**
   * The designated INVALID instruction has been hit during execution.
   *
   * The EIP-141 (https://github.com/ethereum/EIPs/blob/master/EIPS/eip-141.md)
   * defines the instruction 0xfe as INVALID instruction to indicate execution
   * abortion coming from high-level languages. This status code is reported
   * in case this INVALID instruction has been encountered.
   */
  EVMC_INVALID_INSTRUCTION = 4,

  /** An undefined instruction has been encountered. */
  EVMC_UNDEFINED_INSTRUCTION = 5,

  /**
   * The execution has attempted to put more items on the EVM stack
   * than the specified limit.
   */
  EVMC_STACK_OVERFLOW = 6,

  /** Execution of an opcode has required more items on the EVM stack. */
  EVMC_STACK_UNDERFLOW = 7,

  /** Execution has violated the jump destination restrictions. */
  EVMC_BAD_JUMP_DESTINATION = 8,

  /**
   * Tried to read outside memory bounds.
   *
   * An example is RETURNDATACOPY reading past the available buffer.
   */
  EVMC_INVALID_MEMORY_ACCESS = 9,

  /** Call depth has exceeded the limit (if any) */
  EVMC_CALL_DEPTH_EXCEEDED = 10,

  /** Tried to execute an operation which is restricted in static mode. */
  EVMC_STATIC_MODE_VIOLATION = 11,

  /**
   * A call to a precompiled or system contract has ended with a failure.
   *
   * An example: elliptic curve functions handed invalid EC points.
   */
  EVMC_PRECOMPILE_FAILURE = 12,

  /**
   * Contract validation has failed (e.g. due to EVM 1.5 jump validity,
   * Casper's purity checker or ewasm contract rules).
   */
  EVMC_CONTRACT_VALIDATION_FAILURE = 13,

  /**
   * An argument to a state accessing method has a value outside of the
   * accepted range of values.
   */
  EVMC_ARGUMENT_OUT_OF_RANGE = 14,

  /**
   * A WebAssembly `unreachable` instruction has been hit during execution.
   */
  EVMC_WASM_UNREACHABLE_INSTRUCTION = 15,

  /**
   * A WebAssembly trap has been hit during execution. This can be for many
   * reasons, including division by zero, validation errors, etc.
   */
  EVMC_WASM_TRAP = 16,

  /** The caller does not have enough funds for value transfer. */
  EVMC_INSUFFICIENT_BALANCE = 17,

  /** EVM implementation generic internal error. */
  EVMC_INTERNAL_ERROR = -1,

  /**
   * The execution of the given code and/or message has been rejected
   * by the EVM implementation.
   *
   * This error SHOULD be used to signal that the EVM is not able to or
   * willing to execute the given code type or message.
   * If an EVM returns the ::EVMC_REJECTED status code,
   * the Client MAY try to execute it in other EVM implementation.
   * For example, the Client tries running a code in the EVM 1.5. If the
   * code is not supported there, the execution falls back to the EVM 1.0.
   */
  EVMC_REJECTED = -2,

  /** The VM failed to allocate the amount of memory needed for execution. */
  EVMC_OUT_OF_MEMORY = -3
}

/** The kind of call to invoke the EVM with. */
export enum EvmcCallKind {
  EVMC_CALL = 0,         /** Request CALL. */
  EVMC_DELEGATECALL = 1, /**
                          * Request DELEGATECALL. Valid since Homestead. The
                          * value param ignored.
                          */
  EVMC_CALLCODE = 2,     /** Request CALLCODE. */
  EVMC_CREATE = 3,       /**  Request CREATE. */
  EVMC_CREATE2 = 4       /** Request CREATE2. Valid since Constantinople. */
}

/**
 * The effect of an attempt to modify a contract storage item.
 *
 * For the purpose of explaining the meaning of each element, the following
 * notation is used:
 * - 0 is zero value,
 * - X != 0 (X is any value other than 0),
 * - Y != X, Y != 0 (Y is any value other than X and 0),
 * - Z != Y (Z is any value other than Y),
 * - the "->" means the change from one value to another.
 */
export enum EvmcStorageStatus {
  /**
   * The value of a storage item has been left unchanged: 0 -> 0 and X -> X.
   */
  EVMC_STORAGE_UNCHANGED = 0,

  /**
   * The value of a storage item has been modified: X -> Y.
   */
  EVMC_STORAGE_MODIFIED = 1,

  /**
   * A storage item has been modified after being modified before: X -> Y -> Z.
   */
  EVMC_STORAGE_MODIFIED_AGAIN = 2,

  /**
   * A new storage item has been added: 0 -> X.
   */
  EVMC_STORAGE_ADDED = 3,

  /**
   * A storage item has been deleted: X -> 0.
   */
  EVMC_STORAGE_DELETED = 4
}

export enum EvmcAccessStatus {
  /**
   * The entry hasn't been accessed before – it's the first access.
   */
  EVMC_ACCESS_COLD = 0,

  /**
   * The entry is already in accessed_addresses or accessed_storage_keys.
   */
  EVMC_ACCESS_WARM = 1
}

export interface EvmcMessage {
  gas: bigint;
  flags?: evmc_flags;
  depth: number;
  sender: bigint;
  destination: bigint;
  inputData: Buffer;
  value: bigint;
  kind: EvmcCallKind;
  create2Salt: bigint;
}

export interface EvmcExecutionParameters {
  revision: EvmcRevision;
  message: EvmcMessage;
  code: Buffer;
}

export interface EvmcResult {
  statusCode: EvmcStatusCode;
  gasLeft: bigint;
  outputData: Buffer;
  createAddress: bigint;
}

/** The context that the current transaction is executed in */
export interface EvmcTxContext {
  txGasPrice: bigint;      /** The transaction gas price. */
  txOrigin: bigint;        /** The transaction origin account. */
  blockCoinbase: bigint;   /** The miner of the block. */
  blockNumber: bigint;     /**  The block number. */
  blockTimestamp: bigint;  /**  The block timestamp. */
  blockGasLimit: bigint;   /**  The block gas limit. */
  blockDifficulty: bigint; /** The block difficulty. */
  chainId: bigint;         /** The chain id. */
  blockBaseFee: bigint;    /** The block base fee. */
}

/** Private interface to interact with the EVM binding. */
interface EvmcBinding {
  createEvmcEvm(path: string, context: EvmJsContext, obj: {}): EvmcHandle;
  executeEvmcEvm(handle: EvmcHandle, parameters: EvmcExecutionParameters):
      EvmcResult;
  releaseEvmcEvm(handle: EvmcHandle): void;
}

/** Private interface to pass as callback to the EVM binding. */
interface EvmJsContext {
  getAccountExists(account: bigint): Promise<boolean>|boolean;
  getStorage(account: bigint, key: bigint): Promise<bigint>|bigint;
  setStorage(account: bigint, key: bigint, val: bigint):
      Promise<EvmcStorageStatus>|EvmcStorageStatus;
  getBalance(account: bigint): Promise<bigint>|bigint;
  getCodeSize(account: bigint): Promise<bigint>|bigint;
  getCodeHash(account: bigint): Promise<bigint>|bigint;
  copyCode(account: bigint, offset: number, length: number):
      Promise<Buffer>|Buffer;
  selfDestruct(account: bigint, beneficiary: bigint): Promise<void>|void;
  call(message: EvmcMessage): Promise<EvmcResult>|EvmcResult;
  getTxContext(): Promise<EvmcTxContext>|EvmcTxContext;
  getBlockHash(num: bigint): Promise<bigint>|bigint;
  emitLog(account: bigint, data: Buffer, topics: Array<bigint>): Promise<void>|
      void;
  accessAccount(account: bigint): Promise<EvmcAccessStatus>|EvmcAccessStatus;
  accessStorage(address: bigint, key: bigint):
      Promise<EvmcAccessStatus>|EvmcAccessStatus;
  executeComplete(): void;
}

const getDynamicLibraryExtension = () => {
  return process.platform === 'win32' ?
      'dll' :
      process.platform === 'darwin' ? 'dylib' : 'so';
};

export abstract class Evmc {
  _evm: EvmcHandle;
  released = false;

  constructor(_path?: string) {
    this._evm = evmc.createEvmcEvm(
        _path ||
            path.join(
                __dirname,
                `../libbuild/evmone/lib/libevmone.${
                    getDynamicLibraryExtension()}`),
        {
          getAccountExists: this.getAccountExists,
          getStorage: this.getStorage,
          setStorage: this.setStorage,
          getBalance: this.getBalance,
          getCodeSize: this.getCodeSize,
          getCodeHash: this.getCodeHash,
          copyCode: this.copyCode,
          selfDestruct: this.selfDestruct,
          getTxContext: this.getTxContext,
          call: this.call,
          getBlockHash: this.getBlockHash,
          emitLog: this.emitLog,
          accessAccount: this.accessAccount,
          accessStorage: this.accessStorage,
          executeComplete: () => {}
        },
        this);
  }


  /**
   * Check account existence callback function.
   *
   * This callback function is used by the VM to check if
   * there exists an account at given address.
   * @param address  The address of the account the query is about.
   * @return         true if exists, false otherwise.
   */
  abstract getAccountExists(address: bigint): Promise<boolean>|boolean;

  /**
   * Get storage callback function.
   *
   * This callback function is used by a VM to query the given account storage
   * entry.
   *
   * @param address  The address of the account.
   * @param key      The index of the account's storage entry.
   * @return         The storage value at the given storage key or null bytes
   *                 if the account does not exist.
   */
  abstract getStorage(account: bigint, key: bigint): Promise<bigint>|bigint;

  /**
   * Set storage callback function.
   *
   * This callback function is used by a VM to update the given account storage
   * entry. The VM MUST make sure that the account exists. This requirement is
   * only a formality because VM implementations only modify storage of the
   * account of the current execution context (i.e. referenced by
   * evmc_message::destination).
   *
   * @param address  The address of the account.
   * @param key      The index of the storage entry.
   * @param value    The value to be stored.
   * @return         The effect on the storage item.
   */
  abstract setStorage(account: bigint, key: bigint, value: bigint):
      Promise<EvmcStorageStatus>|EvmcStorageStatus;

  /**
   * Get balance callback function.
   *
   * This callback function is used by a VM to query the balance of the given
   * account.
   *
   * @param address  The address of the account.
   * @return         The balance of the given account or 0 if the account does
   * not exist.
   */
  abstract getBalance(account: bigint): Promise<bigint>|bigint;

  /**
   * Get code size callback function.
   *
   * This callback function is used by a VM to get the size of the code stored
   * in the account at the given address.
   *
   * @param address  The address of the account.
   * @return         The size of the code in the account or 0 if the account
   * does not exist.
   */
  abstract getCodeSize(address: bigint): Promise<bigint>|bigint;

  /**
   * Get code hash callback function.
   *
   * This callback function is used by a VM to get the keccak256 hash of the
   * code stored in the account at the given address. For existing accounts not
   * having a code, this function returns keccak256 hash of empty data.
   *
   * @param address  The address of the account.
   * @return         The hash of the code in the account or null bytes if the
   * account does not exist.
   */
  abstract getCodeHash(address: bigint): Promise<bigint>|bigint;

  /**
   * Copy code callback function.
   *
   *  This callback function is used by an EVM to request a copy of the code
   *  of the given account to the memory buffer provided by the EVM.
   *  The Client MUST copy the requested code, starting with the given offset,
   *  to the returned memory buffer up to length or the size of
   *  the code, whichever is smaller.
   *
   *  @param address      The address of the account.
   *  @param offset       The offset of the code to copy.
   *  @param length       The length of the code to copy. A buffer returned
   * larger than length will be truncated.
   *  @param buffer       A buffer containing the code, up to size length.
   * Client.
   */
  abstract copyCode(account: bigint, offset: number, length: number):
      Promise<Buffer>|Buffer;


  /**
   * Selfdestruct callback function.
   *
   *  This callback function is used by an EVM to SELFDESTRUCT given contract.
   *  The execution of the contract will not be stopped, that is up to the EVM.
   *
   *  @param address      The address of the contract to be selfdestructed.
   *  @param beneficiary  The address where the remaining ETH is going to be
   *                      transferred.
   */
  abstract selfDestruct(address: bigint, beneficiary: bigint): Promise<void>|
      void;

  /**
   * Pointer to the callback function supporting EVM calls.
   *
   * @param  msg     The call parameters.
   * @return         The result of the call.
   */
  abstract call(message: EvmcMessage): Promise<EvmcResult>|EvmcResult;


  /**
   * Get transaction context callback function.
   *
   *  This callback function is used by an EVM to retrieve the transaction and
   *  block context.
   *
   *  @return              The transaction context.
   */
  abstract getTxContext(): Promise<EvmcTxContext>|EvmcTxContext;

  /**
   * Get block hash callback function.
   *
   * This callback function is used by a VM to query the hash of the header of
   * the given block. If the information about the requested block is not
   * available, then this is signalled by returning null bytes.
   *
   * @param num      The block number.
   * @return         The block hash or null bytes
   *                 if the information about the block is not available.
   */
  abstract getBlockHash(num: bigint): Promise<bigint>|bigint;

  /**
   * Log callback function.
   *
   *  This callback function is used by an EVM to inform about a LOG that
   * happened during an EVM bytecode execution.
   *  @param address       The address of the contract that generated the log.
   *  @param data          The buffer to unindexed data attached to the log.
   *  @param topics        An array of topics attached to the log.
   */
  abstract emitLog(address: bigint, data: Buffer, topics: Array<bigint>):
      Promise<void>|void;

  /**
   * Access account callback function.
   *
   * This callback function is used by a VM to add the given address
   * to accessed_addresses substate (EIP-2929).
   *
   * @param context  The Host execution context.
   * @param address  The address of the account.
   * @return         EVMC_ACCESS_WARM if accessed_addresses already contained
   * the address or EVMC_ACCESS_COLD otherwise.
   */
  abstract accessAccount(account: bigint):
      Promise<EvmcAccessStatus>|EvmcAccessStatus;

  /**
   * Access storage callback function.
   *
   * This callback function is used by a VM to add the given account storage
   * entry to accessed_storage_keys substate (EIP-2929).
   *
   * @param context  The Host execution context.
   * @param address  The address of the account.
   * @param key      The index of the account's storage entry.
   * @return         EVMC_ACCESS_WARM if accessed_storage_keys already contained
   * the key or EVMC_ACCESS_COLD otherwise.
   */
  abstract accessStorage(address: bigint, key: bigint):
      Promise<EvmcAccessStatus>|EvmcAccessStatus;

  /**
   * Executes the given EVM bytecode using the input in the message
   * @param msg        Call parameters.
   * @param code       Reference to the bytecode to be executed.
   * @param rev        Requested EVM specification revision.
   */
  execute(
      message: EvmcMessage, code: Buffer,
      revision = EvmcRevision.EVMC_MAX_REVISION): EvmcResult {
    if (this.released) {
      throw new Error('EVM has been released!');
    }
    return evmc.executeEvmcEvm(this._evm, {revision, message, code});
  }

  /**
   * Releases all resources from this EVM. Once released, you may no longer
   * call execute.
   */
  release() {
    evmc.releaseEvmcEvm(this._evm);
    this.released = true;
  }
}