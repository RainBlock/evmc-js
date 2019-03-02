import 'mocha';

import * as chai from 'chai';
import * as path from 'path';

import {Evmc, EvmcCallKind, EvmcMessage, EvmcStatusCode, EvmcStorageStatus} from './evmc';

const evmasm = require('evmasm');

require('segfault-handler').registerHandler();

// Needed for should.not.be.undefined.
/* tslint:disable:no-unused-expression */

// tslint:disable-next-line:no-any
const assertEquals = (n0: any, n1: any) => {
  n0.toString(16).should.equal(n1.toString(16));
};

chai.should();
const should = chai.should();

const STORAGE_ADDRESS = 0x42n;
const STORAGE_VALUE = 0x05n;
const BALANCE_ACCOUNT = 0x174201554d57715a2382555c6dd9028166ab20ean;
const BALANCE_BALANCE = 0xabcdef123455n;
const BALANCE_CODESIZE = 24023n;

const BLOCK_NUMBER = 0x10001000n;
const BLOCK_COINBASE = 0x2fab01632ab26a6349aedd19f5f8e4bbd477718n;
const BLOCK_TIMESTAMP = 1551402771n;
const BLOCK_GASLIMIT = 100000n;
const BLOCK_DIFFICULTY = 2427903418305647n;

const TX_ORIGIN = 0xEA674fdDe714fd979de3EdF0F56AA9716B898ec8n;
const TX_GASPRICE = 100n;
const TX_DESTINATION = BALANCE_ACCOUNT;
const TX_GAS = 60000n;

const BLOCKHASH_NUM = BLOCK_NUMBER - 4n;
const BLOCKHASH_HASH =
    0xecd99ffdcb9df33c9ca049ed55f74447201e3774684815bc590354427595232bn;

const SELF_DESTRUCT_BENEFICIARY = 0xA643e67B31F2E0A7672FD87d3faa28eAa845E311n;

const LOG_DATA = Buffer.from([0xab, 0xfe]);
const LOG_TOPIC1 =
    0xecd99eedcb9df33c9ca049ed55f74447201e3774684815bc590354427595232bn;
const LOG_TOPIC2 = 0x2fab01632ab26a6349aedd19f5f8e4bbd47771n;

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

  async copyCode(account: bigint, offset: bigint, data: Buffer) {
    return 0n;
  }

  async selfDestruct(account: bigint, beneficiary: bigint) {
    if (account === TX_ORIGIN && beneficiary === SELF_DESTRUCT_BENEFICIARY) {
      return;
    }
    throw new Error(
        `Self destruct on unexpected origin or beneficary (origin: ${
            account.toString(16)} beneficairy:${beneficiary.toString(16)})`);
  }

  async call(message: EvmcMessage) {
    return {
      statusCode: EvmcStatusCode.EVMC_SUCCESS,
      gasLeft: 0n,
      outputData: Buffer.from([]),
      createAddress: 0n
    };
  }

  async getTxContext() {
    return {
      txGasPrice: TX_GASPRICE,
      txOrigin: TX_ORIGIN,
      blockCoinbase: BLOCK_COINBASE,
      blockNumber: BLOCK_NUMBER,
      blockTimestamp: BLOCK_TIMESTAMP,
      blockGasLimit: BLOCK_GASLIMIT,
      blockDifficulty: BLOCK_DIFFICULTY
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
    if (account === BALANCE_ACCOUNT && data.equals(LOG_DATA) && topics.length === 2 && topics[0] === LOG_TOPIC1 && topics[1] === LOG_TOPIC2) {
        return;
    }
    throw new Error(`Unexpected log emitted: account: ${account.toString(16)} data: ${data.toString('hex')} topics: ${topics}`);
  }
}

