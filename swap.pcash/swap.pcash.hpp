#pragma once
#include <cmath>
#include <eosio/eosio.hpp>
#include <eosio/asset.hpp>
#include <eosio/system.hpp>
#include <eosio/transaction.hpp>
#include "account.hpp"
#include "inheritrance.hpp"
#include "stat.hpp"
#include "pool.hpp"
#include "resources.hpp"

using namespace eosio;

class [[eosio::contract("swap.pcash")]] swap : public contract
{
public:
    swap(name receiver, name code, datastream<const char *> ds);

    [[eosio::action("open")]] void open(const name &owner, const symbol &symbol, const name &ram_payer);

    [[eosio::action("close")]] void close(const name &owner, const symbol &symbol);

    [[eosio::action("withdraw")]] void withdraw(const name &owner, const asset &lq_tokens);

    [[eosio::action("create")]] void create_token(const name &issuer, const asset &maximum_supply);

    //For managing liquidity tokens
    [[eosio::action("issue")]] void issue(const name &to, const asset &quantity, const std::string &memo);

    [[eosio::action("retire")]] void retire(const name &from, const asset &quantity, const std::string &memo);

    [[eosio::action("transfer")]] void transfer_token(const name &from, const name &to, const asset &quantity, const std::string &memo);

    //For managing pools
    [[eosio::action("createpool")]] void create_pool(const name &creator, const extended_symbol &token1, const extended_symbol &token2);

    [[eosio::action("removepool")]] void remove_pool(const uint64_t &pool_id);

    //For init inheritance distribution
    [[eosio::action("dstrinh")]] void distribute_inheritance(const name &initiator, const name &inheritance_owner, const symbol_code &token);

    //For inheritor programm
    [[eosio::action("updinhdate")]] void update_inheritance_date(const name &owner, const uint32_t &inactive_period);

    [[eosio::action("updtokeninhs")]] void update_inheritors(const name &owner, const std::vector<inheritor_record> &inheritors);

    //For notifying
    [[eosio::action("swapdetails")]] void swap_details(const uint64_t &pool_id, const name &owner, const extended_asset &token_in, const extended_asset &token_out, const extended_asset &pool_fee, const extended_asset &platform_fee, const double &price);

    [[eosio::action("addlqdetails")]] void add_lq_details(const uint64_t &pool_id, const name &owner, const asset &lqtoken, const extended_asset &token1, const extended_asset &token2);

    [[eosio::action("rmvlqdetails")]] void remove_lq_details(const uint64_t &pool_id, const name &owner, const asset &lqtoken, const extended_asset &token1, const extended_asset &token2);

    [[eosio::action("notify")]] void notify(const std::string &action_type, const name &to, const name &from, const asset &quantity, const std::string &memo);

    //For incoming payments
    [[eosio::on_notify("*::transfer")]] void on_transfer(const name &from, const name &to, const asset &quantity, const std::string &memo);

private:
    void on_transfer_self_token(const name &from, const name &to, const asset &quantity, const std::string &memo);

    void do_swap(const name &from, const asset &quantity, const std::string &memo, const std::string &assert_prefix);
    void do_deposit(const name &from, const asset &quantity, const std::string &memo, const std::string &assert_prefix);

    void add_balance(const name &user, const asset &quantity, const name &ram_payer);
    void sub_balance(const name &user, const asset &quantity);

    void add_pool_balance(const uint64_t &pool_id, const extended_asset &token1, const extended_asset &token2);
    void sub_pool_balance(const uint64_t &pool_id, const extended_asset &token1, const extended_asset &token2);

    void add_pool_balance(const uint64_t &pool_id, const extended_asset &tokens);
    void sub_pool_balance(const uint64_t &pool_id, const extended_asset &tokens);

    void create_inheritance(const name &owner, const name &ram_payer);
    void close_inheritance(const name &owner);
    void extend_inheritance(const name &owner, const name &ram_payer);

    void add_inh_balances(const name &owner, const asset &value, const std::vector<inheritor_record> &inheritors, const name &ram_payer);
    void add_inh_balance(const name &from, const name &to, const asset &value, const name &ram_payer);

    void send_inheritance(const name &owner, const asset &quantity, const std::vector<inheritor_record> &inheritors,
                          const int64_t &min_amount, const name &ram_payer);

    std::vector<deposit> parse_deposit_actions(const transaction &trx);

    std::vector<std::string>
    split(const std::string &s, const std::string &delimiter);

