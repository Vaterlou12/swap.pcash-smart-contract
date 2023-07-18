# swap.pcash-smart-contract

The swap.pcash smart contract is designed to be AMM for EOS tokens.

# Dependencies

* eosio 2.0^
* eosio.cdt 1.6^
* cmake 3.5^

# Compiling

```
./build.sh -e /root/eosio/2.0 -c /usr/opt/eosio.cdt
```

# Deploying

```
cleos set contract <your_account> ./build/Release/swap.pcash swap.pcash.wasm swap.pcash.abi
```