import * as benchmark from 'benchmark';
import * as path from 'path';
import * as process from 'process';
import * as util from 'util';

import {Evmc, EvmcAccessStatus, EvmcCallKind, EvmcMessage, EvmcStatusCode, EvmcStorageStatus} from './evmc';

const evmasm = require('evmasm');

interface BenchmarkRun {
  name: string;
  hz: number;
  stats: benchmark.Stats;
}

// This file contains the benchmark test suite. It includes the benchmark and
// some lightweight boilerplate code for running benchmark.js. To
// run the benchmarks, execute `npm run benchmark` from the package directory.
const runSuite =
    (suite: benchmark.Suite, name: string, async = false): Promise<void> => {
      return new Promise((resolve, reject) => {
        console.log(`\nRunning ${name}...`);
        // Reporter for each benchmark
        suite.on('cycle', (event: benchmark.Event) => {
          const benchmarkRun: BenchmarkRun = event.target as BenchmarkRun;
          const stats = benchmarkRun.stats as benchmark.Stats;
          const meanInNanos = (stats.mean * 1000000000).toFixed(2);
          const stdDevInNanos = (stats.deviation * 1000000000).toFixed(3);
          const runs = stats.sample.length;
          const ops = benchmarkRun.hz.toFixed(benchmarkRun.hz < 100 ? 2 : 0);
          const err = stats.rme.toFixed(2);

          console.log(
              `${benchmarkRun.name}: ${ops}±${err}% ops/s ${meanInNanos}±${
                  stdDevInNanos} ns/op (${runs} run${runs === 0 ? '' : 's'})`);
        });

        suite.on('complete', () => {
          console.log(
              'Fastest is ' +
              suite.filter('fastest').map('name' as unknown as Function));
          resolve();
        });
        // Runs the test suite
        suite.run({async});
      });
    };

interface BenchmarkDeferrable {
  resolve: () => void;
}

interface BenchmarkRun {
  name: string;
  hz: number;
  stats: benchmark.Stats;
}

/**
 * Simple wrapper for benchmark.js to add an asynchronous test.
 *  @param name         The name of the test to run.
 *  @param asyncTest    An async function which contains the test to be run. If
 * a setup function is provided, the state will be present in the {state}
 * parameter. Otherwise, the {state} parameter will be undefined.
 *  @param setup        Optional setup which provides state to {asyncTest}.
 */
const addAsyncTest = <T>(
    name: string, asyncTest: (state: T) => Promise<void>, setup?: () => T) => {
  let state: T;
  suite.add(name, {
    defer: true,
    setup: () => {
      if (setup !== undefined) {
        state = setup();
      }
    },
    fn: (deferred: BenchmarkDeferrable) => {
      asyncTest(state).then(() => deferred.resolve());
    }
  });
};


let suite = new benchmark.Suite();
// Tests the performance of a no-op.
suite.add('no-op', () => {});
runSuite(suite, 'basic');


const STORAGE_ADDRESS = 0x42n;
const STORAGE_VALUE = 0x05n;
const BALANCE_ACCOUNT = 0x174201554d57715a2382555c6dd9028166ab20ean;
const BALANCE_BALANCE = 0xabcdef123455n;
const BALANCE_CODESIZE = 24023n;
const BALANCE_CODEHASH =
    0xecd99ffdcb9df33c9ca049ed55f74447201e3774684815bc590354427595232bn;

const BLOCK_NUMBER = 0x10001000n;
const BLOCK_COINBASE = 0x2fab01632ab26a6349aedd19f5f8e4bbd477718n;
const BLOCK_TIMESTAMP = 1551402771n;
const BLOCK_GASLIMIT = 100000n;
const BLOCK_DIFFICULTY = 2427903418305647n;
const BLOCK_BASE_FEE = 0n;

const CHAIN_ID = 1n;

const TX_ORIGIN = 0xEA674fdDe714fd979de3EdF0F56AA9716B898ec8n;
const TX_GASPRICE = 100n;
const TX_DESTINATION = BALANCE_ACCOUNT;
const TX_GAS = 60000n;

const CALL_ACCOUNT = 0x44fD3AB8381cC3d14AFa7c4aF7Fd13CdC65026E1n;
const CODE_INPUT_DATA = Buffer.from(
    'ccd99eedcb9df33c9ca049ed55f74447201e3774684815bc590354427595232b', 'hex');
const CODE_OUTPUT_DATA = Buffer.from(
    '0xb745858cc23a311a303b43f18813d7331a257a817201576533298ffbe3809b32',
    'hex');

