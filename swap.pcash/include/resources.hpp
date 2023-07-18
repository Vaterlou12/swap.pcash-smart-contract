#pragma once
#include <eosio/eosio.hpp>
#include <eosio/crypto.hpp>

using namespace eosio;

#ifdef DEBUG
    const uint32_t min_inh_period = 2;
    const uint32_t max_inh_period = 5;

    constexpr name TOKEN_PCASH_ACCOUNT("cash.token");
    constexpr name FEE_RECEIVER_ACCOUNT("fee.pcash");
#else
    const uint32_t min_inh_period = 86400; //1 day
    const uint32_t max_inh_period = 315360000; //10 years in sec

    #ifdef PREPROD
        constexpr name TOKEN_PCASH_ACCOUNT("cashescashes");
        constexpr name FEE_RECEIVER_ACCOUNT("kuphqnfckybk");
    #else
        constexpr name TOKEN_PCASH_ACCOUNT("token.pcash");
        constexpr name FEE_RECEIVER_ACCOUNT("sw.pcash");
    #endif
#endif

const std::string swap_prefix("swap:");
const std::string deposit_prefix("deposit:");

constexpr symbol fee_percent("PERCENT", 2);
const asset pool_fee(20, fee_percent);
const asset platform_fee(5, fee_percent);

const int64_t min_swap_amount = 800;

constexpr symbol inh_percent("PERCENT", 1);
const asset min_percent(1, inh_percent);
const asset max_percent(1000, inh_percent);

struct transfer_action
{
    name from;
    name to;
    asset quantity;
    std::string memo;

    EOSLIB_SERIALIZE(transfer_action, (from)(to)(quantity)(memo))
};

struct deposit
{
    name from;
    extended_asset quantity;
    std::string memo;

    EOSLIB_SERIALIZE(deposit, (from)(quantity)(memo))
};

bool operator==(const deposit &lhs, const deposit &rhs)
{
    return (lhs.from == rhs.from && lhs.quantity.quantity.symbol == rhs.quantity.quantity.symbol && lhs.quantity.quantity.amount == rhs.quantity.quantity.amount && lhs.memo == rhs.memo) ? true : false;
}

std::string to_string(const extended_symbol &token)
{
    std::string str = token.get_symbol().code().to_string() + "@" + token.get_contract().to_string();
    return str;
}

std::string to_string(const extended_asset &token)
{
    std::string str = std::to_string(token.quantity.amount) + " " + to_string(token.get_extended_symbol());
    return str;
}

checksum256 to_pair_hash(const extended_symbol &token1, const extended_symbol &token2)
{
    std::string str = to_string(token1) + "/" + to_string(token2);
    return sha256(str.data(), str.size());
}