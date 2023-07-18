#pragma once
#include <eosio/eosio.hpp>
#include <eosio/asset.hpp>
#include <eosio/system.hpp>
#include <eosio/time.hpp>

using namespace eosio;

struct inheritor_record
{
    name inheritor;
    asset share;

    EOSLIB_SERIALIZE(inheritor_record, (inheritor)(share))
};

struct [[eosio::contract("swap.pcash"), eosio::table]] member
{
    name user_name;
    time_point_sec inheritance_date;
    uint32_t inactive_period;
    std::vector<inheritor_record> inheritors;

    uint64_t primary_key() const
    {
        return user_name.value;
    }
    uint64_t date_key() const
    {
        return (uint64_t)inheritance_date.utc_seconds;
    }
};
using by_date = indexed_by<name("bydate"), const_mem_fun<member, uint64_t, &member::date_key>>;
using inheritance = multi_index<name("inheritance"), member, by_date>;