const BLOCKHASH_NUM = BLOCK_NUMBER - 4n;
const BLOCKHASH_HASH =
    0xecd99ffdcb9df33c9ca049ed55f74447201e3774684815bc590354427595232bn;

const SELF_DESTRUCT_BENEFICIARY = 0xA643e67B31F2E0A7672FD87d3faa28eAa845E311n;

const LOG_DATA = Buffer.from([0xab, 0xfe]);
const LOG_TOPIC1 =
    0xecd99eedcb9df33c9ca049ed55f74447201e3774684815bc590354427595232bn;
const LOG_TOPIC2 = 0x2fab01632ab26a6349aedd19f5f8e4bbd47771n;

const CODE_ACCOUNT = 0xa53432ff16287dae8c4e09209a70cca8aaa3f50an;
const CODE_CODE = Buffer.from(
    'ecd99eedcb9df33c9ca049ed55f74447201e3774684815bc590354427595232b', 'hex');

const EVM_MESSAGE = {
  kind: EvmcCallKind.EVMC_CALL,
  sender: TX_ORIGIN,
  depth: 0,
  destination: TX_DESTINATION,
  gas: TX_GAS,
  inputData: Buffer.from([]),
  value: 0n
};


class TestEVM extends Evmc {
  async getAccountExists(account: bigint) {
    // Accounts always exist
    return true;
  }

  async getStorage(account: bigint, key: bigint) {
    // Our "test" storage always returns the magic number
    if (key === STORAGE_ADDRESS) {
      return STORAGE_VALUE;
    }
    throw new Error(`Invalid test addresss (got ${key.toString(16)}`);
  }

  async setStorage(account: bigint, key: bigint, val: bigint) {
    if (key === STORAGE_ADDRESS && val === STORAGE_VALUE) {
      return EvmcStorageStatus.EVMC_STORAGE_ADDED;
    }
    throw new Error(`Invalid storage address (got ${key.toString(16)})`);
  }

  async getBalance(account: bigint) {
    if (account === BALANCE_ACCOUNT) {
      return BALANCE_BALANCE;
    }
    throw new Error(`Invalid balance account (got ${account.toString(16)})`);
  }

  async getCodeSize(account: bigint) {
    if (account === BALANCE_ACCOUNT) {
      return BALANCE_CODESIZE;
    }
    throw new Error(`Invalid code size account (got ${account.toString(16)})`);
  }

  async copyCode(account: bigint, offset: number, length: number) {
    if (account === CODE_ACCOUNT && offset === 0 &&
        length === CODE_CODE.length) {
      return CODE_CODE;
    }
    throw new Error(`Invalid code to copy for ${CODE_ACCOUNT}`);
  }

  async selfDestruct(account: bigint, beneficiary: bigint) {
    if (account === TX_ORIGIN && beneficiary === SELF_DESTRUCT_BENEFICIARY) {
      return;
    }
    throw new Error(
        `Self destruct on unexpected origin or beneficary (origin: ${
            account.toString(16)} beneficairy:${beneficiary.toString(16)})`);
  }

  async getCodeHash(account: bigint) {
    if (account === BALANCE_ACCOUNT) {
      return BALANCE_CODEHASH;
    }
    throw new Error(`Invalid code hash account (got ${account.toString(16)})`);
  }

  async call(message: EvmcMessage) {
    if (message.inputData.equals(CODE_INPUT_DATA)) {
      return {
        statusCode: EvmcStatusCode.EVMC_SUCCESS,
        gasLeft: 10000n,
        outputData: CODE_OUTPUT_DATA,
        createAddress: 0n
      };
    }
    throw new Error(`Unexpected input message ${util.inspect(message)}`);
  }

  async getTxContext() {
    return {
      txGasPrice: TX_GASPRICE,
      txOrigin: TX_ORIGIN,
      blockCoinbase: BLOCK_COINBASE,
      blockNumber: BLOCK_NUMBER,
      blockTimestamp: BLOCK_TIMESTAMP,
      blockGasLimit: BLOCK_GASLIMIT,
      blockDifficulty: BLOCK_DIFFICULTY,
      chainId: CHAIN_ID,
      blockBaseFee: BLOCK_BASE_FEE
    };
  }

  async getBlockHash(num: bigint) {
    if (num === BLOCKHASH_NUM) {
      return BLOCKHASH_HASH;
    }
    throw new Error(
        `Unexpected block number requested for blockhash (got ${num})`);
  }

