#pragma once
#include <eosio/asset.hpp>
#include <eosio/eosio.hpp>

using namespace eosio;

struct [[eosio::contract("swap.pcash"), eosio::table]] account
{
    asset balance;

    uint64_t primary_key() const
    {
        return balance.symbol.code().raw();
    }
};
using accounts = multi_index<name("accounts"), account>;