    std::map<std::string, std::string>
    to_key_value(const std::string &memo);
    symbol to_pool_symbol(uint64_t pool_id);
    std::vector<uint64_t> to_uint64_ids(const std::vector<std::string> &str);

    asset count_share(const asset &quantity, const asset &share);

    asset count_lq_tokens(const asset &supply, const extended_asset &amount1_in, const extended_asset &amount1_before);

    std::tuple<asset, extended_asset, extended_asset, extended_asset>
    count_deposit_amounts(const asset &lq_supply, const pool &current_pool, const extended_asset &token1, const extended_asset &token2);

    std::tuple<asset, extended_asset, extended_asset, extended_asset>
    count_add_lq_amounts(const uint64_t &pool_id, const extended_asset &token1, const extended_asset &token2);

    std::tuple<extended_asset, extended_asset>
    count_earnings_amounts(const asset &lqtokens);

    extended_asset count_platform_fee(const asset &platform_fee, const extended_asset &income);

    std::tuple<extended_asset, extended_asset>
    count_swap_fees(const extended_asset &income, const asset &pool_fee, const asset &platform_fee);

    std::tuple<extended_asset, extended_asset, extended_asset, extended_asset, name, double>
    count_swap_amounts(const uint64_t &pool_id, const extended_asset &income);

    uint64_t get_new_pool_id(const uint64_t &available_id);
    asset get_lq_supply(const symbol_code &token);
    std::tuple<extended_asset, extended_asset> get_pool_tokens(const symbol_code &pool_code);
    transaction get_income_trx();
    uint64_t get_pool_id(const symbol_code &code);
    time_point_sec get_inheritance_exp_date(const uint32_t &inactive_period);

    bool is_account_exist(const name &owner, const extended_symbol &token);
    bool is_valid_deposits(const std::vector<deposit> &deposits);
    bool is_initial_add_lq(const asset &supply, const extended_asset &token1, const extended_asset &token2);

    bool is_token_exist(const extended_symbol &token);
    bool is_lq_tokens(const extended_symbol &token);
    bool is_pool_exist(const uint64_t &pool_id);
    bool is_pool_exist(const symbol_code &code);
    bool is_pool_exist(const extended_symbol &token1, const extended_symbol &token2);
    bool is_pools_exist(const std::vector<uint64_t> &pool_ids);

    bool is_pool_match(const uint64_t &pool_id, const extended_asset &income);
    bool is_pool_match(const uint64_t &pool_id, const extended_asset &token1, const extended_asset &token2);
    bool is_last_deposit(const deposit &current_deposit, const std::vector<deposit> &deposits);

    bool is_swap_memo(const std::string &memo);
    bool is_deposit_memo(const std::string &memo);

    bool is_digit(const std::string &str);
    bool is_digit(const std::vector<std::string> &str);

    std::tuple<bool, std::vector<uint64_t>, uint64_t>
    is_valid_swap_memo(const std::map<std::string, std::string> &params);

    std::tuple<bool, uint64_t>
    is_valid_deposit_memo(const std::map<std::string, std::string> &params);

    bool is_valid_inactive_period(const uint32_t &inactive_period);
    bool is_not_self_in_inheritors(const name &owner, const std::vector<inheritor_record> &inheritors);
    bool is_inheritors_unique(const std::vector<inheritor_record> &inheritors);
    bool is_valid_inheritors_amount(const size_t &size);
    bool is_valid_share(const asset &share);
    bool is_valid_share_sum(const asset &sum);
    bool is_valid_inheritors(const std::vector<inheritor_record> &inheritors);

    void send_issue(const name &to, const asset &quantity, const std::string &memo);
    void send_retire(const name &from, const asset &quantity, const std::string &memo);
    void send_transfer(const name &contract, const name &to, const asset &quantity, const std::string &memo);
    void send_swap_details(const uint64_t &pool_id, const name &owner, const extended_asset &token_in, const extended_asset &token_out, const extended_asset &pool_fee, const extended_asset &platform_fee, const double &price);
    void send_add_lq_details(const uint64_t &pool_id, const name &owner, const asset &lqtoken, const extended_asset &token1, const extended_asset &token2);
    void send_rmv_lq_details(const uint64_t &pool_id, const name &owner, const asset &lqtoken, const extended_asset &token1, const extended_asset &token2);
    void send_notify(const std::string &action_type, const name &to, const name &from, const asset &quantity, const std::string &memo);
};
