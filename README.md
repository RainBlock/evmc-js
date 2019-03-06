# ☔️🔗 EVMC bindings for node.js

This project includes node.js bindings for the [Ethereum VM Connector API](https://github.com/ethereum/evmc). It is capable of loading EVM implementations such as [aleth-interpreter](https://github.com/ethereum/aleth/tree/master/libaleth-interpreter), [evmjit](https://github.com/ethereum/evmjit) and [Hera](https://github.com/ewasm/hera) and enables interop with javascript code.

# Usage

Install evmc-js to your project using npm:
```
npm i evmc
```

You'll need to extend the provided EVMC class and implement the various abstract function calls. You'll also need to pass the path to the EVMC shared library you're targeting in the constructor. See the unit test for an example, but an abbreviated version of the class would look like:

```typescript
import {Evmc} from 'evmc';

class MyEVM extends Evmc {

async getAccountExists(account: bigint) {
    // check if account exists and return..
    return doesAccountExists(account);
}

// and so on for all the callbacks...
}
```
Callbacks can be asynchronous (i.e., return a `Promise`), or synchronous. See the documentation for full details on the API you need to implement.

When you're ready to execute, you instantiate a instance of your `Evmc` class and call the `execute` function using the message and code you with to execute, as below:

```typescript
const evm = new MyEVM(evmPath);
const result = await evm.execute(message, code));
```

Execution is asynchronous, but (for now), you should not call execute concurrently.
However, you may instantiate multiple EVMs and run them concurrently. Each EVM runs on its
own thread outside of the main event loop, so you can take full advantage of the parallelism
available on the machine.

# Roadmap

Currently, the C part of the binding could use a lot of cleanup and it does have a lot of repetitive code.

In addition, there are a lot of assertions which kill the Node process, which should probably throw an error back to javascript so the error can be handled gracefully.

Tracing is not yet supported, but could be easily added for EVMs with support.