describe('Try EVM creation', () => {
  let evm: TestEVM;

  it('should be created', () => {
    evm = new TestEVM(path.join(
        __dirname,
        '../libbuild/aleth/libaleth-interpreter/libaleth-interpreter.dylib'));
  });

  it('should fail to execute a bad message', async () => {
    const result = await evm.execute(EVM_MESSAGE, Buffer.from([0xfe]));
    result.statusCode.should.equal(EvmcStatusCode.EVMC_UNDEFINED_INSTRUCTION);
  });

  it('should successfully execute a STOP opcode', async () => {
    const result = await evm.execute(EVM_MESSAGE, Buffer.from([0x00]));
    result.statusCode.should.equal(EvmcStatusCode.EVMC_SUCCESS);
    assertEquals(TX_GAS, result.gasLeft);
  });

  it('should successfully read magic from storage', async () => {
    const result = await evm.execute(
        EVM_MESSAGE,
        Buffer.from(
            evmasm.compile(`
          jumpi(success, eq(sload(${STORAGE_ADDRESS}), ${STORAGE_VALUE}))
          data(0xFE) // Invalid Opcode
          success:
          stop
          `),
            'hex'));
    result.statusCode.should.equal(EvmcStatusCode.EVMC_SUCCESS);
  });

  it('should successfully write magic to storage', async () => {
    const result = await evm.execute(
        EVM_MESSAGE,
        Buffer.from(
            evmasm.compile(`
          sstore(${STORAGE_ADDRESS}, ${STORAGE_VALUE})
          `),
            'hex'));
    result.statusCode.should.equal(EvmcStatusCode.EVMC_SUCCESS);
  });

  it('should successfully get the senders balance', async () => {
    const result = await evm.execute(
        EVM_MESSAGE,
        Buffer.from(
            evmasm.compile(`
          jumpi(success, eq(balance(0x${BALANCE_ACCOUNT.toString(16)}), ${
                BALANCE_BALANCE}))
          data(0xFE) // Invalid Opcode
          success:
          stop
          `),
            'hex'));
    result.statusCode.should.equal(EvmcStatusCode.EVMC_SUCCESS);
  });


  it('should successfully get block number', async () => {
    const result = await evm.execute(
        EVM_MESSAGE,
        Buffer.from(
            evmasm.compile(`
          jumpi(success, eq(blocknumber(), 0x${BLOCK_NUMBER.toString(16)}))
          data(0xFE) // Invalid Opcode
          success:
          stop
          `),
            'hex'));
    result.statusCode.should.equal(EvmcStatusCode.EVMC_SUCCESS);
  });

  it('should successfully get the timestamp', async () => {
    const result = await evm.execute(
        EVM_MESSAGE,
        Buffer.from(
            evmasm.compile(`
          jumpi(success, eq(timestamp(), 0x${BLOCK_TIMESTAMP.toString(16)}))
          data(0xFE) // Invalid Opcode
          success:
          stop
          `),
            'hex'));
    result.statusCode.should.equal(EvmcStatusCode.EVMC_SUCCESS);
  });

  it('should successfully get the coinbase', async () => {
    const result = await evm.execute(
        EVM_MESSAGE,
        Buffer.from(
            evmasm.compile(`
          jumpi(success, eq(coinbase(), 0x${BLOCK_COINBASE.toString(16)}))
          data(0xFE) // Invalid Opcode
          success:
          stop
          `),
            'hex'));
    result.statusCode.should.equal(EvmcStatusCode.EVMC_SUCCESS);
  });

  it('should successfully get the difficulty', async () => {
    const result = await evm.execute(
        EVM_MESSAGE,
        Buffer.from(
            evmasm.compile(`
          jumpi(success, eq(difficutly(), 0x${BLOCK_DIFFICULTY.toString(16)}))
          data(0xFE) // Invalid Opcode
          success:
          stop
          `),
            'hex'));
    result.statusCode.should.equal(EvmcStatusCode.EVMC_SUCCESS);
  });


  it('should successfully get the gas limit', async () => {
    const result = await evm.execute(
        EVM_MESSAGE,
        Buffer.from(
            evmasm.compile(`
          jumpi(success, eq(gaslimit(), 0x${BLOCK_GASLIMIT.toString(16)}))
          data(0xFE) // Invalid Opcode
          success:
          stop
          `),
            'hex'));
    result.statusCode.should.equal(EvmcStatusCode.EVMC_SUCCESS);
  });

  it('should successfully get the code size of an account', async () => {
    const result = await evm.execute(
        EVM_MESSAGE,
        Buffer.from(
            evmasm.compile(`
          jumpi(success, eq(extcodesize(0x${BALANCE_ACCOUNT.toString(16)}), 0x${
                BALANCE_CODESIZE.toString(16)}))
          data(0xFE) // Invalid Opcode
          success:
          stop
          `),
            'hex'));
    result.statusCode.should.equal(EvmcStatusCode.EVMC_SUCCESS);
  });

  it('should successfully successfully fetch a blockhash', async () => {
    const result = await evm.execute(
        EVM_MESSAGE,
        Buffer.from(
            evmasm.compile(`
          jumpi(success, eq(blockhash(0x${BLOCKHASH_NUM.toString(16)}), 0x${
                BLOCKHASH_HASH.toString(16)}))
          data(0xFE) // Invalid Opcode
          success:
          stop
          `),
            'hex'));
    result.statusCode.should.equal(EvmcStatusCode.EVMC_SUCCESS);
  });


  it('should successfully emit a log', async () => {
    const result = await evm.execute(
        EVM_MESSAGE,
        Buffer.from(
            evmasm.compile(`
            mstore(0, 0x${LOG_DATA.toString('hex')})
            log2(${32 - LOG_DATA.length}, ${LOG_DATA.length}, 0x${
                LOG_TOPIC1.toString(16)}, 0x${LOG_TOPIC2.toString(16)})
          `),
            'hex'));
    result.statusCode.should.equal(EvmcStatusCode.EVMC_SUCCESS);
  });
});