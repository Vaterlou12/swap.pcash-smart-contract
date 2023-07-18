#pragma once
#include <eosio/eosio.hpp>
#include <eosio/asset.hpp>
#include "resources.hpp"
using namespace eosio;

struct [[eosio::contract("swap.pcash"), eosio::table]] pool
{
    uint64_t id;
    symbol_code code;
    asset pool_fee;
    asset platform_fee;
    name fee_receiver;
    time_point_sec create_time;
    time_point_sec last_update_time;
    extended_asset token1;
    extended_asset token2;

    uint64_t primary_key() const {
        return id;
    }

    uint64_t code_key() const {
        return code.raw();
    }

    checksum256 pair_key() const {
        return to_pair_hash(token1.get_extended_symbol(), token2.get_extended_symbol());
    }
};
using by_code = indexed_by<name("bycode"), const_mem_fun<pool, uint64_t, &pool::code_key>>;
using by_pair_key = indexed_by<name("bypair"), const_mem_fun<pool, checksum256, &pool::pair_key>>;
using pools = multi_index<name("pools"), pool, by_code, by_pair_key>;