  async emitLog(account: bigint, data: Buffer, topics: Array<bigint>) {
    if (account === BALANCE_ACCOUNT && data.equals(LOG_DATA) &&
        topics.length === 2 && topics[0] === LOG_TOPIC1 &&
        topics[1] === LOG_TOPIC2) {
      return;
    }
    throw new Error(`Unexpected log emitted: account: ${
        account.toString(16)} data: ${data.toString('hex')} topics: ${topics}`);
  }

  accessAccount(account: bigint) {
    return EvmcAccessStatus.EVMC_ACCESS_COLD;
  }

  accessStorage(address: bigint, key: bigint) {
    return EvmcAccessStatus.EVMC_ACCESS_COLD;
  }
}

const getDynamicLibraryExtension = () => {
  return process.platform === 'win32' ?
      'dll' :
      process.platform === 'darwin' ? 'dylib' : 'so';
};

const evmonePath = path.join(
    __dirname,
    `../libbuild/evmone/lib/libevmone.${getDynamicLibraryExtension()}`);
// Test the performance of evmc creation
suite = new benchmark.Suite('evmc_creation');
suite.add('create', () => {
  const evm = new TestEVM(evmonePath);
  evm.release();
});
runSuite(suite, 'evmc_creation');
// Test the performance of evmc execution
suite = new benchmark.Suite('evmc_execution');

const MAX_PARALLELISM = 4;
const evm: Evmc[] = [];
for (let i = 0; i < MAX_PARALLELISM; i++) {
  evm.push(new TestEVM(evmonePath));
}
const SIMPLE_MESSAGE = {
  kind: EvmcCallKind.EVMC_CALL,
  sender: TX_ORIGIN,
  depth: 0,
  destination: TX_DESTINATION,
  gas: TX_GAS,
  inputData: Buffer.from([]),
  value: 0n,
  create2Salt: 0n
};

const SINGLE_STORE_CONTRACT = Buffer.from(
    evmasm.compile(`
  sstore(0x${STORAGE_ADDRESS.toString(16)}, 0x${STORAGE_VALUE.toString(16)})
`),
    'hex');

const TEN_STORE_CONTRACT = Buffer.from(
    evmasm.compile(`
  sstore(0x${STORAGE_ADDRESS.toString(16)}, 0x${STORAGE_VALUE.toString(16)})
  sstore(0x${STORAGE_ADDRESS.toString(16)}, 0x${STORAGE_VALUE.toString(16)})
  sstore(0x${STORAGE_ADDRESS.toString(16)}, 0x${STORAGE_VALUE.toString(16)})
  sstore(0x${STORAGE_ADDRESS.toString(16)}, 0x${STORAGE_VALUE.toString(16)})
  sstore(0x${STORAGE_ADDRESS.toString(16)}, 0x${STORAGE_VALUE.toString(16)})
  sstore(0x${STORAGE_ADDRESS.toString(16)}, 0x${STORAGE_VALUE.toString(16)})
  sstore(0x${STORAGE_ADDRESS.toString(16)}, 0x${STORAGE_VALUE.toString(16)})
  sstore(0x${STORAGE_ADDRESS.toString(16)}, 0x${STORAGE_VALUE.toString(16)})
  sstore(0x${STORAGE_ADDRESS.toString(16)}, 0x${STORAGE_VALUE.toString(16)})
  sstore(0x${STORAGE_ADDRESS.toString(16)}, 0x${STORAGE_VALUE.toString(16)})
`),
    'hex');


addAsyncTest('async no-op', async () => {});
addAsyncTest('execute null contract', async () => {
  await evm[0].execute(SIMPLE_MESSAGE, Buffer.from([]));
});
addAsyncTest('parallel execute null contract', async () => {
  await Promise.all(evm.map(e => {
    return e.execute(SIMPLE_MESSAGE, Buffer.from([]));
  }));
});

addAsyncTest('execute 1x store contract', async () => {
  await evm[0].execute(SIMPLE_MESSAGE, SINGLE_STORE_CONTRACT);
});
addAsyncTest('parallel execute 1x store contract', async () => {
  await Promise.all(evm.map(e => {
    return e.execute(SIMPLE_MESSAGE, SINGLE_STORE_CONTRACT);
  }));
});

addAsyncTest('execute 10x store contract', async () => {
  await evm[0].execute(SIMPLE_MESSAGE, TEN_STORE_CONTRACT);
});
addAsyncTest('parallel execute 10x store contract', async () => {
  await Promise.all(evm.map(e => {
    return e.execute(SIMPLE_MESSAGE, TEN_STORE_CONTRACT);
  }));
});

const evmExeuctionRun = async () => {
  await runSuite(suite, 'evmc_execution');
  evm.map(e => {
    e.release();
  });
};

evmExeuctionRun();
